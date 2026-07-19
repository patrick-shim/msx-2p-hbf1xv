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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/config_runtime.h"
#include "frontend/sdl3_cli.h"
#include "machine/emulator_config.h"

// Suite: Frontend_ConfigRuntime_Wiring_Integration (M50-S2, docs/m50-planner-
// package.md §4.2/§4.6, AC-S2-2/AC-5).
//
// Exercises the LOAD + SEARCH + determinism WIRING against real on-disk files:
//   * the auto-load search order and "not found" WARNING;
//   * an explicit --config unreadable path WARNING (named);
//   * a valid file loading its XML values (auto-found + explicit);
//   * a structurally-broken file whole-file fallback WARNING;
//   * a per-key bad value WARNING (key named) + that key defaulting;
//   * the DETERMINISM proof: with a NON-default config file present on disk, the
//     headless / --hidden-window paths (gate=false) resolve to the BUILT-IN
//     defaults (config ignored) while an interactive launch (gate=true) resolves
//     to the XML values, and --config forces the load even headless.

namespace {

namespace fs = std::filesystem;

using sony_msx::frontend::config_should_load;
using sony_msx::frontend::load_config_with_search;
using sony_msx::frontend::parse_sdl3_cli;
using sony_msx::frontend::resolve_runtime_config;
using sony_msx::machine::EmulatorConfig;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

bool any_contains(const std::vector<std::string>& lines, const std::string& needle) {
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Mirror of the sdl3_main.cpp wiring: decide via the gate, then either load
// (auto-search the given candidates, or force the explicit path) or keep
// built-in defaults; return the effective config.
EmulatorConfig effective_config(const bool interactive, const std::optional<std::string>& explicit_path,
                                const std::vector<std::string>& auto_paths,
                                std::vector<std::string>& warnings) {
    EmulatorConfig cfg;  // built-in defaults
    if (config_should_load(interactive, explicit_path.has_value())) {
        cfg = load_config_with_search(explicit_path, auto_paths, warnings);
    }
    return cfg;
}

}  // namespace

int main() {
    const fs::path base = fs::temp_directory_path() / "hbf1xv_config_wiring";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);

    const fs::path valid = base / "valid.xml";
    write_file(valid,
               "<hbf1xv-config version=\"1\">\n"
               "  <defaults>\n"
               "    <fast-disk enabled=\"false\"/>\n"
               "    <border enabled=\"true\"/>\n"
               // M52 (DEC-0079): volume + disk-writable in the wiring path. The
               // disk-writable XML value is FALSE (differs from the flipped built-in
               // default TRUE), so the determinism proof below distinguishes the
               // no-config resolution (default ON) from the loaded XML (OFF).
               "    <volume percent=\"30\"/>\n"
               "    <disk-writable enabled=\"false\"/>\n"
               "  </defaults>\n"
               "  <machine>\n"
               "    <ram kb=\"128\"/>\n"
               "  </machine>\n"
               "</hbf1xv-config>\n");

    // --- Case 1: auto-search, NO file found -> one "not found (searched: ...)"
    // WARNING + built-in defaults. ---
    {
        std::vector<std::string> warnings;
        const std::vector<std::string> candidates{(base / "does-not-exist-a.xml").string(),
                                                  (base / "does-not-exist-b.xml").string()};
        const EmulatorConfig cfg = load_config_with_search(std::nullopt, candidates, warnings);
        expect(any_contains(warnings, "no sony_msx_hbf1xv.xml found"),
               "AutoSearch_NotFound_WarnsNotFound");
        expect(any_contains(warnings, "does-not-exist-a.xml"),
               "AutoSearch_NotFound_WarningNamesSearchedPaths");
        expect(cfg == EmulatorConfig{}, "AutoSearch_NotFound_ReturnsBuiltinDefaults");
    }

    // --- Case 2: explicit --config to an UNREADABLE path -> WARNING naming the
    // path + built-in defaults (no crash). ---
    {
        std::vector<std::string> warnings;
        const std::string missing = (base / "nope.xml").string();
        const EmulatorConfig cfg = load_config_with_search(missing, {}, warnings);
        expect(any_contains(warnings, missing), "ExplicitConfig_Unreadable_WarningNamesPath");
        expect(cfg == EmulatorConfig{}, "ExplicitConfig_Unreadable_BuiltinDefaults");
    }

    // --- Case 3: auto-search FINDS the valid file (first candidate missing,
    // second present) -> loads its XML values, no not-found warning. ---
    {
        std::vector<std::string> warnings;
        const std::vector<std::string> candidates{(base / "missing.xml").string(), valid.string()};
        const EmulatorConfig cfg = load_config_with_search(std::nullopt, candidates, warnings);
        expect(!cfg.fast_disk, "AutoSearch_Found_FastDiskFromXML");
        expect(cfg.border_enabled, "AutoSearch_Found_BorderFromXML");
        expect(cfg.ram_kb == 128, "AutoSearch_Found_Ram128FromXML");
        expect(cfg.master_volume == 30, "AutoSearch_Found_VolumeFromXML");       // M52
        expect(!cfg.disk_writable, "AutoSearch_Found_DiskWritableFalseFromXML");  // M52 (XML off)
        expect(!any_contains(warnings, "no sony_msx_hbf1xv.xml found"),
               "AutoSearch_Found_NoNotFoundWarning");
    }

