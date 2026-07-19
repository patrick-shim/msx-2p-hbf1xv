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
        // M39-D (oracle RE-DERIVED, old->new: true->false): the DEFAULT present
        // reverts to the bare edge-to-edge active area -- the Sony-original look
        // -- per the human's explicit stated preference. 7fac03d (M39-B) had made
        // the openMSX-matching framed canvas the default (border_enabled==true);
        // this reverts to the original correct value. NOT a weakening: it is a
        // deliberate preference revert to the human-chosen default, with --border
        // restored as the active opt-in to the framed canvas (docs/m39-fix-plans.md).
        expect(!parsed.border_enabled, "NoFlags_BorderDefaultsFalse_BareEdgeToEdgeSonyOriginal");
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

    // --- Case 6: --border is a bare boolean flag; M39-D it is the active
    // OPT-IN to the openMSX-matching framed canvas (the default is now the bare
    // edge-to-edge present). --no-border is the explicit off (matches the
    // default); order-independent and last-wins on the linear scan. ---
    {
        const auto parsed = parse_sdl3_cli({"--border"});
        expect(parsed.errors.empty(), "BorderFlag_NoErrors");
        expect(parsed.border_enabled, "BorderFlag_OptsInToFramedCanvas_SetsTrue");
    }
    {
        const auto parsed = parse_sdl3_cli({"--no-border"});
        expect(parsed.errors.empty(), "NoBorderFlag_NoErrors");
        expect(!parsed.border_enabled, "NoBorderFlag_SetsFalse_BareActiveAreaPresentation");
    }
    {
        // Last-wins: --border then --no-border -> false; --no-border then
        // --border -> true (plain linear scan, mirrors the other boolean flags).
        expect(!parse_sdl3_cli({"--border", "--no-border"}).border_enabled,
               "BorderThenNoBorder_LastWins_False");
        expect(parse_sdl3_cli({"--no-border", "--border"}).border_enabled,
               "NoBorderThenBorder_LastWins_True");
    }
    {
        const auto parsed = parse_sdl3_cli({"--max-frames", "5", "--no-border", "--hidden-window"});
        expect(parsed.errors.empty(), "NoBorderFlag_WithOtherFlags_NoErrors");
        expect(!parsed.border_enabled && parsed.hidden_window && parsed.max_frames.has_value() &&
                   *parsed.max_frames == 5,
               "NoBorderFlag_OrderIndependent_AlongsideOtherFlags");
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

    // --- Case 16 (M42, DEC-0061): --ram <64|128|256|512> main-RAM size (KB).
    // Each enum value parses into ram_kb; any other value (0, 96, 1024,
    // non-numeric) records a loud error and leaves ram_kb absent (mirrors the
    // --speed error policy). Absent -> nullopt (hard regression guard: stock
    // 64 KB is applied downstream by sdl3_main.cpp). ---
    {
        const auto p64 = parse_sdl3_cli({"--ram", "64"});
        expect(p64.errors.empty() && p64.ram_kb.has_value() && *p64.ram_kb == 64, "Ram64_Parses_Stock");
        const auto p128 = parse_sdl3_cli({"--ram", "128"});
        expect(p128.errors.empty() && p128.ram_kb.has_value() && *p128.ram_kb == 128, "Ram128_Parses");
        const auto p256 = parse_sdl3_cli({"--ram", "256"});
        expect(p256.errors.empty() && p256.ram_kb.has_value() && *p256.ram_kb == 256, "Ram256_Parses");
        const auto p512 = parse_sdl3_cli({"--ram", "512"});
        expect(p512.errors.empty() && p512.ram_kb.has_value() && *p512.ram_kb == 512, "Ram512_Parses");
        // Rejected values: not in the enum.
        const auto p1024 = parse_sdl3_cli({"--ram", "1024"});
        expect(!p1024.errors.empty() && !p1024.ram_kb.has_value(), "Ram1024_AboveCeiling_Error");
        const auto p96 = parse_sdl3_cli({"--ram", "96"});
        expect(!p96.errors.empty() && !p96.ram_kb.has_value(), "Ram96_NonEnum_Error");
        const auto p0 = parse_sdl3_cli({"--ram", "0"});
        expect(!p0.errors.empty() && !p0.ram_kb.has_value(), "Ram0_Error");
        const auto pbig = parse_sdl3_cli({"--ram", "big"});
        expect(!pbig.errors.empty() && !pbig.ram_kb.has_value(), "RamNonNumeric_Error");
        const auto pmiss = parse_sdl3_cli({"--ram"});
        expect(!pmiss.errors.empty() && !pmiss.ram_kb.has_value(), "RamMissingArg_Error");
        // Absent -> nullopt (downstream default = stock 64 KB).
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.ram_kb.has_value(), "RamAbsent_Nullopt_Stock64");
        // Order-independent alongside a pre-existing flag.
        const auto pcombo = parse_sdl3_cli({"--max-frames", "3", "--ram", "256", "--hidden-window"});
        expect(pcombo.errors.empty() && pcombo.ram_kb.has_value() && *pcombo.ram_kb == 256 &&
                   pcombo.max_frames.has_value() && *pcombo.max_frames == 3 && pcombo.hidden_window,
               "Ram256_OrderIndependent_WithOtherFlags");
    }

    // --- Case 16b: parse_ram_kb is the SHARED validator the headless
    // src/main.cpp --ram path also uses (ONE enum policy across both exes).
    // Exercise it directly so the headless path's validation is covered too
    // (mirrors Case 10b for --speed). ---
    {
        using sony_msx::frontend::parse_ram_kb;
        std::vector<std::string> errs;
        int kb = -99;
        expect(parse_ram_kb("64", kb, errs, "main") && kb == 64, "SharedRam_64_OK");
        expect(parse_ram_kb("128", kb, errs, "main") && kb == 128, "SharedRam_128_OK");
        expect(parse_ram_kb("256", kb, errs, "main") && kb == 256, "SharedRam_256_OK");
        expect(parse_ram_kb("512", kb, errs, "main") && kb == 512, "SharedRam_512_OK");
        expect(errs.empty(), "SharedRam_NoErrorsForValid");
        expect(!parse_ram_kb("1024", kb, errs, "main"), "SharedRam_1024_Rejected");
        expect(!parse_ram_kb("96", kb, errs, "main"), "SharedRam_96_Rejected");
        expect(!parse_ram_kb("x", kb, errs, "main"), "SharedRam_NonNumeric_Rejected");
        expect(!errs.empty(), "SharedRam_ErrorsRecordedForInvalid");
    }

    // =====================================================================
    // M46 (DEC-0071) additive cases: --fast-disk/--no-fast-disk tri-state,
    // --no-fmpac, --stock parser fields + the resolve_session_defaults()
    // convenience-vs-stock resolver. Every case ABOVE stays valid: the parser
    // still reports raw intent (fast_disk_opt tri-state, no_fmpac, stock).
    // =====================================================================

    // --- Case 17: --fast-disk / --no-fast-disk are TRI-STATE (nullopt = the
    // user did not say; the DEFAULT lives in the resolver, not this field). ---
    {
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.fast_disk_opt.has_value(),
               "FastDiskAbsent_TriStateNullopt");
        const auto pon = parse_sdl3_cli({"--fast-disk"});
        expect(pon.fast_disk_opt.has_value() && *pon.fast_disk_opt, "FastDisk_ExplicitOn");
        const auto poff = parse_sdl3_cli({"--no-fast-disk"});
        expect(poff.fast_disk_opt.has_value() && !*poff.fast_disk_opt, "NoFastDisk_ExplicitOff");
        // Last-wins on a linear scan (mirrors --border/--no-border).
        expect(*parse_sdl3_cli({"--fast-disk", "--no-fast-disk"}).fast_disk_opt == false,
               "FastDisk_ThenNo_LastWinsOff");
        expect(*parse_sdl3_cli({"--no-fast-disk", "--fast-disk"}).fast_disk_opt == true,
               "NoFastDisk_ThenYes_LastWinsOn");
    }

    // --- Case 18: --no-fmpac and --stock are bare booleans; absent -> false. ---
    {
        const auto pabsent = parse_sdl3_cli({});
        expect(!pabsent.no_fmpac && !pabsent.stock, "NoFmpacStock_Absent_False");
        const auto pnf = parse_sdl3_cli({"--no-fmpac"});
        expect(pnf.no_fmpac && !pnf.stock && pnf.errors.empty(), "NoFmpac_SetsTrue");
        const auto ps = parse_sdl3_cli({"--stock"});
        expect(ps.stock && !ps.no_fmpac && ps.errors.empty(), "Stock_SetsTrue");
    }

    // --- Case 19 (AC-1): the resolver with EMPTY intent yields the convenience
    // defaults {512 KB, fast-disk ON, FM-PAC auto-load ON}. Non-tautology: this
    // asserts CONCRETE values, not that the resolver echoes its input. ---
    {
        using sony_msx::frontend::resolve_session_defaults;
        const auto r = resolve_session_defaults({});
        expect(r.ram_bytes == 512u * 1024u, "Resolver_Empty_Ram512");
        expect(r.fast_disk, "Resolver_Empty_FastDiskOn");
        expect(r.fmpac_autoload, "Resolver_Empty_FmpacAutoloadOn");
        expect(!r.is_stock, "Resolver_Empty_NotStock");
    }

    // --- Case 20 (AC-2): --ram 64 and --stock each yield 64 KB (byte-identical
    // to the old default); --stock also forces fast-disk OFF + no FM-PAC. ---
    {
        using sony_msx::frontend::resolve_session_defaults;
        auto r64 = resolve_session_defaults({/*ram_kb=*/64, {}, false, false});
        expect(r64.ram_bytes == 64u * 1024u && r64.fast_disk && r64.fmpac_autoload,
               "Resolver_Ram64_Only_RamStock_ButConvenienceRest");
        auto rs = resolve_session_defaults({{}, {}, false, /*stock=*/true});
        expect(rs.ram_bytes == 64u * 1024u, "Resolver_Stock_Ram64");
        expect(!rs.fast_disk, "Resolver_Stock_FastDiskOff");
        expect(!rs.fmpac_autoload, "Resolver_Stock_NoFmpac");
        expect(rs.is_stock, "Resolver_Stock_IsStockLabel");
    }

    // --- Case 21 (AC-3): --stock precedence = specificity, NOT argv order.
    // The parser is order-independent, so both {ram 512, stock} orderings feed
    // the resolver identical intent -> identical outcome (512 KB). --fast-disk
    // overrides --stock's fast-disk OFF. ---
    {
        using sony_msx::frontend::resolve_session_defaults;
        // Both spellings parse to the same intent, so resolve once from each.
        const auto p_a = parse_sdl3_cli({"--stock", "--ram", "512"});
        const auto p_b = parse_sdl3_cli({"--ram", "512", "--stock"});
        const auto r_a = resolve_session_defaults({p_a.ram_kb, p_a.fast_disk_opt, p_a.no_fmpac, p_a.stock});
        const auto r_b = resolve_session_defaults({p_b.ram_kb, p_b.fast_disk_opt, p_b.no_fmpac, p_b.stock});
        expect(r_a.ram_bytes == 512u * 1024u, "Resolver_StockThenRam512_Is512");
        expect(r_b.ram_bytes == 512u * 1024u, "Resolver_Ram512ThenStock_Is512_OrderIndependent");
        expect(r_a.ram_bytes == r_b.ram_bytes, "Resolver_StockRamOrder_Independent");
        // --stock --fast-disk -> fast-disk ON (explicit per-field wins).
        const auto p_c = parse_sdl3_cli({"--stock", "--fast-disk"});
        const auto r_c = resolve_session_defaults({p_c.ram_kb, p_c.fast_disk_opt, p_c.no_fmpac, p_c.stock});
        expect(r_c.ram_bytes == 64u * 1024u && r_c.fast_disk && !r_c.fmpac_autoload,
               "Resolver_StockFastDisk_Ram64_FastOn_NoFmpac");
    }

    // --- Case 22 (AC-4): fast-disk default ON; explicit flags flip it. ---
    {
        using sony_msx::frontend::resolve_session_defaults;
        expect(resolve_session_defaults({}).fast_disk, "Resolver_FastDiskDefaultOn");
        expect(!resolve_session_defaults({{}, /*fast_disk_opt=*/false, false, false}).fast_disk,
               "Resolver_NoFastDisk_Off");
        expect(resolve_session_defaults({{}, /*fast_disk_opt=*/true, false, false}).fast_disk,
               "Resolver_FastDisk_On");
        // --no-fmpac only suppresses FM-PAC, not RAM/fast-disk.
        const auto rnf = resolve_session_defaults({{}, {}, /*no_fmpac=*/true, false});
        expect(rnf.ram_bytes == 512u * 1024u && rnf.fast_disk && !rnf.fmpac_autoload,
               "Resolver_NoFmpac_OnlySuppressesFmpac");
    }

    // --- Case 23 (phosphor persistence): --persistence <0..100> parses into the
    // optional; boundaries 0 and 100 are valid; out-of-range (-1, 101),
    // non-numeric, and missing-value each record a loud error and leave
    // persistence absent (mirrors the --scale error policy). Absent -> nullopt
    // (a hard regression guard: sdl3_main.cpp leaves the config default 0/OFF,
    // so the present path is byte-identical). ---
    {
        const auto p0 = parse_sdl3_cli({"--persistence", "0"});
        expect(p0.errors.empty() && p0.persistence.has_value() && *p0.persistence == 0,
               "Persistence0_Parses_LowerBound");
        const auto p50 = parse_sdl3_cli({"--persistence", "50"});
        expect(p50.errors.empty() && p50.persistence.has_value() && *p50.persistence == 50,
               "Persistence50_Parses");
        const auto p100 = parse_sdl3_cli({"--persistence", "100"});
        expect(p100.errors.empty() && p100.persistence.has_value() && *p100.persistence == 100,
               "Persistence100_Parses_UpperBound");
        const auto p101 = parse_sdl3_cli({"--persistence", "101"});
        expect(!p101.errors.empty() && !p101.persistence.has_value(), "Persistence101_OutOfRange_Error");
        const auto pneg = parse_sdl3_cli({"--persistence", "-1"});
        expect(!pneg.errors.empty() && !pneg.persistence.has_value(), "PersistenceNeg1_OutOfRange_Error");
        const auto pbad = parse_sdl3_cli({"--persistence", "half"});
        expect(!pbad.errors.empty() && !pbad.persistence.has_value(), "PersistenceNonNumeric_Error");
        const auto pmiss = parse_sdl3_cli({"--persistence"});
        expect(!pmiss.errors.empty() && !pmiss.persistence.has_value(), "PersistenceMissingArg_Error");
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.persistence.has_value(),
               "PersistenceAbsent_Nullopt_DefaultOff");
        // Order-independent alongside pre-existing flags.
        const auto pcombo = parse_sdl3_cli({"--scale", "4", "--persistence", "75", "--hidden-window"});
        expect(pcombo.errors.empty() && pcombo.persistence.has_value() && *pcombo.persistence == 75 &&
                   pcombo.scale.has_value() && *pcombo.scale == 4 && pcombo.hidden_window,
               "Persistence75_OrderIndependent_WithOtherFlags");
    }

    // --- Case 24 (phosphor mode): --persistence-mode <avg|peak> sets the enum;
    // 'average' is accepted as an alias of 'avg'; ABSENT defaults to Average (a
    // hard regression guard -- no flag keeps the byte-identical original blend);
    // an unrecognized value and a missing value each record a loud error and
    // leave the mode at the Average default (mirrors --filter's value policy). ---
    {
        using sony_msx::frontend::PhosphorMode;
        const auto absent = parse_sdl3_cli({});
        expect(absent.errors.empty() && absent.persistence_mode == PhosphorMode::Average,
               "PersistenceModeAbsent_DefaultsAverage");
        const auto avg = parse_sdl3_cli({"--persistence-mode", "avg"});
        expect(avg.errors.empty() && avg.persistence_mode == PhosphorMode::Average, "PersistenceModeAvg_Parses");
        const auto avg2 = parse_sdl3_cli({"--persistence-mode", "average"});
        expect(avg2.errors.empty() && avg2.persistence_mode == PhosphorMode::Average,
               "PersistenceModeAverageAlias_Parses");
        const auto peak = parse_sdl3_cli({"--persistence-mode", "peak"});
        expect(peak.errors.empty() && peak.persistence_mode == PhosphorMode::Peak, "PersistenceModePeak_Parses");
        const auto bad = parse_sdl3_cli({"--persistence-mode", "glow"});
        expect(!bad.errors.empty() && bad.persistence_mode == PhosphorMode::Average,
               "PersistenceModeBadValue_Error_StaysAverage");
        const auto miss = parse_sdl3_cli({"--persistence-mode"});
        expect(!miss.errors.empty() && miss.persistence_mode == PhosphorMode::Average,
               "PersistenceModeMissingArg_Error");
        // Order-independent alongside --persistence and other flags.
        const auto combo = parse_sdl3_cli({"--persistence", "50", "--persistence-mode", "peak", "--scale", "4"});
        expect(combo.errors.empty() && combo.persistence.has_value() && *combo.persistence == 50 &&
                   combo.persistence_mode == PhosphorMode::Peak && combo.scale.has_value() && *combo.scale == 4,
               "PersistenceModePeak_OrderIndependent_WithPersistenceAndScale");
    }

    // --- Case 25 (M52, DEC-0079): --volume <0..100> master gain percent. Valid
    // boundaries 0/100 and a mid value parse into the optional; out-of-range
    // (-1, 101), non-numeric, and missing-value each record a loud error and
    // leave volume absent (mirrors --persistence). Absent -> nullopt (resolver
    // falls back to XML then default 100 -- byte-identical to pre-M52). ---
    {
        const auto p0 = parse_sdl3_cli({"--volume", "0"});
        expect(p0.errors.empty() && p0.volume.has_value() && *p0.volume == 0,
               "Volume0_Parses_LowerBound");
        const auto p50 = parse_sdl3_cli({"--volume", "50"});
        expect(p50.errors.empty() && p50.volume.has_value() && *p50.volume == 50, "Volume50_Parses");
        const auto p100 = parse_sdl3_cli({"--volume", "100"});
        expect(p100.errors.empty() && p100.volume.has_value() && *p100.volume == 100,
               "Volume100_Parses_UpperBound");
        const auto p101 = parse_sdl3_cli({"--volume", "101"});
        expect(!p101.errors.empty() && !p101.volume.has_value(), "Volume101_OutOfRange_Error");
        const auto pneg = parse_sdl3_cli({"--volume", "-1"});
        expect(!pneg.errors.empty() && !pneg.volume.has_value(), "VolumeNeg1_OutOfRange_Error");
        const auto pbad = parse_sdl3_cli({"--volume", "loud"});
        expect(!pbad.errors.empty() && !pbad.volume.has_value(), "VolumeNonNumeric_Error");
        const auto pmiss = parse_sdl3_cli({"--volume"});
        expect(!pmiss.errors.empty() && !pmiss.volume.has_value(), "VolumeMissingArg_Error");
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.volume.has_value(), "VolumeAbsent_Nullopt_Default100");
        // Order-independent alongside pre-existing flags.
        const auto pcombo = parse_sdl3_cli({"--scale", "4", "--volume", "70", "--hidden-window"});
        expect(pcombo.errors.empty() && pcombo.volume.has_value() && *pcombo.volume == 70 &&
                   pcombo.scale.has_value() && *pcombo.scale == 4 && pcombo.hidden_window,
               "Volume70_OrderIndependent_WithOtherFlags");
    }

    // --- Case 26 (M52, DEC-0079): --disk-writable / --no-disk-writable both set
    // disk_writable_specified so the resolver honors the explicit CLI choice over
    // the (now ON) XML/built-in default. --disk-writable -> true, --no-disk-writable
    // -> false; last-wins on the linear scan (mirrors --border/--no-border). Absent
    // leaves disk_writable false + UNspecified (raw parser intent -- the default
    // flip lives in config resolution, not this field). ---
    {
        const auto pabsent = parse_sdl3_cli({});
        expect(pabsent.errors.empty() && !pabsent.disk_writable && !pabsent.disk_writable_specified,
               "DiskWritableAbsent_FalseUnspecified");
        const auto pon = parse_sdl3_cli({"--disk-writable"});
        expect(pon.disk_writable && pon.disk_writable_specified, "DiskWritable_ExplicitOn_Specified");
        const auto poff = parse_sdl3_cli({"--no-disk-writable"});
        expect(!poff.disk_writable && poff.disk_writable_specified, "NoDiskWritable_ExplicitOff_Specified");
        // Last-wins on a linear scan.
        const auto pw_no = parse_sdl3_cli({"--disk-writable", "--no-disk-writable"});
        expect(!pw_no.disk_writable && pw_no.disk_writable_specified, "DiskWritable_ThenNo_LastWinsOff");
        const auto pno_w = parse_sdl3_cli({"--no-disk-writable", "--disk-writable"});
        expect(pno_w.disk_writable && pno_w.disk_writable_specified, "NoDiskWritable_ThenYes_LastWinsOn");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3Cli_Unit cases passed\n";
    return 0;
}
