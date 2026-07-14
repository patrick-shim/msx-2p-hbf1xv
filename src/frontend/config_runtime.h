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

#include <optional>
#include <string>
#include <vector>

#include "frontend/phosphor_blend.h"  // PhosphorMode (SDL-free)
#include "frontend/sdl3_cli.h"        // ParsedSdl3Cli, TextureFilter, resolve_session_defaults
#include "machine/emulator_config.h"  // EmulatorConfig (the loaded XML model)

namespace sony_msx::frontend {

// ---------------------------------------------------------------------------
// M50-S2 (DEC-0077, docs/m50-planner-package.md §4.1/§4.6): the CLI-default
// RESOLUTION seam that wires the externalized strict-XML config into the run
// paths with precedence CLI > XML > built-in default, plus the determinism
// auto-load gate.
//
// This unit is SDL-free (it deals only in the SDL-free enums TextureFilter /
// PhosphorMode; sdl3_main.cpp maps the resolved values onto the SDL-typed
// Sdl3AppConfig), so it lives in sony_msx_core and is headlessly unit-testable.
// It touches NO built-in default VALUE (the M46 anti-drift ctor/struct defaults
// are untouched); it only chooses, per knob, which of {CLI, XML, built-in} is
// effective.
// ---------------------------------------------------------------------------

// The resolved, effective session + presentation knobs after applying the
// precedence CLI > XML > built-in default. SDL-free (filter / persistence_mode
// stay as the SDL-free enums). sdl3_main.cpp copies these onto Sdl3AppConfig.
struct ResolvedRuntimeConfig {
    // --- Session (machine-affecting) knobs -----------------------------------
    std::size_t ram_bytes = 512u * 1024u;  // resolved main-RAM byte count
    bool fast_disk = true;                 // resolved FDC-turbo default
    bool fmpac_autoload = true;            // resolved FM-PAC slot auto-load
    int fmpac_autoload_slot = 2;           // resolved FM-PAC auto-load slot (1|2)
    bool is_stock = false;                 // --stock banner label passthrough

    // --- Presentation (SDL3-only) knobs --------------------------------------
    bool border_enabled = false;
    bool disk_writable = false;
    int speed_level = 0;                          // 0..7 (0 == full speed)
    int scale = 3;                                // window scale (1..8); w/h = 320N x 240N
    TextureFilter filter = TextureFilter::Linear;
    int persistence = 0;                          // 0..100
    PhosphorMode persistence_mode = PhosphorMode::Average;
    bool fullscreen = false;
    bool capture_enabled = false;
};

// The determinism rule (§4.6), as a named + testable predicate: whether this
// launch may consult a config file at all.
//   * `interactive_sdl3` = a genuinely interactive SDL3 launch (the SDL3 exe
//     run WITHOUT --hidden-window). Only THIS may AUTO-load.
//   * `explicit_config_path` = --config <path> was passed. This FORCES a load in
//     ANY mode (headless or SDL3, hidden or not) -- the opt-in.
// Headless (default scaffold / --debug-session / parity) and SDL3 --hidden-window
// pass interactive_sdl3=false, so they NEVER auto-load; with no --config they do
// not load at all. Because no ctest test passes --config and every SDL3 test uses
// --hidden-window, the whole suite stays on the no-config path -> byte-identical.
[[nodiscard]] bool config_should_load(bool interactive_sdl3, bool explicit_config_path);

// Load the effective EmulatorConfig honoring the determinism search order
// (§4.6). Precondition (caller): only invoke when config_should_load() is true.
//   * `explicit_path` set (--config): FORCE-load from that path; an unreadable /
//     missing path emits ONE WARNING naming the path and returns built-in
//     defaults (never throws).
//   * else AUTO-search `auto_search_paths` in order (first existing wins); if
//     none exists, emit ONE "not found (searched: ...)" WARNING and return
//     built-in defaults.
// Per-key parse WARNINGs from EmulatorConfig::parse() are appended to `warnings`
// (named, one per bad key). The caller prints `warnings` to stderr.
[[nodiscard]] machine::EmulatorConfig load_config_with_search(
    const std::optional<std::string>& explicit_path,
    const std::vector<std::string>& auto_search_paths, std::vector<std::string>& warnings);

// Apply precedence CLI > XML > built-in default across every S2-scope knob.
// `cfg` is the loaded config (or all-built-in-defaults when nothing was loaded,
// which makes the result byte-identical to pre-M50). `parsed` carries the CLI
// intents + explicit-tracking. Pure, SDL-free, headlessly unit-testable.
[[nodiscard]] ResolvedRuntimeConfig resolve_runtime_config(const machine::EmulatorConfig& cfg,
                                                           const ParsedSdl3Cli& parsed);

// ---------------------------------------------------------------------------
// M50-S3 (DEC-0077, docs/m50-planner-package.md §6-S3/§4.4): the MACHINE-sizing
// + asset-path resolution seam. Same precedence discipline as
// resolve_runtime_config (CLI > XML > built-in default), for the fields routed
// into machine construction / asset loading rather than the SDL3 presentation
// layer. Pure, SDL-free, headlessly unit-testable.
// ---------------------------------------------------------------------------
struct ResolvedMachineConfig {
    // BIOS/ROM directory + the 7 role-keyed BIOS filenames (loaded under bios_dir
    // by RomAssetLoader; expected sizes stay code-owned in load_rom_assets()).
    std::string bios_dir = "bios";
    machine::EmulatorConfig::BiosRoms bios_roms{};

    // FM-PAC auto-load ROM path (no dedicated CLI flag -- an explicit --slot2 fills
    // the bay and the auto-load skips; here it is XML > built-in default).
    std::string fmpac_autoload_rom = "roms/fmpac.rom";

    // FM-PAC SRAM host-file path and softwaredb path are OPTIONAL at the consumer
    // seam: std::nullopt means "use the consumer's own built-in behavior" (SRAM
    // auto-derive beside the cart; softwaredb = kDefaultSoftwareDbPath with a
    // quiet default-unavailable message). A value is surfaced ONLY when the CLI
    // set it OR the XML set it to a NON-default value -- so with no config loaded
    // both stay std::nullopt and the resolved result is byte-identical to pre-M50.
    std::optional<std::string> fmpac_sram;
    std::optional<std::string> softwaredb;

    // VRAM KB: validated-to-128 by the parser (strict HB-F1XV spec, §4.4). There
    // is NO runtime VRAM-sizing seam (VdpVram::kVramBytes is constexpr), so this
    // is ALWAYS 128 after resolution; it is carried for the banner/tests and is
    // NEVER used to resize the VDP (a value != 128 already warned + clamped in the
    // parser).
    int vram_kb = 128;
};

// Resolve the machine-sizing/path fields with precedence CLI > XML > built-in
// default. `cfg` is the loaded config (or all-built-in-defaults when nothing was
// loaded -> the result is byte-identical to pre-M50). `parsed` carries the CLI
// override intents (--bios-dir, --fmpac-sram, --softwaredb).
[[nodiscard]] ResolvedMachineConfig resolve_machine_config(const machine::EmulatorConfig& cfg,
                                                           const ParsedSdl3Cli& parsed);

}  // namespace sony_msx::frontend
