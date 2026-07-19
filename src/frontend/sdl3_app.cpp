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

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/fdc/disk_image.h"
#include "frontend/app_icon_data.h"
#include "frontend/blank_disk_image.h"  // M56 (DEC-0084): F5 New Blank Disk writer
#include "frontend/config_xml_writer.h"  // DEC-0095: settings persistence (EmulatorConfig -> XML)
#include "frontend/audio_pacer.h"
#include "frontend/dialog_default_dir.h"  // M63/M64: pure dialog default-dir pick (all pickers)
#include "frontend/master_volume.h"  // M52 (DEC-0079): step_master_volume for Alt+D/Alt+U
#include "frontend/sdl3_menu.h"      // M55 (DEC-0083): interactive ImGui menu (complete type)
#include "frontend/window_fit.h"     // M61 (DEC-0090): pure display-fit for the interactive window
#include "machine/cartridge_identifier.h"
#include "machine/debug_format.h"
#include "peripherals/msx_key_names.h"

namespace sony_msx::frontend {

namespace {
// kFrameCycles is private to hbf1xv_machine.cpp; the machine's own public
// frame_cycles_per_frame() accessor is the authoritative source used at
// runtime (never duplicated as a literal below). This constant only anchors
// the doc comment's ms/frame arithmetic for humans reading the source.
constexpr std::uint64_t kFrameCycles = 228 * 262;

// M63: the emulator's working directory as an absolute native string, for the
// file-dialog default_location. Degrades to "" (= "no preference" -> the
// launcher passes SDL nullptr, today's behavior) on ANY filesystem error --
// the dialog opener must never throw.
std::string current_dir_or_empty() {
    try {
        return std::filesystem::absolute(std::filesystem::current_path()).string();
    } catch (const std::exception&) {
        return {};
    }
}

// DEC-0095-AMENDMENT-B: media-type + write-back safety guards for the runtime
// media-open seams. Motivation: opening bios/f1xvbios.rom via File > Open Disk(s)
// with disk-writable ON bound it for host write-back; DiskImage 0x00-pads any
// bytes to the 2DD size and flush() wrote the 720 KB-padded image back over the
// 32 KB ROM (data loss). Two layers close it: (1) accept ONLY disk-extension
// files as disks and ROM-extension files as cartridges; (2) never bind a host
// file for write-back unless it is EXACTLY the 2DD size.
//
// kDiskImageBytes MUST match devices::fdc::DiskImage::kImageBytes (737280); it is
// re-expressed here (not #included) to keep this frontend guard free of a device
// header dependency -- the same "re-express the documented layout fact" discipline
// the blank-disk writer uses.
constexpr std::uintmax_t kDiskImageBytes = 737280;  // 720 KB: 2 sides x 80 tracks x 9 x 512

// The lowercased file extension INCLUDING the leading dot ("" when the basename
// has none). Tolerant of dots in parent directory names.
std::string extension_lower(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    std::string e = path.substr(dot);
    for (char& c : e) {
        c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    }
    return e;
}

// Accepted disk-image extensions (the HB-F1XV drive is 720 KB 2DD; .dsk is the
// canonical raw sector image, the rest are common raw-image aliases).
bool is_disk_extension(const std::string& path) {
    const std::string e = extension_lower(path);
    return e == ".dsk" || e == ".di1" || e == ".di2" || e == ".360" || e == ".720";
}

// Accepted cartridge-ROM extensions.
bool is_cartridge_extension(const std::string& path) {
    const std::string e = extension_lower(path);
    return e == ".rom" || e == ".mx1" || e == ".mx2";
}

// M64: resolve a CONFIGURED dialog default directory (config cartridge_dir /
// disk_dir / bios_dir) to the absolute default_location string for
// SDL_Show*Dialog: absolute(configured) when it exists as a directory, else
// the working directory, else "" (= nullptr, no preference). The policy is the
// pure choose_dialog_dir() (frontend/dialog_default_dir.h, unit-tested); the
// filesystem probing here must NEVER throw out of a dialog opener, so it
// degrades to "" on any error. Shared by all the M56/M60 dialog launchers.
std::string resolve_dialog_default_dir(const std::string& configured) {
    try {
        const std::filesystem::path p(configured);
        const bool is_dir =
            !configured.empty() && std::filesystem::exists(p) && std::filesystem::is_directory(p);
        const std::string abs = is_dir ? std::filesystem::absolute(p).string() : std::string();
        return choose_dialog_dir(abs, current_dir_or_empty(), is_dir);
    } catch (const std::exception&) {
        return {};
    }
}
}  // namespace

// M42 (DEC-0061): size the machine's main RAM from config_.ram_bytes. config_ is
// declared BEFORE machine_ (member init order), so config_ is fully constructed
// (with the moved-in value) when machine_ reads it. Default ram_bytes == stock
// 64 KB, so a default config yields a byte-identical machine.
Sdl3App::Sdl3App(Sdl3AppConfig config)
    // M57 (DEC-0085-AMENDMENT-A): machine_ is a std::optional so a live RAM-size
    // change can rebuild it in place (emplace) -- the machine is non-movable
    // (reference-wired members), so reconstruction-through-the-ctor is the ONLY
    // correct way to resize dram_. std::in_place constructs it engaged at
    // config_.ram_bytes here; it is re-emplaced on power_cycle and NEVER
    // disengaged (the "always engaged after construction" invariant).
    : config_(std::move(config)), machine_(std::in_place, config_.ram_bytes) {}

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
        // guarded on FmPac + a non-empty path), mirroring the headless
        // --fmpac-sram path (src/main.cpp). resolution.type is authoritative
        // here (auto-identification already resolved), so this is the point we
        // know a bay is an FM-PAC. Path derivation/override/opt-out semantics
        // are documented on Sdl3AppConfig::fmpac_sram_path (sdl3_app.h).
        if (resolution.type == devices::cartridge::CartridgeMapperType::FmPac &&
            !config_.fmpac_sram_disabled) {
            const std::filesystem::path sram_path =
                config_.fmpac_sram_path.has_value()
                    ? std::filesystem::path(*config_.fmpac_sram_path)
                    : std::filesystem::path(*path + ".sram");
            machine_->set_fmpac_sram_path(sram_path);
        }
        const CartridgeLoadResult result = machine_->load_cartridge(slot_number, resolution.type, image);
        if (result != CartridgeLoadResult::Ok) {
            last_error_ = "failed to load --cart" + std::to_string(slot_number) + " (" + *path + ")";
            return false;
        }
        // M57 (DEC-0085-AMENDMENT-A): record the startup cart source so a power-
        // cycle can re-read this bay.
        cart_path_[slot_number - 1] = *path;
        // DEC-0095: a --cartN cartridge opened at startup also enters File > Recent
        // (inert unless recent persistence is on).
        push_recent(*path);
        return true;
    };

    if (!load_cart(1, config_.cart1_path, config_.cart1_type, config_.cart1_type_explicit)) {
        return false;
    }
    if (!load_cart(2, config_.cart2_path, config_.cart2_type, config_.cart2_type_explicit)) {
        return false;
    }

    // M46 (DEC-0071): FM-PAC slot-2 auto-load (planner §2.5). Runs AFTER the
    // explicit --slotN carts above so the occupancy / already-present checks see
    // the final slot state. Gated by config_.fmpac_autoload (the resolved
    // convenience default; a bare stock Sdl3AppConfig{} leaves it FALSE, so the
    // anti-drift default never auto-loads). EVERY skip is a graceful note
    // recorded in fmpac_autoload_outcome_ (for the banner) -- NEVER a boot
    // failure. DEC-0050 "NO S-RAM AVAILABLE" stays correct on every skip path:
    // no FM-PAC is inserted, and no internal SRAM is ever fabricated.
    fmpac_autoload_outcome_ = FmPacAutoloadOutcome::NotAttempted;
    if (config_.fmpac_autoload) {
        // M50-S2 (DEC-0077): the target slot is configurable (<defaults><fmpac
        // slot>), default 2 (byte-identical). Occupancy is checked against the
        // matching explicit --slotN cart path.
        const int autoload_slot = config_.fmpac_autoload_slot;
        const std::optional<std::string>& slot_cart_path =
            (autoload_slot == 1) ? config_.cart1_path : config_.cart2_path;
        if (slot_cart_path.has_value()) {
            // An explicit --slotN/--cartN cart owns the auto-load slot -- user wins.
            fmpac_autoload_outcome_ = FmPacAutoloadOutcome::SkippedSlot2InUse;
        } else if (machine_->fmpac(1) != nullptr || machine_->fmpac(2) != nullptr) {
            // Avoid a double FM-PAC (the human's slot-1 habit): the already-
            // present FM-PAC keeps its own per-cart .sram binding from load_cart.
            fmpac_autoload_outcome_ = FmPacAutoloadOutcome::SkippedAlreadyPresent;
        } else {
            std::ifstream in(config_.fmpac_autoload_rom_path, std::ios::binary);
            if (!in) {
                fmpac_autoload_outcome_ = FmPacAutoloadOutcome::SkippedAbsent;
                std::cerr << "sdl3: FM-PAC auto-load skipped: " << config_.fmpac_autoload_rom_path
                          << " not found (boot proceeds; \"NO S-RAM AVAILABLE\" -- DEC-0050)\n";
            } else {
                std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                                std::istreambuf_iterator<char>());
                // Bind the .sram BEFORE the insert so it restores on load
                // (mirrors the explicit-cart path). Default: beside the ROM;
                // --fmpac-sram/--no-fmpac-sram override/opt-out, reusing the
                // same fields as an explicit FM-PAC cart.
                if (!config_.fmpac_sram_disabled) {
                    const std::filesystem::path sram_path =
                        config_.fmpac_sram_path.has_value()
                            ? std::filesystem::path(*config_.fmpac_sram_path)
                            : std::filesystem::path(config_.fmpac_autoload_rom_path + ".sram");
                    machine_->set_fmpac_sram_path(sram_path);
                }
                const CartridgeLoadResult result = machine_->load_cartridge(
                    autoload_slot, devices::cartridge::CartridgeMapperType::FmPac, std::move(image));
                if (result != CartridgeLoadResult::Ok) {
                    fmpac_autoload_outcome_ = FmPacAutoloadOutcome::SkippedInvalid;
                    std::cerr << "sdl3: FM-PAC auto-load skipped: " << config_.fmpac_autoload_rom_path
                              << " invalid (not a 1..4 x 16 KB FM-PAC image)\n";
                } else {
                    fmpac_autoload_outcome_ = FmPacAutoloadOutcome::LoadedSlot2;
                    // M57 (DEC-0085-AMENDMENT-A): record the autoloaded FM-PAC
                    // source so a power-cycle re-reads + re-binds its SRAM.
                    cart_path_[autoload_slot - 1] = config_.fmpac_autoload_rom_path;
                }
            }
        }
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
        machine_->disk_image() = devices::fdc::DiskImage(disk_images_[0]);
        // M36-S-c: opt-in host-file write-back. Default false leaves this
        // byte-for-byte unchanged (no host path -> flush() is a no-op).
        if (config_.disk_writable && current_disk_index_ < config_.disk_paths.size()) {
            machine_->disk_image().set_host_path(config_.disk_paths[current_disk_index_]);
        }
        machine_->disk_drive().attach_image(&machine_->disk_image());
        update_window_title_for_current_disk();
        log_disk_swap();
        // DEC-0095: a --disk media set opened at startup enters File > Recent (its
        // first disk; inert unless recent persistence is on).
        push_recent(config_.disk_paths.front());
    } else {
        // M35-S2: explicitly detach when no disk in list (safety/clarity)
        machine_->disk_drive().attach_image(nullptr);
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
    // live (references/sdl3/include/SDL3/SDL_video.h:237); still honor
    // hidden_window (test/CI) and start fullscreen when requested
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

    // Give the live window (and thus its taskbar/Dock button) an explicit icon.
    // On Windows the executable's Explorer icon comes from the linked app_icon.rc
    // resource (an if(WIN32) CMake branch -- not compiled on macOS, where a .icns
    // bundle would be the equivalent and is deferred); this call covers the
    // RUNNING window on every platform. The pixels are embedded
    // (app_icon_data.h, 64x64 RGBA) so this is fully self-contained -- no icon
    // file to locate and no SDL_image dependency. Every step is non-fatal: a
    // failure here just leaves the default icon, never aborts startup.
    // SDL_SetWindowIcon copies the surface, so we free it immediately.
    if (SDL_Surface* icon = SDL_CreateSurfaceFrom(
            kAppIconWidth, kAppIconHeight, SDL_PIXELFORMAT_RGBA32,
            const_cast<unsigned char*>(kAppIconRGBA), kAppIconWidth * 4)) {
        SDL_SetWindowIcon(window_, icon);
        SDL_DestroySurface(icon);
    }

    // M37 Slice E (DEC-0056): aspect-correct, never-distorted letterboxed
    // scaling. 320x240 is the MSX 4:3 framing (the presenter's
    // SDL_RenderTexture(..., nullptr, nullptr) fills this logical area; SDL
    // letterboxes it to the actual window/fullscreen size at any --scale).
    // Grounded in references/sdl3/include/SDL3/SDL_render.h:1574
    // (SDL_SetRenderLogicalPresentation) + :136 (SDL_LOGICAL_PRESENTATION_LETTERBOX).
    if (!SDL_SetRenderLogicalPresentation(renderer_, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        last_error_ = SDL_GetError();
        shutdown();
        return false;
    }

    video_presenter_ = std::make_unique<Sdl3VideoPresenter>(renderer_, config_.border_enabled,
                                                            config_.texture_filter, config_.persistence,
                                                            config_.persistence_mode);

    audio_presenter_ = std::make_unique<Sdl3AudioPresenter>();
    if (!audio_presenter_->init()) {
        last_error_ = audio_presenter_->last_error();
        shutdown();
        return false;
    }
    // M52 (DEC-0079): apply the resolved master volume to the presenter once
    // before playback, and cache it for the live Alt+D/Alt+U steppers. Default
    // 100 (unity) is byte-identical to pre-M52 (the presenter short-circuits at
    // 100). Presentation-only: never touches emulation/determinism.
    master_volume_ = config_.master_volume;
    audio_presenter_->set_master_volume(master_volume_);

    // M27-S4, R-M27-2 (easy-to-get-wrong sequencing constraint): event
    // logging MUST be enabled BEFORE cold_boot() to capture the Reset event
    // (hbf1xv_machine.h:306-309's documented ordering requirement) -- mirrors
    // the headless --debug-session mode's identical ordering.
    if (config_.event_log_filename.has_value()) {
        machine_->set_event_logging_enabled(true);
    }

    // M36 Phase 3: route snapshot output to <dir>/snapshot/<id>/ when
    // --snapshot <dir> is given (default: the machine's "debug" root). Set
    // before cold_boot so any F12 capture lands in the configured place.
    if (config_.snapshot_dir.has_value()) {
        machine_->set_debug_root(*config_.snapshot_dir);
    }

    // M50-S3 (DEC-0077): apply the config-resolved BIOS dir + role-keyed BIOS
    // filenames BEFORE cold_boot (which drives load_rom_assets). A bare config
    // keeps the strict spec dir/filenames, byte-identical to before.
    machine_->set_bios_filenames(config_.bios_roms);
    machine_->set_asset_root(config_.bios_dir);
    machine_->cold_boot();

    // M39-A: enable the two additive-voice seams for real-time playback --
    // (Fix B, CONFIRMED) the PSG sync-before-change path that makes software-PCM
    // voice (Aleste 2, Laydock) audible, and (Fix A infra) the 1-bit key-click
    // DAC capture (keyclicks). Both are inert until driven; enabled here AFTER
    // cold_boot (which resets the sync cursor / click latch). The interleaved
    // audio production below (run_one_frame) drives the PSG sync.
    machine_->psg().set_audio_sync_enabled(true);
    machine_->psg().reset_audio_sync(machine_->elapsed_cycles());
    machine_->click_dac().set_capture_enabled(true);
    audio_next_boundary_ = Sdl3AudioPresenter::kSystemClockHz / Sdl3AudioPresenter::kSampleRateHz;

    // M37 Slice D (DEC-0056): apply the launch-time initial Sony Speed
    // Controller level AFTER cold_boot() -- cold_boot() resets the controller
    // to level 0 (hbf1xv_machine.cpp:316), so setting it earlier would be
    // clobbered. std::nullopt (default) leaves it at level 0 (full speed).
    // The F6/F7 runtime stepping (sdl3_input_mapper.cpp) is unchanged; this
    // only sets the initial value.
    if (config_.speed_level.has_value()) {
        machine_->pause_controller().set_speed_level(*config_.speed_level);
    }

    // Fast-disk (FDC turbo) launch state. Applied AFTER cold_boot(); the FDC/
    // drive device reset() deliberately does NOT clear the fast flag, but we set
    // it here (mirroring the speed-level ordering) so a re-cold_boot mid-session
    // is robust. Default false => byte-identical accurate FDC timing. Alt+D
    // flips it live below.
    machine_->set_fast_disk(config_.fast_disk);

    // DEC-0095: load the File > Recent sidecar BEFORE load_configured_assets() so
    // any --disk / --cartN media opened at startup stacks on TOP of the saved MRU.
    // Interactive-only (recent_save_path unset headless -> never read); a missing
    // or unreadable file is a clean empty list (RecentFiles::parse tolerates it).
    if (config_.recent_save_path.has_value()) {
        std::ifstream rin(*config_.recent_save_path, std::ios::binary);
        if (rin) {
            const std::string rtext((std::istreambuf_iterator<char>(rin)),
                                    std::istreambuf_iterator<char>());
            recent_ = RecentFiles::parse(rtext);
        }
    }

    if (!load_configured_assets()) {
        shutdown();
        return false;
    }

    if (config_.trace_cpu_filename.has_value()) {
        machine_->set_cpu_trace_enabled(true);
    }

    // M27-S7 (item 3, §2.4): load the scripted-input mechanism, if
    // configured. A malformed script is a real init() failure (mirrors
    // load_configured_assets()'s "never partially initialize" contract),
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

    // Input RECORDER (DEC-0072): open the record file (writes the format tag)
    // BEFORE any run_one_frame(), so the very first frame's input is captured. A
    // file that cannot be opened is a real init() failure (mirrors the
    // --input-script "never partially initialize" contract above), never a silent
    // no-op. Left closed (default) when --record-input is absent -> every
    // record_*() call is a no-op and the session is byte-for-byte unchanged.
    if (config_.record_input_path.has_value()) {
        if (!input_recorder_.open(*config_.record_input_path)) {
            last_error_ = "cannot open --record-input file: " + *config_.record_input_path;
            shutdown();
            return false;
        }
        std::cerr << "sdl3: recording input to \"" << *config_.record_input_path
                  << "\" (replay with --input-script; F11 swaps -> --swap-disk-frame <N>)\n";
    }

    // M55 (DEC-0083): create the ImGui in-window menu ONLY for a genuinely
    // interactive launch. --hidden-window (every ctest, per A3) NEVER creates a
    // context, so the deterministic suite is byte-identical and no ImGui code
    // ever runs in the ctest path (the determinism guard, §5.1). Created last,
    // after window_/renderer_ exist and every other init step succeeded.
    if (!config_.hidden_window) {
        menu_ = std::make_unique<Sdl3Menu>(window_, renderer_);
        // M56 (DEC-0084, planner §3.1): allocate the async-dialog mailbox mutex
        // ONLY here (interactive launch), next to menu_. --hidden-window / ctest
        // never creates it, so the whole M56 dialog path is inert on the
        // deterministic path (open_*_dialog() early-returns on a null mutex).
        dialog_.mutex = SDL_CreateMutex();
        // M57 (DEC-0085, §4.4): DEF-2. Reserve the menu-strip height so the
        // emulated picture is inset BELOW the bar (never hidden behind it). GROW
        // the window by the strip height so --scale N still yields an unclipped
        // N-tall picture (window becomes 320N x (240N + h)); tell the presenter to
        // letterbox into the band below the strip. hidden-window (menu_ == null)
        // never runs this -> presenter inset stays 0 -> the legacy full-window
        // LETTERBOX path is byte-identical. In fullscreen the SetWindowSize is a
        // no-op (SDL keeps the fullscreen surface) and the band math insets within
        // the fixed surface instead -- both correct.
        const int bar_h = menu_->bar_height();
        if (bar_h > 0) {
            // M61 (DEC-0090): FIT the interactive window to the display so it
            // never opens larger than the usable screen bounds (owner Pi 4B +
            // 800x480 panel: the 960x739 default overflowed and the menu bar
            // landed off-screen). Query the usable bounds of the display the
            // window opened on; on ANY query failure fall back to today's
            // requested size unchanged (log one line, no regression). The fit
            // itself is the pure window_fit.h function: largest integer scale
            // whose 320N x (240N + bar) fits, else clamp to min(requested,
            // usable) -- the EXISTING M57 letterbox (set_top_inset +
            // letterbox_into_band) then fits the 4:3 picture into whatever band
            // remains, so the picture stays aspect-correct + fully visible on
            // ANY display. hidden-window never reaches this block (menu_ ==
            // null path unchanged: no display query, no resize). In fullscreen
            // SDL_SetWindowSize does not alter the fullscreen surface (it only
            // updates the restored windowed size) and the band math insets
            // within the fixed surface -- path unchanged.
            int win_w = config_.window_width;
            int win_h = config_.window_height + bar_h;
            int usable_x = 0;
            int usable_y = 0;
            int usable_w = 0;
            int usable_h = 0;
            if (query_display_usable_bounds(usable_x, usable_y, usable_w, usable_h)) {
                const geometry::WindowFit fit = geometry::fit_window_to_display(
                    config_.window_width, config_.window_height, bar_h, usable_w, usable_h);
                win_w = fit.w;
                win_h = fit.h;
                // The ONE diagnostic line (DEC-0090): the owner is live on the Pi
                // and needs the real display metrics behind "--scale 1 made it
                // worse" -- keep the exact format stable.
                std::cerr << "sdl3: display usable " << usable_w << "x" << usable_h << "; requested "
                          << config_.window_width << "x" << (config_.window_height + bar_h)
                          << "; window " << win_w << "x" << win_h << " (scale " << fit.scale
                          << ")\n";
            } else {
                std::cerr << "sdl3: display usable-bounds query failed (" << SDL_GetError()
                          << "); keeping requested window size\n";
            }
            SDL_SetWindowSize(window_, win_w, win_h);
            if (video_presenter_) {
                video_presenter_->set_top_inset(bar_h);
            }
        }
        // M63 (owner Pi 4B, 7" 800x480 -- the definitive menu-bar-shift fix):
        // TURN OFF SDL's 320x240 LETTERBOX logical presentation for the whole
        // INTERACTIVE session. The on-device diagnostic proved window == pixels
        // == render output with FramebufferScale 1x1 in every state (identical
        // to Windows, where the bar is correct), so the bar is drawn 1:1 in
        // output pixels and the ONLY thing that could displace it is the
        // letterbox transform -- and the observed shift matched the pillarbox
        // offset EXACTLY (~80px @ 800x480, ~253px @ 800x221). The Pi builds
        // against SYSTEM SDL3 (not src/external/sdl3), whose logical-presentation
        // + SDL_RenderGeometryRaw interaction differs from Windows/D3D and leaks
        // that offset into the ImGui menu (the picture is immune -- it draws via
        // explicit output-pixel band rects). The interactive path NEVER needs
        // SDL letterboxing: the presenter already letterboxes the picture itself
        // (set_top_inset + letterbox_into_band, explicit rects). Removing the
        // transform from the pipeline removes a letterbox-sized shift on ANY
        // SDL version. HEADLESS is untouched (this block is inside the
        // !hidden_window guard): the 320x240 LETTERBOX set above (for the
        // top_inset==0 nullptr-dst path / the pixel-integration test) stays,
        // so the deterministic suite is byte-identical.
        SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    }

    // DEC-0095: seed the settings debounce cache from the initial live state so a
    // frame that changes nothing writes nothing (the file is created on the FIRST
    // real change, not at every launch). Interactive-only; a no-op when settings
    // persistence is disabled. current_settings_xml() reads the just-initialized
    // machine + presenter, so it must run here, after the whole setup above.
    if (config_.settings_save_path.has_value()) {
        last_saved_settings_xml_ = current_settings_xml();
    }

    initialized_ = true;
    return true;
}

void Sdl3App::flush_debug_session_outputs() {
    // M27-S4 (docs/m27-planner-package.md §2.2, items 1/4): mirrors the
    // headless --debug-session mode's own end-of-run write-out, via the same
    // existing Hbf1xvMachine APIs (M10-S3) -- zero new machine-level method
    // needed.
    if (config_.dump_state_filename.has_value()) {
        machine_->write_state_dump(*config_.dump_state_filename);
    }
    if (config_.trace_cpu_filename.has_value()) {
        machine_->write_cpu_trace(*config_.trace_cpu_filename);
    }
    if (config_.event_log_filename.has_value()) {
        machine_->write_event_log(*config_.event_log_filename);
    }
    // DEC-0072 per-frame CPU fingerprint CSV (diagnostic).
    if (config_.fingerprint_path.has_value()) {
        std::ofstream fp(*config_.fingerprint_path, std::ios::binary | std::ios::trunc);
        if (fp) {
            fp << "frame,cycles,pc,sp,af,bc,de,hl,ix,iy,r\n";
            fp.write(fingerprint_csv_.data(), static_cast<std::streamsize>(fingerprint_csv_.size()));
            std::cerr << "sdl3: wrote fingerprint CSV " << *config_.fingerprint_path << "\n";
        } else {
            std::cerr << "sdl3: cannot write --fingerprint " << *config_.fingerprint_path << "\n";
        }
    }
}

void Sdl3App::shutdown() {
    // Input RECORDER (DEC-0072): finalize the record file (writes the trailing
    // "[END]" line) so it's a complete, replayable HBF1XV-INPUT-SCRIPT v1. Safe
    // to call unconditionally -- a no-op when --record-input was absent (the
    // recorder was never opened). Done first so the script is flushed even if a
    // later teardown step is skipped.
    input_recorder_.close();
    // M36-S-c: persist any pending writable-disk writes before tearing down.
    // No-op unless disk-writable bound a host path (default behavior unchanged).
    if (initialized_ && !disk_images_.empty()) {
        machine_->disk_image().flush();
    }
    // M36 FM-PAC SRAM persistence: save the inserted FM-PAC's 8 KB battery SRAM
    // to its bound .sram host file (mirrors the --disk-writable flush above and
    // the headless --fmpac-sram flush-on-exit, src/main.cpp:1083-1086). A no-op
    // when no FM-PAC is inserted or no path was bound -- flush_fmpac_sram()
    // returns false. Guarded on initialized_ so a failed-init teardown never
    // writes (matches the disk flush).
    if (initialized_ && machine_->flush_fmpac_sram()) {
        std::cerr << "sdl3: flushed FM-PAC SRAM to \"" << machine_->fmpac_sram_path().string()
                  << "\"\n";
    }
    audio_presenter_.reset();
    video_presenter_.reset();
    // M55 (DEC-0083, R7): tear down ImGui (backends + context) BEFORE the
    // renderer -- ImGui_ImplSDLRenderer3_Shutdown must run while the SDL_Renderer
    // is still alive. No-op when no menu was created (--hidden-window).
    menu_.reset();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    // M56 (DEC-0084, planner §3.1, R3): destroy the dialog mutex LAST (after the
    // window/renderer), so an OS dialog dismissed by window teardown can still
    // take the lock in its callback. No-op when no mutex was created
    // (--hidden-window). Reset the transient flags so a re-init starts clean.
    if (dialog_.mutex != nullptr) {
        SDL_DestroyMutex(dialog_.mutex);
        dialog_.mutex = nullptr;
    }
    dialog_.in_flight = false;
    dialog_.pending = false;
    dialog_.kind = DialogKind::None;
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
    initialized_ = false;
}

void Sdl3App::poll_and_dispatch_events() {
    SDL_Event event;
    // M61 (DEC-0090-AMENDMENT-A): at most ONE window-fit check per poll batch.
    // A single WM maximize delivers a burst (SDL_EVENT_WINDOW_MAXIMIZED +
    // _RESIZED + _PIXEL_SIZE_CHANGED); SDL's cached window geometry is already
    // final when the first of them is delivered, so one check reads the same
    // state the last would -- and this keeps the corrective SetWindowSize /
    // SetWindowPosition from being issued three times for one gesture (part of
    // the settle-in-one-step loop-safety story in clamp_window_to_display()).
    bool window_fit_checked = false;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit_requested_ = true;
            continue;
        }
        // M55 (DEC-0083, §4.1): the single input choke point. When the
        // interactive menu exists, feed EVERY event to ImGui (so hover / click /
        // scroll accumulate), then -- if the menu owns the keyboard or mouse --
        // SWALLOW the event so it can never reach the host-hotkey block below,
        // the DEC-0072 input recorder (~:603), OR the MSX keyboard matrix
        // (input_mapper_.dispatch_event). menu_ is null under --hidden-window, so
        // this whole block is inert on the deterministic ctest path. v1 keeps
        // ImGui keyboard-nav OFF and has no text widgets, so wants_keyboard() is
        // effectively always false -- Alt+letter host hotkeys stay unshadowed --
        // but the gate is correct in advance for a future filename field.
        if (menu_) {
            menu_->process_event(event);
            const bool is_key =
                (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP);
            const bool is_mouse =
                (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_UP || event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_WHEEL);
            if (menu_captures_event(is_key, is_mouse, menu_->wants_keyboard(),
                                    menu_->wants_mouse())) {
                continue;
            }
        }
        // M61 (DEC-0090-AMENDMENT-A): re-apply the window fit on window
        // geometry events, INTERACTIVE path only (window_ non-null and not
        // --hidden-window; a hidden window never receives user resize/maximize
        // events, so the deterministic ctest path stays byte-identical --
        // clamp_window_to_display() additionally guards itself). Event enums per
        // src/external/sdl3/include/SDL3/SDL_events.h:143/144/147. Placed AFTER
        // the menu block so ImGui still sees every window event
        // (menu_captures_event never swallows them: not key, not mouse), and
        // consumed here -- window events carry nothing for the recorder or the
        // MSX keyboard matrix.
        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_MAXIMIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            if (!window_fit_checked && window_ != nullptr && !config_.hidden_window) {
                window_fit_checked = true;
                clamp_window_to_display();
            }
            continue;
        }
        // M35-S3/S4: F11 hotkey for disk-swap (fresh key-down only, not a
        // repeat). Consumed here; never dispatched to input_mapper_, which
        // would otherwise feed it into the MSX keyboard matrix.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F11 && !event.key.repeat) {
            on_disk_swap_hotkey();
            // Input RECORDER (DEC-0072): capture the swap ONLY when one actually
            // occurred (a >1-disk list; on_disk_swap_hotkey() no-ops otherwise,
            // exactly as the replay's --swap-disk-frame guard does). Stamp the
            // CURRENT frame index (frames_run_ is incremented at the END of
            // run_one_frame(), so during this poll it is the 0-based index of the
            // frame about to run) -- the same frame the headless replay swaps at
            // the top of when given --swap-disk-frame <this>.
            if (input_recorder_.is_open() && disk_images_.size() > 1) {
                input_recorder_.record_disk_swap(frames_run_);
                std::cerr << "sdl3: [record] disk swap at frame " << frames_run_
                          << " -> replay with --swap-disk-frame " << frames_run_ << "\n";
            }
            continue;
        }
        // M36 Phase 3: F12 hotkey for a comprehensive debug snapshot (fresh
        // key-down only, not a repeat). Consumed HERE as a HOST hotkey; never
        // dispatched to input_mapper_ (would leak into the MSX keyboard
        // matrix) -- mirrors the F11 disk-swap discipline.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F12 && !event.key.repeat) {
            on_snapshot_hotkey();
            continue;
        }
        // DEC-0052 + M37 Slice F: F10 toggles live stream-capture (fresh key-
        // down only, not a repeat), but ONLY when --capture on was given
        // (config_.capture_enabled). Default OFF makes F10 completely INERT --
        // a mis-struck F10 falls through to the normal MSX input path like any
        // other unbound key, so default gameplay is byte-identical. When
        // enabled, it's consumed HERE as a HOST hotkey, never dispatched to
        // input_mapper_ -- mirrors the F11/F12 discipline. Only F10 is gated;
        // F6-F9 speed/rensha + F11 disk-swap + F12 snapshot stay wired as before.
        if (config_.capture_enabled && event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_F10 && !event.key.repeat) {
            on_stream_toggle_hotkey();
            continue;
        }
        // M37 Slice E (DEC-0056): Alt+Enter toggles fullscreen at runtime (fresh
        // key-down only, not a repeat). Consumed HERE as a HOST hotkey; never
        // dispatched to input_mapper_ -- RETURN is an MSX matrix key, so it must
        // not leak into the emulated keyboard (mirrors the F10/F11/F12
        // discipline above). Mod mask SDL_KMOD_ALT (either Alt) per
        // references/sdl3/include/SDL3/SDL_keycode.h:344; SDL_SetWindowFullscreen
        // per SDL_video.h:2435.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_RETURN &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            toggle_fullscreen();  // M55: extracted so the menu Video>Fullscreen shares it
            continue;
        }
        // Alt+F toggles fast-disk (FDC turbo) live (fresh key-down only, not a
        // repeat -- it is a TOGGLE, so a held key must not thrash the state).
        // M52 (DEC-0079): MOVED from Alt+D to Alt+F ("fast disk" mnemonic) to free
        // Alt+D for volume-down. Consumed HERE as a HOST hotkey; never dispatched
        // to input_mapper_ -- 'F' is an MSX matrix key, so it must not leak into
        // the emulated keyboard (mirrors the Alt+Enter / F10-F12 discipline). The
        // standalone Alt keydown still reaches the matrix -- only the 'F' keydown
        // is swallowed. Mod mask SDL_KMOD_ALT (either Alt) per
        // references/sdl3/include/SDL3/SDL_keycode.h:344.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            const bool on = !machine_->fast_disk();
            machine_->set_fast_disk(on);
            std::cerr << "sdl3: fast-disk (FDC turbo) " << (on ? "ENABLED" : "disabled")
                      << " (Alt+F); default is accurate FDC timing\n";
            continue;
        }
        // M52 (DEC-0079): Alt+D = volume DOWN, Alt+U = volume UP. Key-repeat is
        // HONORED for these two (the guard is deliberately DROPPED, unlike every
        // other host hotkey which is fresh-keydown-only): holding Alt+D/Alt+U to
        // RAMP volume is the ergonomic expectation, and volume is a pure
        // presentation knob that never touches emulation/determinism/any recorded
        // input/any ctest fixture, so honoring OS auto-repeat is zero-risk (the
        // clamp makes an over-held key harmless -- it saturates at 0/100). Consumed
        // HERE as HOST hotkeys; only the 'D'/'U' keydown is swallowed (the repeated
        // keydowns too, so they never leak to the matrix), the standalone Alt
        // keydown still reaches the matrix. Mod mask SDL_KMOD_ALT (either Alt).
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_D &&
            (event.key.mod & SDL_KMOD_ALT) != 0) {
            on_volume_step_hotkey(-1);
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_U &&
            (event.key.mod & SDL_KMOD_ALT) != 0) {
            on_volume_step_hotkey(+1);
            continue;
        }
        // M52 (DEC-0079): Alt+S toggles disk-writable at runtime (fresh key-down
        // only, not a repeat -- it is a TOGGLE). Consumed HERE as a HOST hotkey;
        // never dispatched to input_mapper_ -- 'S' is an MSX matrix key, so only
        // the 'S' keydown is swallowed; the standalone Alt keydown still reaches
        // the matrix (mirrors the Alt+F/Alt+D discipline). Mod mask SDL_KMOD_ALT.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_S &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            on_disk_writable_toggle_hotkey();
            continue;
        }
        // Alt+B / Shift+Alt+B step the phosphor-persistence inter-frame blend
        // weight in FINE 10% increments (Alt+B = +10, Shift+Alt+B = -10, both
        // wrapping 0..100) live, so the owner can dial in the CRT-persistence
        // smoothing per game (frontend/phosphor_blend.h). Consumed HERE as a HOST
        // hotkey; never dispatched to input_mapper_ -- 'B' is an MSX matrix key,
        // so it must not leak into the emulated keyboard (mirrors the Alt+D
        // discipline; the standalone Alt/Shift keydowns still reach the matrix --
        // only the 'B' keydown is swallowed). Alt+B is free (F1-F5 are the
        // emulated MSX function keys and F6-F12 + Alt+Enter/Alt+D are already
        // host-bound). Mod mask SDL_KMOD_ALT (either Alt) / SDL_KMOD_SHIFT per
        // references/sdl3/include/SDL3/SDL_keycode.h:344.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_B &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            on_persistence_step_hotkey((event.key.mod & SDL_KMOD_SHIFT) != 0 ? -1 : +1);
            continue;
        }
        // Alt+M toggles the phosphor blend MODE (Average <-> Peak). Same HOST-
        // hotkey discipline as Alt+B/Alt+D -- 'M' is an MSX matrix key, so only
        // the 'M' keydown is swallowed; the standalone Alt keydown still reaches
        // the matrix. Peak mode keeps multiplexed sprites full-brightness (no
        // dimming), the fix for the Average-mode flicker-vs-ghost tradeoff.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_M &&
            (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat) {
            on_persistence_mode_hotkey();
            continue;
        }
        // Input RECORDER (DEC-0072): capture MSX matrix key edges just before
        // they are dispatched to the keyboard matrix. This point is reached only
        // AFTER every host-hotkey branch above has had its chance to `continue`
        // (F6-F12 / PAUSE / Alt+Enter / Alt+D), so those are naturally excluded:
        // Sdl3InputMapper::map_scancode() returns a coordinate ONLY for the 72
        // real matrix keys (F6-F9/PAUSE are not in the table), the SAME single
        // source of truth the mapper uses to drive KeyboardMatrix::set_key(). The
        // cycle stamp is machine_->elapsed_cycles() at this poll (the frame-start
        // cycle), so replay via --input-script fires the key on the same frame.
        // OS auto-repeat DOWNs are skipped: the matrix is level-based, so a repeat
        // is an idempotent no-op -- recording edges only keeps the script clean
        // and matches the openMSX-derived example format.
        if (input_recorder_.is_open() &&
            (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
            const bool pressed = event.key.down;
            if (!(pressed && event.key.repeat)) {
                const auto coord = Sdl3InputMapper::map_scancode(event.key.scancode);
                if (coord.has_value()) {
                    const auto name = peripherals::row_col_to_key_name(coord->first, coord->second);
                    if (name.has_value()) {
                        input_recorder_.record_key(machine_->elapsed_cycles(), std::string(*name), pressed);
                    }
                }
            }
        }
        // M65 diagnostic: a key that maps to NO matrix coordinate currently does
        // nothing AND says nothing, which makes "my arrow keys don't work"
        // impossible to diagnose from outside the process (it forces a hunt
        // through the compositor, the joystick port and the keyboard itself).
        // Warn ONCE per distinct scancode, on a fresh key-down only, naming the
        // key so an unmapped/miscoded key identifies itself. Reached only by
        // events the host hotkeys above did NOT consume (each of those
        // `continue`s), so this never fires for Alt+Enter / F10-F12 & friends.
        // Interactive-only: --hidden-window (every ctest) never runs this loop,
        // and the guard keeps the deterministic path silent regardless.
        if (!config_.hidden_window && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            !Sdl3InputMapper::map_scancode(event.key.scancode).has_value()) {
            if (warned_unmapped_scancodes_.insert(static_cast<int>(event.key.scancode)).second) {
                const char* key_name = SDL_GetScancodeName(event.key.scancode);
                std::cerr << "sdl3: key \""
                          << ((key_name != nullptr && *key_name != '\0') ? key_name : "?")
                          << "\" (scancode " << static_cast<int>(event.key.scancode)
                          << ") is not mapped to the MSX keyboard matrix -- ignored\n";
            }
        }
        input_mapper_.dispatch_event(event, machine_->keyboard(), machine_->joystick(), machine_->pause_controller(),
                                     machine_->rensha_turbo());
    }
}

