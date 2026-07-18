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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "frontend/dialog_result.h"
#include "frontend/sdl3_audio_presenter.h"
#include "frontend/sdl3_input_mapper.h"
#include "frontend/sdl3_video_presenter.h"
#include "frontend/session_summary.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

namespace sony_msx::frontend {

// Session configuration (M26-S2/S7, docs/m26-planner-package.md §2.8).
struct Sdl3AppConfig {
    std::string bios_dir = "bios";
    // M50-S3 (DEC-0077): role-keyed BIOS ROM filenames resolved from config
    // (CLI/XML/built-in default). Default = the strict HB-F1XV spec set, so a
    // bare Sdl3AppConfig{} loads the same 7 files as before (byte-identical).
    // init() applies these via machine_.set_bios_filenames() before cold_boot.
    machine::EmulatorConfig::BiosRoms bios_roms{};
    std::optional<std::string> cart1_path;
    devices::cartridge::CartridgeMapperType cart1_type = devices::cartridge::CartridgeMapperType::Mirrored;
    std::optional<std::string> cart2_path;
    devices::cartridge::CartridgeMapperType cart2_type = devices::cartridge::CartridgeMapperType::Mirrored;
    // M30 (backlog G2): whether cartN_type was explicitly chosen. Defaults to
    // true so existing programmatic Sdl3AppConfig construction stays byte-
    // for-byte unchanged; sdl3_main.cpp sets it from the parsed CLI flag, so
    // --cartN-type omitted (or `auto`) triggers auto-identification through
    // the ONE shared resolver (machine/cartridge_identifier.h).
    bool cart1_type_explicit = true;
    bool cart2_type_explicit = true;
    // M30: --softwaredb override; std::nullopt selects the CWD-relative
    // default (machine::kDefaultSoftwareDbPath).
    std::optional<std::string> softwaredb_path;
    // M35-S2: repeatable --disk list (A-M26-6 + M35-S2, see sdl3_cli.h).
    // Empty = no disk. First disk loads at boot (AC-S2-1); all disks are
    // pre-loaded into memory for deterministic swapping (AC-S2-2).
    std::vector<std::string> disk_paths;
    // M36-S-c: opt-in host-file disk-save persistence. Default false =
    // in-memory-only (never touches a real .dsk). When true, each mounted
    // disk's host `.dsk` path is bound for write-back; a dirty image flushes
    // on shutdown and before a swap discards it -- so game disks stay
    // read-only unless the user opts in, with a writable data disk as the
    // save target.
    bool disk_writable = false;
    // Fast-disk (FDC turbo) QoL mode (--fast-disk / Alt+D). Default false = 100%
    // cycle-accurate FDC timing (byte-identical to before). When true, init()
    // calls machine_.set_fast_disk(true) after cold_boot() so disk loads finish
    // near-instantly; Alt+D toggles it live in poll_and_dispatch_events().
    bool fast_disk = false;
    // M37 Slice F: default window is now scale 3 = 320x3 x 240x3 = 960x720
    // (was 640x480/scale 2 through Slice E) so the out-of-box window is
    // comfortably sized without passing --scale. --scale N still overrides
    // via sdl3_main.cpp (320N x 240N, N in [1,8]); the logical presentation
    // stays 320x240 letterbox -- only the initial window size default changed.
    int window_width = 960;
    int window_height = 720;
    // SDL_WINDOW_HIDDEN -- test/CI convenience; never required for a real
    // interactive session.
    bool hidden_window = false;
    // Border-canvas composition (default OFF, M39-D human preference revert):
    // the DEFAULT present is the bare edge-to-edge active area -- the Sony-
    // original look, with the active display filling the window and no framing
    // border. When true (opt-in via --border) the video presenter places the
    // active area at its raster-true openMSX-matching position inside the live
    // R#7-colored 320x240 / 640x240 canvas (frontend/border_composer.h): active
    // top row 24 @192-line / 14 @212-line, matching openMSX's vertical framing.
    // R#18-neutral software is byte-identical either way. 7fac03d had defaulted
    // this ON; the human prefers the Sony edge-to-edge default, so it reverts to
    // false and --border becomes the active opt-in again (docs/m39-fix-plans.md).
    bool border_enabled = false;
    // A bounded, non-interactive run length for headless/manual-verification
    // use (§2.3). std::nullopt (the default) means run_interactive() only
    // stops on SDL_EVENT_QUIT.
    std::optional<std::uint32_t> max_frames;

    // M27-S4/S7 additive debug/scripted-input fields (docs/m27-planner-
    // package.md §2.2/§2.4, items 1/3/4). All std::nullopt by default -- a
    // regression guard (§4 Acceptance Criterion 10): every pre-existing M26
    // Sdl3App/Sdl3AppConfig test stays green when these are left unset.
    //
    // dump_state_filename/trace_cpu_filename/event_log_filename mirror the
    // headless `--debug-session` mode's own flags of the same name
    // (write_state_dump()/write_cpu_trace()/write_event_log(), M10-S3).
    // Written once via flush_debug_session_outputs() at run_interactive()'s
    // loop exit (max_frames reached or SDL_EVENT_QUIT, whichever first); also
    // directly callable after a bounded run_one_frame() loop for ctest, since
    // run_interactive() itself is never exercised by ctest (A-M26-8).
    std::optional<std::string> dump_state_filename;
    std::optional<std::string> trace_cpu_filename;
    std::optional<std::string> event_log_filename;

