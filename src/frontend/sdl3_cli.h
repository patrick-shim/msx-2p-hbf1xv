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
// DEFAULT Linear -- the "smooth" look the human asked for (DEC-0056), which
// is also the SDL3 renderer's own default texture scale mode
// (references/sdl3/include/SDL3/SDL_render.h:1260), so an absent --filter is
// byte-identical to the pre-M37 presentation. Nearest = crisp pixels.
enum class TextureFilter { Nearest, Linear };

// Pure argv parser for the SDL3 frontend's own flags (M26-S2/S7,
// docs/m26-planner-package.md §2.8): `--bios-dir <path>`, `--disk <path>`
// (A-M26-6), `--max-frames <N>` (a bounded, non-interactive run length for
// headless/test use, §2.3), plus the REUSED M19 cartridge flags
// (`--cart1`/`--cart1-type`/`--cart2`/`--cart2-type`, delegated verbatim to
// the existing `sony_msx::machine::parse_cartridge_cli` -- never
// reimplemented, mirrors `src/main.cpp`'s own `load_cartridges_from_args`
// reuse). Does NO file I/O and has NO Sdl3App/SDL3 dependency, so it is
// directly unit-testable headlessly, without any SDL3 build (mirrors
// cartridge_cli.h's own parse/spec-vs-load separation, A-M19-4).
//
// M35-S1 (multi-disk hot-swap): `--disk` is now REPEATABLE (M35-S1,
// docs/m35-planner-package.md §3.1). A single `--disk <path>` produces
// a list of size 1 (backward-compatible, AC-S1-2). Multiple `--disk`
// flags accumulate in order (AC-S1-3). No `--disk` flags produce an empty
// list (AC-S1-4, existing behavior preserved).
struct ParsedSdl3Cli {
    std::optional<std::string> bios_dir;
    std::vector<std::string> disk_paths;  // M35-S1: now a vector, accumulates repeatable --disk
    std::optional<std::uint32_t> max_frames;
    bool hidden_window = false;  // --hidden-window (test/CI convenience; never SDL3-dependent to parse)
    // --border: opt-in border-box composition around the active area
    // (border_composer.h). Default OFF -- the bare active area is presented
    // edge-to-edge (human-decided presentation preference, docs/konami-
    // splash-regression-investigation.md).
    bool border_enabled = false;
    // M36-S-c: --disk-writable opt-in host-file disk-save persistence. Default
    // OFF = in-memory-only (never clobbers a real .dsk); a dirty writable image
    // flushes on shutdown and before a swap discards it.
    bool disk_writable = false;
    machine::ParsedCartridgeCli cartridges;
    // M27-S4/S7 additive debug/scripted-input flags (docs/m27-planner-
    // package.md §2.2/§2.4, items 1/3/4): `--dump-state`/`--trace-cpu`/
    // `--event-log` mirror the headless `--debug-session` mode's own flags
    // of the same name; `--input-script` mirrors its own flag too. All four
    // are std::nullopt by default -- a hard regression guard (§4 Acceptance
    // Criterion 10): every pre-existing M26 parse_sdl3_cli() case remains
    // green unmodified.
    std::optional<std::string> dump_state_filename;
    std::optional<std::string> trace_cpu_filename;
    std::optional<std::string> event_log_filename;
    std::optional<std::string> input_script_path;
    // M36 Phase 3 (DEC-0051): --snapshot <dir> enables the comprehensive debug
    // snapshot and overrides the output root (<dir>/snapshot/<id>/). F12
    // in-session capture is ALWAYS active (read-only, harmless); this flag only
    // controls where captures land. std::nullopt by default -- a hard
    // regression guard: every pre-existing parse_sdl3_cli() case stays green
    // unmodified (mirrors the M27 dump_state_filename additive field).
    std::optional<std::string> snapshot_dir;
    // DEC-0052 stream-light: --stream-light arms the F10 live stream-capture in
    // the LIGHTWEIGHT mode (per-frame snapshot bundles suppressed -> coarse
    // anchors + the per-event watchlog) for a LONG armed session. Default OFF =
    // the heavy every-frame F10 mode (byte-for-byte the prior behavior).
    bool stream_light = false;
    // M36 FM-PAC SRAM persistence (SDL3 side): a real FM-PAC always
    // battery-persists, so the SDL3 build auto-derives the SRAM host file from
    // the inserted FM-PAC cart's ROM path (<cart>.rom -> <cart>.rom.sram) with
    // NO opt-in flag. --fmpac-sram <path> OVERRIDES that derived default;
    // std::nullopt (default) = derive from the cart path. --no-fmpac-sram opts
    // OUT entirely (in-memory-only, never touches the host filesystem). Both are
    // complete no-ops unless a loaded cartridge resolves to an FM-PAC (mirrors
    // the headless --fmpac-sram flag, src/main.cpp).
    std::optional<std::string> fmpac_sram_path;
    bool fmpac_sram_disabled = false;
    // M37 Slice D (DEC-0056): --speed <0..7> launch-time initial Sony Speed
    // Controller level (devices::chipset::Mb670836PauseController). Range is
    // validated against [0, kMaxSpeedLevel] via parse_speed_level() below;
    // std::nullopt (default) leaves the controller untouched -> level 0 (full
    // speed), byte-identical to today. The SDL3 F6/F7 runtime stepping is
    // unchanged; --speed only sets the INITIAL value. Out-of-range/non-numeric/
    // missing-value push a message into `.errors` (mirrors --max-frames).
    std::optional<int> speed_level;
    // M37 Slice E (DEC-0056): --scale <N> initial window size = 320N x 240N.
    // Clamped to [1, 8]; out-of-range/non-numeric/missing push into `.errors`.
    // std::nullopt (default) keeps the 640x480 window (= scale 2), non-regressive.
    // The parser only records N; sdl3_main.cpp maps it to window_width/height.
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
    // other value pushes a `.errors` entry (mirrors the --filter error policy).
    // Only the F10 stream-capture hotkey is gated; F11/F12 and every other
    // hotkey are untouched. --stream-light/--snapshot still parse and configure
    // capture behavior, but only take effect when F10 is enabled+triggered.
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
// default run path (--speed parity, headless + SDL3). Parses `value` as an
// integer; on success sets `out_level` and returns true; on a non-integer or
// out-of-range value it appends a diagnostic (prefixed with `context`, e.g.
// "sdl3_cli" or "main") to `errors` and returns false. No SDL3 dependency --
// headlessly unit-testable.
[[nodiscard]] bool parse_speed_level(const std::string& value, int& out_level,
                                     std::vector<std::string>& errors, const char* context);

}  // namespace sony_msx::frontend
