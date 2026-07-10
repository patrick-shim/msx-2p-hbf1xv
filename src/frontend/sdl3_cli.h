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
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or a non-numeric --max-frames). Never silently swallowed by
    // the caller (mirrors cartridge_cli's own `errors` field/policy).
    std::vector<std::string> errors;
};

[[nodiscard]] ParsedSdl3Cli parse_sdl3_cli(const std::vector<std::string>& args);

}  // namespace sony_msx::frontend
