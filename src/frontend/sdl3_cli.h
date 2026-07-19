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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "frontend/phosphor_blend.h"  // PhosphorMode (Average / Peak) -- SDL-free, header-only
#include "machine/cartridge_cli.h"

namespace sony_msx::frontend {

// --filter <nearest|linear> texture scale mode.
// DEFAULT Linear -- matches the SDL3 renderer's own default scale mode
// (src/external/sdl3/include/SDL3/SDL_render.h:1260), so an absent --filter
// leaves the presentation byte-identical. Nearest = crisp pixels. (DEC-0056)
enum class TextureFilter { Nearest, Linear };

// Pure argv parser for the SDL3 frontend's own flags:
// `--bios-dir`, `--disk` (repeatable),
// `--max-frames` (bounded headless/test run length), plus
// the REUSED cartridge flags (`--cart1`/`--cart1-type`/`--cart2`/
// `--cart2-type`), delegated verbatim to `sony_msx::machine::parse_cartridge_cli`
// -- never reimplemented, mirrors `src/main.cpp`'s own reuse. No file I/O,
// no Sdl3App/SDL3 dependency -- headlessly unit-testable (mirrors
// cartridge_cli.h's own parse/spec-vs-load separation).
//
// Multi-disk hot-swap: `--disk`
// is REPEATABLE and accumulates in order. A single flag yields a size-1
// list (backward-compatible); multiple flags accumulate;
// none yields an empty list.
struct ParsedSdl3Cli {
    std::optional<std::string> bios_dir;
    std::vector<std::string> disk_paths;  // accumulates the repeatable --disk, in order
    std::optional<std::uint32_t> max_frames;
    bool hidden_window = false;  // --hidden-window (test/CI convenience; never SDL3-dependent to parse)
    // Border-canvas composition around the active area (border_composer.h).
    // Default OFF (an owner preference revert): the DEFAULT present is the
    // bare edge-to-edge active area -- the Sony-original look (the active display
    // fills the window with no framing border). `--border` OPTS IN to the
    // openMSX-matching framed canvas, which places the active area at its
    // raster-true position (active top row 24 @192-line / 14 @212-line, the
    // border_geometry() y0 anchors measured vs openMSX 19.1 Sony_HB-F1XV) inside
    // the live R#7-colored 320x240 / 640x240 canvas. `--no-border` is the
    // explicit off (matches the default). Last-wins on a linear scan. Note:
    // 7fac03d had made the framed canvas the default and turned --border into a
    // no-op alias; per the human's explicit preference the default reverts to
    // the Sony edge-to-edge look and --border is restored as the active opt-in
    // (a deliberate preference revert, NOT a weakening).
    bool border_enabled = false;
    // --disk-writable opts into host-file disk-save persistence.
    // The RESOLVED SDL3 default is ON (via the config-resolution
    // seam), so this parser field carries only raw CLI intent -- it stays
    // false here (anti-drift: the flip lives in config resolution, never in this
    // struct/ctor default). `--disk-writable` sets it true, `--no-disk-writable`
    // sets it false; both set disk_writable_specified so the resolver honors the
    // explicit CLI choice over the (ON) XML/built-in default. Last-wins on the
    // linear scan. (DEC-0079)
    bool disk_writable = false;
    // Fast-disk (FDC turbo) QoL mode. TRI-STATE so the
    // DEFAULT lives in resolve_session_defaults() (below), NOT this field: the
    // convenience default is ON, `--no-fast-disk`/`--stock` force it OFF, and an
    // explicit `--fast-disk` forces it ON. std::nullopt = unspecified. Also
    // toggleable live via Alt+D in the SDL3 window. (DEC-0071)
    std::optional<bool> fast_disk_opt;
    // Opt out of the default FM-PAC slot-2 auto-load. Only the
    // negative needs a field (auto-load is the resolved default); the load layer
    // also skips when slot 2 is explicitly occupied / an FM-PAC is already
    // present. (DEC-0071)
    bool no_fmpac = false;
    // One-shot authentic bare machine (RAM 64 + fast-disk OFF +
    // no FM-PAC). Explicit per-field flags override the corresponding --stock
    // field, order-independently (resolve_session_defaults()). (DEC-0071)
    bool stock = false;
    machine::ParsedCartridgeCli cartridges;
    // Additive debug/scripted-input flags: `--dump-state`/`--trace-cpu`/
    // `--event-log`/`--input-script` mirror the headless `--debug-session`
    // mode's own flags of the same name. All four default to std::nullopt --
    // pre-existing parse_sdl3_cli() cases stay green.
    std::optional<std::string> dump_state_filename;
    std::optional<std::string> trace_cpu_filename;
    std::optional<std::string> event_log_filename;
    std::optional<std::string> input_script_path;
    // Input RECORDER (diagnostic tooling, DEC-0072): --record-input <path> streams
    // the live keyboard session + F11 disk hot-swaps into an HBF1XV-INPUT-SCRIPT
    // v1 file (machine::InputScriptRecorder) that replays deterministically via
    // --input-script (+ --swap-disk-frame <N> for the recorded swaps). std::nullopt
    // (default) = no recording, byte-for-byte the prior behavior.
    std::optional<std::string> record_input_path;
    // --snapshot <dir> overrides the debug-snapshot
    // output root (<dir>/snapshot/<id>/). F12 in-session capture is ALWAYS
    // active (read-only, harmless); this flag only controls where captures
    // land. std::nullopt by default -- pre-existing parse_sdl3_cli() cases
    // stay green (mirrors the dump_state_filename field). (DEC-0051)
    std::optional<std::string> snapshot_dir;
    // --stream-light arms F10's LIGHTWEIGHT capture mode (per-frame
    // snapshot bundles suppressed -> coarse anchors + the per-event watchlog)
    // for a long armed session. Default OFF = the heavy every-frame F10 mode
    // (byte-for-byte the prior behavior). (DEC-0052)
    bool stream_light = false;
    // FM-PAC SRAM persistence (SDL3 side): a real FM-PAC always
    // battery-persists, so the SDL3 build auto-derives the SRAM host file
    // from the inserted cart's ROM path (<cart>.rom -> <cart>.rom.sram),
    // no opt-in needed. --fmpac-sram <path> OVERRIDES that derived default;
    // std::nullopt (default) = derive from the cart path. --no-fmpac-sram
    // opts OUT entirely (in-memory-only, never touches the host filesystem).
    // Both are no-ops unless a loaded cartridge resolves to an FM-PAC
    // (mirrors the headless --fmpac-sram flag, src/main.cpp).
    std::optional<std::string> fmpac_sram_path;
    bool fmpac_sram_disabled = false;
    // --speed <0..7> launch-time initial Sony Speed
    // Controller level (devices::chipset::Mb670836PauseController), validated
    // against [0, kMaxSpeedLevel] via parse_speed_level() below. std::nullopt
    // (default) leaves the controller untouched -> level 0 (full speed).
    // SDL3 F6/F7 runtime stepping is unchanged;
    // --speed only sets the INITIAL value. Bad/missing values push into
    // `.errors` (mirrors --max-frames). (DEC-0056)
    std::optional<int> speed_level;
    // --scale <N> initial window size = 320N x 240N.
    // Clamped to [1, 8]; out-of-range/non-numeric/missing push into `.errors`.
    // std::nullopt (default) keeps the 960x720 window (= scale 3, DEC-0057),
    // non-regressive. The parser only records N; sdl3_main.cpp maps it to
    // window_width/height. (DEC-0056)
    std::optional<int> scale;
    // --filter <nearest|linear>; DEFAULT Linear (see TextureFilter). A bad
    // value pushes into `.errors`; absent stays Linear. (DEC-0056)
    TextureFilter filter = TextureFilter::Linear;
    // Phosphor-persistence inter-frame blend (--persistence <0..100>): the
    // percent of the previously presented frame retained by the SDL3 present
    // post-process (frontend/phosphor_blend.h). Validated to [0,100]; a
    // non-numeric / out-of-range value pushes into `.errors` (mirrors --scale).
    // std::nullopt (default) means OFF -> sdl3_main.cpp leaves the Sdl3AppConfig
    // default 0, so the present path is byte-identical to before. This is a
    // frontend PRESENTATION knob only: it never affects emulation/determinism/
    // headless output.
    std::optional<int> persistence;
    // --volume <0..100> master
    // gain percent (SDL3 presentation only). Validated to [0,100]; a non-numeric /
    // out-of-range value pushes into `.errors` (mirrors --persistence). std::nullopt
    // (default) = unspecified -> the resolver falls back to XML then the built-in
    // default 100 (unity -- the audio stays byte-identical). has_value() encodes
    // "specified" (no shadow bool -- same as --persistence). Alt+D/Alt+U step it
    // live. Attenuation only (max 100 = unity; never amplifies). (DEC-0079)
    std::optional<int> volume;
    // Phosphor-persistence blend MODE (--persistence-mode <avg|peak>). DEFAULT
    // Average (byte-identical to the original blend behavior); Peak selects the
    // peak-hold-with-decay blend that keeps multiplexed sprites full-brightness
    // instead of dimming them (frontend/phosphor_blend.h). A bad value pushes a
    // `.errors` entry (mirrors --filter's value policy); absent stays Average.
    // Presentation-only: never affects emulation/determinism/headless output.
    PhosphorMode persistence_mode = PhosphorMode::Average;
    // --fullscreen starts the window fullscreen
    // (Alt+Enter toggles at runtime). Bare boolean flag; default false. (DEC-0056)
    bool fullscreen = false;
    // --capture <on|off> gates the F10 live stream-capture hotkey.
    // DEFAULT false (OFF) -- F10 is INERT unless explicitly enabled, so a
    // mis-struck F10 during gameplay does nothing (a disruptive-toggle
    // complaint from the owner). `on`->true, `off`->false, absent->false; any
    // other value pushes a `.errors` entry (mirrors --filter). Only the F10
    // stream-capture hotkey is gated; F11/F12 and every other hotkey are
    // untouched. --stream-light/--snapshot still parse and configure capture
    // behavior, but only take effect once F10 is enabled+triggered.
    bool capture_enabled = false;
    // --ram <64|128|256|512> main-RAM size in KB. Validated
    // against the {64,128,256,512} enum via parse_ram_kb() below (any other value
    // -- non-numeric, 0, 96, 1024 -- is a loud parse error, mirroring the
    // --max-frames/--speed policy). std::nullopt (default) means the stock 64 KB
    // spec RAM; 128/256/512 are opt-in NON-STOCK
    // "fully-populated S1985" mods. sdl3_main.cpp maps the value to the machine's
    // DRAM byte count (kb * 1024). (DEC-0061)
    std::optional<int> ram_kb;
    // Replay-fidelity diagnostics (DEC-0072): --swap-disk-frame <N>
    // reproduces the headless scripted disk hot-swap on the SDL3 path (so a
    // recorded owner script replays on a hidden-window SDL3 build), and
    // --fingerprint <path> dumps a per-frame CPU-state CSV. Both std::nullopt by
    // default (no effect; existing sessions byte-for-byte unchanged).
    std::optional<std::uint32_t> swap_disk_frame;
    std::optional<std::string> fingerprint_path;
    // --config <path>
    // forces load of an externalized strict-XML config from that path in ANY
    // mode (headless or SDL3, hidden or not) -- the opt-in that overrides the
    // determinism auto-load gating. std::nullopt (default) = no explicit config;
    // an interactive SDL3 launch then AUTO-searches (<exe-dir> then <cwd>) while
    // headless / --hidden-window never auto-load, so the ctest suite (no test
    // passes --config) stays on the byte-identical no-config path. (DEC-0077)
    std::optional<std::string> config_path;
    // Explicit-tracking (config precedence CLI > XML > built-in default).
    // The plain-bool / plain-enum presentation knobs below default to a value
    // indistinguishable from an explicit CLI pass, so the config resolver
    // (config_runtime.h) must know whether the user ACTUALLY passed the flag to
    // honor precedence. Each is set true only when the corresponding flag appears
    // on argv. (The optional<> knobs -- fast_disk_opt / speed_level / scale /
    // persistence / ram_kb -- already encode "specified" via has_value(), so they
    // need no shadow.)
    bool border_specified = false;
    bool disk_writable_specified = false;
    bool fullscreen_specified = false;
    bool capture_specified = false;
    bool filter_specified = false;
    bool persistence_mode_specified = false;
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or a non-numeric --max-frames). Never silently swallowed by
    // the caller (mirrors cartridge_cli's own `errors` field/policy).
    std::vector<std::string> errors;
};