    // input_script_path mirrors the headless `--debug-session` mode's own
    // `--input-script` flag: a machine::InputScriptPlayer is loaded at
    // init() time and driven once per step_cpu_instruction() call inside
    // run_one_frame()'s existing CPU sub-loop (item 3, §2.4).
    std::optional<std::string> input_script_path;

    // Input RECORDER (DEC-0072 diagnostic tooling): when set (--record-input
    // <path>), init() opens an InputScriptRecorder that streams every MSX matrix
    // key edge (via the SDL3 scancode->matrix mapping) and every F11 disk swap
    // into an HBF1XV-INPUT-SCRIPT v1 file, so a live playthrough replays
    // deterministically via --input-script (+ --swap-disk-frame <N>). std::nullopt
    // (default) = no recording: is_open() stays false and every record call is a
    // no-op, so a non-recording session is byte-for-byte identical to before.
    std::optional<std::string> record_input_path;

    // M36 Phase 3 (DEC-0051): comprehensive debug snapshot. F12 in-session
    // ALWAYS triggers a capture (read-only + harmless); --snapshot <dir>
    // overrides the output root to <dir>/snapshot/<id>/. std::nullopt default =
    // captures land under the machine's default "debug/snapshot/" root.
    // Additive: no capture ever happens unless F12 is pressed, so a run that
    // never presses F12 is byte-for-byte identical to before.
    std::optional<std::string> snapshot_dir;

    // DEC-0052 stream-light (M36 Bug B long-session upstream hunt): when true,
    // F10 arms the LIGHTWEIGHT stream-capture mode (per-frame snapshots
    // suppressed -> coarse anchors + the per-event watchlog), for long armed
    // sessions the heavy per-frame I/O would bog down. Default false = the
    // heavy every-frame mode (prior F10 behavior). Set from `--stream-light`.
    bool stream_light = false;

    // M36 FM-PAC SRAM persistence (SDL3 side): a real FM-PAC always battery-
    // persists, so this is AUTOMATIC (no opt-in flag). When a loaded cartridge
    // resolves to an FM-PAC, load_configured_assets() binds a .sram host file
    // so the SRAM loads on insertion and flushes on shutdown -- mirroring the
    // headless --fmpac-sram path (src/main.cpp) and the --disk-writable flush
    // discipline above.
    //   * fmpac_sram_path == std::nullopt (default): auto-derive from the
    //     cart's ROM path (<cart>.rom -> <cart>.rom.sram), landing the save
    //     beside the cart, like a real FM-PAC battery.
    //   * fmpac_sram_path set (--fmpac-sram <path>): overrides the derived path.
    //   * fmpac_sram_disabled (--no-fmpac-sram): opts out entirely -- no path
    //     is bound, so the SRAM stays in-memory-only.
    // All three are no-ops when no inserted cartridge is an FM-PAC
    // (flush_fmpac_sram() returns false harmlessly): a non-FM-PAC run is
    // byte-for-byte unchanged.
    std::optional<std::string> fmpac_sram_path;
    bool fmpac_sram_disabled = false;

    // M37 Slice D (DEC-0056): launch-time initial Sony Speed Controller level
    // (Mb670836PauseController). std::nullopt (default) leaves it at level 0
    // (full speed). Applied in init() AFTER cold_boot() (which resets the
    // controller); F6/F7 runtime stepping is unchanged -- this only sets the
    // initial value.
    std::optional<int> speed_level;

    // M37 Slice E (DEC-0056): start the window fullscreen. Default false =
    // windowed (byte-identical to before). Alt+Enter toggles at runtime.
    bool fullscreen = false;

    // M37 Slice F: gates the F10 live stream-capture hotkey (--capture <on|off>).
    // Default false = F10 is INERT (no toggle, no log), so a mis-struck F10
    // during play is harmless and default gameplay is byte-identical. Only
    // when true does poll_and_dispatch_events() route F10 to
    // on_stream_toggle_hotkey(); --stream-light still selects light-vs-heavy
    // mode once triggered. F11/F12 and other hotkeys are unaffected.
    bool capture_enabled = false;

    // M37 Slice E (DEC-0056): texture scale mode fed to the video presenter
    // (--filter). Default SDL_SCALEMODE_LINEAR = the renderer's own default
    // (references/sdl3/include/SDL3/SDL_render.h:1260), the "smooth" look,
    // byte-identical to before; SDL_SCALEMODE_NEAREST = crisp pixels.
    SDL_ScaleMode texture_filter = SDL_SCALEMODE_LINEAR;