void Sdl3App::run_one_frame() {
    // M56 (DEC-0084, planner §3.1): drain any pending async file-dialog result and
    // apply it on the MAIN thread, before polling events. Gated on menu_ (== null
    // under --hidden-window / ctest), so the whole M56 dialog path never runs on
    // the deterministic path -- the baseline suite stays byte-identical.
    if (menu_) {
        drain_dialog_mailbox();
    }
    poll_and_dispatch_events();

    // DEC-0072 replay-fidelity diagnostic: scripted disk hot-swap at a frame
    // boundary (the SDL3 analogue of the headless --swap-disk-frame), applied at
    // frame-START just like the live F11 poll path, so a recorded owner script
    // replays cycle-for-cycle here. No-op unless config_.swap_disk_frame matches
    // this frame index and there is more than one disk (mirrors on_disk_swap_hotkey).
    if (config_.swap_disk_frame.has_value() &&
        frames_run_ == static_cast<std::uint64_t>(*config_.swap_disk_frame) && disk_images_.size() > 1) {
        on_disk_swap_hotkey();
    }

    // The deterministic core step (§2.3): step the CPU purely via
    // step_cpu_instruction() until the next frame boundary, then call
    // on_vsync_boundary() directly -- never run_frame() (A-M26-5's
    // double-count hazard).
    const std::uint64_t frame_start_cycle = machine_->elapsed_cycles();
    const std::uint64_t target = machine_->frame_cycles_per_frame();
    // M39-A Fix B: produce audio samples INTERLEAVED with CPU stepping (over
    // absolute machine-cycle boundaries) so the PSG sync-before-change writes
    // land at their true sub-frame position -- the digitized-voice fix. Samples
    // accumulate in audio_frame_pcm_ and are pushed (paced) after the frame.
    audio_frame_pcm_.clear();
    const bool produce_audio = static_cast<bool>(audio_presenter_);
    const auto fmpac_opll = [](devices::cartridge::CartridgeFmPacRom* cart) {
        return cart != nullptr ? &cart->opll() : nullptr;
    };
    const auto emit_due_audio = [&]() {
        const std::uint64_t now = machine_->elapsed_cycles();
        while (audio_next_boundary_ <= now) {
            const std::uint64_t window = audio_next_boundary_ - audio_prev_boundary_;
            const std::array<std::int16_t, 2> smp = audio_presenter_->mixer().produce_synced_sample(
                machine_->psg(),
                MachineAudioMixer::SccSources{machine_->scc_chip(1), machine_->scc_chip(2)},
                &machine_->ym2413(),
                MachineAudioMixer::FmSources{fmpac_opll(machine_->fmpac(1)), fmpac_opll(machine_->fmpac(2))},
                &machine_->click_dac(), audio_next_boundary_, window);
            audio_frame_pcm_.push_back(smp[0]);
            audio_frame_pcm_.push_back(smp[1]);
            audio_prev_boundary_ = audio_next_boundary_;
            ++audio_sample_index_;
            audio_next_boundary_ =
                audio_sample_index_ * Sdl3AudioPresenter::kSystemClockHz / Sdl3AudioPresenter::kSampleRateHz;
        }
    };
    while (machine_->elapsed_cycles() - frame_start_cycle < target) {
        machine_->step_cpu_instruction();
        // M27-S7 (item 3, §2.4): driven through the EXACT same CPU sub-loop
        // the headless --debug-session mode's own loop uses. A genuine
        // no-op when input_script_player_ is empty (the default).
        input_script_player_.apply_due(machine_->elapsed_cycles(), machine_->keyboard());
        if (produce_audio) {
            emit_due_audio();
        }
    }
    machine_->on_vsync_boundary();

    // DEC-0072 per-frame CPU-state fingerprint (post-boundary), byte-format
    // identical to the headless --fingerprint CSV, so the two drivers can be
    // diffed frame-for-frame to find the first divergence.
    if (config_.fingerprint_path.has_value()) {
        const auto& r = machine_->cpu().state().regs();
        fingerprint_csv_ += std::to_string(frames_run_) + "," +
                            std::to_string(machine_->elapsed_cycles()) + "," +
                            machine::debug_format::to_hex(r.pc, 4) + "," +
                            machine::debug_format::to_hex(r.sp, 4) + "," +
                            machine::debug_format::to_hex(r.af, 4) + "," +
                            machine::debug_format::to_hex(r.bc, 4) + "," +
                            machine::debug_format::to_hex(r.de, 4) + "," +
                            machine::debug_format::to_hex(r.hl, 4) + "," +
                            machine::debug_format::to_hex(r.ix, 4) + "," +
                            machine::debug_format::to_hex(r.iy, 4) + "," +
                            machine::debug_format::to_hex(r.r, 2) + "\n";
    }

    // M36 Phase 3: service a pending F12 snapshot request at this clean frame
    // boundary, where the deterministic id (frame_count()/elapsed_cycles()) is
    // stable. Read-only; never perturbs emulation. The manifest carries the
    // frontend multi-disk index (planner A4) as a caller note.
    if (snapshot_requested_) {
        snapshot_requested_ = false;
        const std::vector<std::string> notes = {
            "disk_index=" + std::to_string(current_disk_index_),
            "disk_count=" + std::to_string(disk_images_.size()),
        };
        machine_->write_snapshot(machine_->snapshot_id(), notes);
    }

    if (video_presenter_) {
        const auto frame = machine_->render_frame();  // Always Field::Progressive (§1.2).
        if (video_presenter_->blit_frame(frame)) {
            // M55 (DEC-0083, §4.2): draw the ImGui menu AFTER the MSX frame is
            // composited (letterbox + phosphor already applied) and BEFORE
            // present(). The menu is never fed into prev_pixels_, so phosphor
            // blending is untouched; the logical-presentation save/restore
            // bracket lives inside menu_->render() so blit_frame() stays
            // byte-identical. menu_ is null under --hidden-window (no-op).
            if (menu_) {
                menu_->begin_frame();
                menu_->build(*this);
                menu_->render(renderer_);
                // M57 (DEC-0085, §4.4): track the LIVE bar height (it can change
                // with DPI/font) so the NEXT frame's blit_frame insets the picture
                // correctly. init() set the initial value so frame 0 is already
                // correct; this keeps it robust. Presentation-only; menu_ == null
                // (--hidden-window) never runs it, so the inset stays 0.
                video_presenter_->set_top_inset(menu_->bar_height());
                // M63 diagnostic: log window/pixel/render-output geometry once
                // per change (io.DisplaySize is valid now that begin_frame ran).
                log_geometry_if_changed();
            }
            video_presenter_->present();
        } else {
            last_error_ = video_presenter_->last_error();
        }
    }

    if (audio_presenter_) {
        // Exact-accounting audio production (audio-latency fix, docs/audio-
        // latency-investigation.md): this batch's sample count is derived from
        // the machine's CUMULATIVE elapsed cycles (floor(cycles * 44100 /
        // 3579545), integer math), never a per-frame rounded count. The old
        // `target / kCyclesPerSample` = floor(59736/81) = 737 samples/frame
        // overproduced vs the exact 735.948/frame, and with no backpressure on
        // SDL_PutAudioStreamData's unbounded queue this accumulated unlimited
        // audio latency (+29.7 ms/s measured, compounded by the ms-truncated
        // pacing in run_interactive() below). samples_to_pump is a pure
        // function of elapsed cycles, so the deterministic ctest path is
        // unaffected by host-queue state.
        //
        // M29-S5: SCC sources are queried fresh each frame -- nullptr when a
        // bay holds no KonamiSCC cart, so the mix is byte-identical to the
        // pre-M29 PSG-only path (the regression oracle).
        //
        // M31-S5: the machine's YM2413 (OPLL) is a third mixed source (backlog
        // E1). A silent (never-keyed) OPLL contributes exactly 0 to every
        // sample, so FM-less software is byte-identical to v1.0.31.
        //
        // M37 Slice B (DEC-0055): OPLL(s) of any inserted external FM-PAC
        // cartridge are additional mixed sources (nullptr when no FM-PAC is
        // present), so with none inserted the mix is byte-identical to v1.0.36.
        // This is what makes FM-PAC music (e.g. SRAM-save games) audible.
        // M39-A Fix B: push the sub-frame-accurate samples produced during the
        // step loop above (the voice is in there now), paced with the same
        // AudioPacer backpressure/silence-prime policy the batch path used --
        // the interleaved production count matches the pacer's exact-accounting
        // count by construction (both floor(elapsed*44100/3579545)).
        audio_presenter_->push_produced_paced(audio_frame_pcm_, machine_->elapsed_cycles());
    }

    ++frames_run_;

    // DEC-0095: persist interactive settings once per frame at the clean frame
    // boundary. INERT under --hidden-window / headless (settings_save_path unset ->
    // an immediate return, no filesystem touch), and debounced so an unchanged
    // frame writes nothing. Placed here, AFTER input + menu dispatch have settled
    // this frame's state, so both the hotkey and menu paths are captured by one
    // call site (no per-knob hooks).
    maybe_save_settings();
}

