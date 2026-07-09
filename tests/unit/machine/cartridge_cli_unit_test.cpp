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
#include <string>
#include <vector>

#include "machine/cartridge_cli.h"

// Suite: Machine_CartridgeCli_Unit (M19-S5, backlog B7)
//
// Pure argv parser (A-M19-4): order-independent --cart1/--cart1-type/
// --cart2/--cart2-type recognition, the A-M19-5 omitted-flag-defaults-to-
// Mirrored contract, and the "unrecognized VALUE is always an error, never
// silently defaulted" contract (contrast A-M19-5). No file I/O, no
// Hbf1xvMachine dependency.

namespace {

using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::machine::parse_cartridge_cli;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Absent --cart1/--cart2 entirely: no per-slot request (status quo). ---
    {
        const auto parsed = parse_cartridge_cli({});
        expect(!parsed.slot1.path.has_value(), "NoFlags_Slot1_NoPathRequest");
        expect(!parsed.slot2.path.has_value(), "NoFlags_Slot2_NoPathRequest");
        expect(parsed.errors.empty(), "NoFlags_NoErrors");
    }
    {
        const auto parsed = parse_cartridge_cli({"--parity-trace", "prog.bin", "C000", "1000", "out.txt"});
        expect(!parsed.slot1.path.has_value() && !parsed.slot2.path.has_value(),
               "UnrelatedFlags_NoCartridgeRequest_ExistingBehaviorUnchanged");
        expect(parsed.errors.empty(), "UnrelatedFlags_NoErrors");
    }

    // --- --cart1 <path> alone defaults to Mirrored (A-M19-5). ---
    {
        const auto parsed = parse_cartridge_cli({"--cart1", "roms/game.rom"});
        expect(parsed.slot1.path.has_value() && *parsed.slot1.path == "roms/game.rom",
               "Cart1Alone_PathRecorded");
        expect(parsed.slot1.type == CartridgeMapperType::Mirrored, "Cart1Alone_DefaultsToMirrored");
        expect(!parsed.slot1.type_was_explicit, "Cart1Alone_TypeNotExplicit");
        expect(parsed.errors.empty(), "Cart1Alone_NoErrors");
        expect(!parsed.slot2.path.has_value(), "Cart1Alone_Slot2Untouched");
    }

    // --- Explicit --cart1-type for all 6 canonical names, case-insensitively,
    //     regardless of flag order. ---
    {
        const struct {
            const char* value;
            CartridgeMapperType expected;
        } cases[] = {
            {"Mirrored", CartridgeMapperType::Mirrored},   {"mirrored", CartridgeMapperType::Mirrored},
            {"8kB", CartridgeMapperType::Generic8kB},       {"8KB", CartridgeMapperType::Generic8kB},
            {"16kB", CartridgeMapperType::Generic16kB},     {"16KB", CartridgeMapperType::Generic16kB},
            {"ASCII8", CartridgeMapperType::Ascii8kB},      {"ascii8", CartridgeMapperType::Ascii8kB},
            {"ASCII16", CartridgeMapperType::Ascii16kB},    {"ascii16", CartridgeMapperType::Ascii16kB},
            {"Konami", CartridgeMapperType::Konami},        {"KONAMI", CartridgeMapperType::Konami},
            // M29 (backlog G1): the KonamiSCC mapper type, additive (QA F1).
            {"KonamiSCC", CartridgeMapperType::KonamiSCC},  {"konamiscc", CartridgeMapperType::KonamiSCC},
        };
        bool all_ok = true;
        for (const auto& c : cases) {
            // Type flag BEFORE the path flag -- order-independence (A-M19-4).
            const auto parsed = parse_cartridge_cli({"--cart1-type", c.value, "--cart1", "x.rom"});
            if (!parsed.slot1.path.has_value() || parsed.slot1.type != c.expected ||
                !parsed.slot1.type_was_explicit || !parsed.errors.empty()) {
                all_ok = false;
            }
        }
        expect(all_ok, "ExplicitCart1Type_AllSixCanonicalNames_CaseInsensitive_OrderIndependent");
    }

    // --- An unrecognized --cart1-type value is a parse error, never silently
    //     defaulted (contrast A-M19-5's omitted-flag default). ---
    {
        const auto parsed = parse_cartridge_cli({"--cart1", "x.rom", "--cart1-type", "NotARealType"});
        expect(!parsed.errors.empty(), "UnrecognizedType_IsParseError");
        expect(parsed.slot1.type == CartridgeMapperType::Mirrored,
               "UnrecognizedType_FieldStaysAtStructDefault_NotSilentlyAcceptedAsExplicit");
        expect(!parsed.slot1.type_was_explicit, "UnrecognizedType_NotMarkedExplicit");
    }

