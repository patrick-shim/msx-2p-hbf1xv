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

#include "machine/cartridge_cli.h"

namespace sony_msx::frontend {

// M37 Slice E (DEC-0056): --filter <nearest|linear> texture scale mode.
// DEFAULT Linear -- matches the SDL3 renderer's own default scale mode
// (references/sdl3/include/SDL3/SDL_render.h:1260), so an absent --filter
// is byte-identical to the pre-M37 presentation. Nearest = crisp pixels.
enum class TextureFilter { Nearest, Linear };

// Pure argv parser for the SDL3 frontend's own flags (M26-S2/S7,
// docs/m26-planner-package.md §2.8): `--bios-dir`, `--disk` (repeatable,
// A-M26-6), `--max-frames` (bounded headless/test run length, §2.3), plus
// the REUSED M19 cartridge flags (`--cart1`/`--cart1-type`/`--cart2`/
// `--cart2-type`), delegated verbatim to `sony_msx::machine::parse_cartridge_cli`
// -- never reimplemented, mirrors `src/main.cpp`'s own reuse. No file I/O,
// no Sdl3App/SDL3 dependency -- headlessly unit-testable (mirrors
// cartridge_cli.h's own parse/spec-vs-load separation, A-M19-4).
//
// M35-S1 (multi-disk hot-swap, docs/m35-planner-package.md §3.1): `--disk`
// is REPEATABLE and accumulates in order. A single flag yields a size-1
// list (backward-compatible, AC-S1-2); multiple flags accumulate (AC-S1-3);
// none yields an empty list (AC-S1-4).
struct ParsedSdl3Cli {
    std::optional<std::string> bios_dir;
    std::vector<std::string> disk_paths;  // M35-S1: now a vector, accumulates repeatable --disk
    std::optional<std::uint32_t> max_frames;
    bool hidden_window = false;  // --hidden-window (test/CI convenience; never SDL3-dependent to parse)
    // Border-canvas composition around the active area (border_composer.h).
    // Default OFF (M39-D, human preference revert): the DEFAULT present is the
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
    // M36-S-c: --disk-writable opts into host-file disk-save persistence.
    // Default OFF = in-memory-only (never clobbers a real .dsk); a dirty
    // writable image flushes on shutdown and before a swap discards it.
    bool disk_writable = false;
    machine::ParsedCartridgeCli cartridges;
    // M27-S4/S7 additive debug/scripted-input flags (docs/m27-planner-
    // package.md §2.2/§2.4, items 1/3/4): `--dump-state`/`--trace-cpu`/
    // `--event-log`/`--input-script` mirror the headless `--debug-session`
    // mode's own flags of the same name. All four default to std::nullopt --
    // pre-existing M26 parse_sdl3_cli() cases stay green (§4 AC 10).
    std::optional<std::string> dump_state_filename;
    std::optional<std::string> trace_cpu_filename;
    std::optional<std::string> event_log_filename;
    std::optional<std::string> input_script_path;
    // M36 Phase 3 (DEC-0051): --snapshot <dir> overrides the debug-snapshot
    // output root (<dir>/snapshot/<id>/). F12 in-session capture is ALWAYS
    // active (read-only, harmless); this flag only controls where captures
    // land. std::nullopt by default -- pre-existing parse_sdl3_cli() cases
    // stay green (mirrors the M27 dump_state_filename field).
    std::optional<std::string> snapshot_dir;
    // DEC-0052: --stream-light arms F10's LIGHTWEIGHT capture mode (per-frame
    // snapshot bundles suppressed -> coarse anchors + the per-event watchlog)
    // for a long armed session. Default OFF = the heavy every-frame F10 mode
    // (byte-for-byte the prior behavior).
    bool stream_light = false;
    // M36 FM-PAC SRAM persistence (SDL3 side): a real FM-PAC always
    // battery-persists, so the SDL3 build auto-derives the SRAM host file
    // from the inserted cart's ROM path (<cart>.rom -> <cart>.rom.sram),
    // no opt-in needed. --fmpac-sram <path> OVERRIDES that derived default;
    // std::nullopt (default) = derive from the cart path. --no-fmpac-sram
    // opts OUT entirely (in-memory-only, never touches the host filesystem).
    // Both are no-ops unless a loaded cartridge resolves to an FM-PAC
    // (mirrors the headless --fmpac-sram flag, src/main.cpp).
    std::optional<std::string> fmpac_sram_path;
    bool fmpac_sram_disabled = false;
    // M37 Slice D (DEC-0056): --speed <0..7> launch-time initial Sony Speed
    // Controller level (devices::chipset::Mb670836PauseController), validated
    // against [0, kMaxSpeedLevel] via parse_speed_level() below. std::nullopt
    // (default) leaves the controller untouched -> level 0 (full speed),
    // byte-identical to today. SDL3 F6/F7 runtime stepping is unchanged;
    // --speed only sets the INITIAL value. Bad/missing values push into
    // `.errors` (mirrors --max-frames).
    std::optional<int> speed_level;
    // M37 Slice E (DEC-0056): --scale <N> initial window size = 320N x 240N.
    // Clamped to [1, 8]; out-of-range/non-numeric/missing push into `.errors`.
    // std::nullopt (default) keeps the 960x720 window (= scale 3, DEC-0057),
    // non-regressive. The parser only records N; sdl3_main.cpp maps it to
    // window_width/height.
    std::optional<int> scale;
    // M37 Slice E (DEC-0056): --filter <nearest|linear>; DEFAULT Linear (see
    // TextureFilter). A bad value pushes into `.errors`; absent stays Linear.
    TextureFilter filter = TextureFilter::Linear;
    // M37 Slice E (DEC-0056): --fullscreen starts the window fullscreen
    // (Alt+Enter toggles at runtime). Bare boolean flag; default false.
    bool fullscreen = false;
    // M37 Slice F: --capture <on|off> gates the F10 live stream-capture hotkey.
    // DEFAULT false (OFF) -- F10 is INERT unless explicitly enabled, so a
    // mis-struck F10 during gameplay does nothing (a disruptive-toggle
    // complaint from the human). `on`->true, `off`->false, absent->false; any
    // other value pushes a `.errors` entry (mirrors --filter). Only the F10
    // stream-capture hotkey is gated; F11/F12 and every other hotkey are
    // untouched. --stream-light/--snapshot still parse and configure capture
    // behavior, but only take effect once F10 is enabled+triggered.
    bool capture_enabled = false;
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or a non-numeric --max-frames). Never silently swallowed by
    // the caller (mirrors cartridge_cli's own `errors` field/policy).
    std::vector<std::string> errors;
};

[[nodiscard]] ParsedSdl3Cli parse_sdl3_cli(const std::vector<std::string>& args);

// M37 Slice D (DEC-0056): shared `--speed` value validator, so the range
// policy [0, Mb670836PauseController::kMaxSpeedLevel] has ONE implementation
// consumed by BOTH parse_sdl3_cli() (above) and src/main.cpp's headless
// default run path (--speed parity, headless + SDL3). On success sets
// `out_level` and returns true; on a non-integer or out-of-range value it
// appends a diagnostic (prefixed with `context`, e.g. "sdl3_cli" or "main")
// to `errors` and returns false. No SDL3 dependency -- headlessly
// unit-testable.
[[nodiscard]] bool parse_speed_level(const std::string& value, int& out_level,
                                     std::vector<std::string>& errors, const char* context);

}  // namespace sony_msx::frontend