void Sdl3App::on_disk_swap_hotkey() {
    // M35-S4: Hotkey handler for F11 disk-swap. Rotate the current disk
    // index, load the new image from cache, re-attach it, and set the
    // disk-changed flag (AC-S4-1..4). No-op if list <= 1 (AC-S3-3).
    if (disk_images_.size() <= 1) {
        return;
    }

    // M36-S-c/R9: before discarding the outgoing image, persist and cache its
    // writes so a swap-back sees them. Guarded on disk_writable so the
    // default path stays byte-for-byte pre-M36.
    if (config_.disk_writable) {
        machine_->disk_image().flush();  // no-op if no host path / not dirty
        if (current_disk_index_ < disk_images_.size()) {
            disk_images_[current_disk_index_] = machine_->disk_image().data();
        }
    }

    current_disk_index_ = (current_disk_index_ + 1) % disk_images_.size();
    machine_->disk_image() = devices::fdc::DiskImage(disk_images_[current_disk_index_]);
    if (config_.disk_writable && current_disk_index_ < config_.disk_paths.size()) {
        machine_->disk_image().set_host_path(config_.disk_paths[current_disk_index_]);
    }
    machine_->disk_drive().attach_image(&machine_->disk_image());
    machine_->disk_drive().set_disk_changed(true);
    update_window_title_for_current_disk();
    log_disk_swap();
}

