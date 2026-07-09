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

#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "frontend/sdl3_audio_presenter.h"
#include "frontend/sdl3_input_mapper.h"
#include "frontend/sdl3_video_presenter.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

namespace sony_msx::frontend {

// Session configuration (M26-S2/S7, docs/m26-planner-package.md §2.8).
struct Sdl3AppConfig {
    std::string bios_dir = "bios";
    std::optional<std::string> cart1_path;
    devices::cartridge::CartridgeMapperType cart1_type = devices::cartridge::CartridgeMapperType::Mirrored;
    std::optional<std::string> cart2_path;
    devices::cartridge::CartridgeMapperType cart2_type = devices::cartridge::CartridgeMapperType::Mirrored;
    // M30 (backlog G2): whether cartN_type was EXPLICITLY chosen. Defaults
    // to true so every pre-existing PROGRAMMATIC Sdl3AppConfig construction
    // keeps byte-for-byte current behavior (the config's type field stays
    // authoritative, mirroring the machine-API rule of planner §2.4.1 step
    // 5); sdl3_main.cpp sets it from the parsed `type_was_explicit`, so a
    // CLI run with no --cartN-type (or `auto`) triggers auto-identification
    // through the ONE shared resolver (machine/cartridge_identifier.h).
    bool cart1_type_explicit = true;
    bool cart2_type_explicit = true;
    // M30: --softwaredb override; std::nullopt selects the CWD-relative
    // default (machine::kDefaultSoftwareDbPath).
    std::optional<std::string> softwaredb_path;
    // Real disk-image loading (A-M26-6): reads the file's bytes and mounts
    // them via the existing, unmodified devices::fdc::DiskImage(bytes)
    // constructor -- zero machine-level change required.
    std::optional<std::string> disk_path;
    int window_width = 640;
    int window_height = 480;
    // SDL_WINDOW_HIDDEN -- test/CI convenience; never required for a real
    // interactive session.
    bool hidden_window = false;
    // Opt-in border-box composition (--border): when true the video
    // presenter composes the active area inside the live R#7-colored border
    // canvas (frontend/border_composer.h); default false presents the bare
    // active area edge-to-edge (human-decided presentation preference,
    // docs/konami-splash-regression-investigation.md).
    bool border_enabled = false;
    // A bounded, non-interactive run length for headless/manual-verification
    // use (§2.3). std::nullopt (the default) means run_interactive() only
    // stops on SDL_EVENT_QUIT.
    std::optional<std::uint32_t> max_frames;

    // M27-S4/S7 additive debug/scripted-input session fields (docs/m27-
    // planner-package.md §2.2/§2.4, items 1/3/4). All std::nullopt by
    // default -- a hard regression guard (§4 Acceptance Criterion 10): every
    // pre-existing M26 Sdl3App/Sdl3AppConfig test remains green unmodified
    // when these are left unset.
    //
    // dump_state_filename/trace_cpu_filename/event_log_filename mirror the
    // headless `--debug-session` mode's own flags of the same name
    // (write_state_dump()/write_cpu_trace()/write_event_log() -- already
    // existing, already-tested Hbf1xvMachine APIs, M10-S3). Written once, at
    // the end of run_interactive()'s bounded loop (max_frames reached) or on
    // SDL_EVENT_QUIT -- whichever comes first -- via
    // flush_debug_session_outputs() (also directly, publicly callable after
    // a bounded run_one_frame() loop, for ctest use, since run_interactive()
    // itself is NEVER exercised by ctest, A-M26-8).
    std::optional<std::string> dump_state_filename;
    std::optional<std::string> trace_cpu_filename;
    std::optional<std::string> event_log_filename;

