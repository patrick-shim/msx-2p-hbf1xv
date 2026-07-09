#include <iostream>
#include <vector>

#include "frontend/sdl3_cli.h"

// Suite: Frontend_Sdl3Cli_Unit (M26-S2/S7, docs/m26-planner-package.md §2.8).
//
// Pure argv-parser unit tests -- zero SDL3 dependency, zero file I/O, mirrors
// tests/unit/machine/cartridge_cli_unit_test.cpp's own discipline. Registered
// OUTSIDE the SONY_MSX_ENABLE_SDL3 guard: this parsing logic is fully
// headless-testable (the developer's own placement choice, planner §3
// M26-S7).

namespace {

using sony_msx::frontend::parse_sdl3_cli;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Case 1: all flags present, order-independent, plus a reused
    // cartridge flag -- proves the delegation to machine::parse_cartridge_cli
    // genuinely works (not a stub). ---
    {
        const std::vector<std::string> args{
            "--max-frames", "120",  "--disk",  "disks/msxdos22.dsk", "--cart1",
            "roms/foo.rom", "--bios-dir", "C:/custom/bios",
        };
        const auto parsed = parse_sdl3_cli(args);
        expect(parsed.errors.empty(), "AllFlagsPresent_NoErrors");
        expect(parsed.max_frames.has_value() && *parsed.max_frames == 120, "AllFlagsPresent_MaxFramesParsed");
        expect(parsed.disk_path.has_value() && *parsed.disk_path == "disks/msxdos22.dsk",
               "AllFlagsPresent_DiskPathParsed");
        expect(parsed.bios_dir.has_value() && *parsed.bios_dir == "C:/custom/bios",
               "AllFlagsPresent_BiosDirParsed");
        expect(parsed.cartridges.slot1.path.has_value() && *parsed.cartridges.slot1.path == "roms/foo.rom",
               "AllFlagsPresent_Cart1Delegated_ToExistingCartridgeCliParser");
    }

    // --- Case 2: absent flags -> std::nullopt, never a silent default value
    // (mirrors cartridge_cli's own "absent means nullopt" contract). ---
    {
        const auto parsed = parse_sdl3_cli({});
        expect(parsed.errors.empty(), "NoFlags_NoErrors");
        expect(!parsed.max_frames.has_value(), "NoFlags_MaxFramesAbsent");
        expect(!parsed.disk_path.has_value(), "NoFlags_DiskPathAbsent");
        expect(!parsed.bios_dir.has_value(), "NoFlags_BiosDirAbsent");
        expect(!parsed.hidden_window, "NoFlags_HiddenWindowDefaultsFalse");
        expect(!parsed.border_enabled, "NoFlags_BorderDefaultsFalse_BareActiveAreaPresentation");
    }

    // --- Case 3: a flag missing its required value argument is a loud,
    // recorded parse error -- never silently swallowed (mirrors
    // cartridge_cli's take_value error policy). ---
    {
        const auto parsed = parse_sdl3_cli({"--disk"});
        expect(!parsed.errors.empty(), "MissingValueArgument_RecordsError");
        expect(!parsed.disk_path.has_value(), "MissingValueArgument_DiskPathStaysAbsent");
    }

    // --- Case 4: a non-numeric --max-frames value is a loud, recorded parse
    // error -- never silently defaulted to 0 or ignored. ---
    {
        const auto parsed = parse_sdl3_cli({"--max-frames", "not-a-number"});
        expect(!parsed.errors.empty(), "NonNumericMaxFrames_RecordsError");
        expect(!parsed.max_frames.has_value(), "NonNumericMaxFrames_StaysAbsent");
    }

    // --- Case 5: --hidden-window is a bare boolean flag (test/CI
    // convenience), never SDL3-dependent to parse. ---
    {
        const auto parsed = parse_sdl3_cli({"--hidden-window"});
        expect(parsed.errors.empty(), "HiddenWindowFlag_NoErrors");
        expect(parsed.hidden_window, "HiddenWindowFlag_SetsTrue");
    }

    // --- Case 6: --border is a bare boolean flag enabling the opt-in
    // border-box presentation (default OFF -- docs/konami-splash-regression-
    // investigation.md); order-independent alongside other flags. ---
    {
        const auto parsed = parse_sdl3_cli({"--border"});
        expect(parsed.errors.empty(), "BorderFlag_NoErrors");
        expect(parsed.border_enabled, "BorderFlag_SetsTrue");
    }
    {
        const auto parsed = parse_sdl3_cli({"--max-frames", "5", "--border", "--hidden-window"});
        expect(parsed.errors.empty(), "BorderFlag_WithOtherFlags_NoErrors");
        expect(parsed.border_enabled && parsed.hidden_window && parsed.max_frames.has_value() &&
                   *parsed.max_frames == 5,
               "BorderFlag_OrderIndependent_AlongsideOtherFlags");
    }

    // --- Case 7 (M30 additive, backlog G2): --softwaredb flows through
    // parse_sdl3_cli's delegated cartridge parse (docs/m30-planner-package.md
    // §4-S4), alongside the `auto` type value and type_was_explicit. ---
    {
        const auto parsed = parse_sdl3_cli(
            {"--softwaredb", "custom/softwaredb.xml", "--cart1", "roms/foo.rom"});
        expect(parsed.errors.empty(), "SoftwareDbFlag_NoErrors");
        expect(parsed.cartridges.softwaredb_path.has_value() &&
                   *parsed.cartridges.softwaredb_path == "custom/softwaredb.xml",
               "SoftwareDbFlag_FlowsThroughDelegatedCartridgeParse");
        expect(!parsed.cartridges.slot1.type_was_explicit,
               "SoftwareDbFlag_TypelessCart1_NotExplicit");
    }
    {
        const auto parsed = parse_sdl3_cli({"--cart1", "roms/foo.rom", "--cart1-type", "auto"});
        expect(parsed.errors.empty(), "AutoTypeValue_NoErrors");
        expect(!parsed.cartridges.slot1.type_was_explicit,
               "AutoTypeValue_NotExplicit_ThroughDelegatedParse");
    }
    {
        const auto parsed = parse_sdl3_cli({});
        expect(!parsed.cartridges.softwaredb_path.has_value(),
               "SoftwareDbFlag_Absent_Nullopt");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3Cli_Unit cases passed\n";
    return 0;
}