    // Phosphor-persistence inter-frame blend weight fed to the video presenter
    // (--persistence <0..100>; Alt+B cycles it live). DEFAULT 0 = OFF, so the
    // present path is byte-for-byte the current path (no blend buffer, no extra
    // allocation) and every existing test is unaffected. > 0 simulates a CRT
    // phosphor's decay so multiplexed sprites stop flickering on an LCD -- a
    // PRESENTATION-ONLY post-process (frontend/phosphor_blend.h) that never
    // affects emulation, determinism, or headless output.
    int persistence = 0;

    // Phosphor blend MODE fed to the video presenter (--persistence-mode; Alt+M
    // toggles it live). DEFAULT Average = the original weighted-mean blend
    // (byte-identical to before, and irrelevant while persistence == 0). Peak =
    // peak-hold-with-decay, which keeps multiplexed sprites full-brightness
    // instead of dimming them (frontend/phosphor_blend.h). PRESENTATION-ONLY:
    // never affects emulation, determinism, or headless output.
    PhosphorMode persistence_mode = PhosphorMode::Average;

    // M52 (DEC-0079): SDL3 master-volume percent [0,100] fed to the audio
    // presenter (--volume; Alt+D down / Alt+U up step it live). DEFAULT 100 =
    // unity (full) -- the struct default STAYS at unity 100 (anti-drift: the
    // flipped-default pattern only ever applies to disk-writable, planner §2.2/§2.4),
    // so a bare Sdl3AppConfig{} is byte-identical to pre-M52. A POST-MIX
    // presentation scalar only: never affects emulation, determinism, or headless
    // output (the headless build never constructs Sdl3AudioPresenter).
    int master_volume = 100;

    // M42 (DEC-0061): main-RAM (DRAM) size in BYTES passed to the Hbf1xvMachine
    // constructor. Default = the strict stock 64 KB spec (byte-identical to
    // before). --ram 128/256/512 (opt-in, NON-STOCK) set 128/256/512 KB here;
    // sdl3_main.cpp maps the parsed KB enum to this byte count (kb * 1024).
    std::size_t ram_bytes = 64u * 1024u;

    // M46 (DEC-0071): auto-load an FM-PAC cartridge into primary slot 2 from
    // fmpac_autoload_rom_path. Default FALSE = the STOCK anti-drift default
    // (planner §2.7): a bare Sdl3AppConfig{} never auto-loads, so every
    // direct-construction test stays stock. sdl3_main.cpp sets it TRUE as the
    // resolved convenience default. The load layer (load_configured_assets)
    // additionally skips when slot 2 is explicitly occupied, an FM-PAC is
    // already present, or the ROM is absent/invalid -- a graceful banner note,
    // NEVER a boot failure. DEC-0050 "NO S-RAM AVAILABLE" stays correct in every
    // skip path (no internal SRAM is ever fabricated).
    bool fmpac_autoload = false;
    // The FM-PAC auto-load ROM path (CWD-relative production default). Only
    // consulted when fmpac_autoload==true; overridable (e.g. by an integration
    // test pointing at a temp copy) so the auto-load never touches a real user
    // save under roms/. The derived SRAM path is <this>.sram unless
    // fmpac_sram_path/fmpac_sram_disabled override it.
    std::string fmpac_autoload_rom_path = "roms/fmpac.rom";

    // M50-S2 (DEC-0077): the primary slot the FM-PAC auto-load targets. Default 2
    // (the byte-identical M46 behavior; a bare Sdl3AppConfig{} keeps slot 2, so
    // every direct-construction test is unchanged). Externalized via
    // <defaults><fmpac slot="1|2">; sdl3_main.cpp maps the resolved config value
    // here. Only consulted when fmpac_autoload==true.
    int fmpac_autoload_slot = 2;

