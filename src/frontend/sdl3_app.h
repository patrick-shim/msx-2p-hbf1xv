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
    // M35-S2: Real disk-image loading (A-M26-6 + M35-S2): a repeatable
    // --disk list (see sdl3_cli.h). Empty means no disk. First disk loads
    // at boot (AC-S2-1). All disks are pre-loaded into memory for
    // deterministic swapping (AC-S2-2).
    std::vector<std::string> disk_paths;
    // M36-S-c: opt-in host-file disk-save persistence. Default false =
    // in-memory-only, byte-for-byte the pre-M36 behavior (never clobbers a
    // real .dsk). When true, each mounted disk gets its host `.dsk` path bound
    // for write-back; a dirty writable image is flushed on shutdown and before
    // a swap discards it. Game disks stay effectively read-only unless the
    // user opts in; the SAVE target is a writable data disk.
    bool disk_writable = false;
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

    // M36 Phase 3 (DEC-0051): comprehensive debug snapshot. F12 in-session
    // ALWAYS triggers a capture (read-only + harmless); --snapshot <dir>
    // overrides the output root to <dir>/snapshot/<id>/. std::nullopt default =
    // captures land under the machine's default "debug/snapshot/" root.
    // Additive: no capture ever happens unless F12 is pressed, so a run that
    // never presses F12 is byte-for-byte identical to before.
    std::optional<std::string> snapshot_dir;
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

    // M36-S-f (R-M35-1): the deterministic disk-swap seam behind the F11
    // hotkey, exposed publicly so an integration test can assert the mounted
    // disk index advances (0 -> 1 -> ... -> wrap) WITHOUT SDL event injection.
    // Rotates to the next cached disk (no-op for a <=1-disk list); when
    // disk-writable is enabled it flushes the outgoing dirty image first.
    void swap_to_next_disk() { on_disk_swap_hotkey(); }
    [[nodiscard]] std::size_t current_disk_index() const { return current_disk_index_; }

    // M36 Phase 3: the F12 snapshot-request seam, exposed publicly so an
    // integration test can drive a capture WITHOUT SDL event injection (mirrors
    // swap_to_next_disk()'s R-M35-1 precedent exactly). Sets the deferred-capture
    // flag; the actual write happens at the end of the next run_one_frame().
    void request_snapshot() { on_snapshot_hotkey(); }
    [[nodiscard]] bool snapshot_requested() const { return snapshot_requested_; }

    // DEC-0052: the F10 live stream-capture toggle seam, exposed publicly so an
    // integration test can arm/finalize a stream WITHOUT SDL event injection
    // (mirrors request_snapshot()/swap_to_next_disk()). Flips the machine-level
    // stream capture: ON stamps a deterministic stream id (the current
    // snapshot_id()); OFF finalizes (dumps the trace ring + a final snapshot).
    void request_stream_toggle() { on_stream_toggle_hotkey(); }
    [[nodiscard]] bool stream_capture_active() const { return machine_.stream_capture_active(); }

    // M36-S-c: explicit flush of the mounted disk's writes back to its host
    // `.dsk` (no-op unless disk-writable was set + a host path is bound).
    // Called on shutdown; also directly callable after a bounded run.
    bool flush_current_disk();

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
    // M35-S4/S5: hotkey handler for F11 disk-swap and title/logging helpers.
    void on_disk_swap_hotkey();
    // M36 Phase 3: F12 hotkey handler -- requests a comprehensive debug snapshot
    // serviced at the END of run_one_frame() (a clean frame boundary + a stable
    // deterministic id). Consumed as a HOST hotkey, NEVER routed to the MSX
    // keyboard matrix (mirrors on_disk_swap_hotkey's F11 discipline exactly).
    void on_snapshot_hotkey();
    // DEC-0052: F10 hotkey handler -- toggles live stream-capture ON/OFF at the
    // machine level. Consumed as a HOST hotkey, NEVER routed to the MSX keyboard
    // matrix (mirrors on_disk_swap_hotkey/on_snapshot_hotkey discipline; F10 is
    // otherwise unbound -- only F6-F9/F11/F12 are wired).
    void on_stream_toggle_hotkey();
    void update_window_title_for_current_disk();
    void log_disk_swap();

    Sdl3AppConfig config_;
    machine::Hbf1xvMachine machine_;
    std::string last_error_;
    bool sdl_initialized_ = false;
    bool initialized_ = false;
    bool quit_requested_ = false;
    // M36 Phase 3: set by F12 (on_snapshot_hotkey), serviced + cleared at the
    // end of run_one_frame() so the capture happens at a clean frame boundary.
    bool snapshot_requested_ = false;

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

    // M35-S2/S4: multi-disk hot-swap state. disk_images_ caches all
    // pre-loaded disk bytes in memory (deterministic, no runtime I/O).
    // current_disk_index_ tracks which disk is mounted. Empty list is
    // safe (no-op swaps); single-disk is a regression guard (F11 no-op).
    std::vector<std::vector<std::uint8_t>> disk_images_;
    std::size_t current_disk_index_ = 0;

    std::uint64_t frames_run_ = 0;
};

}  // namespace sony_msx::frontend