[[nodiscard]] ParsedSdl3Cli parse_sdl3_cli(const std::vector<std::string>& args);

// Shared `--ram` value validator, so the {64,128,256,512}KB
// enum policy has ONE implementation consumed by BOTH parse_sdl3_cli() (above)
// and src/main.cpp's headless boot path (--ram parity, headless + SDL3). On
// success sets `out_kb` (one of 64/128/256/512) and returns true; on a
// non-integer or non-enum value it appends a diagnostic (prefixed with
// `context`, e.g. "sdl3_cli" or "main") to `errors` and returns false. No SDL3
// dependency -- headlessly unit-testable (mirrors parse_speed_level). (DEC-0061)
[[nodiscard]] bool parse_ram_kb(const std::string& value, int& out_kb, std::vector<std::string>& errors,
                                const char* context);

// Shared `--speed` value validator, so the range
// policy [0, Mb670836PauseController::kMaxSpeedLevel] has ONE implementation
// consumed by BOTH parse_sdl3_cli() (above) and src/main.cpp's headless
// default run path (--speed parity, headless + SDL3). On success sets
// `out_level` and returns true; on a non-integer or out-of-range value it
// appends a diagnostic (prefixed with `context`, e.g. "sdl3_cli" or "main")
// to `errors` and returns false. No SDL3 dependency -- headlessly
// unit-testable. (DEC-0056)
[[nodiscard]] bool parse_speed_level(const std::string& value, int& out_level,
                                     std::vector<std::string>& errors, const char* context);