void Sdl3App::on_snapshot_hotkey() {
    // M36 Phase 3: request a comprehensive debug snapshot. The actual capture
    // is deferred to the end of run_one_frame() (after on_vsync_boundary())
    // so the machine is always at a clean frame boundary and the
    // deterministic id (frame_count()) is stable -- the safe, non-perturbing
    // capture point (planner §4.1). Setting a flag keeps this handler O(1).
    snapshot_requested_ = true;
}

void Sdl3App::on_stream_toggle_hotkey() {
    // DEC-0052: F10 toggles live stream-capture at the machine level. Arming is
    // side-effect-free at this instant (per-frame bundles + the trace ring only
    // begin accumulating on the following frames/instructions); finalizing dumps
    // the trace ring + a final snapshot. Both are non-perturbing to emulation.
    if (machine_->stream_capture_active()) {
        machine_->set_stream_capture_enabled(false);  // manual OFF -> finalize
        std::cerr << "Stream capture OFF (finalized).\n";
    } else {
        // Stamp the stream id from the current deterministic frame/cycle id so
        // an identical run toggling at the same frame yields identical stream
        // paths. DEC-0052 stream-light: arm the lightweight mode when
        // --stream-light was given, so a long armed session (e.g. YS-II game
        // start -> building entry) isn't bogged down by heavy per-frame I/O.
        const std::string stream_id = machine_->snapshot_id();
        machine_->set_stream_capture_enabled(true, stream_id, config_.stream_light);
        std::cerr << "Stream capture ON: stream_" << stream_id
                  << (config_.stream_light ? " (light)" : " (heavy)") << "\n";
    }
}

void Sdl3App::on_persistence_step_hotkey(const int dir) {
    // Presentation-only: step the video presenter's phosphor-persistence blend
    // weight by +/-10% on a 0,10,...,100 grid, wrapping at the ends (Alt+B = +10,
    // Shift+Alt+B = -10). Derives the next step from the presenter's CURRENT
    // weight so the presenter stays the single source of truth. Snaps a non-grid
    // launch value (e.g. --persistence 55) to the nearest grid point first. No-op
    // with no presenter (never happens post-init).
    if (!video_presenter_) {
        return;
    }
    const int cur = video_presenter_->persistence();
    int idx = (cur + 5) / 10;              // nearest 0..10 grid index
    idx = (idx + dir + 11) % 11;           // wrap inclusive [0..100]
    const int next = idx * 10;
    video_presenter_->set_persistence(next);
    std::cerr << "sdl3: phosphor persistence " << next << "%"
              << (next == 0 ? " (off)" : " (inter-frame blend; Alt+B +10 / Shift+Alt+B -10)") << "\n";
}

void Sdl3App::on_persistence_mode_hotkey() {
    // Presentation-only: toggle the phosphor blend mode Average <-> Peak (Alt+M).
    // No-op with no presenter (never happens post-init).
    if (!video_presenter_) {
        return;
    }
    const bool to_peak = video_presenter_->persistence_mode() == PhosphorMode::Average;
    video_presenter_->set_persistence_mode(to_peak ? PhosphorMode::Peak : PhosphorMode::Average);
    std::cerr << "sdl3: phosphor mode " << (to_peak ? "PEAK (peak-hold; sprites stay full brightness)"
                                                    : "AVERAGE (weighted mean)")
              << " (Alt+M)\n";
}