    // --- Both slots specified simultaneously and independently. ---
    {
        const auto parsed = parse_cartridge_cli(
            {"--cart1", "a.rom", "--cart1-type", "Konami", "--cart2", "b.rom", "--cart2-type", "ASCII16"});
        expect(parsed.slot1.path.has_value() && *parsed.slot1.path == "a.rom", "BothSlots_Slot1PathRecorded");
        expect(parsed.slot1.type == CartridgeMapperType::Konami, "BothSlots_Slot1TypeRecorded");
        expect(parsed.slot2.path.has_value() && *parsed.slot2.path == "b.rom", "BothSlots_Slot2PathRecorded");
        expect(parsed.slot2.type == CartridgeMapperType::Ascii16kB, "BothSlots_Slot2TypeRecorded");
        expect(parsed.errors.empty(), "BothSlots_NoErrors");
    }

    // --- --cart2 alone, no type flag -> Mirrored default, slot1 untouched. ---
    {
        const auto parsed = parse_cartridge_cli({"--cart2", "b.rom"});
        expect(parsed.slot2.path.has_value() && *parsed.slot2.path == "b.rom", "Cart2Alone_PathRecorded");
        expect(parsed.slot2.type == CartridgeMapperType::Mirrored, "Cart2Alone_DefaultsToMirrored");
        expect(!parsed.slot1.path.has_value(), "Cart2Alone_Slot1Untouched");
    }

    // --- A flag with no following value is a parse error (never crashes / never
    //     silently consumes an unrelated later argument). ---
    {
        const auto parsed = parse_cartridge_cli({"--cart1"});
        expect(!parsed.errors.empty(), "DanglingFlag_NoValue_IsParseError");
        expect(!parsed.slot1.path.has_value(), "DanglingFlag_NoValue_PathNotSet");
    }

    // =====================================================================
    // M30 additive rows (backlog G2, docs/m30-planner-package.md §2.4/§4-S4).
    // Every assertion ABOVE this line is the unmodified pre-M30 suite -- the
    // parser's Mirrored default and type_was_explicit semantics are
    // unchanged (planner Acceptance Criterion 5).
    // =====================================================================

    // --- --softwaredb <path> recorded; absent flag -> std::nullopt. ---
    {
        const auto parsed = parse_cartridge_cli({"--softwaredb", "my/softwaredb.xml"});
        expect(parsed.softwaredb_path.has_value() && *parsed.softwaredb_path == "my/softwaredb.xml",
               "SoftwareDb_PathRecorded");
        expect(parsed.errors.empty(), "SoftwareDb_NoErrors");
    }
    {
        const auto parsed = parse_cartridge_cli({"--cart1", "x.rom"});
        expect(!parsed.softwaredb_path.has_value(), "SoftwareDb_AbsentFlag_Nullopt");
    }
    {
        const auto parsed = parse_cartridge_cli({"--softwaredb"});
        expect(!parsed.errors.empty(), "SoftwareDb_DanglingFlag_IsParseError");
        expect(!parsed.softwaredb_path.has_value(), "SoftwareDb_DanglingFlag_PathNotSet");
    }

    // --- --cartN-type auto == "as if omitted" (type_was_explicit=false,
    //     Mirrored struct default), case-insensitively; never an error. ---
    {
        const auto parsed = parse_cartridge_cli({"--cart1", "x.rom", "--cart1-type", "auto"});
        expect(parsed.errors.empty(), "AutoType_NotAParseError");
        expect(parsed.slot1.type == CartridgeMapperType::Mirrored, "AutoType_FieldStaysAtStructDefault");
        expect(!parsed.slot1.type_was_explicit, "AutoType_NotMarkedExplicit");
    }
    {
        const auto parsed = parse_cartridge_cli({"--cart2", "y.rom", "--cart2-type", "AUTO"});
        expect(parsed.errors.empty() && !parsed.slot2.type_was_explicit &&
                   parsed.slot2.type == CartridgeMapperType::Mirrored,
               "AutoType_CaseInsensitive_Slot2");
    }
    {
        // Last occurrence wins (the parser's existing repeated-flag rule):
        // an explicit type followed by `auto` reverts to auto-identify.
        const auto parsed = parse_cartridge_cli(
            {"--cart1", "x.rom", "--cart1-type", "Konami", "--cart1-type", "auto"});
        expect(parsed.errors.empty() && !parsed.slot1.type_was_explicit &&
                   parsed.slot1.type == CartridgeMapperType::Mirrored,
               "AutoType_AfterExplicit_LastWins_RevertsToAuto");
    }
    {
        // And the reverse: `auto` followed by an explicit type is explicit.
        const auto parsed = parse_cartridge_cli(
            {"--cart1", "x.rom", "--cart1-type", "auto", "--cart1-type", "KonamiSCC"});
        expect(parsed.errors.empty() && parsed.slot1.type_was_explicit &&
                   parsed.slot1.type == CartridgeMapperType::KonamiSCC,
               "AutoType_ThenExplicit_LastWins_Explicit");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_CartridgeCli_Unit cases passed\n";
    return 0;
}