    // DEC-0072 replay-fidelity diagnostic (M47-followup): reproduce the headless
    // `--swap-disk-frame <N>` scripted disk hot-swap on the SDL3 path so a
    // RECORDED owner script (its "# SWAP_DISK frame=<N>" marker) can be replayed
    // on a hidden-window SDL3 build cycle-for-cycle -- the determinism cross-check
    // vs the headless driver. std::nullopt (default) = no scripted swap (F11 is
    // the only swap path, byte-for-byte unchanged).
    std::optional<std::uint32_t> swap_disk_frame;
    // DEC-0072 per-frame CPU-state fingerprint CSV path (same columns as the
    // headless --fingerprint). std::nullopt (default) = no dump.
    std::optional<std::string> fingerprint_path;
};

// M55 (DEC-0083): the ImGui in-window menu view/controller, owned by Sdl3App
// only in an interactive launch. Forward-declared so sdl3_app.h stays ImGui-free
// (the unique_ptr member works with the incomplete type because ~Sdl3App is
// defined out-of-line in sdl3_app.cpp, which includes the full sdl3_menu.h).
class Sdl3Menu;

// M56 (DEC-0084): the kind of the single owned async file dialog in flight.
// M60 (DEC-0089) adds OpenBiosFolder (SDL_ShowOpenFolderDialog through the SAME
// mailbox -- the folder picker shares the SDL_DialogFileCallback contract).
enum class DialogKind { None, OpenDisks, OpenCartSlot1, OpenCartSlot2, SaveBlankDisk, OpenBiosFolder };

// M56 (DEC-0084, planner §3.1): the mutex-mailbox that carries an async SDL file
// dialog's result from the (possibly off-thread, SDL_dialog.h:113,125-126)
// callback to the main-loop drain. The callback deep-copies every UTF-8 path
// INSIDE the mutex (SDL frees the filelist on callback return, SDL_dialog.h:90-91)
// and mutates NO machine state; all machine mutation happens on the main thread
// in drain_dialog_mailbox(). Only ever allocated interactively (menu_ != null),
// so the whole path is inert under --hidden-window / ctest.
struct DialogMailbox {
    SDL_Mutex* mutex = nullptr;  // created interactively (next to menu_); destroyed in shutdown()
    bool in_flight = false;      // a dialog is currently open (double-open guard)
    bool pending = false;        // a result is waiting to be drained on the main loop
    DialogKind kind = DialogKind::None;
    bool cancelled = false;      // filelist[0] == nullptr: user cancelled / chose nothing
    bool error = false;          // filelist == nullptr: SDL error
    std::string error_message;   // SDL_GetError() copied inside the callback
    std::vector<std::string> paths;  // DEEP-COPIED UTF-8 paths (OpenDisks: many; others: 1)
};

// The SDL3 real-time application (M26, backlog C9). Owns a real
// Hbf1xvMachine session end to end: window/renderer/audio-device creation,
// cold_boot() + asset loading, and the real-time frame loop.
//
// Architecture (docs/m26-planner-package.md §2.3, A-M26-5/A-M26-8): the CORE
// STEP (run_one_frame()) is a pure function -- poll input, step the CPU to
// the next frame boundary via step_cpu_instruction(), call
// on_vsync_boundary() (NEVER run_frame(), the double-count hazard, R-M25-5),
// blit one video frame, pump one batch of audio samples -- with ZERO wall-
// clock delay/pacing, so ctest can call it directly, a bounded number of
// times, with fully deterministic, assertable results. The REAL-TIME wrapper
// (run_interactive()) adds SDL_DelayNS-based nanosecond pacing + SDL_EVENT_QUIT
// handling on top and is NEVER exercised by ctest.
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
    // presenter/audio-presenter creation, then cold_boot()s the machine and
    // loads the configured BIOS/cartridge(s)/disk. Returns false (records
    // last_error()) on any failure -- never partially initializes (a failed
    // init() leaves shutdown() already applied).
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
    // configured, via the same existing Hbf1xvMachine APIs the headless
    // `--debug-session` mode uses for its end-of-run write-out. A no-op when
    // all three are unset (default) -- callable directly after a bounded
    // run_one_frame() loop (ctest, since run_interactive() itself is never
    // exercised by ctest); also called automatically at run_interactive()'s
    // loop-exit point.
    void flush_debug_session_outputs();

    // M36-S-f (R-M35-1): the deterministic disk-swap seam behind the F11
    // hotkey, exposed publicly so an integration test can assert the mounted
    // disk index advances (0 -> 1 -> ... -> wrap) without SDL event injection.
    // Rotates to the next cached disk (no-op for a <=1-disk list); flushes the
    // outgoing dirty image first when disk-writable is enabled.
    void swap_to_next_disk() { on_disk_swap_hotkey(); }
    [[nodiscard]] std::size_t current_disk_index() const { return current_disk_index_; }

    // M56 (DEC-0084): the F4 Reset (and F2/F3 cartridge-implied-reset) seam,
    // exposed publicly so the reset-determinism integration test can drive it
    // under hidden_window=true without a menu (mirrors swap_to_next_disk()'s
    // R-M35-1 precedent). reset_machine() is a FRONTEND orchestrator over the
    // UNCHANGED machine cold_boot() -- see reset_machine() in sdl3_app.cpp.
    void request_reset() { reset_machine(); }

    // M57 (DEC-0085-AMENDMENT-A): the Machine>RAM power-cycle seam -- rebuild the
    // machine at `ram_bytes` and boot fresh, with mounted disks re-attached and
    // inserted cartridges re-inserted (media survive; RAM contents do not).
    // Exposed publicly (mirrors request_reset()'s R-M35-1 precedent) so the
    // power-cycle oracle can drive it under hidden_window=true without a menu. A
    // no-op-safe transactional operation: if any occupied cartridge file cannot be
    // re-read, it ABORTS with the current machine fully intact (see power_cycle()).
    void request_power_cycle(std::size_t ram_bytes) { power_cycle(ram_bytes); }