void Sdl3App::on_volume_step_hotkey(const int dir) {
    // M52 (DEC-0079): step the SDL3 master volume by kVolumeStep on the
    // {0,10,...,100} grid, CLAMPING (never wrapping) at both ends -- turning
    // volume down past 0 must NEVER jump to 100 (a dangerous loudness surprise).
    // Derives the next step from the cached master_volume_, applies it to the
    // audio presenter, and prints one stderr feedback line per accepted step
    // (cheap; the owner wants feedback, and key-repeat is honored so a held key
    // ramps). Presentation-only: never perturbs emulation/determinism.
    master_volume_ = step_master_volume(master_volume_, dir, kVolumeStep);
    if (audio_presenter_) {
        audio_presenter_->set_master_volume(master_volume_);
    }
    std::cerr << "sdl3: master volume " << master_volume_ << "%"
              << (master_volume_ == 0 ? " (muted)" : master_volume_ == 100 ? " (full)" : "")
              << " (Alt+D -10 / Alt+U +10)\n";
}

void Sdl3App::on_disk_writable_toggle_hotkey() {
    // M52 (DEC-0079): toggle disk-writable at runtime, keeping config_.disk_writable
    // AND the current image's host-path binding in lockstep so the two flush gates
    // (exit flush guarded by the binding; swap flush guarded by config_.disk_writable)
    // always agree. ZERO src/devices/fdc edits -- ON binds via DiskImage::set_host_path,
    // OFF unbinds with an empty path (flush() then no-ops).
    const bool turning_on = !config_.disk_writable;
    config_.disk_writable = turning_on;
    if (turning_on) {
        // Bind the CURRENT disk's host path. A late-ON (after in-memory writes
        // accumulated while OFF) binds the path so the WHOLE in-memory image --
        // including those earlier writes -- is flushed at the next swap/exit.
        if (current_disk_index_ < config_.disk_paths.size()) {
            machine_->disk_image().set_host_path(config_.disk_paths[current_disk_index_]);
        }
        std::cerr << "sdl3: disk-writable ENABLED (Alt+S) -- host .dsk saves persist on swap/exit\n";
    } else {
        // Unbind: future swap/exit flushes no-op for the current image. The
        // in-memory writes are KEPT (never rolled back) but, being unbound, a
        // dirty image toggled OFF is DISCARDED at exit (never written to host).
        machine_->disk_image().set_host_path(std::filesystem::path{});
        std::cerr << "sdl3: disk-writable disabled (Alt+S) -- writes stay in memory; "
                     "current disk will NOT be saved\n";
    }
}

bool Sdl3App::flush_current_disk() {
    // M36-S-c: explicit write-back of the mounted disk. A genuine no-op unless
    // disk-writable bound a host path (default: no host path -> flush() false).
    if (!initialized_ || disk_images_.empty()) {
        return false;
    }
    return machine_->disk_image().flush();
}

void Sdl3App::reset_machine() {
    // M56 (DEC-0084, planner §4.2): a FRONTEND reset orchestrator over the
    // UNCHANGED machine cold_boot() -- no src/machine edit. cold_boot() keeps
    // any inserted cartridges (hbf1xv_machine.cpp:496-500 "NEVER unloads") but
    // RE-SYNTHESIZES the disk medium (:490-492), so a runtime reset must snapshot
    // and re-attach the live floppy or the session silently loses it.

    // (a) Snapshot the LIVE mounted disk (bytes + host_path + dirty) so the
    //     physical floppy -- and its in-session in-memory writes -- survive the
    //     reset. A reset button does not change the disk in the drive.
    devices::fdc::DiskImage live = std::move(machine_->disk_image());
    const bool had_disk = !disk_images_.empty();

    machine_->cold_boot();  // carts persist; disk re-synthesized; scheduler -> cycle 0

    // (b) Re-attach the surviving disk (or leave the drive empty if none mounted),
    //     mirroring load_configured_assets()'s attach/else-detach shape.
    if (had_disk) {
        machine_->disk_image() = std::move(live);
        machine_->disk_drive().attach_image(&machine_->disk_image());
        machine_->disk_drive().set_disk_changed(true);  // media re-present after the power cycle
    } else {
        machine_->disk_drive().attach_image(nullptr);
    }

    // (c) Re-run the init() post-cold_boot device-enable block VERBATIM
    //     (init() :322-325,333-335,342) so a runtime reset lands in the same
    //     state a fresh init() would.
    machine_->psg().set_audio_sync_enabled(true);
    machine_->psg().reset_audio_sync(machine_->elapsed_cycles());  // elapsed==0 now
    machine_->click_dac().set_capture_enabled(true);
    if (config_.speed_level.has_value()) {
        machine_->pause_controller().set_speed_level(*config_.speed_level);
    }
    machine_->set_fast_disk(config_.fast_disk);

    // (d) Reset the frontend audio-production cursors to their init() values
    //     (sdl3_app.h:537-539 + init() :325). cold_boot restarted the scheduler at
    //     cycle 0, so leaving the cursors at pre-reset (large) values would make
    //     emit_due_audio() compute against stale boundaries -> post-reset audio
    //     glitch/silence (R7). Presentation-only; never affects the state dump.
    audio_sample_index_ = 1;
    audio_prev_boundary_ = 0;
    audio_next_boundary_ = Sdl3AudioPresenter::kSystemClockHz / Sdl3AudioPresenter::kSampleRateHz;
    // M57 (DEC-0085, REQ-M57-002 H7): the (d) cursor reset above fixed the FRONTEND
    // cursors, but the Sdl3AudioPresenter (and its AudioPacer) DELIBERATELY survives
    // the reset -- and the pacer's cumulative samples_produced_ is keyed to the
    // machine's elapsed cycles, which cold_boot() just restarted at 0. Left as-is,
    // plan()'s monotonic guard (total_due > samples_produced_) stays false until the
    // machine re-accumulates past the pre-reset count => minutes of permanent
    // post-reset silence (the DEF-1 root cause: audio dies after the first
    // menu-driven reset / cartridge-or-disk insert). Rewind the pacer baseline too.
    // Presentation-only; never touches emulated device state or the state dump.
    if (audio_presenter_) {
        audio_presenter_->reset_pacing();
    }
}

void Sdl3App::power_cycle(const std::size_t ram_bytes) {
    // M57 (DEC-0085-AMENDMENT-A, addendum §2.4/§3): rebuild the machine at a new
    // RAM size and boot fresh, media surviving. The machine is non-movable, so the
    // ONLY correct resize is reconstruction through its M42 size ctor -- ZERO
    // src/machine edit. Transactional: any failure in the PRE-READ phase aborts
    // with the OLD machine fully intact.

    // (1) TRANSACTIONAL PRE-READ (before touching the live machine): read + resolve
    //     EVERY occupied cartridge bay from its tracked path. Any missing/unreadable/
    //     unresolvable file ABORTS the whole power-cycle (apply_open_disks
    //     discipline) -- the old machine keeps running unchanged.
    struct PreReadCart {
        int slot = 0;
        std::string path;
        std::vector<std::uint8_t> image;
        devices::cartridge::CartridgeMapperType type =
            devices::cartridge::CartridgeMapperType::Mirrored;
    };
    std::vector<PreReadCart> carts;
    machine::CartridgeIdentificationSession ident_session(config_.softwaredb_path);
    for (int slot = 1; slot <= 2; ++slot) {
        const std::optional<std::string>& path = cart_path_[slot - 1];
        if (!path.has_value()) {
            continue;  // empty bay
        }
        std::ifstream in(*path, std::ios::binary);
        if (!in) {
            std::cerr << "sdl3: cannot re-read cartridge " << *path
                      << "; power-cycle aborted, machine unchanged\n";
            return;  // OLD machine intact, no mutation yet
        }
        std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        machine::ParsedCartridgeSlotCli spec;
        spec.path = *path;
        spec.type = devices::cartridge::CartridgeMapperType::Mirrored;
        spec.type_was_explicit = false;
        const auto resolution = ident_session.resolve(slot, spec, image);
        if (!resolution.ok) {
            std::cerr << "sdl3: cannot re-resolve cartridge " << *path
                      << "; power-cycle aborted, machine unchanged\n";
            return;  // OLD machine intact
        }
        carts.push_back(PreReadCart{slot, *path, std::move(image), resolution.type});
    }

    // (2) Flush each occupied FM-PAC's battery SRAM to its host file BEFORE the old
    //     machine is destroyed (the eject_cartridge pattern) so game saves survive
    //     the cycle -- the re-insert below reloads it via the re-bound path.
    if ((machine_->fmpac(1) != nullptr || machine_->fmpac(2) != nullptr) &&
        machine_->flush_fmpac_sram()) {
        std::cerr << "sdl3: flushed FM-PAC SRAM to \"" << machine_->fmpac_sram_path().string()
                  << "\" (power cycle)\n";
    }

    // (3) Snapshot the LIVE disk (bytes + host_path + dirty). Physical-medium
    //     semantics: a power cycle does not erase the floppy (reset_machine (a)).
    devices::fdc::DiskImage live = std::move(machine_->disk_image());
    const bool had_disk = !disk_images_.empty();

    // (4) REBUILD at the new RAM size. optional::emplace destroys the old instance
    //     and constructs a fresh one through the EXACT M42 size ctor -- so the new
    //     machine IS a freshly-constructed + cold-booted machine of size ram_bytes
    //     (the fresh-boot-equivalence oracle holds by construction).
    machine_.emplace(ram_bytes);

    // (5) Re-run init()'s post-construction sequence against the fresh instance
    //     (addendum §2.4 steps 1-6,8). SDL/window/presenter/menu persist.
    if (config_.event_log_filename.has_value()) {
        machine_->set_event_logging_enabled(true);
    }
    if (config_.snapshot_dir.has_value()) {
        machine_->set_debug_root(*config_.snapshot_dir);
    }
    machine_->set_bios_filenames(config_.bios_roms);
    machine_->set_asset_root(config_.bios_dir);
    machine_->cold_boot();
    machine_->psg().set_audio_sync_enabled(true);
    machine_->psg().reset_audio_sync(machine_->elapsed_cycles());  // elapsed==0 now
    machine_->click_dac().set_capture_enabled(true);
    if (config_.speed_level.has_value()) {
        machine_->pause_controller().set_speed_level(*config_.speed_level);
    }
    machine_->set_fast_disk(config_.fast_disk);
    if (config_.trace_cpu_filename.has_value()) {
        machine_->set_cpu_trace_enabled(true);
    }

    // (6) Re-attach the surviving disk (reset_machine (b) pattern).
    if (had_disk) {
        machine_->disk_image() = std::move(live);
        machine_->disk_drive().attach_image(&machine_->disk_image());
        machine_->disk_drive().set_disk_changed(true);  // media re-present after the power cycle
    } else {
        machine_->disk_drive().attach_image(nullptr);
    }

    // (7) Re-insert each pre-read cartridge (FM-PAC SRAM re-bind BEFORE load, the
    //     apply_open_cartridge order), reloading the just-flushed battery SRAM.
    for (const PreReadCart& c : carts) {
        if (c.type == devices::cartridge::CartridgeMapperType::FmPac &&
            !config_.fmpac_sram_disabled) {
            const std::filesystem::path sram_path =
                config_.fmpac_sram_path.has_value()
                    ? std::filesystem::path(*config_.fmpac_sram_path)
                    : std::filesystem::path(c.path + ".sram");
            machine_->set_fmpac_sram_path(sram_path);
        }
        // The image was validated in the PRE-READ resolve, so this re-insert into
        // the fresh machine is expected to succeed; note (do not abort) on the rare
        // failure -- the machine is already rebuilt, so a stderr note is the honest
        // outcome (the bay is simply empty).
        const devices::cartridge::CartridgeLoadResult result =
            machine_->load_cartridge(c.slot, c.type, c.image);
        if (result != devices::cartridge::CartridgeLoadResult::Ok) {
            std::cerr << "sdl3: warning: re-insert of " << c.path << " into slot " << c.slot
                      << " failed after power-cycle (bay left empty)\n";
            cart_path_[c.slot - 1] = std::nullopt;
        }
    }

    // (8) Reset the frontend audio cursors + rewind the surviving pacer -- the H7
    //     fix applies identically (reset_machine (d) verbatim), so post-cycle audio
    //     is healthy from cycle 0.
    audio_sample_index_ = 1;
    audio_prev_boundary_ = 0;
    audio_next_boundary_ = Sdl3AudioPresenter::kSystemClockHz / Sdl3AudioPresenter::kSampleRateHz;
    if (audio_presenter_) {
        audio_presenter_->reset_pacing();
    }

    // (9) Keep config consistent for any later reset/cycle, and note it.
    config_.ram_bytes = ram_bytes;
    std::cerr << "sdl3: power-cycled: RAM " << (ram_bytes / 1024u) << " KB\n";
}

// --- M56 (DEC-0084): async file-dialog mailbox + F1-F5 media seams -----------

