#include "frontend/sdl3_app.h"

#include <exception>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/fdc/disk_image.h"

namespace sony_msx::frontend {

namespace {
// kFrameCycles is private to hbf1xv_machine.cpp; the machine's own public
// frame_cycles_per_frame() accessor is the authoritative source used at
// runtime (never duplicated as a literal below). This constant only anchors
// the doc comment's ms/frame arithmetic for humans reading the source.
constexpr std::uint64_t kFrameCycles = 228 * 262;
}  // namespace

Sdl3App::Sdl3App(Sdl3AppConfig config) : config_(std::move(config)) {}

Sdl3App::~Sdl3App() {
    shutdown();
}

bool Sdl3App::load_configured_assets() {
    using devices::cartridge::CartridgeLoadResult;

    auto load_cart = [&](const int slot_number, const std::optional<std::string>& path,
                         const devices::cartridge::CartridgeMapperType type) -> bool {
        if (!path.has_value()) {
            return true;
        }
        std::ifstream in(*path, std::ios::binary);
        if (!in) {
            last_error_ = "cannot open --cart" + std::to_string(slot_number) + " file: " + *path;
            return false;
        }
        const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                               std::istreambuf_iterator<char>());
        const CartridgeLoadResult result = machine_.load_cartridge(slot_number, type, image);
        if (result != CartridgeLoadResult::Ok) {
            last_error_ = "failed to load --cart" + std::to_string(slot_number) + " (" + *path + ")";
            return false;
        }
        return true;
    };

    if (!load_cart(1, config_.cart1_path, config_.cart1_type)) {
        return false;
    }
    if (!load_cart(2, config_.cart2_path, config_.cart2_type)) {
        return false;
    }

    // A-M26-6: real disk-image loading via the existing, unmodified
    // devices::fdc::DiskImage(bytes) constructor -- zero machine-level
    // change required.
    if (config_.disk_path.has_value()) {
        std::ifstream in(*config_.disk_path, std::ios::binary);
        if (!in) {
            last_error_ = "cannot open --disk file: " + *config_.disk_path;
            return false;
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        machine_.disk_image() = devices::fdc::DiskImage(std::move(bytes));
        machine_.disk_drive().attach_image(&machine_.disk_image());
    }

    return true;
}

bool Sdl3App::init() {
    if (initialized_) {
        return true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        last_error_ = SDL_GetError();
        return false;
    }
    sdl_initialized_ = true;

    const SDL_WindowFlags flags = config_.hidden_window ? SDL_WINDOW_HIDDEN : 0;
    if (!SDL_CreateWindowAndRenderer("sony-msx-hbf1xv", config_.window_width, config_.window_height, flags,
                                     &window_, &renderer_)) {
        last_error_ = SDL_GetError();
        shutdown();
        return false;
    }

    video_presenter_ = std::make_unique<Sdl3VideoPresenter>(renderer_, config_.border_enabled);

    audio_presenter_ = std::make_unique<Sdl3AudioPresenter>();
    if (!audio_presenter_->init()) {
        last_error_ = audio_presenter_->last_error();
        shutdown();
        return false;
    }

    // M27-S4, R-M27-2 (a real, easy-to-get-wrong sequencing constraint):
    // event logging MUST be enabled BEFORE cold_boot() to capture the Reset
    // event (hbf1xv_machine.h:306-309's own documented ordering
    // requirement) -- mirrors the headless --debug-session mode's identical
    // ordering exactly.
    if (config_.event_log_filename.has_value()) {
        machine_.set_event_logging_enabled(true);
    }

    machine_.set_asset_root(config_.bios_dir);
    machine_.cold_boot();
    if (!load_configured_assets()) {
        shutdown();
        return false;
    }

    if (config_.trace_cpu_filename.has_value()) {
        machine_.set_cpu_trace_enabled(true);
    }

    // M27-S7 (item 3, §2.4): load the scripted-input mechanism, if
    // configured. A malformed script is a real init() failure (mirrors
    // load_configured_assets()'s own "never partially initialize" contract),
    // never a silent no-op.
    if (config_.input_script_path.has_value()) {
        std::ifstream in(*config_.input_script_path, std::ios::binary);
        if (!in) {
            last_error_ = "cannot open --input-script file: " + *config_.input_script_path;
            shutdown();
            return false;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        try {
            input_script_player_ = machine::InputScriptPlayer(machine::parse_input_script(text));
        } catch (const std::exception& e) {
            last_error_ = std::string("malformed --input-script: ") + e.what();
            shutdown();
            return false;
        }
    }

    initialized_ = true;
    return true;
}

void Sdl3App::flush_debug_session_outputs() {
    // M27-S4 (docs/m27-planner-package.md §2.2, items 1/4): mirrors the
    // headless --debug-session mode's own end-of-run write-out exactly, via
    // the SAME already-existing Hbf1xvMachine APIs (M10-S3) -- zero new
    // machine-level method needed.
    if (config_.dump_state_filename.has_value()) {
        machine_.write_state_dump(*config_.dump_state_filename);
    }
    if (config_.trace_cpu_filename.has_value()) {
        machine_.write_cpu_trace(*config_.trace_cpu_filename);
    }
    if (config_.event_log_filename.has_value()) {
        machine_.write_event_log(*config_.event_log_filename);
    }
}

void Sdl3App::shutdown() {
    audio_presenter_.reset();
    video_presenter_.reset();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
    initialized_ = false;
}

void Sdl3App::poll_and_dispatch_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit_requested_ = true;
            continue;
        }
        input_mapper_.dispatch_event(event, machine_.keyboard(), machine_.joystick(), machine_.pause_controller(),
                                     machine_.rensha_turbo());
    }
}

