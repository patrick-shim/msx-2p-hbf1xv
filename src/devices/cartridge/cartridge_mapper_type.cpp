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

#include "devices/cartridge/cartridge_mapper_type.h"

#include <algorithm>
#include <cctype>

namespace sony_msx::devices::cartridge {

namespace {

bool iequals(const std::string_view a, const std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    return std::equal(a.begin(), a.end(), b.begin(), [](const char x, const char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

}  // namespace

std::optional<CartridgeMapperType> parse_cartridge_mapper_type(const std::string_view name) {
    if (iequals(name, "Mirrored")) return CartridgeMapperType::Mirrored;
    if (iequals(name, "8kB")) return CartridgeMapperType::Generic8kB;
    if (iequals(name, "16kB")) return CartridgeMapperType::Generic16kB;
    if (iequals(name, "ASCII8")) return CartridgeMapperType::Ascii8kB;
    if (iequals(name, "ASCII16")) return CartridgeMapperType::Ascii16kB;
    if (iequals(name, "Konami")) return CartridgeMapperType::Konami;
    // M29 (backlog G1): canonical openMSX name, RomInfo.cc:24. Checked AFTER
    // the exact "Konami" compare (iequals requires equal length, so there is
    // no prefix ambiguity either way; order kept for readability).
    if (iequals(name, "KonamiSCC")) return CartridgeMapperType::KonamiSCC;
    return std::nullopt;
}

std::string_view to_string(const CartridgeMapperType type) {
    switch (type) {
        case CartridgeMapperType::Mirrored: return "Mirrored";
        case CartridgeMapperType::Generic8kB: return "8kB";
        case CartridgeMapperType::Generic16kB: return "16kB";
        case CartridgeMapperType::Ascii8kB: return "ASCII8";
        case CartridgeMapperType::Ascii16kB: return "ASCII16";
        case CartridgeMapperType::Konami: return "Konami";
        case CartridgeMapperType::KonamiSCC: return "KonamiSCC";
    }
    return "Mirrored";  // unreachable for a valid enumerator; keeps -Wreturn-type quiet
}

}  // namespace sony_msx::devices::cartridge