    // M36 Phase 3: the F12 snapshot-request seam, exposed publicly so an
    // integration test can drive a capture without SDL event injection
    // (mirrors swap_to_next_disk()'s R-M35-1 precedent). Sets the deferred-
    // capture flag; the actual write happens at the end of the next
    // run_one_frame().
    void request_snapshot() { on_snapshot_hotkey(); }
    [[nodiscard]] bool snapshot_requested() const { return snapshot_requested_; }

    // DEC-0052: the F10 live stream-capture toggle seam, exposed publicly so an
    // integration test can arm/finalize a stream without SDL event injection
    // (mirrors request_snapshot()/swap_to_next_disk()). Flips the machine-level
    // stream capture: ON stamps a deterministic stream id (the current
    // snapshot_id()); OFF finalizes (dumps the trace ring + a final snapshot).
    void request_stream_toggle() { on_stream_toggle_hotkey(); }
    [[nodiscard]] bool stream_capture_active() const { return machine_->stream_capture_active(); }

    // Phosphor-persistence live-control seams, exposed publicly so an
    // integration test can drive them without SDL event injection (mirrors
    // request_snapshot()/swap_to_next_disk()). Presentation-only: they adjust the
    // video presenter's inter-frame blend, never perturb emulation.
    //   * step_persistence_up/down: +/-10% (wrapping 0..100), the FINE control
    //     (Alt+B / Shift+Alt+B). cycle_persistence() is retained as an alias for
    //     the up-step (backward-compatible seam name).
    //   * toggle_persistence_mode: Average <-> Peak (Alt+M).
    // persistence()/persistence_mode() read the presenter's current state (0 /
    // Average when no presenter / not initialized).
    void step_persistence_up() { on_persistence_step_hotkey(+1); }
    void step_persistence_down() { on_persistence_step_hotkey(-1); }
    void cycle_persistence() { on_persistence_step_hotkey(+1); }
    void toggle_persistence_mode() { on_persistence_mode_hotkey(); }
    [[nodiscard]] int persistence() const {
        return video_presenter_ ? video_presenter_->persistence() : 0;
    }
    [[nodiscard]] PhosphorMode persistence_mode() const {
        return video_presenter_ ? video_presenter_->persistence_mode() : PhosphorMode::Average;
    }

    // M36-S-c: explicit flush of the mounted disk's writes back to its host
    // `.dsk` (no-op unless disk-writable was set + a host path is bound).
    // Called on shutdown; also directly callable after a bounded run.
    bool flush_current_disk();

    // --- M55 (DEC-0083): narrow public seams for the ImGui menu view/controller.
    // All are PRESENTATION-ONLY forwarders (never touch emulation state) or thin
    // read-only accessors the menu builds its per-item state from. The menu is
    // created only in an interactive launch (init() gates on !hidden_window), so
    // these are inert to the deterministic ctest path.
    //
    // menu_active() lets a test assert no ImGui context exists under
    // --hidden-window (menu_ stays null).
    [[nodiscard]] bool menu_active() const;

    // Request an orderly quit (menu File > Exit) -- identical effect to the
    // SDL_EVENT_QUIT path (sets quit_requested_; run_interactive() exits at the
    // next frame boundary and flushes disk/SRAM saves via shutdown()).
    void request_quit() { quit_requested_ = true; }

    // Extract of the inline Alt+Enter handler (both the hotkey and the menu call
    // this): flip fullscreen_ and apply via SDL_SetWindowFullscreen.
    void toggle_fullscreen();
    // Menu Video > Scale N: resize the window to 320N x 240N (logical
    // presentation letterboxes automatically; N clamped 1..8).
    void set_scale(int scale);
    // Menu Video > Filter: swap the presenter's texture scale mode live.
    void set_filter(SDL_ScaleMode mode);
    // Menu Audio > Volume Up/Down (dir +1/-1): the same {0..100} grid step the
    // Alt+U/Alt+D hotkeys use.
    void step_volume(int dir) { on_volume_step_hotkey(dir); }
    // Menu Audio > Mute: store the pre-mute level and set volume 0, or restore
    // it. Presentation-only (built on the existing volume seam).
    void toggle_mute();
    // Menu Disk > Disk Writable: the same runtime toggle as the Alt+S hotkey.
    void toggle_disk_writable() { on_disk_writable_toggle_hotkey(); }