    // --- Case 4: explicit --config to the valid file loads it (any mode). ---
    {
        std::vector<std::string> warnings;
        const EmulatorConfig cfg = load_config_with_search(valid.string(), {}, warnings);
        expect(!cfg.fast_disk && cfg.ram_kb == 128, "ExplicitConfig_Valid_LoadsXML");
    }

    // --- Case 5: a structurally-broken (wrong-root) file -> whole-file fallback
    // WARNING + built-in defaults, never a crash (§4.2). ---
    {
        const fs::path junk = base / "junk.xml";
        write_file(junk, "<not-our-config><garbage/></not-our-config>");
        std::vector<std::string> warnings;
        const EmulatorConfig cfg = load_config_with_search(junk.string(), {}, warnings);
        expect(any_contains(warnings, "not a valid hbf1xv-config") ||
                   any_contains(warnings, "using built-in defaults"),
               "Malformed_StructuralFallback_Warns");
        expect(cfg == EmulatorConfig{}, "Malformed_StructuralFallback_BuiltinDefaults");
    }

    // --- Case 6: a per-key BAD value -> WARNING naming the offending key + that
    // key defaults, while a VALID sibling key still applies (§4.2 per-key). ---
    {
        const fs::path perkey = base / "perkey.xml";
        write_file(perkey,
                   "<hbf1xv-config version=\"1\">\n"
                   "  <machine><ram kb=\"99\"/></machine>\n"      // invalid enum -> default 512
                   "  <defaults><border enabled=\"true\"/></defaults>\n"  // valid -> applies
                   "</hbf1xv-config>\n");
        std::vector<std::string> warnings;
        const EmulatorConfig cfg = load_config_with_search(perkey.string(), {}, warnings);
        expect(any_contains(warnings, "machine/ram@kb"), "PerKeyBad_WarningNamesKey");
        expect(cfg.ram_kb == 512, "PerKeyBad_RamFallsBackToDefault");
        expect(cfg.border_enabled, "PerKeyBad_ValidSiblingKeyStillApplies");
    }

    // ================================================================
    // Case 7 -- DETERMINISM at the wiring seam (§4.6, AC-S2-2). The valid config
    // (fast-disk OFF, ram 128) sits on disk as an auto-search candidate. Prove
    // the headless / hidden-window paths IGNORE it while interactive loads it.
    // ================================================================
    {
        const std::vector<std::string> auto_paths{valid.string()};
        const auto parsed = parse_sdl3_cli({});  // no CLI flags

        // (a) --hidden-window / headless (interactive=false, no --config):
        //     config IGNORED -> resolved knobs are the BUILT-IN defaults, even
        //     though a config file is present on disk. This is what keeps the
        //     ctest suite byte-identical.
        {
            std::vector<std::string> warnings;
            const EmulatorConfig cfg =
                effective_config(/*interactive=*/false, std::nullopt, auto_paths, warnings);
            const auto r = resolve_runtime_config(cfg, parsed);
            expect(r.fast_disk, "Determinism_Hidden_IgnoresConfig_FastDiskDefaultOn");
            expect(r.ram_bytes == 512u * 1024u, "Determinism_Hidden_IgnoresConfig_Ram512");
            expect(!r.border_enabled, "Determinism_Hidden_IgnoresConfig_BorderDefaultOff");
            // M52 (DEC-0079): config ignored -> resolved knobs are built-in defaults
            // (volume 100 unity; disk-writable flipped default ON), even though the
            // on-disk XML sets volume 30 / disk-writable OFF.
            expect(r.master_volume == 100, "Determinism_Hidden_IgnoresConfig_Volume100Default");
            expect(r.disk_writable, "Determinism_Hidden_IgnoresConfig_DiskWritableDefaultOn");
            expect(warnings.empty(), "Determinism_Hidden_NoConfigTouched_NoWarnings");
        }

        // (b) a genuinely interactive launch (interactive=true): auto-loads the
        //     SAME on-disk config -> the XML values take effect.
        {
            std::vector<std::string> warnings;
            const EmulatorConfig cfg =
                effective_config(/*interactive=*/true, std::nullopt, auto_paths, warnings);
            const auto r = resolve_runtime_config(cfg, parsed);
            expect(!r.fast_disk, "Determinism_Interactive_LoadsConfig_FastDiskOff");
            expect(r.ram_bytes == 128u * 1024u, "Determinism_Interactive_LoadsConfig_Ram128");
            expect(r.border_enabled, "Determinism_Interactive_LoadsConfig_BorderOn");
            // M52 (DEC-0079): the interactive launch auto-loads the XML -> volume 30
            // and disk-writable OFF take effect (XML overrides the flipped default).
            expect(r.master_volume == 30, "Determinism_Interactive_LoadsConfig_Volume30");
            expect(!r.disk_writable, "Determinism_Interactive_LoadsConfig_DiskWritableOff");
        }

        // (c) --config forces load even in a NON-interactive (headless) run.
        {
            std::vector<std::string> warnings;
            const EmulatorConfig cfg = effective_config(/*interactive=*/false,
                                                        valid.string(), /*auto_paths=*/{}, warnings);
            const auto r = resolve_runtime_config(cfg, parsed);
            expect(!r.fast_disk && r.ram_bytes == 128u * 1024u,
                   "Determinism_HeadlessExplicitConfig_ForcesLoad");
        }
    }

    fs::remove_all(base, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_ConfigRuntime_Wiring_Integration cases passed\n";
    return 0;
}