void SDLCALL Sdl3App::dialog_callback(void* userdata, const char* const* filelist,
                                      int /*filter*/) {
    // M56 (DEC-0084, planner §3.1): the callback may run OFF the main thread
    // (SDL_dialog.h:113,125-126) and the filelist is freed on return
    // (:90-91) -- so deep-copy every UTF-8 path INSIDE the mutex and record only
    // data (NO machine mutation here). error (NULL) vs cancel ([0]==NULL) are
    // classified by the SDL-free classify_dialog_filelist().
    auto* self = static_cast<Sdl3App*>(userdata);
    SDL_LockMutex(self->dialog_.mutex);
    self->dialog_.paths.clear();
    self->dialog_.cancelled = false;
    self->dialog_.error = false;
    self->dialog_.error_message.clear();
    const DialogResultKind kind = classify_dialog_filelist(
        filelist == nullptr, filelist != nullptr && filelist[0] == nullptr);
    switch (kind) {
        case DialogResultKind::Error:
            self->dialog_.error = true;
            self->dialog_.error_message = SDL_GetError();  // copy the string NOW
            break;
        case DialogResultKind::Cancel:
            self->dialog_.cancelled = true;
            break;
        case DialogResultKind::Selection:
            for (const char* const* p = filelist; *p != nullptr; ++p) {
                self->dialog_.paths.emplace_back(*p);  // DEEP COPY each
            }
            break;
    }
    self->dialog_.pending = true;
    self->dialog_.in_flight = false;
    SDL_UnlockMutex(self->dialog_.mutex);
}

void Sdl3App::drain_dialog_mailbox() {
    // M56 (DEC-0084, planner §3.1): main-loop drain. Pops a pending dialog result
    // under the mutex, then applies it on the MAIN thread (all machine mutation
    // lives here, never in the callback). Called ONLY when menu_ != null.
    DialogKind kind = DialogKind::None;
    bool cancelled = false;
    bool error = false;
    std::vector<std::string> paths;
    std::string err;
    SDL_LockMutex(dialog_.mutex);
    if (!dialog_.pending) {
        SDL_UnlockMutex(dialog_.mutex);
        return;
    }
    kind = dialog_.kind;
    cancelled = dialog_.cancelled;
    error = dialog_.error;
    paths = std::move(dialog_.paths);
    err = std::move(dialog_.error_message);
    dialog_.pending = false;
    dialog_.kind = DialogKind::None;
    SDL_UnlockMutex(dialog_.mutex);

    if (error) {
        std::cerr << "sdl3: file dialog error: " << err << "\n";
        return;
    }
    if (cancelled || paths.empty()) {
        return;  // cancel / empty selection = no-op
    }
    switch (kind) {
        case DialogKind::OpenDisks: apply_open_disks(paths); break;
        case DialogKind::OpenCartSlot1: apply_open_cartridge(1, paths[0]); break;
        case DialogKind::OpenCartSlot2: apply_open_cartridge(2, paths[0]); break;
        case DialogKind::SaveBlankDisk: apply_new_blank_disk(paths[0]); break;
        case DialogKind::OpenBiosFolder: apply_bios_folder(paths[0]); break;  // M60 (DEC-0089)
        case DialogKind::None: break;
    }
}

void Sdl3App::open_disk_dialog() {
    // Main thread only (menu dispatch runs inside run_one_frame()). Inert when no
    // mailbox mutex exists (--hidden-window: never allocated).
    if (dialog_.mutex == nullptr) {
        return;
    }
    SDL_LockMutex(dialog_.mutex);
    const bool busy = dialog_.in_flight || dialog_.pending;  // double-open / undrained guard
    if (!busy) {
        dialog_.in_flight = true;
        dialog_.kind = DialogKind::OpenDisks;
    }
    SDL_UnlockMutex(dialog_.mutex);
    if (busy) {
        return;
    }
    // static: the filters must outlive the (possibly deferred) callback (SDL_dialog.h:145).
    // DEC-0095-AMENDMENT-B: disk-image extensions ONLY (no "All files" escape hatch)
    // so a non-disk file (e.g. a BIOS ROM) cannot be picked as a disk; apply_open_disks
    // re-validates regardless of how the path arrives (recent/CLI/typed).
    static const SDL_DialogFileFilter kDiskFilters[] = {{"MSX disk image", "dsk;di1;di2;360;720"}};
    // M64: default the picker to the configured disk directory (XML
    // <machine><disk dir>, default "disks") when it exists, else the working
    // directory; "" degrades to nullptr = no preference. Stored in the MEMBER
    // so the string outlives the async (possibly deferred) callback -- the same
    // lifetime rule as the static filter arrays above (SDL_dialog.h:145).
    dialog_default_dir_ = resolve_dialog_default_dir(config_.disk_dir);
    SDL_ShowOpenFileDialog(&Sdl3App::dialog_callback, this, window_, kDiskFilters, 1,
                           dialog_default_dir_.empty() ? nullptr : dialog_default_dir_.c_str(),
                           /*allow_many=*/true);  // multi-select (owner UX contract)
}

void Sdl3App::open_cartridge_dialog(const int slot) {
    if (dialog_.mutex == nullptr) {
        return;
    }
    const DialogKind kind = (slot == 2) ? DialogKind::OpenCartSlot2 : DialogKind::OpenCartSlot1;
    SDL_LockMutex(dialog_.mutex);
    const bool busy = dialog_.in_flight || dialog_.pending;
    if (!busy) {
        dialog_.in_flight = true;
        dialog_.kind = kind;
    }
    SDL_UnlockMutex(dialog_.mutex);
    if (busy) {
        return;
    }
    // DEC-0095-AMENDMENT-B: cartridge-ROM extensions ONLY (no "All files"); a .dsk
    // cannot be picked as a cart. apply_open_cartridge re-validates the extension.
    static const SDL_DialogFileFilter kCartFilters[] = {{"MSX cartridge ROM", "rom;mx1;mx2"}};
    // M64 (was M63 cwd): default the picker to the configured cartridge
    // directory (XML <machine><cartridge dir>, default "roms") when it exists,
    // else the working directory. Stored in the MEMBER so the string outlives
    // the async (possibly deferred) callback -- the same lifetime rule as the
    // static filter arrays above (SDL_dialog.h:145); "" degrades to nullptr =
    // no preference.
    dialog_default_dir_ = resolve_dialog_default_dir(config_.cartridge_dir);
    SDL_ShowOpenFileDialog(&Sdl3App::dialog_callback, this, window_, kCartFilters, 1,
                           dialog_default_dir_.empty() ? nullptr : dialog_default_dir_.c_str(),
                           /*allow_many=*/false);
}

void Sdl3App::new_blank_disk_dialog() {
    if (dialog_.mutex == nullptr) {
        return;
    }
    SDL_LockMutex(dialog_.mutex);
    const bool busy = dialog_.in_flight || dialog_.pending;
    if (!busy) {
        dialog_.in_flight = true;
        dialog_.kind = DialogKind::SaveBlankDisk;
    }
    SDL_UnlockMutex(dialog_.mutex);
    if (busy) {
        return;
    }
    static const SDL_DialogFileFilter kDiskFilters[] = {{"MSX disk image", "dsk"},
                                                        {"All files", "*"}};
    // M64: the save picker's default_location is its LAST parameter
    // (src/external/sdl3/include/SDL3/SDL_dialog.h:213) -- same disk-dir default
    // + member-lifetime rule as open_disk_dialog above (one dialog in flight).
    dialog_default_dir_ = resolve_dialog_default_dir(config_.disk_dir);
    SDL_ShowSaveFileDialog(&Sdl3App::dialog_callback, this, window_, kDiskFilters, 2,
                           dialog_default_dir_.empty() ? nullptr : dialog_default_dir_.c_str());
}

void Sdl3App::open_bios_folder_dialog() {
    // M60 (DEC-0089): Machine > BIOS Folder... -- the SAME mailbox discipline as
    // the M56 file dialogs (double-open/undrained guard; deep-copy in the
    // callback; main-loop drain gated on menu_). Inert when no mailbox mutex
    // exists (--hidden-window: never allocated).
    if (dialog_.mutex == nullptr) {
        return;
    }
    SDL_LockMutex(dialog_.mutex);
    const bool busy = dialog_.in_flight || dialog_.pending;
    if (!busy) {
        dialog_.in_flight = true;
        dialog_.kind = DialogKind::OpenBiosFolder;
    }
    SDL_UnlockMutex(dialog_.mutex);
    if (busy) {
        return;
    }
    // M63/M64: default the picker to the CURRENT BIOS directory when it exists
    // (the folder the ROMs already live in); otherwise the working directory;
    // "" -> nullptr = no preference. Routed through the ONE shared
    // resolve_dialog_default_dir() helper (same effective behavior as M63);
    // the policy core is the pure choose_dialog_dir()
    // (frontend/dialog_default_dir.h, unit-tested) and the probing never
    // throws out of the opener.
    dialog_default_dir_ = resolve_dialog_default_dir(config_.bios_dir);
    // The folder picker (src/external/sdl3/include/SDL3/SDL_dialog.h:258) shares
    // the SDL_DialogFileCallback contract with the file dialogs -- filelist[0]
    // is the chosen folder path -- so the ONE dialog_callback handles it too.
    // The MEMBER default-dir outlives the async callback (see the M63 note in
    // open_cartridge_dialog; one member suffices -- one dialog in flight).
    SDL_ShowOpenFolderDialog(&Sdl3App::dialog_callback, this, window_,
                             dialog_default_dir_.empty() ? nullptr : dialog_default_dir_.c_str(),
                             /*allow_many=*/false);
}

void Sdl3App::apply_bios_folder(const std::string& path) {
    // M60 (DEC-0089): TRANSACTIONAL pre-validate -- the chosen folder must
    // contain ALL 7 configured BIOS ROM files (config_.bios_roms, the role-keyed
    // set load_rom_assets() resolves under asset_root), each openable AND
    // readable, BEFORE anything mutates. Never power-cycle into a folder that
    // cannot boot (DEC-0089 requirement; the apply_open_disks discipline).
    const machine::EmulatorConfig::BiosRoms& roms = config_.bios_roms;
    const std::array<const std::string*, 7> required = {
        &roms.bios,     &roms.sub,        &roms.kanji_driver, &roms.disk,
        &roms.fm_music, &roms.kanji_font, &roms.firmware};
    for (const std::string* name : required) {
        const std::filesystem::path p = std::filesystem::path(path) / *name;
        std::ifstream in(p, std::ios::binary);
        char first = 0;
        if (!in || !in.get(first)) {  // absent, unopenable, or unreadable/empty
            std::cerr << "sdl3: BIOS folder " << path << " is missing/unreadable " << *name
                      << "; keeping current BIOS folder " << config_.bios_dir << "\n";
            return;  // OLD config + machine fully intact, no mutation
        }
    }

    // COMMIT: point config at the new folder, then power-cycle at the SAME RAM
    // size. power_cycle() re-applies set_asset_root(config_.bios_dir) +
    // set_bios_filenames + cold_boot + the audio-cursor/reset_pacing block
    // (LIFECYCLE-AUDIO INVARIANT), with mounted disks + inserted carts surviving.
    const std::string previous = config_.bios_dir;
    config_.bios_dir = path;
    power_cycle(config_.ram_bytes);
    // power_cycle() has its OWN transactional pre-read (occupied cartridge bays);
    // if THAT aborted, the machine was never rebuilt -- roll bios_dir back so
    // config and the live machine stay consistent. asset_root() is the witness:
    // a successful cycle set it to the new path.
    if (machine_->asset_root() != std::filesystem::path(path)) {
        config_.bios_dir = previous;
        std::cerr << "sdl3: BIOS folder unchanged (power-cycle aborted); still "
                  << config_.bios_dir << "\n";
        return;
    }
    std::cerr << "sdl3: BIOS folder -> " << path << "\n";
}

void Sdl3App::apply_open_disks(const std::vector<std::string>& new_paths) {
    // M56 (DEC-0084, planner §3.3, R1): F1 apply. Ordering is load-bearing --
    // read + validate the WHOLE new set BEFORE touching the current mount, so a
    // bad selection never destroys the running disk. REPLACE semantics: the
    // selection becomes the whole F11 cycle, index -> 0, first disk mounts.
    if (new_paths.empty()) {
        return;
    }

    // 0. DEC-0095-AMENDMENT-B: accept ONLY disk-image files. Primary fix for the
    //    f1xvbios.rom incident -- a ROM opened as a WRITABLE disk gets 0x00-padded
    //    to 720 KB and flushed back over the original. Reject the WHOLE set on any
    //    non-disk path; the current mount is never touched (checked before the
    //    pre-read so a mixed selection cannot partially apply).
    for (const std::string& path : new_paths) {
        if (!is_disk_extension(path)) {
            std::cerr << "sdl3: refusing to open '" << path
                      << "' as a disk -- not a disk image (expected .dsk/.di1/.di2/.360/.720). "
                         "Nothing changed.\n";
            return;
        }
    }

    // 1. TRANSACTIONAL PRE-READ (no state change yet).
    std::vector<std::vector<std::uint8_t>> temp;
    temp.reserve(new_paths.size());
    for (const std::string& path : new_paths) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "sdl3: cannot open " << path << "; keeping current disk\n";
            return;  // no partial state, ever
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        temp.push_back(std::move(bytes));
    }

    // 2. FLUSH-THEN-DISCARD the outgoing mounted disk (mirrors on_disk_swap_hotkey
    //    :814-819) -- the SAME DiskImage::flush() primitive the WD2793 write suite
    //    depends on (no src/devices/fdc edit).
    if (config_.disk_writable) {
        machine_->disk_image().flush();  // no-op if no host path / not dirty
    } else if (machine_->disk_image().dirty()) {
        std::cerr << "sdl3: discarding unsaved writes to the current disk "
                     "(disk-writable OFF; enable Alt+S to persist)\n";
    }

    // 3. COMMIT the new set.
    disk_images_ = std::move(temp);
    config_.disk_paths = new_paths;  // keep the parallel list in lockstep (index binding)
    current_disk_index_ = 0;
    machine_->disk_image() = devices::fdc::DiskImage(disk_images_[0]);
    if (config_.disk_writable) {
        // DEC-0095-AMENDMENT-B: bind host write-back ONLY for an exact 2DD image.
        // A smaller/larger file would be 0x00-padded to 720 KB and overwritten on
        // flush, so it mounts READ-ONLY instead (readable, never corrupted).
        std::error_code ec;
        const std::uintmax_t sz = std::filesystem::file_size(config_.disk_paths[0], ec);
        if (!ec && sz == kDiskImageBytes) {
            machine_->disk_image().set_host_path(config_.disk_paths[0]);
        } else {
            std::cerr << "sdl3: '" << config_.disk_paths[0] << "' is "
                      << (ec ? std::string("unsized") : std::to_string(sz)) << " bytes, not "
                      << kDiskImageBytes
                      << " (720 KB) -- mounted READ-ONLY (write-back disabled to avoid "
                         "overwriting a non-2DD file)\n";
        }
    }
    machine_->disk_drive().attach_image(&machine_->disk_image());
    machine_->disk_drive().set_disk_changed(true);  // media-change signal (like a swap)
    update_window_title_for_current_disk();
    log_disk_swap();
    // DEC-0095: record the opened set's first disk in File > Recent (the entry
    // that represents "open this game"); inert unless recent persistence is on.
    if (!new_paths.empty()) {
        push_recent(new_paths.front());
    }
}