    // --- M56 (DEC-0084): the F1-F5 runtime media seams the menu dispatch calls.
    // The dialog launchers run on the MAIN thread only (ImGui build/dispatch runs
    // inside run_one_frame(), satisfying SDL_dialog.h:152,201); their results are
    // applied at the next frame boundary via drain_dialog_mailbox(). The apply_* /
    // eject_* seams are ALSO public so an integration test can drive them directly
    // under hidden_window=true (the swap_to_next_disk() precedent) without opening
    // a real OS dialog. All are inert under --hidden-window (menu_ == null, so no
    // dialog is ever launched and the mailbox never becomes pending).
    void open_disk_dialog();            // File > Open Disk(s)... (multi-select)
    void open_cartridge_dialog(int slot);  // File > Open Cartridge > Slot 1/2...
    void new_blank_disk_dialog();       // Disk > New Blank Disk...
    // F1 apply: REPLACE the F11-cycle disk list with `new_paths` (index -> 0, first
    // mounts). Flushes the outgoing dirty disk to host first when writable; a bad
    // selection is transactional (the current mount is never destroyed).
    void apply_open_disks(const std::vector<std::string>& new_paths);
    // F2 apply: resolve the mapper via the existing resolver, bind FM-PAC SRAM
    // before load, load into the slot, then reset_machine() (insert implies reset).
    void apply_open_cartridge(int slot, const std::string& rom_path);
    // F5 apply: write a fresh blank 720 KB MSX-DOS FAT12 image to `path` (no mount).
    void apply_new_blank_disk(const std::string& path);
    // F3 Eject: disk = flush-if-writable then detach + empty the drive (no reset);
    // cartridge = flush FM-PAC SRAM if present -> unload_cartridge -> reset_machine().
    void eject_disk();
    void eject_cartridge(int slot);

    // --- M60 (DEC-0089): Machine > BIOS Folder... runtime BIOS-directory selector.
    // open_bios_folder_dialog() launches SDL_ShowOpenFolderDialog (allow_many=false)
    // through the EXISTING mailbox (same double-open guard / deep-copy-in-callback /
    // main-loop drain); inert under --hidden-window (mailbox mutex never allocated).
    void open_bios_folder_dialog();
    // The drained apply: TRANSACTIONALLY validate that `path` contains ALL 7
    // configured BIOS ROM files (config_.bios_roms, each openable + readable)
    // BEFORE mutating anything -- any missing/unreadable file aborts with a stderr
    // note and config_.bios_dir + the live machine fully intact. On success:
    // config_.bios_dir = path, then power_cycle(config_.ram_bytes) -- which
    // re-applies set_asset_root(config_.bios_dir) + set_bios_filenames +
    // cold_boot + the audio-cursor/reset_pacing block (LIFECYCLE-AUDIO
    // INVARIANT), same RAM, mounted disks + inserted carts surviving. Public
    // (the apply_open_disks precedent) so the hidden-window integration test
    // can drive it directly without opening a real OS dialog.
    void apply_bios_folder(const std::string& path);

    // Read-only accessors the menu builds its checkmarks / radio state from.
    [[nodiscard]] std::size_t disk_count() const { return disk_images_.size(); }
    [[nodiscard]] bool fullscreen() const { return fullscreen_; }
    [[nodiscard]] int master_volume() const { return master_volume_; }
    [[nodiscard]] bool disk_writable() const { return config_.disk_writable; }
    // M60 (DEC-0089): the CURRENT BIOS directory -- feeds the menu snapshot
    // (MenuState::bios_dir) and the BIOS-folder integration oracle.
    [[nodiscard]] const std::string& bios_dir() const { return config_.bios_dir; }
    // Current window scale N (window width / 320), clamped to [1,8] for the radio.
    [[nodiscard]] int window_scale() const;

