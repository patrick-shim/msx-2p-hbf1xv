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
    }
    return "Mirrored";  // unreachable for a valid enumerator; keeps -Wreturn-type quiet
}

}  // namespace sony_msx::devices::cartridge