void Sdl3App::apply_open_cartridge(const int slot, const std::string& rom_path) {
    // M56 (DEC-0084, planner §4.3): F2 apply. Reuse the EXISTING resolver +
    // startup-path FM-PAC SRAM bind-before-load; insert IMPLIES a machine reset
    // (the cart persists cold_boot).
    using devices::cartridge::CartridgeLoadResult;
    using devices::cartridge::CartridgeMapperType;

    // DEC-0095-AMENDMENT-B: accept ONLY ROM-image files as cartridges (symmetry
    // with the disk guard; a .dsk inserted as a cart is a user error). Slot state
    // is untouched on rejection.
    if (!is_cartridge_extension(rom_path)) {
        std::cerr << "sdl3: refusing to insert '" << rom_path
                  << "' -- not a cartridge ROM (expected .rom/.mx1/.mx2). Slot " << slot
                  << " unchanged.\n";
        return;
    }

    std::ifstream in(rom_path, std::ios::binary);
    if (!in) {
        std::cerr << "sdl3: cannot open cartridge " << rom_path << "; slot " << slot
                  << " unchanged\n";
        return;
    }
    std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());

    // Resolve the mapper via the ONE shared resolver, auto-identification ON.
    machine::CartridgeIdentificationSession ident_session(config_.softwaredb_path);
    machine::ParsedCartridgeSlotCli spec;
    spec.path = rom_path;
    spec.type = CartridgeMapperType::Mirrored;
    spec.type_was_explicit = false;
    const auto resolution = ident_session.resolve(slot, spec, image);
    for (const std::string& message : resolution.messages) {
        std::cerr << message << "\n";
    }
    if (!resolution.ok) {
        std::cerr << "sdl3: cartridge not inserted (unsupported mapper); slot " << slot
                  << " unchanged\n";
        return;  // no insert, no reset
    }

    // FM-PAC SRAM bind BEFORE load (mirrors the startup order, :103-110).
    if (resolution.type == CartridgeMapperType::FmPac && !config_.fmpac_sram_disabled) {
        const std::filesystem::path sram_path =
            config_.fmpac_sram_path.has_value()
                ? std::filesystem::path(*config_.fmpac_sram_path)
                : std::filesystem::path(rom_path + ".sram");
        machine_->set_fmpac_sram_path(sram_path);
    }

    const CartridgeLoadResult result =
        machine_->load_cartridge(slot, resolution.type, std::move(image));
    if (result != CartridgeLoadResult::Ok) {
        std::cerr << "sdl3: failed to load cartridge " << rom_path << " into slot " << slot << "\n";
        return;
    }
    // M57 (DEC-0085-AMENDMENT-A): record this bay's source so a power-cycle can
    // re-read it. Set BEFORE reset_machine() (which does not touch cart_path_).
    cart_path_[slot - 1] = rom_path;
    reset_machine();  // insert implies power-on-with-cart-inserted (cart persists cold_boot)
    std::cerr << "sdl3: inserted " << rom_path << " into slot " << slot << " ("
              << devices::cartridge::to_string(resolution.type) << "); machine reset\n";
    // DEC-0095: record the inserted cartridge in File > Recent; inert unless
    // recent persistence is on.
    push_recent(rom_path);
}

void Sdl3App::apply_new_blank_disk(const std::string& path) {
    // M56 (DEC-0084, planner §7.3/§7.4): F5 apply. Write the blank ONLY -- NO
    // auto-mount (that would silently drop the running game's disk, a data-loss
    // vector). The layout is the frontend-local re-expression (build_blank_disk_
    // image), NEVER msx_diskutil (DEC-0080 boundary).
    const std::vector<std::uint8_t> img = build_blank_disk_image();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "sdl3: cannot create blank disk " << path << "\n";
        return;
    }
    out.write(reinterpret_cast<const char*>(img.data()), static_cast<std::streamsize>(img.size()));
    std::cerr << "sdl3: created blank 720 KB MSX-DOS disk " << path
              << " (open it with File > Open Disk to use)\n";
}

void Sdl3App::eject_disk() {
    // M56 (DEC-0084, planner §6): flush-then-unbind-then-detach. Flush FIRST while
    // the host path is still bound; NO machine reset (ejecting a floppy does not
    // reset an MSX).
    if (config_.disk_writable) {
        machine_->disk_image().flush();  // no-op if no host path / not dirty
    } else if (machine_->disk_image().dirty()) {
        std::cerr << "sdl3: discarding unsaved writes to the ejected disk "
                     "(disk-writable OFF; enable Alt+S to persist)\n";
    }
    machine_->disk_drive().attach_image(nullptr);
    disk_images_.clear();
    config_.disk_paths.clear();
    current_disk_index_ = 0;
    update_window_title_for_current_disk();  // -> "(no disk)"
    std::cerr << "sdl3: ejected disk\n";
}

void Sdl3App::eject_cartridge(const int slot) {
    // M56 (DEC-0084, planner §6): flush FM-PAC battery SRAM FIRST (before unload
    // frees the FM-PAC), then unload, then reset (cart removal implies a power
    // cycle -- R8). flush_fmpac_sram() is a no-op for a non-FM-PAC slot, but
    // gating on fmpac(slot) keeps the stderr note accurate.
    if (machine_->fmpac(slot) != nullptr && machine_->flush_fmpac_sram()) {
        std::cerr << "sdl3: flushed FM-PAC SRAM to \"" << machine_->fmpac_sram_path().string()
                  << "\"\n";
    }
    machine_->unload_cartridge(slot);
    cart_path_[slot - 1] = std::nullopt;  // M57: the bay is now empty for a power-cycle re-read.
    reset_machine();
    std::cerr << "sdl3: ejected cartridge from slot " << slot << "; machine reset\n";
}

// --- M55 (DEC-0083): menu-shared presentation-only seams ---------------------

bool Sdl3App::menu_active() const {
    return menu_ != nullptr;
}

void Sdl3App::toggle_fullscreen() {
    // Extracted from the inline Alt+Enter handler so both the hotkey and the
    // menu Video > Fullscreen item share one implementation. Presentation-only.
    fullscreen_ = !fullscreen_;
    if (window_ != nullptr) {
        SDL_SetWindowFullscreen(window_, fullscreen_);
        // M63: the actual post-transition geometry is logged by the per-frame
        // log_geometry_if_changed() (below) -- reading sizes synchronously here
        // can report PRE-transition values on compositors that apply fullscreen
        // asynchronously (owner Pi). This just marks the intent.
        std::cerr << "sdl3: fullscreen toggled " << (fullscreen_ ? "on" : "off") << "\n";
    }
}

void Sdl3App::log_geometry_if_changed() {
    // M63 diagnostic (interactive-only): the Pi fullscreen menu-bar mis-scaling
    // is a points-vs-pixels problem, so print -- once per geometry CHANGE, from
    // the per-frame path so the numbers are always POST-transition/accurate --
    // the SDL window size (points), window size in pixels, and the true render
    // output size, alongside what ImGui itself sees (io.DisplaySize +
    // FramebufferScale, via menu_->log_io_geometry). Never runs headless
    // (menu_/window_ null). Change-gated so it never spams.
    if (window_ == nullptr || config_.hidden_window || menu_ == nullptr) {
        return;
    }
    int pt_w = 0;
    int pt_h = 0;
    int px_w = 0;
    int px_h = 0;
    int out_w = 0;
    int out_h = 0;
    SDL_GetWindowSize(window_, &pt_w, &pt_h);
    SDL_GetWindowSizeInPixels(window_, &px_w, &px_h);
    if (renderer_ != nullptr) {
        SDL_GetRenderOutputSize(renderer_, &out_w, &out_h);
    }
    if (pt_w == diag_last_pt_w_ && pt_h == diag_last_pt_h_ && px_w == diag_last_px_w_ &&
        px_h == diag_last_px_h_ && out_w == diag_last_out_w_ && out_h == diag_last_out_h_) {
        return;  // no geometry change since the last log -- stay quiet
    }
    diag_last_pt_w_ = pt_w;
    diag_last_pt_h_ = pt_h;
    diag_last_px_w_ = px_w;
    diag_last_px_h_ = px_h;
    diag_last_out_w_ = out_w;
    diag_last_out_h_ = out_h;
    std::cerr << "sdl3: geometry " << (fullscreen_ ? "[fullscreen]" : "[windowed]") << " window "
              << pt_w << "x" << pt_h << " pts, " << px_w << "x" << px_h << " px, render output "
              << out_w << "x" << out_h << " px, bar_h " << menu_->bar_height() << "\n";
    menu_->log_io_geometry(fullscreen_ ? "fullscreen" : "windowed");
}

bool Sdl3App::query_display_usable_bounds(int& out_x, int& out_y, int& out_w, int& out_h) {
    // M61 (DEC-0090): the usable bounds of the display the window lives on.
    // STRUCTURALLY inert on the deterministic path: --hidden-window (and a null
    // window) NEVER queries the display, so ctest runs are byte-identical.
    // SDL_GetDisplayForWindow returns the display containing the window's
    // center (src/external/sdl3/include/SDL3/SDL_video.h:991); fall back to the
    // primary display (SDL_video.h:679) if that fails, then
    // SDL_GetDisplayUsableBounds (SDL_video.h:783) -- the desktop area minus
    // system-reserved portions (taskbar/dock/menubar).
    // DEC-0090-AMENDMENT-A: also outputs the usable rect's ORIGIN -- the
    // resize/maximize position clamp must know where the usable TOP is (the
    // owner-Pi desktop panel makes it y=36, not 0).
    if (config_.hidden_window || window_ == nullptr) {
        return false;
    }
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (display == 0) {
        display = SDL_GetPrimaryDisplay();
    }
    if (display == 0) {
        return false;
    }
    SDL_Rect usable{};
    if (!SDL_GetDisplayUsableBounds(display, &usable) || usable.w <= 0 || usable.h <= 0) {
        return false;
    }
    out_x = usable.x;
    out_y = usable.y;
    out_w = usable.w;
    out_h = usable.h;
    return true;
}