    [[nodiscard]] machine::Hbf1xvMachine& machine() { return *machine_; }
    [[nodiscard]] const machine::Hbf1xvMachine& machine() const { return *machine_; }
    // M46 (DEC-0071): the outcome of the FM-PAC slot-2 auto-load attempt during
    // load_configured_assets(), for the startup banner + integration tests.
    [[nodiscard]] FmPacAutoloadOutcome fmpac_autoload_outcome() const { return fmpac_autoload_outcome_; }
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
    // M56 (DEC-0084, planner §4.2): the F4 Reset orchestrator. A FRONTEND
    // sequence built entirely from existing public machine seams (ZERO
    // src/machine edit): snapshot the live disk -> cold_boot() (carts persist,
    // disk re-synthesized) -> re-attach the disk -> re-run the init()
    // post-cold_boot device-enable block VERBATIM -> reset the frontend audio
    // cursors. Mounted disks and inserted carts survive the reset.
    void reset_machine();
    // M57 (DEC-0085-AMENDMENT-A, addendum §2.4/§3): the Machine>RAM power-cycle
    // orchestrator. Transactional: PRE-READ + resolve every occupied cartridge
    // slot from its tracked path FIRST -- any missing/unreadable/unresolvable file
    // ABORTS with the OLD machine fully intact (+ stderr note), the apply_open_disks
    // discipline. On success: flush FM-PAC SRAM -> snapshot the live disk ->
    // machine_.emplace(ram_bytes) (fresh instance at the new RAM size) -> re-run the
    // init() post-construction sequence (BIOS, cold_boot, audio block +
    // reset_pacing, media re-attach + cart re-insert with SRAM re-bind) ->
    // config_.ram_bytes = ram_bytes. ZERO src/machine edits (the machine is rebuilt
    // through its EXISTING M42 size ctor). A same-size cycle is a full fresh boot,
    // media surviving (indistinguishable from Reset -- the caller guards on size).
    void power_cycle(std::size_t ram_bytes);
    // M56 (DEC-0084, planner §3.1): the async file-dialog callback (static;
    // SDLCALL; may run off the main thread). Deep-copies the result INTO dialog_
    // under its mutex and sets pending -- NO machine mutation here.
    static void SDLCALL dialog_callback(void* userdata, const char* const* filelist, int filter);
    // Main-loop drain (once per frame, gated on menu_): pops a pending dialog
    // result under the mutex and applies it on the main thread (apply_*). Inert
    // under --hidden-window (never called: menu_ == null).
    void drain_dialog_mailbox();
    // M61 (DEC-0090): the usable bounds of the display the live window is on
    // (SDL_GetDisplayForWindow -> SDL_GetPrimaryDisplay fallback ->
    // SDL_GetDisplayUsableBounds). Returns false -- and NEVER queries SDL -- under
    // --hidden-window or a null window, so the deterministic ctest path performs
    // no display query at all (structural inertness). Both the init-time window
    // fit and the runtime Video > Scale clamp feed the SAME pure
    // geometry::fit_window_to_display (window_fit.h) from this.
    bool query_display_usable_bounds(int& out_w, int& out_h);
    // M35-S4/S5: hotkey handler for F11 disk-swap and title/logging helpers.
    void on_disk_swap_hotkey();
    // M36 Phase 3: F12 hotkey handler -- requests a comprehensive debug snapshot
    // serviced at the END of run_one_frame() (a clean frame boundary + a stable
    // deterministic id). Consumed as a HOST hotkey, never routed to the MSX
    // keyboard matrix (mirrors on_disk_swap_hotkey's F11 discipline).
    void on_snapshot_hotkey();
    // DEC-0052: F10 hotkey handler -- toggles live stream-capture ON/OFF at the
    // machine level. Consumed as a HOST hotkey, never routed to the MSX keyboard
    // matrix (mirrors on_disk_swap_hotkey/on_snapshot_hotkey discipline).
    void on_stream_toggle_hotkey();
    // Alt+B / Shift+Alt+B hotkey handler -- steps the video presenter's
    // phosphor-persistence blend weight by +/-10% (dir = +1 / -1), wrapping
    // 0..100, and prints the new value to stderr. Consumed as a HOST hotkey,
    // never routed to the MSX keyboard matrix (mirrors the Alt+D / Alt+Enter
    // discipline). Presentation-only.
    void on_persistence_step_hotkey(int dir);
    // Alt+M hotkey handler -- toggles the phosphor blend mode (Average <-> Peak)
    // and prints it to stderr. Same HOST-hotkey discipline. Presentation-only.
    void on_persistence_mode_hotkey();
    // M52 (DEC-0079): Alt+D (dir=-1) / Alt+U (dir=+1) step the audio presenter's
    // master volume by kVolumeStep on a {0,10,...,100} grid, CLAMPING (never
    // wrapping) at both ends, and print the new percent to stderr. Consumed as a
    // HOST hotkey (only the letter keydown swallowed; standalone Alt reaches the
    // matrix). Key-repeat is HONORED for these two (a pure presentation knob that
    // never touches emulation/determinism -- planner §2.3). Presentation-only.
    void on_volume_step_hotkey(int dir);
    // M52 (DEC-0079): Alt+S toggles disk-writable at runtime, keeping
    // config_.disk_writable AND the current image's host-path binding in sync so
    // both flush gates agree. ON binds the current disk's host path (a late-ON
    // flushes the whole in-memory image at the next swap/exit); OFF unbinds
    // (set_host_path({}) -- future flushes no-op, in-memory writes kept but
    // discarded at exit). Fresh-keydown-only (it is a toggle). HOST-hotkey
    // discipline. ZERO src/devices/fdc edits (reuses DiskImage::set_host_path).
    void on_disk_writable_toggle_hotkey();
    void update_window_title_for_current_disk();
    void log_disk_swap();

