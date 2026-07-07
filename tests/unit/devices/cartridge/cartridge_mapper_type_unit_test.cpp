#include <iostream>

#include "devices/cartridge/cartridge_mapper_type.h"

// Suite: Devices_CartridgeMapperType_Unit (M19-S1, backlog B7)
//
// Name<->enum round-trip for all 6 canonical openMSX strings (A-M19-3,
// references/openmsx-21.0/src/memory/RomInfo.cc:19-20,23,26-27,92),
// case-insensitivity, and the "unrecognized name -> nullopt, never a silent
// default" contract (A-M19-5 only applies to an OMITTED flag, not an
// unrecognized value).

namespace {

using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::parse_cartridge_mapper_type;
using sony_msx::devices::cartridge::to_string;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Canonical name -> enum, exact case. ---
    expect(parse_cartridge_mapper_type("Mirrored") == CartridgeMapperType::Mirrored,
           "Parse_Mirrored_ExactCase");
    expect(parse_cartridge_mapper_type("8kB") == CartridgeMapperType::Generic8kB, "Parse_8kB_ExactCase");
    expect(parse_cartridge_mapper_type("16kB") == CartridgeMapperType::Generic16kB, "Parse_16kB_ExactCase");
    expect(parse_cartridge_mapper_type("ASCII8") == CartridgeMapperType::Ascii8kB, "Parse_ASCII8_ExactCase");
    expect(parse_cartridge_mapper_type("ASCII16") == CartridgeMapperType::Ascii16kB, "Parse_ASCII16_ExactCase");
    expect(parse_cartridge_mapper_type("Konami") == CartridgeMapperType::Konami, "Parse_Konami_ExactCase");

    // --- Case-insensitivity. ---
    expect(parse_cartridge_mapper_type("mirrored") == CartridgeMapperType::Mirrored, "Parse_Mirrored_Lowercase");
    expect(parse_cartridge_mapper_type("MIRRORED") == CartridgeMapperType::Mirrored, "Parse_Mirrored_Uppercase");
    expect(parse_cartridge_mapper_type("8KB") == CartridgeMapperType::Generic8kB, "Parse_8kB_Uppercase");
    expect(parse_cartridge_mapper_type("ascii8") == CartridgeMapperType::Ascii8kB, "Parse_Ascii8_Lowercase");
    expect(parse_cartridge_mapper_type("ascii16") == CartridgeMapperType::Ascii16kB, "Parse_Ascii16_Lowercase");
    expect(parse_cartridge_mapper_type("konami") == CartridgeMapperType::Konami, "Parse_Konami_Lowercase");
    expect(parse_cartridge_mapper_type("KONAMI") == CartridgeMapperType::Konami, "Parse_Konami_Uppercase");

    // --- Unrecognized name -> nullopt, never a silent default. ---
    expect(!parse_cartridge_mapper_type("NotARealType").has_value(), "Parse_UnrecognizedName_Nullopt");
    expect(!parse_cartridge_mapper_type("").has_value(), "Parse_EmptyString_Nullopt");
    expect(!parse_cartridge_mapper_type("KonamiSCC").has_value(),
           "Parse_KonamiScc_Nullopt_OutOfScopeMapper");  // G1, deliberately not an MVP value

    // --- to_string round-trip (enum -> canonical string -> enum). ---
    const CartridgeMapperType all_types[] = {
        CartridgeMapperType::Mirrored,   CartridgeMapperType::Generic8kB,  CartridgeMapperType::Generic16kB,
        CartridgeMapperType::Ascii8kB,   CartridgeMapperType::Ascii16kB,   CartridgeMapperType::Konami,
    };
    bool round_trip_ok = true;
    for (const CartridgeMapperType type : all_types) {
        const std::string_view name = to_string(type);
        const auto parsed = parse_cartridge_mapper_type(name);
        if (!parsed.has_value() || *parsed != type) {
            round_trip_ok = false;
        }
    }
    expect(round_trip_ok, "ToString_RoundTrip_AllSixTypes");

    // --- Exact canonical strings (openMSX's own labels, RomInfo.cc). ---
    expect(to_string(CartridgeMapperType::Mirrored) == "Mirrored", "ToString_Mirrored_ExactLabel");
    expect(to_string(CartridgeMapperType::Generic8kB) == "8kB", "ToString_Generic8kB_ExactLabel");
    expect(to_string(CartridgeMapperType::Generic16kB) == "16kB", "ToString_Generic16kB_ExactLabel");
    expect(to_string(CartridgeMapperType::Ascii8kB) == "ASCII8", "ToString_Ascii8kB_ExactLabel");
    expect(to_string(CartridgeMapperType::Ascii16kB) == "ASCII16", "ToString_Ascii16kB_ExactLabel");
    expect(to_string(CartridgeMapperType::Konami) == "Konami", "ToString_Konami_ExactLabel");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeMapperType_Unit cases passed\n";
    return 0;
}