// The resolved convenience-vs-stock session defaults. The
// FLIPPED convenience defaults live ONLY here (the CLI-resolution layer), never
// in the Hbf1xvMachine ctor default (kDramBytes=64KB) nor the Sdl3AppConfig
// struct field defaults -- the anti-drift seam that keeps every
// direct-construction test stock. Consumed VERBATIM by BOTH executables
// (sdl3_main.cpp + src/main.cpp's --debug-session path) so their defaults
// match. (DEC-0071)
struct ResolvedSessionDefaults {
    std::size_t ram_bytes = 0;
    bool fast_disk = false;
    bool fmpac_autoload = false;
    bool is_stock = false;  // banner label only
};

// Tri-state CLI intent fed to resolve_session_defaults(): each convenience field
// carries "user asked" vs "unspecified" so the RESOLVER (not a parser field)
// owns the default.
struct SessionDefaultsRequest {
    std::optional<int> ram_kb;          // --ram value (nullopt = unspecified)
    std::optional<bool> fast_disk_opt;  // --fast-disk / --no-fast-disk (nullopt = unspecified)
    bool no_fmpac = false;              // --no-fmpac
    bool stock = false;                 // --stock
    // The XML-sourced BASE
    // defaults the resolver falls back to when the CLI field is unspecified AND
    // --stock is not set. The externalized config REPLACES the hardcoded
    // convenience default here (precedence CLI > XML > built-in default); an
    // explicit per-field CLI flag still wins, and --stock still forces the bare
    // machine. These default to the convenience values so an unspecified
    // request (every config-less caller, and every no-config launch) is
    // BYTE-IDENTICAL to the built-in defaults -- the anti-drift seam is preserved
    // because the Sdl3AppConfig/ctor stock defaults are untouched; only THIS
    // resolver's fallback source moves from a literal to the config value.
    // (DEC-0077)
    int base_ram_kb = 512;              // convenience default (KB)
    bool base_fast_disk = true;         // convenience default
    bool base_fmpac_autoload = true;    // convenience default
};

// Precedence: explicit per-field flag > --stock preset > XML base
// default > convenience default; positional-INDEPENDENT
// (`--stock --ram 512` == `--ram 512 --stock` -> 512 KB). Pure, SDL-free,
// headlessly unit-testable. With the base_* fields left at their convenience
// defaults, the empty-CLI result is {512 KB, fast-disk ON, FM-PAC auto-load ON}.
[[nodiscard]] ResolvedSessionDefaults resolve_session_defaults(const SessionDefaultsRequest& request);

}  // namespace sony_msx::frontend