    Sdl3AppConfig config_;
    // M57 (DEC-0085-AMENDMENT-A): the machine is a std::optional so a live RAM
    // change (Machine>RAM radio) can rebuild it at the new size via emplace() --
    // the machine is non-movable (reference-wired members: MemoryMapperRam binds
    // dram_ by reference, etc.), so `machine_ = Hbf1xvMachine(newSize)` is
    // impossible and reconstruction through the size ctor is the only correct
    // resize. Engaged at construction (std::in_place) and re-emplaced on
    // power_cycle; NEVER disengaged, so every *machine_ / machine_-> access is
    // valid (the "always engaged" invariant).
    std::optional<machine::Hbf1xvMachine> machine_;
    std::string last_error_;
    // M46 (DEC-0071): recorded by load_configured_assets() (NotAttempted until
    // then). Read by the startup banner + FM-PAC auto-load integration test.
    FmPacAutoloadOutcome fmpac_autoload_outcome_ = FmPacAutoloadOutcome::NotAttempted;
    bool sdl_initialized_ = false;
    bool initialized_ = false;
    bool quit_requested_ = false;
    // M36 Phase 3: set by F12 (on_snapshot_hotkey), serviced + cleared at the
    // end of run_one_frame() so the capture happens at a clean frame boundary.
    bool snapshot_requested_ = false;
    // M37 Slice E (DEC-0056): tracked fullscreen state for the Alt+Enter
    // runtime toggle. Seeded from config_.fullscreen in init().
    bool fullscreen_ = false;
    // M52 (DEC-0079): cached master-volume percent for the live Alt+D/Alt+U
    // steppers (the audio presenter holds the authoritative copy; this mirrors it
    // so the hotkeys can step without querying SDL). Seeded from
    // config_.master_volume in init(). Default 100 (unity).
    int master_volume_ = 100;
    // M52 (DEC-0079): the master-volume hotkey grid step (an 11-point
    // {0,10,...,100} grid, mirroring the Alt+B persistence step's feel; reaches
    // exact 0/100 -- planner §2.3).
    static constexpr int kVolumeStep = 10;

    // M52 (DEC-0079): the master volume before Mute engaged, so the menu Mute
    // toggle (and a future hotkey) can restore the prior level. Default 100.
    int pre_mute_volume_ = 100;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Sdl3VideoPresenter> video_presenter_;
    std::unique_ptr<Sdl3AudioPresenter> audio_presenter_;
    Sdl3InputMapper input_mapper_;

    // M55 (DEC-0083): the ImGui in-window menu. Created in init() ONLY when the
    // launch is interactive (!config_.hidden_window); null under ctest /
    // --hidden-window, so no ImGui context ever exists in the deterministic path
    // and the whole SDL3 suite stays byte-identical. Destroyed in shutdown()
    // BEFORE SDL_DestroyRenderer (R7).
    std::unique_ptr<Sdl3Menu> menu_;

    // M56 (DEC-0084): the single owned async-dialog mailbox. Its mutex is created
    // ONLY in the interactive (!hidden_window) init() block next to menu_ and
    // destroyed last in shutdown(), so --hidden-window / ctest never allocates it
    // and the whole dialog path stays inert on the deterministic path.
    DialogMailbox dialog_;

    // M27-S7 (item 3, §2.4): default-constructed = empty = a genuine no-op
    // (the cursor never advances because events_ is empty) -- zero effect on
    // any pre-existing M26 run_one_frame() caller when
    // config_.input_script_path is unset.
    machine::InputScriptPlayer input_script_player_;

    // Input RECORDER (DEC-0072): the write-side counterpart to
    // input_script_player_. Default-constructed = closed = every record_*()
    // call is a no-op, so a session without --record-input is byte-for-byte
    // unchanged. Opened in init() when config_.record_input_path is set;
    // record calls happen in poll_and_dispatch_events(); closed (writing the
    // trailing "[END]") in shutdown().
    machine::InputScriptRecorder input_recorder_;

    // M35-S2/S4: multi-disk hot-swap state. disk_images_ caches all
    // pre-loaded disk bytes in memory (deterministic, no runtime I/O).
    // current_disk_index_ tracks which disk is mounted. Empty list is
    // safe (no-op swaps); single-disk is a regression guard (F11 no-op).
    std::vector<std::vector<std::uint8_t>> disk_images_;
    std::size_t current_disk_index_ = 0;

    // M57 (DEC-0085-AMENDMENT-A, addendum §3.2): the CURRENT cartridge source path
    // per slot (index 0 = slot 1, index 1 = slot 2), so a power-cycle can RE-READ
    // the occupied bays from disk transactionally. Set by load_configured_assets
    // (startup --cartN + FM-PAC autoload) and apply_open_cartridge (menu insert);
    // cleared by eject_cartridge. std::nullopt == empty bay. Cart images are not
    // otherwise retained in the frontend (apply_open_cartridge loads transiently),
    // so this path is the sole re-insert source across a power-cycle.
    std::optional<std::string> cart_path_[2];

    std::uint64_t frames_run_ = 0;

    // M39-A Fix B (digitized-voice fix): interleaved sync-before-change audio
    // production cursor. Samples are produced DURING the CPU-step loop over
    // absolute machine-cycle boundaries so the PSG software-PCM voice survives
    // (boundary[k] = k * kSystemClockHz / kSampleRateHz, exact accounting that
    // matches the AudioPacer's sample count). Persist across frames.
    std::uint64_t audio_sample_index_ = 1;
    std::uint64_t audio_prev_boundary_ = 0;
    std::uint64_t audio_next_boundary_ = 0;  // set in init() from the sample rate
    std::vector<std::int16_t> audio_frame_pcm_;  // reused per-frame scratch buffer

    // DEC-0072 diagnostic accumulator for the per-frame CPU fingerprint CSV
    // (config_.fingerprint_path). Flushed in flush_debug_session_outputs().
    std::string fingerprint_csv_;
};

}  // namespace sony_msx::frontend
