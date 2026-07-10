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
    // genuinely works (not a stub). M35-S1: --disk now accumulates to a
    // vector (single entry for backward compat). ---
    {
        const std::vector<std::string> args{
            "--max-frames", "120",  "--disk",  "disks/msxdos22.dsk", "--cart1",
            "roms/foo.rom", "--bios-dir", "C:/custom/bios",
        };
        const auto parsed = parse_sdl3_cli(args);
        expect(parsed.errors.empty(), "AllFlagsPresent_NoErrors");
        expect(parsed.max_frames.has_value() && *parsed.max_frames == 120, "AllFlagsPresent_MaxFramesParsed");
        expect(parsed.disk_paths.size() == 1 && parsed.disk_paths[0] == "disks/msxdos22.dsk",
               "AllFlagsPresent_DiskPathParsed_M35S1_SingleItemList");
        expect(parsed.bios_dir.has_value() && *parsed.bios_dir == "C:/custom/bios",
               "AllFlagsPresent_BiosDirParsed");
        expect(parsed.cartridges.slot1.path.has_value() && *parsed.cartridges.slot1.path == "roms/foo.rom",
               "AllFlagsPresent_Cart1Delegated_ToExistingCartridgeCliParser");
    }

    // --- Case 2: absent flags -> empty disk_paths vector (M35-S1), never a
    // default value (mirrors cartridge_cli's own "absent means absent" contract). ---
    {
        const auto parsed = parse_sdl3_cli({});
        expect(parsed.errors.empty(), "NoFlags_NoErrors");
        expect(!parsed.max_frames.has_value(), "NoFlags_MaxFramesAbsent");
        expect(parsed.disk_paths.empty(), "NoFlags_DiskPathsEmpty_M35S1");
        expect(!parsed.bios_dir.has_value(), "NoFlags_BiosDirAbsent");
        expect(!parsed.hidden_window, "NoFlags_HiddenWindowDefaultsFalse");
        expect(!parsed.border_enabled, "NoFlags_BorderDefaultsFalse_BareActiveAreaPresentation");
    }

    // --- Case 3: a flag missing its required value argument is a loud,
    // recorded parse error -- never silently swallowed (mirrors
    // cartridge_cli's take_value error policy). M35-S1: disk_paths stays
    // empty when error occurs. ---
    {
        const auto parsed = parse_sdl3_cli({"--disk"});
        expect(!parsed.errors.empty(), "MissingValueArgument_RecordsError");
        expect(parsed.disk_paths.empty(), "MissingValueArgument_DiskPathsEmpty_M35S1");
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

    // --- Case 8 (M35-S1): --disk is repeatable. Multiple --disk flags
    // accumulate in order. ---
    {
        const std::vector<std::string> args{
            "--disk", "disk1.dsk",
            "--disk", "disk2.dsk",
            "--disk", "disk3.dsk",
        };
        const auto parsed = parse_sdl3_cli(args);
        expect(parsed.errors.empty(), "RepeatableDisk_NoErrors");
        expect(parsed.disk_paths.size() == 3, "RepeatableDisk_AccumulatesThreePaths");
        expect(parsed.disk_paths[0] == "disk1.dsk", "RepeatableDisk_FirstPathCorrect");
        expect(parsed.disk_paths[1] == "disk2.dsk", "RepeatableDisk_SecondPathCorrect");
        expect(parsed.disk_paths[2] == "disk3.dsk", "RepeatableDisk_ThirdPathCorrect");
    }

    // --- Case 9 (M35-S1): order-independence -- --disk interleaved with
    // other flags. ---
    {
        const std::vector<std::string> args{
            "--disk", "d1.dsk",
            "--max-frames", "50",
            "--disk", "d2.dsk",
            "--bios-dir", "my/bios",
        };
        const auto parsed = parse_sdl3_cli(args);
        expect(parsed.errors.empty(), "RepeatableDisk_WithOtherFlags_NoErrors");
        expect(parsed.disk_paths.size() == 2, "RepeatableDisk_WithOtherFlags_TwoPaths");
        expect(parsed.disk_paths[0] == "d1.dsk" && parsed.disk_paths[1] == "d2.dsk",
               "RepeatableDisk_WithOtherFlags_OrderPreserved");
        expect(parsed.max_frames.has_value() && *parsed.max_frames == 50,
               "RepeatableDisk_WithOtherFlags_MaxFramesStillWorks");
    }

    // --- Case 10 (M37 Slice D, DEC-0056): --speed <0..7> launch-time initial
    // Speed Controller level. Valid values parse into speed_level; out-of-range
    // / non-numeric / missing-value each record a loud error and leave
    // speed_level absent (mirrors the --max-frames error policy). Absent flag ->
    // nullopt (a hard regression guard: no touch to the controller by default). ---
    {
        const auto p0 = parse_sdl3_cli({"--speed", "0"});
        expect(p0.errors.empty() && p0.speed_level.has_value() && *p0.speed_level == 0, "Speed0_Parses");
        const auto p7 = parse_sdl3_cli({"--speed", "7"});
        expect(p7.errors.empty() && p7.speed_level.has_value() && *p7.speed_level == 7, "Speed7_Parses");
        const auto p8 = parse_sdl3_cli({"--speed", "8"});
        expect(!p8.errors.empty() && !p8.speed_level.has_value(), "Speed8_OutOfRange_Error");
        const auto pneg = parse_sdl3_cli({"--speed", "-1"});
        expect(!pneg.errors.empty() && !pneg.speed_level.has_value(), "SpeedNeg1_OutOfRange_Error");
        const auto pabc = parse_sdl3_cli({"--speed", "abc"});
        expect(!pabc.errors.empty() && !pabc.speed_level.has_value(), "SpeedAbc_NonNumeric_Error");
        const auto pmiss = parse_sdl3_cli({"--speed"});
        expect(!pmiss.errors.empty() && !pmiss.speed_level.has_value(), "SpeedMissingArg_Error");
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.speed_level.has_value(), "SpeedAbsent_Nullopt");
    }

    // --- Case 10b: parse_speed_level is the SHARED validator that src/main.cpp's
    // headless --speed path also uses (ONE range policy). Exercise it directly
    // so the headless path's validation is covered too (no headless-main
    // arg-parse harness exists; this is the honest testable seam). ---
    {
        using sony_msx::frontend::parse_speed_level;
        std::vector<std::string> errs;
        int level = -99;
        expect(parse_speed_level("0", level, errs, "main") && level == 0, "SharedSpeed_0_OK");
        expect(parse_speed_level("7", level, errs, "main") && level == 7, "SharedSpeed_7_OK");
        expect(errs.empty(), "SharedSpeed_NoErrorsForValid");
        expect(!parse_speed_level("8", level, errs, "main"), "SharedSpeed_8_Rejected");
        expect(!parse_speed_level("-1", level, errs, "main"), "SharedSpeed_Neg_Rejected");
        expect(!parse_speed_level("x", level, errs, "main"), "SharedSpeed_NonNumeric_Rejected");
        expect(!errs.empty(), "SharedSpeed_ErrorsRecordedForInvalid");
    }

    // --- Case 11 (M37 Slice E, DEC-0056): --scale <N> initial window scale.
    // Valid N in [1,8] parses; sdl3_main.cpp maps N -> 320N x 240N. Out-of-range
    // / non-numeric / missing each record an error and leave scale absent. Absent
    // flag -> nullopt; sdl3_main.cpp then falls back to the Sdl3AppConfig window
    // default, which M37 Slice F changed from 640x480 (scale 2) to 960x720
    // (scale 3) BY DESIGN -- the parser field policy (absent -> nullopt) is
    // unchanged; only the downstream default window size moved. ---
    {
        const auto p3 = parse_sdl3_cli({"--scale", "3"});
        expect(p3.errors.empty() && p3.scale.has_value() && *p3.scale == 3, "Scale3_Parses");
        // The production mapping sdl3_main.cpp applies: 320*3 x 240*3 = 960x720.
        expect(p3.scale.has_value() && 320 * *p3.scale == 960 && 240 * *p3.scale == 720,
               "Scale3_MapsTo960x720");
        const auto p1 = parse_sdl3_cli({"--scale", "1"});
        expect(p1.errors.empty() && p1.scale.has_value() && *p1.scale == 1, "Scale1_Parses_LowerBound");
        const auto p8 = parse_sdl3_cli({"--scale", "8"});
        expect(p8.errors.empty() && p8.scale.has_value() && *p8.scale == 8, "Scale8_Parses_UpperBound");
        const auto p0 = parse_sdl3_cli({"--scale", "0"});
        expect(!p0.errors.empty() && !p0.scale.has_value(), "Scale0_OutOfRange_Error");
        const auto p99 = parse_sdl3_cli({"--scale", "99"});
        expect(!p99.errors.empty() && !p99.scale.has_value(), "Scale99_OutOfRange_Error");
        const auto px = parse_sdl3_cli({"--scale", "big"});
        expect(!px.errors.empty() && !px.scale.has_value(), "ScaleNonNumeric_Error");
        const auto pmiss = parse_sdl3_cli({"--scale"});
        expect(!pmiss.errors.empty(), "ScaleMissingArg_Error");
        // Absent --scale stays nullopt (parser contract unchanged). The window
        // default it defers to is now scale 3 / 960x720 (M37 Slice F, by design;
        // the Sdl3AppConfig default is asserted in the scaling integration test).
        const auto pabsent = parse_sdl3_cli({});
        expect(!pabsent.scale.has_value(), "ScaleAbsent_Nullopt_DefaultWindowIsScale3_960x720_M37F");
    }

    // --- Case 12 (M37 Slice E, DEC-0056): --filter <nearest|linear>. Both valid
    // values parse; a bad value records an error; the default (absent) is Linear
    // (the "smooth" look the human asked for). ---
    {
        using sony_msx::frontend::TextureFilter;
        const auto pn = parse_sdl3_cli({"--filter", "nearest"});
        expect(pn.errors.empty() && pn.filter == TextureFilter::Nearest, "FilterNearest_Parses");
        const auto pl = parse_sdl3_cli({"--filter", "linear"});
        expect(pl.errors.empty() && pl.filter == TextureFilter::Linear, "FilterLinear_Parses");
        const auto pbad = parse_sdl3_cli({"--filter", "bilinear"});
        expect(!pbad.errors.empty(), "FilterBadValue_Error");
        const auto pmiss = parse_sdl3_cli({"--filter"});
        expect(!pmiss.errors.empty(), "FilterMissingArg_Error");
        const auto pdefault = parse_sdl3_cli({});
        expect(pdefault.filter == TextureFilter::Linear, "FilterDefault_IsLinear");
    }

    // --- Case 13 (M37 Slice E, DEC-0056): --fullscreen bare boolean flag. ---
    {
        const auto pf = parse_sdl3_cli({"--fullscreen"});
        expect(pf.errors.empty() && pf.fullscreen, "Fullscreen_SetsTrue");
        const auto pabsent = parse_sdl3_cli({});
        expect(!pabsent.fullscreen, "FullscreenAbsent_DefaultsFalse");
    }

    // --- Case 14 (M37): all new flags together, order-independent, alongside
    // pre-existing flags -- the additive-field regression guard (the M27/M36
    // discipline): pre-existing flags stay intact and no spurious errors. ---
    {
        const std::vector<std::string> args{
            "--scale",  "4",       "--speed", "3",     "--filter", "nearest", "--fullscreen",
            "--max-frames", "10",  "--disk",  "d.dsk", "--border",
        };
        const auto p = parse_sdl3_cli(args);
        expect(p.errors.empty(), "M37Combined_NoErrors");
        expect(p.scale.has_value() && *p.scale == 4, "M37Combined_Scale");
        expect(p.speed_level.has_value() && *p.speed_level == 3, "M37Combined_Speed");
        expect(p.filter == sony_msx::frontend::TextureFilter::Nearest, "M37Combined_Filter");
        expect(p.fullscreen, "M37Combined_Fullscreen");
        expect(p.max_frames.has_value() && *p.max_frames == 10 && p.disk_paths.size() == 1 &&
                   p.disk_paths[0] == "d.dsk" && p.border_enabled,
               "M37Combined_PreExistingFlagsIntact_AdditiveRegressionGuard");
    }

    // --- Case 15 (M37 Slice F): --capture <on|off> gates the F10 stream-capture
    // hotkey. `on` -> true, `off` -> false, absent -> false (default OFF, F10
    // inert), a bad value records a loud error (mirrors --filter). Absent stays
    // false as a hard regression guard (default gameplay unchanged). ---
    {
        const auto pon = parse_sdl3_cli({"--capture", "on"});
        expect(pon.errors.empty() && pon.capture_enabled, "CaptureOn_SetsTrue");
        const auto poff = parse_sdl3_cli({"--capture", "off"});
        expect(poff.errors.empty() && !poff.capture_enabled, "CaptureOff_SetsFalse");
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.capture_enabled, "CaptureAbsent_DefaultsFalse");
        const auto pbad = parse_sdl3_cli({"--capture", "yes"});
        expect(!pbad.errors.empty() && !pbad.capture_enabled, "CaptureBadValue_Error_StaysFalse");
        const auto pmiss = parse_sdl3_cli({"--capture"});
        expect(!pmiss.errors.empty() && !pmiss.capture_enabled, "CaptureMissingArg_Error");
        // Order-independent alongside a pre-existing flag; --capture on coexists
        // with --stream-light (which selects light mode once F10 is triggered).
        const auto pcombo = parse_sdl3_cli({"--stream-light", "--capture", "on", "--max-frames", "3"});
        expect(pcombo.errors.empty() && pcombo.capture_enabled && pcombo.stream_light &&
                   pcombo.max_frames.has_value() && *pcombo.max_frames == 3,
               "CaptureOn_WithStreamLight_OrderIndependent");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3Cli_Unit cases passed\n";
    return 0;
}
