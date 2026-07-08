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
struct ParsedSdl3Cli {
    std::optional<std::string> bios_dir;
    std::optional<std::string> disk_path;
    std::optional<std::uint32_t> max_frames;
    bool hidden_window = false;  // --hidden-window (test/CI convenience; never SDL3-dependent to parse)
    // --border: opt-in border-box composition around the active area
    // (border_composer.h). Default OFF -- the bare active area is presented
    // edge-to-edge (human-decided presentation preference, docs/konami-
    // splash-regression-investigation.md).
    bool border_enabled = false;
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
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or a non-numeric --max-frames). Never silently swallowed by
    // the caller (mirrors cartridge_cli's own `errors` field/policy).
    std::vector<std::string> errors;
};

[[nodiscard]] ParsedSdl3Cli parse_sdl3_cli(const std::vector<std::string>& args);

}  // namespace sony_msx::frontend