    // input_script_path mirrors the headless `--debug-session` mode's own
    // `--input-script` flag: a machine::InputScriptPlayer is loaded at
    // init() time and driven once per step_cpu_instruction() call inside
    // run_one_frame()'s existing CPU sub-loop (item 3, §2.4).
    std::optional<std::string> input_script_path;
};

// The SDL3 real-time application (M26, backlog C9). Owns a real
// Hbf1xvMachine session end to end: window/renderer/audio-device creation,
// cold_boot() + asset loading, and the real-time frame loop.
//
// Architecture (docs/m26-planner-package.md §2.3, A-M26-5/A-M26-8): the
// CORE STEP (run_one_frame()) is a pure function of "poll input, step the
// CPU to the next frame boundary via step_cpu_instruction(), call
// on_vsync_boundary() (NEVER run_frame() -- the double-count hazard,
// R-M25-5), blit one video frame, pump one batch of audio samples" -- ZERO
// wall-clock delay/pacing logic, so ctest can call it directly, a bounded
// number of times, with fully deterministic, assertable results. The
// REAL-TIME wrapper (run_interactive()) adds SDL_Delay-based pacing +
// SDL_EVENT_QUIT handling on top and is NEVER exercised by ctest.
class Sdl3App {
public:
    // The real MSX NTSC system clock (X-pattern precedent: wd2793.h/
    // disk_drive.h/rp5c01.h/rensha_turbo.h all independently declare this
    // same constant).
    static constexpr std::uint64_t kSystemClockHz = 3579545;

    explicit Sdl3App(Sdl3AppConfig config);
    ~Sdl3App();

    Sdl3App(const Sdl3App&) = delete;
    Sdl3App& operator=(const Sdl3App&) = delete;

    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) + window/renderer/video-
    // presenter/audio-presenter creation, then cold_boot()s the owned
    // machine and loads the configured BIOS/cartridge(s)/disk. Returns false
    // (and records last_error()) on ANY failure -- never partially
    // initializes (a failed init() leaves shutdown() already applied).
    bool init();
    void shutdown();
    [[nodiscard]] bool initialized() const { return initialized_; }

    // The deterministic core step -- see class doc comment. Directly,
    // repeatedly callable from a ctest integration test.
    void run_one_frame();

    // The real-time outer loop: run_one_frame() + exact-nanosecond
    // absolute-deadline pacing toward the true 16.688154 ms/frame cadence
    // (kFrameCycles/kSystemClockHz via AudioPacer::scale_cycles -- the
    // audio-latency fix, docs/audio-latency-investigation.md; the previous
    // ms-truncated SDL_Delay pacing ran ~3-4% fast and accumulated unbounded
    // audio latency) + SDL_PollEvent-driven SDL_EVENT_QUIT handling. NEVER
    // called from ctest.
    // Honors config.max_frames when set (a bounded, non-interactive run for
    // headless/manual-verification use). Returns a process exit code.
    int run_interactive();

    // M27-S4 (docs/m27-planner-package.md §2.2, items 1/4): writes whichever
    // of dump_state_filename/trace_cpu_filename/event_log_filename are
    // configured, via the SAME already-existing Hbf1xvMachine APIs the
    // headless `--debug-session` mode's own end-of-run write-out uses. A
    // genuine no-op when all three are unset (the default) -- callable
    // directly after a bounded run_one_frame() loop (ctest use, since
    // run_interactive() itself is never exercised by ctest); ALSO called
    // automatically once at run_interactive()'s own loop-exit point.
    void flush_debug_session_outputs();

    [[nodiscard]] machine::Hbf1xvMachine& machine() { return machine_; }
    [[nodiscard]] const machine::Hbf1xvMachine& machine() const { return machine_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }
    [[nodiscard]] bool quit_requested() const { return quit_requested_; }
    [[nodiscard]] std::uint64_t frames_run() const { return frames_run_; }

    [[nodiscard]] SDL_Window* window() const { return window_; }
    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }
    [[nodiscard]] Sdl3VideoPresenter* video_presenter() const { return video_presenter_.get(); }
    [[nodiscard]] Sdl3AudioPresenter* audio_presenter() const { return audio_presenter_.get(); }
    [[nodiscard]] Sdl3InputMapper& input_mapper() { return input_mapper_; }

private:
    void poll_and_dispatch_events();
    bool load_configured_assets();

    Sdl3AppConfig config_;
    machine::Hbf1xvMachine machine_;
    std::string last_error_;
    bool sdl_initialized_ = false;
    bool initialized_ = false;
    bool quit_requested_ = false;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Sdl3VideoPresenter> video_presenter_;
    std::unique_ptr<Sdl3AudioPresenter> audio_presenter_;
    Sdl3InputMapper input_mapper_;

    // M27-S7 (item 3, §2.4): default-constructed = empty = a genuine no-op
    // (the cursor never advances because events_ is empty) -- zero
    // observable effect on any pre-existing M26 run_one_frame() caller when
    // config_.input_script_path is unset.
    machine::InputScriptPlayer input_script_player_;

    std::uint64_t frames_run_ = 0;
};

}  // namespace sony_msx::frontend
