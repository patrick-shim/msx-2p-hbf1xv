// ============================================================================
//  Sony HB-F1XV MSX2+ Emulator
//  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
//
//  LEGAL NOTICE - Personal reference only.
//  This source code is made available solely for personal, non-commercial
//  reference and educational study. Commercial use, sale, or redistribution
//  for profit is not permitted without the author's written consent.
//  Provided "AS IS", without warranty of any kind.
//  Proprietary BIOS/ROM/disk assets remain the property of their respective
//  rights holders and are NOT licensed by this notice.
// ============================================================================

#include "frontend/sdl3_app.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/fdc/disk_image.h"
#include "frontend/audio_pacer.h"
#include "machine/cartridge_identifier.h"

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

    // M30 (backlog G2): auto-identification for type-less --cartN requests
    // via the ONE shared resolver (machine/cartridge_identifier.h, also
    // consumed by src/main.cpp's load_cartridges_from_args). Explicit types
    // (config default: cartN_type_explicit == true) pass through untouched.
    machine::CartridgeIdentificationSession ident_session(config_.softwaredb_path);

    auto load_cart = [&](const int slot_number, const std::optional<std::string>& path,
                         const devices::cartridge::CartridgeMapperType type,
                         const bool type_explicit) -> bool {
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
        machine::ParsedCartridgeSlotCli spec;
        spec.path = *path;
        spec.type = type;
        spec.type_was_explicit = type_explicit;
        const auto resolution = ident_session.resolve(slot_number, spec, image);
        for (const std::string& message : resolution.messages) {
            std::cerr << message << "\n";
        }
        if (!resolution.ok) {
            // Identified-but-unsupported: startup abort (planner §2.4.3) --
            // the message-B line doubles as last_error_ so sdl3_main.cpp's
            // "initialization failed:" report carries the full reason.
            last_error_ = resolution.messages.empty() ? "unsupported cartridge mapper type"
                                                      : resolution.messages.back();
            return false;
        }
        // M36 FM-PAC SRAM persistence: a real FM-PAC always battery-persists, so
        // bind its .sram host file BEFORE the cartridge is inserted -- the
        // machine loads the SRAM on insertion (hbf1xv_machine.cpp load_cartridge,
        // guarded on FmPac + a non-empty path), so setting it here makes the
        // load-on-insert restore work exactly like the headless --fmpac-sram
        // path (src/main.cpp:923-926 sets it before load_cartridges_from_args).
        // resolution.type is authoritative here (auto-identification already
        // resolved), so THIS is the point we know a bay is an FM-PAC and which
        // ROM path derives its default save. Default (no override, not disabled)
        // derives <cart>.rom -> <cart>.rom.sram so the save lands beside the
        // cart, matching a real FM-PAC. --fmpac-sram overrides; --no-fmpac-sram
        // opts out (in-memory-only). Non-FM-PAC carts never touch this path, so
        // a non-FM-PAC run is byte-for-byte unchanged.
        if (resolution.type == devices::cartridge::CartridgeMapperType::FmPac &&
            !config_.fmpac_sram_disabled) {
            const std::filesystem::path sram_path =
                config_.fmpac_sram_path.has_value()
                    ? std::filesystem::path(*config_.fmpac_sram_path)
                    : std::filesystem::path(*path + ".sram");
            machine_.set_fmpac_sram_path(sram_path);
        }
        const CartridgeLoadResult result = machine_.load_cartridge(slot_number, resolution.type, image);
        if (result != CartridgeLoadResult::Ok) {
            last_error_ = "failed to load --cart" + std::to_string(slot_number) + " (" + *path + ")";
            return false;
        }
        return true;
    };

    if (!load_cart(1, config_.cart1_path, config_.cart1_type, config_.cart1_type_explicit)) {
        return false;
    }
    if (!load_cart(2, config_.cart2_path, config_.cart2_type, config_.cart2_type_explicit)) {
        return false;
    }

    // M35-S2: real disk-image loading (A-M26-6 + M35-S2). Pre-load all
    // disks in the repeatable --disk list into memory for deterministic
    // swapping (AC-S2-2). Load the first disk at boot (AC-S2-1). Empty
    // list = no disk (AC-S2-3, existing behavior).
    disk_images_.clear();
    current_disk_index_ = 0;

    for (const auto& disk_path : config_.disk_paths) {
        std::ifstream in(disk_path, std::ios::binary);
        if (!in) {
            last_error_ = "cannot open --disk file: " + disk_path;
            return false;
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
        disk_images_.push_back(std::move(bytes));
    }

    // Attach the first disk (if any) at boot, exactly as pre-M35 behavior.
    if (!disk_images_.empty()) {
        machine_.disk_image() = devices::fdc::DiskImage(disk_images_[0]);
        // M36-S-c: opt-in host-file write-back. Default false leaves this
        // byte-for-byte unchanged (no host path -> flush() is a no-op).
        if (config_.disk_writable && current_disk_index_ < config_.disk_paths.size()) {
            machine_.disk_image().set_host_path(config_.disk_paths[current_disk_index_]);
        }
        machine_.disk_drive().attach_image(&machine_.disk_image());
        update_window_title_for_current_disk();
        log_disk_swap();
    } else {
        // M35-S2: explicitly detach when no disk in list (safety/clarity)
        machine_.disk_drive().attach_image(nullptr);
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

    // M37 Slice E (DEC-0056): the window is RESIZABLE so drag-resize scales
    // live out of the box (references/sdl3/include/SDL3/SDL_video.h:237); still
    // honor hidden_window (test/CI) and start fullscreen when requested
    // (SDL_video.h:232). fullscreen_ tracks the runtime Alt+Enter toggle state.
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (config_.hidden_window) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    if (config_.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    fullscreen_ = config_.fullscreen;
    if (!SDL_CreateWindowAndRenderer("sony-msx-hbf1xv", config_.window_width, config_.window_height, flags,
                                     &window_, &renderer_)) {
        last_error_ = SDL_GetError();
        shutdown();
        return false;
    }

    // M37 Slice E (DEC-0056): aspect-correct, never-distorted letterboxed
    // scaling. 320x240 is the MSX 4:3 framing, so the picture matches today
    // (the presenter's SDL_RenderTexture(..., nullptr, nullptr) fills this
    // logical area; SDL letterboxes it to the actual window/fullscreen size at
    // any --scale). Grounded in references/sdl3/include/SDL3/SDL_render.h:1574
    // (SDL_SetRenderLogicalPresentation) + :136 (SDL_LOGICAL_PRESENTATION_LETTERBOX).
    if (!SDL_SetRenderLogicalPresentation(renderer_, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        last_error_ = SDL_GetError();
        shutdown();
        return false;
    }

    video_presenter_ = std::make_unique<Sdl3VideoPresenter>(renderer_, config_.border_enabled, config_.texture_filter);

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

    // M36 Phase 3: route snapshot output to <dir>/snapshot/<id>/ when
    // --snapshot <dir> is given (default: the machine's "debug" root). Set
    // before cold_boot so any F12 capture lands in the configured place.
    if (config_.snapshot_dir.has_value()) {
        machine_.set_debug_root(*config_.snapshot_dir);
    }

    machine_.set_asset_root(config_.bios_dir);
    machine_.cold_boot();

    // M37 Slice D (DEC-0056): apply the launch-time initial Sony Speed
    // Controller level AFTER cold_boot() -- cold_boot() resets the controller
    // to level 0 (hbf1xv_machine.cpp:316), so setting it earlier would be
    // clobbered. std::nullopt (default) leaves it untouched -> level 0 (full
    // speed), byte-identical to before. The F6/F7 runtime stepping
    // (sdl3_input_mapper.cpp) is unchanged; this only sets the INITIAL value.
    if (config_.speed_level.has_value()) {
        machine_.pause_controller().set_speed_level(*config_.speed_level);
    }

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
    // M36-S-c: persist any pending writable-disk writes before tearing down.
    // No-op unless disk-writable bound a host path (default behavior unchanged).
    if (initialized_ && !disk_images_.empty()) {
        machine_.disk_image().flush();
    }
    // M36 FM-PAC SRAM persistence: save the inserted FM-PAC's 8 KB battery SRAM
    // to its bound .sram host file (mirrors the --disk-writable flush above and
    // the headless --fmpac-sram flush-on-exit, src/main.cpp:1083-1086). A genuine
    // no-op when no FM-PAC is inserted or no path was bound -- flush_fmpac_sram()
    // returns false -- so a non-FM-PAC run is byte-for-byte unchanged. Guarded on
    // initialized_ so a failed-init teardown never writes (matches the disk flush).
    if (initialized_ && machine_.flush_fmpac_sram()) {
        std::cerr << "sdl3: flushed FM-PAC SRAM to \"" << machine_.fmpac_sram_path().string()
                  << "\"\n";
    }
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
        // M35-S3/S4: F11 hotkey for disk-swap (fresh key-down only, not repeat,
        // not routed to MSX keyboard matrix). Consumed here; never dispatched
        // to input_mapper (which would feed it to the machine's peripherals).
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F11 && !event.key.repeat) {
            on_disk_swap_hotkey();
            continue;
        }
        // M36 Phase 3: F12 hotkey for a comprehensive debug snapshot (fresh
        // key-down only, not a repeat). Consumed HERE as a HOST hotkey; NEVER
        // dispatched to input_mapper_ (which would leak it into the MSX keyboard
        // matrix) -- mirrors the F11 disk-swap discipline. No collision: F11 is
        // the only other host hotkey wired.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F12 && !event.key.repeat) {
            on_snapshot_hotkey();
            continue;
        }
        // DEC-0052 + M37 Slice F: F10 hotkey toggles live stream-capture (fresh
        // key-down only, not a repeat), but ONLY when --capture on was given
        // (config_.capture_enabled). Default OFF makes F10 completely INERT --
        // a mis-struck F10 during gameplay is ignored entirely (no toggle, no
        // log, no continue-consume): it simply falls through to the normal MSX
        // input path like any other unbound host key, so default gameplay is
        // byte-identical. When enabled, it is consumed HERE as a HOST hotkey and
        // NEVER dispatched to input_mapper_ (which would leak it into the MSX
        // keyboard matrix) -- mirrors the F11/F12 discipline. Only F10 is gated;
        // F6-F9 speed/rensha + F11 disk-swap + F12 snapshot stay wired as before.
        if (config_.capture_enabled && event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_F10 && !event.key.repeat) {
            on_stream_toggle_hotkey();
            continue;
        }
        // M37 Slice E (DEC-0056): Alt+Enter toggles fullscreen at runtime (fresh
        // key-down only, not a repeat). Consumed HERE as a HOST hotkey; NEVER
        // dispatched to input_mapper_ -- RETURN is an MSX matrix key, so it must
        // not leak into the emulated keyboard (mirrors the F10/F11/F12
        // discipline above). Mod mask SDL_KMOD_ALT (either Alt) per
        // references/sdl3/include/SDL3/SDL_keycode.h:344; SDL_SetWindowFullscreen
        // per SDL_video.h:2435.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_RETURN &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            fullscreen_ = !fullscreen_;
            SDL_SetWindowFullscreen(window_, fullscreen_);
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

    // M36 Phase 3: service a pending F12 snapshot request at this clean frame
    // boundary -- the deterministic id (frame_count()/elapsed_cycles()) is now
    // stable. Read-only; never perturbs emulation. The manifest carries the
    // frontend multi-disk index (planner A4) as a caller note.
    if (snapshot_requested_) {
        snapshot_requested_ = false;
        const std::vector<std::string> notes = {
            "disk_index=" + std::to_string(current_disk_index_),
            "disk_count=" + std::to_string(disk_images_.size()),
        };
        machine_.write_snapshot(machine_.snapshot_id(), notes);
    }

    if (video_presenter_) {
        const auto frame = machine_.render_frame();  // Always Field::Progressive (§1.2).
        if (video_presenter_->blit_frame(frame)) {
            video_presenter_->present();
        } else {
            last_error_ = video_presenter_->last_error();
        }
    }

    if (audio_presenter_) {
        // Exact-accounting audio production (audio-latency fix, docs/audio-
        // latency-investigation.md): this batch's sample count is derived
        // from the machine's CUMULATIVE elapsed cycles (floor(cycles * 44100
        // / 3579545), integer math), never from a per-frame rounded count.
        // The old `target / kCyclesPerSample` = floor(59736/81) = 737
        // samples/frame overproduced vs the exact 735.948 samples/frame and
        // -- with no backpressure on SDL_PutAudioStreamData's unbounded
        // queue -- accumulated audio latency without limit (measured +29.7
        // ms of lag per second of play, combined with the run_interactive()
        // ms-truncation below). samples_to_pump is a pure function of
        // elapsed cycles, so the deterministic ctest path is unaffected by
        // host-queue state.
        //
        // M29-S5: the SCC sources are queried fresh each frame (cheap, and
        // correct across cartridge load state) -- nullptr when a bay holds
        // no KonamiSCC cart, in which case the mixed output is byte-
        // identical to the pre-M29 PSG-only path (the mixer's regression
        // oracle).
        //
        // M31-S5: the machine's YM2413 (OPLL) is the third mixed source --
        // real FM synthesis (backlog E1). A silent (never-keyed) OPLL
        // contributes exactly 0 to every sample (the M31 hard oracle), so
        // FM-less software sounds byte-identical to v1.0.31.
        //
        // M37 Slice B (DEC-0055): the OPLL(s) of any inserted external FM-PAC
        // cartridge are ADDITIONAL mixed sources -- queried fresh each frame
        // (nullptr when a bay holds no FM-PAC cart), so with no FM-PAC inserted
        // the mixed output is byte-identical to v1.0.36 (the M37 hard oracle).
        // This is what makes FM-PAC music (e.g. SRAM-save games) actually
        // audible, alongside the SRAM the cartridge already provides.
        const auto fmpac_opll = [](devices::cartridge::CartridgeFmPacRom* cart) {
            return cart != nullptr ? &cart->opll() : nullptr;
        };
        audio_presenter_->pump_and_push_paced(
            machine_.psg(), MachineAudioMixer::SccSources{machine_.scc_chip(1), machine_.scc_chip(2)},
            &machine_.ym2413(),
            MachineAudioMixer::FmSources{fmpac_opll(machine_.fmpac(1)), fmpac_opll(machine_.fmpac(2))},
            machine_.elapsed_cycles());
    }

    ++frames_run_;
}

void Sdl3App::on_disk_swap_hotkey() {
    // M35-S4: Hotkey handler for F11 disk-swap. Rotate the current disk
    // index, load the new image from cache, re-attach it, and set the
    // disk-changed flag (AC-S4-1..4). No-op if list <= 1 (AC-S3-3).
    if (disk_images_.size() <= 1) {
        return;  // No-op: empty or single-disk list
    }

    // M36-S-c/R9: before discarding the outgoing image, persist and cache its
    // writes so they are not lost (and a swap-back sees them). Guarded on
    // disk_writable so the default path stays byte-for-byte pre-M36.
    if (config_.disk_writable) {
        machine_.disk_image().flush();  // no-op if no host path / not dirty
        if (current_disk_index_ < disk_images_.size()) {
            disk_images_[current_disk_index_] = machine_.disk_image().data();
        }
    }

    current_disk_index_ = (current_disk_index_ + 1) % disk_images_.size();
    machine_.disk_image() = devices::fdc::DiskImage(disk_images_[current_disk_index_]);
    if (config_.disk_writable && current_disk_index_ < config_.disk_paths.size()) {
        machine_.disk_image().set_host_path(config_.disk_paths[current_disk_index_]);
    }
    machine_.disk_drive().attach_image(&machine_.disk_image());
    machine_.disk_drive().set_disk_changed(true);  // Signal media change
    update_window_title_for_current_disk();
    log_disk_swap();
}

void Sdl3App::on_snapshot_hotkey() {
    // M36 Phase 3: request a comprehensive debug snapshot. The actual capture is
    // DEFERRED to the end of run_one_frame() (after on_vsync_boundary()) so the
    // machine is always at a clean frame boundary and the deterministic id
    // (frame_count()) is stable -- exactly the safe, non-perturbing capture
    // point (planner §4.1). Setting a flag keeps this handler O(1).
    snapshot_requested_ = true;
}

void Sdl3App::on_stream_toggle_hotkey() {
    // DEC-0052: F10 toggles live stream-capture at the machine level. Arming is
    // side-effect-free at this instant (per-frame bundles + the trace ring only
    // begin accumulating on the following frames/instructions); finalizing dumps
    // the trace ring + a final snapshot. Both are non-perturbing to emulation.
    if (machine_.stream_capture_active()) {
        machine_.set_stream_capture_enabled(false);  // manual OFF -> finalize
        std::cerr << "Stream capture OFF (finalized).\n";
    } else {
        // Stamp the stream id from the current deterministic frame/cycle id so an
        // identical run toggling at the same frame yields identical stream paths.
        // DEC-0052 stream-light: arm the lightweight mode when --stream-light was
        // given, so a LONG armed session (YS-II game start -> building entry) is
        // not bogged down by the heavy per-frame snapshot I/O.
        const std::string stream_id = machine_.snapshot_id();
        machine_.set_stream_capture_enabled(true, stream_id, config_.stream_light);
        std::cerr << "Stream capture ON: stream_" << stream_id
                  << (config_.stream_light ? " (light)" : " (heavy)") << "\n";
    }
}

bool Sdl3App::flush_current_disk() {
    // M36-S-c: explicit write-back of the mounted disk. A genuine no-op unless
    // disk-writable bound a host path (default: no host path -> flush() false).
    if (!initialized_ || disk_images_.empty()) {
        return false;
    }
    return machine_.disk_image().flush();
}

void Sdl3App::update_window_title_for_current_disk() {
    // M35-S5: Update window title to show the current disk name (AC-S5-1,
    // AC-S5-2). Format: "sony-msx-hbf1xv — <disk_name>" or "(no disk)".
    std::string title = "sony-msx-hbf1xv";
    if (!config_.disk_paths.empty() && current_disk_index_ < config_.disk_paths.size()) {
        // Extract filename from full path (platform-independent approach)
        const auto& disk_path = config_.disk_paths[current_disk_index_];
        const size_t last_slash = disk_path.find_last_of("/\\");
        const std::string disk_name =
            (last_slash == std::string::npos) ? disk_path : disk_path.substr(last_slash + 1);
        title += " — " + disk_name;
    } else {
        title += " — (no disk)";
    }
    if (window_) {
        SDL_SetWindowTitle(window_, title.c_str());
    }
}

void Sdl3App::log_disk_swap() {
    // M35-S5: Log disk swap to stderr with human-readable feedback
    // (AC-S5-3). Format: "Inserted disk: <name> (i/N)" or "Inserted disk: (no disk)".
    if (!config_.disk_paths.empty() && current_disk_index_ < config_.disk_paths.size()) {
        const auto& disk_path = config_.disk_paths[current_disk_index_];
        const size_t last_slash = disk_path.find_last_of("/\\");
        const std::string disk_name =
            (last_slash == std::string::npos) ? disk_path : disk_path.substr(last_slash + 1);
        std::cerr << "Inserted disk: " << disk_name << " (" << (current_disk_index_ + 1) << "/"
                  << config_.disk_paths.size() << ")\n";
    } else {
        std::cerr << "Inserted disk: (no disk)\n";
    }
}

int Sdl3App::run_interactive() {
    if (!initialized_) {
        return 1;
    }

    // Exact-nanosecond absolute-deadline frame pacing (audio-latency fix,
    // docs/audio-latency-investigation.md). The previous per-frame
    // SDL_GetTicks()/SDL_Delay() arithmetic TRUNCATED the exact 16.688154 ms
    // frame period (kFrameCycles / kSystemClockHz) to a 16 ms integer target,
    // running the whole session ~3-4% fast (measured 61.61 fps vs the real
    // 59.9227 Hz cadence) and overproducing audio by ~1,300 samples/s. Here
    // the deadline for frame N is derived from CUMULATIVE emulated cycles via
    // the same exact integer scaling the audio path uses
    // (AudioPacer::scale_cycles, numerator 1e9) -- no per-frame rounding, no
    // drift over any session length. SDL_GetTicksNS()/SDL_DelayNS() per
    // references/sdl3/include/SDL3/SDL_timer.h:213,281.
    //
    // Stall handling: if the host falls behind, frames run without delay so
    // emulated time catches back up to wall time (the audio queue re-fills,
    // bounded by the presenter's backpressure cap). A backlog larger than
    // kMaxBacklogNs (a debugger pause, laptop sleep, ...) is forgiven by
    // sliding the baseline instead of fast-forwarding the machine.
    constexpr Uint64 kMaxBacklogNs = 100'000'000;  // 100 ms
    std::uint64_t paced_cycles = 0;
    Uint64 base_ns = SDL_GetTicksNS();

    while (!quit_requested_) {
        run_one_frame();

        if (config_.max_frames.has_value() && frames_run_ >= *config_.max_frames) {
            break;
        }

        paced_cycles += machine_.frame_cycles_per_frame();
        const Uint64 deadline_ns =
            base_ns + AudioPacer::scale_cycles(paced_cycles, 1'000'000'000ull, kSystemClockHz);
        const Uint64 now_ns = SDL_GetTicksNS();
        if (now_ns < deadline_ns) {
            SDL_DelayNS(deadline_ns - now_ns);
        } else if (now_ns - deadline_ns > kMaxBacklogNs) {
            base_ns += now_ns - deadline_ns;  // Forgive the backlog; never fast-forward through it.
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