void Sdl3App::run_one_frame() {
    poll_and_dispatch_events();

    // The deterministic core step (§2.3): step the CPU purely via
    // step_cpu_instruction() until the next frame boundary, then call
    // on_vsync_boundary() directly -- NEVER run_frame() (A-M26-5's
    // double-count hazard).
    const std::uint64_t frame_start_cycle = machine_.elapsed_cycles();
    const std::uint64_t target = machine_.frame_cycles_per_frame();
    while (machine_.elapsed_cycles() - frame_start_cycle < target) {
        machine_.step_cpu_instruction();
        // M27-S7 (item 3, §2.4): driven through the EXACT same CPU sub-loop
        // the headless --debug-session mode's own loop uses. A genuine
        // no-op when input_script_player_ is empty (the default).
        input_script_player_.apply_due(machine_.elapsed_cycles(), machine_.keyboard());
    }
    machine_.on_vsync_boundary();

    if (video_presenter_) {
        const auto frame = machine_.render_frame();  // Always Field::Progressive (§1.2).
        if (video_presenter_->blit_frame(frame)) {
            video_presenter_->present();
        } else {
            last_error_ = video_presenter_->last_error();
        }
    }

    if (audio_presenter_) {
        // One frame's worth of audio samples, paced independently of the
        // video/CPU cadence (§2.6 point 3: sample-rate-paced, not
        // frame-paced -- this batch size is simply "however many samples
        // elapse during one frame's worth of emulated time").
        const std::uint64_t sample_count = target / Sdl3AudioPresenter::kCyclesPerSample;
        audio_presenter_->pump_and_push(machine_.psg(), static_cast<std::size_t>(sample_count));
    }

    ++frames_run_;
}

int Sdl3App::run_interactive() {
    if (!initialized_) {
        return 1;
    }

    // ~16.6869 ms/frame (docs/m26-planner-package.md §2.3's own computed
    // frame-cadence arithmetic: kFrameCycles / kSystemClockHz).
    const double frame_ms = 1000.0 * static_cast<double>(kFrameCycles) / static_cast<double>(kSystemClockHz);
    const auto target_ms = static_cast<Uint64>(frame_ms);

    while (!quit_requested_) {
        const Uint64 frame_begin = SDL_GetTicks();

        run_one_frame();

        if (config_.max_frames.has_value() && frames_run_ >= *config_.max_frames) {
            break;
        }

        const Uint64 elapsed = SDL_GetTicks() - frame_begin;
        if (elapsed < target_ms) {
            SDL_Delay(static_cast<Uint32>(target_ms - elapsed));
        }
    }

    // M27-S4 (docs/m27-planner-package.md §2.2): the three write_* calls
    // happen once, at the end of run_interactive()'s bounded loop (max_frames
    // reached) or on SDL_EVENT_QUIT -- whichever comes first -- added to the
    // EXISTING loop-exit path, not a new one.
    flush_debug_session_outputs();

    return 0;
}

}  // namespace sony_msx::frontend