void Sdl3App::clamp_window_to_display() {
    // M61 (DEC-0090-AMENDMENT-A): re-apply the display fit after a window
    // geometry event. Owner-Pi datapoint: display usable = 800x444 @ (0,36) (a
    // ~36px desktop panel on the 800x480 7" screen); the M61 init fit made
    // startup correct, but a user MAXIMIZE grew the window past 444 and pushed
    // the window top -- and the top-anchored ImGui menu bar -- off-screen while
    // the per-frame picture letterbox kept rendering ("rendered area fine, only
    // menu bar truncated").
    //
    // LOOP SAFETY (the hard requirement): act ONLY when the window actually
    // exceeds the usable bounds or its top edge sits above the usable top. The
    // corrected geometry (w <= usable_w, h <= usable_h, y >= usable_y) SATISFIES
    // that predicate, so the SDL_EVENT_WINDOW_RESIZED/MOVED our own corrective
    // calls generate re-enters this function and immediately returns -- the
    // clamp settles in ONE step. SDL additionally suppresses no-change
    // RESIZED/MOVED events, and the caller runs at most one check per poll
    // batch, so the maximize triple (MAXIMIZED + RESIZED + PIXEL_SIZE_CHANGED)
    // issues at most one corrective request.
    if (window_ == nullptr || config_.hidden_window) {
        return;
    }
    // Fullscreen keeps its fixed surface: SetWindowSize/SetWindowPosition are
    // documented no-ops for fullscreen windows (SDL_video.h:1893/:1827) and the
    // M57 band math already insets within the surface. Check the LIVE flag so a
    // WM-initiated fullscreen is covered too, not just the Alt+Enter toggle.
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    if ((flags & SDL_WINDOW_FULLSCREEN) != 0) {
        return;
    }
    int usable_x = 0;
    int usable_y = 0;
    int usable_w = 0;
    int usable_h = 0;
    if (!query_display_usable_bounds(usable_x, usable_y, usable_w, usable_h)) {
        return;  // no display info -> keep today's behavior (matches the init-fit fallback)
    }
    int cur_w = 0;
    int cur_h = 0;
    int cur_x = 0;
    int cur_y = 0;
    SDL_GetWindowSize(window_, &cur_w, &cur_h);
    SDL_GetWindowPosition(window_, &cur_x, &cur_y);
    if (cur_w <= 0 || cur_h <= 0) {
        return;  // failed/degenerate query must never drive a corrective resize
    }
    const bool oversized = cur_w > usable_w || cur_h > usable_h;
    const bool top_off = cur_y < usable_y;
    if (!oversized && !top_off) {
        return;  // within bounds -- the loop-safety predicate (see above)
    }
    // A maximized window IGNORES SetWindowSize/SetWindowPosition requests
    // (SDL_video.h:1893/:1827 -- "If the window is in a ... maximized state,
    // this request has no effect"), so leave the maximized state first. The
    // RESTORED/RESIZED events this generates land in a later poll batch, where
    // the corrected geometry passes the predicate above.
    if ((flags & SDL_WINDOW_MAXIMIZED) != 0) {
        SDL_RestoreWindow(window_);
    }
    // Fitted size: the SAME pure fit function as the init path (window_fit.h,
    // no second fit copy), called with the CURRENT window as its own canvas
    // unit (cw=cur_w, ch=cur picture h), which reduces it to exactly "keep when
    // it fits, else min(current, usable)". A user-driven resize/maximize is
    // deliberately NOT snapped back to an integer 320Nx240N scale: the M57
    // letterbox fills ANY band below the bar, and the owner goal on the 7"
    // panel is the BIGGEST picture that keeps the menu bar visible (a clamped
    // maximize = 800x444, picture ~567x425 -- not a snap-down to 320x259).
    const int bar_h = menu_ ? menu_->bar_height() : 0;
    const int pic_h = std::max(1, cur_h - bar_h);
    const geometry::WindowFit fit =
        geometry::fit_window_to_display(cur_w, pic_h, bar_h, usable_w, usable_h, cur_w, pic_h);
    // Keep the menu bar's FULL height inside the usable area: the top edge must
    // not sit above the usable top. x is left alone -- a horizontal off-screen
    // drag is a user choice and never hides the top-anchored bar; the clamped
    // height (<= usable_h) placed at y >= usable_y also keeps the bottom from
    // exceeding the usable bottom in the maximize case (y == usable_y).
    const int new_y = std::max(cur_y, usable_y);
    SDL_SetWindowSize(window_, fit.w, fit.h);
    SDL_SetWindowPosition(window_, cur_x, new_y);
    // The AMENDMENT-A diagnostic: the owner is live on the Pi and needs to SEE
    // the clamp fire (same stderr channel as the DEC-0090 init-fit line).
    std::cerr << "sdl3: window clamped to fit display usable " << usable_w << "x" << usable_h
              << "@(" << usable_x << "," << usable_y << "): " << cur_w << "x" << cur_h << "@("
              << cur_x << "," << cur_y << ") -> " << fit.w << "x" << fit.h << "@(" << cur_x << ","
              << new_y << ")\n";
}

void Sdl3App::set_scale(const int scale) {
    // Menu Video > Scale N: 320N x (240N + menu-strip) window. Clamp to [1,8] to
    // match the CLI --scale range. No effect in fullscreen (SDL keeps the
    // fullscreen size) or under a null window. Presentation-only.
    // M57 (DEC-0085, §4.4): grow the window height by the menu-strip inset so the
    // PICTURE band stays a full 240N tall (unclipped N-scale) with the strip above
    // it. The inset is 0 when no menu exists -> 320N x 240N, the legacy size.
    // M61 (DEC-0090): clamp the pick through the SAME window_fit function used at
    // init, so a runtime Video > Scale choice can never re-overflow a small
    // display. On a query failure (or hidden-window, where the helper never
    // queries) the legacy unclamped size applies -- today's behavior.
    const int n = std::clamp(scale, 1, 8);
    if (window_ != nullptr) {
        const int inset = video_presenter_ ? video_presenter_->top_inset() : 0;
        int win_w = 320 * n;
        int win_h = 240 * n + inset;
        int usable_x = 0;
        int usable_y = 0;
        int usable_w = 0;
        int usable_h = 0;
        if (query_display_usable_bounds(usable_x, usable_y, usable_w, usable_h)) {
            const geometry::WindowFit fit =
                geometry::fit_window_to_display(320 * n, 240 * n, inset, usable_w, usable_h);
            if (fit.w != win_w || fit.h != win_h) {
                std::cerr << "sdl3: scale " << n << " exceeds display usable " << usable_w << "x"
                          << usable_h << "; window " << fit.w << "x" << fit.h << " (scale "
                          << fit.scale << ")\n";
            }
            win_w = fit.w;
            win_h = fit.h;
        }
        SDL_SetWindowSize(window_, win_w, win_h);
    }
}

void Sdl3App::set_filter(const SDL_ScaleMode mode) {
    // Menu Video > Filter Linear/Nearest: swap the presenter's texture scale
    // mode live (re-applied to the existing texture). Presentation-only.
    if (video_presenter_) {
        video_presenter_->set_scale_mode(mode);
    }
}

void Sdl3App::toggle_mute() {
    // Menu Audio > Mute: store the pre-mute level and drop volume to 0, or
    // restore the prior level. Built on the existing master-volume seam so the
    // presenter stays in sync; presentation-only (never touches emulation).
    if (master_volume_ > 0) {
        pre_mute_volume_ = master_volume_;
        master_volume_ = 0;
    } else {
        master_volume_ = pre_mute_volume_ > 0 ? pre_mute_volume_ : 100;
    }
    if (audio_presenter_) {
        audio_presenter_->set_master_volume(master_volume_);
    }
    std::cerr << "sdl3: master volume " << master_volume_ << "%"
              << (master_volume_ == 0 ? " (muted)" : master_volume_ == 100 ? " (full)" : "")
              << " (menu Mute)\n";
}

int Sdl3App::window_scale() const {
    // Current window scale N = round(width / 320), clamped [1,8], for the menu
    // Scale radio's checkmark. Falls back to the configured default when the
    // window is unavailable.
    if (window_ == nullptr) {
        return std::clamp(config_.window_width / 320, 1, 8);
    }
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    if (w <= 0) {
        return std::clamp(config_.window_width / 320, 1, 8);
    }
    return std::clamp((w + 160) / 320, 1, 8);
}

std::string Sdl3App::current_settings_xml() {
    // DEC-0095: the FULL config = the loaded baseline with the presentation +
    // sticky knobs overwritten from LIVE runtime state, so a hand-authored knob
    // NOT in scope survives a rewrite. Sampled: the 9 presentation/sticky knobs
    // PLUS the effective BIOS location (dir + role filenames) -- the BIOS folder
    // is a "set once, remember it" preference (Machine > BIOS Folder... mutates
    // config_.bios_dir at runtime), so it MUST reflect the live value, not the
    // baseline (DEC-0095-AMENDMENT-A: without this, changing only the BIOS dir
    // produced XML identical to the init seed -> debounced -> nothing written).
    // Still intentionally NOT sampled: RAM / speed / Ren-Sha -- machine-affecting
    // knobs kept session-only, so they retain their baseline values.
    machine::EmulatorConfig c = config_.config_baseline;
    c.video_scale = window_scale();
    c.video_filter =
        (video_presenter_ && video_presenter_->scale_mode() == SDL_SCALEMODE_NEAREST) ? "nearest"
                                                                                      : "linear";
    c.master_volume = master_volume_;
    c.persistence_percent = persistence();
    c.persistence_mode = (persistence_mode() == PhosphorMode::Peak) ? "peak" : "avg";
    c.fullscreen = fullscreen_;
    c.border_enabled = video_presenter_ ? video_presenter_->border_enabled() : c.border_enabled;
    c.fast_disk = machine_->fast_disk();
    c.disk_writable = config_.disk_writable;
    // The effective BIOS asset location the machine is actually using (BIOS
    // Folder... updates config_.bios_dir; the role filenames travel with it).
    c.bios_dir = config_.bios_dir;
    c.bios_roms = config_.bios_roms;
    return emulator_config_to_xml(c);
}

void Sdl3App::maybe_save_settings() {
    // DEC-0095 determinism gate: no save path (default, and ALWAYS so under
    // --hidden-window / headless / an explicit --config) -> return immediately,
    // touching nothing. This is what keeps the deterministic suite byte-identical.
    if (!config_.settings_save_path.has_value()) {
        return;
    }
    const std::string xml = current_settings_xml();
    if (xml == last_saved_settings_xml_) {
        return;  // debounce: nothing changed since the last write / the init() seed
    }
    std::ofstream out(*config_.settings_save_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        // Non-fatal: a read-only install simply cannot persist. Warn ONCE-ish
        // (the debounce below is not updated, but an unwritable path stays
        // unwritable, so this repeats at most once per actual settings change).
        std::cerr << "sdl3: WARNING could not write settings to " << *config_.settings_save_path
                  << " (settings will not persist this session)\n";
        return;
    }
    out << xml;
    last_saved_settings_xml_ = xml;
}

void Sdl3App::push_recent(const std::string& path) {
    // DEC-0095 gate: recent persistence disabled (headless / --hidden-window) ->
    // do nothing, so the in-memory list and the sidecar file are never touched on
    // the deterministic path.
    if (!config_.recent_save_path.has_value()) {
        return;
    }
    recent_.push(path);  // dedup + move-to-front + cap live in RecentFiles
    std::ofstream out(*config_.recent_save_path, std::ios::binary | std::ios::trunc);
    if (out) {
        out << recent_.serialize();
    }
    // A failed recent write is silent -- it is non-critical UX, not user data.
}

void Sdl3App::open_recent(const int index) {
    // DEC-0095: resolve the index -> path and route through the EXISTING media-open
    // seams so a recent pick is indistinguishable from the matching File > Open.
    if (index < 0) {
        return;
    }
    const std::string path = recent_.at(static_cast<std::size_t>(index));
    if (path.empty()) {
        return;  // out-of-range / stale index
    }
    // Extension routing (DEC-0095-AMENDMENT-B): a cartridge ROM -> slot 1;
    // anything else -> disk. Both apply_* seams then re-validate the extension and
    // reject a mismatch, so a stale/odd recent entry can never open unsafely.
    if (is_cartridge_extension(path)) {
        apply_open_cartridge(1, path);
    } else {
        apply_open_disks({path});
    }
}

void Sdl3App::update_window_title_for_current_disk() {
    // M35-S5 (AC-S5-1/2): "sony-msx-hbf1xv — <disk_name>" or "(no disk)".
    std::string title = "sony-msx-hbf1xv";
    if (!config_.disk_paths.empty() && current_disk_index_ < config_.disk_paths.size()) {
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
    // M35-S5 (AC-S5-3): "Inserted disk: <name> (i/N)" or "Inserted disk: (no disk)".
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
    // SDL_GetTicks()/SDL_Delay() arithmetic truncated the exact 16.688154 ms
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

        // DEC-0072 diagnostic: fingerprint mode runs UNTHROTTLED (skip real-time
        // pacing) so an 8000-frame cross-check completes in seconds, not minutes.
        // The emulation is wall-clock-independent, so this changes nothing about
        // the deterministic per-frame state -- only how fast frames are produced.
        if (config_.fingerprint_path.has_value()) {
            continue;
        }
        paced_cycles += machine_->frame_cycles_per_frame();
        const Uint64 deadline_ns =
            base_ns + AudioPacer::scale_cycles(paced_cycles, 1'000'000'000ull, kSystemClockHz);
        const Uint64 now_ns = SDL_GetTicksNS();
        if (now_ns < deadline_ns) {
            SDL_DelayNS(deadline_ns - now_ns);
        } else if (now_ns - deadline_ns > kMaxBacklogNs) {
            base_ns += now_ns - deadline_ns;  // Forgive the backlog; never fast-forward through it.
        }
    }

    // M57 (DEC-0085): env-gated audio-emission diagnostic for OWNER-MACHINE
    // attribution of DEF-1 (the released-build audio silence that does NOT
    // reproduce on the dev machine, where the produce/push path + device drain are
    // demonstrably healthy). INERT unless SONY_MSX_AUDIO_PROBE is set in the
    // environment -> the default run is byte-identical. Prints the REAL-sample
    // push counter (0 => produce/push path not driven on this machine), the live
    // host-queue depth (grows unbounded => device not draining), and the device
    // resume state, so the owner can attribute silence to the exact layer.
    if (audio_presenter_ != nullptr && SDL_getenv("SONY_MSX_AUDIO_PROBE") != nullptr) {
        SDL_AudioStream* const s = audio_presenter_->stream();
        const int queued = s != nullptr ? SDL_GetAudioStreamQueued(s) : -1;
        const SDL_AudioDeviceID dev = s != nullptr ? SDL_GetAudioStreamDevice(s) : 0;
        std::cerr << "sdl3: [audio-probe] real_sample_bytes_pushed="
                  << audio_presenter_->real_sample_bytes_pushed() << " queued_bytes=" << queued
                  << " device=" << (dev != 0 ? (SDL_AudioDevicePaused(dev) ? "PAUSED" : "playing") : "none")
                  << " volume=" << master_volume_ << "%\n";
    }

    // M27-S4 (docs/m27-planner-package.md §2.2): the three write_* calls
    // happen once, at the end of run_interactive()'s bounded loop (max_frames
    // reached) or on SDL_EVENT_QUIT -- whichever comes first -- added to the
    // existing loop-exit path, not a new one.
    flush_debug_session_outputs();

    return 0;
}

}  // namespace sony_msx::frontend
