#include "machine/cartridge_cli.h"

#include <cctype>

namespace sony_msx::machine {

namespace {

using devices::cartridge::parse_cartridge_mapper_type;

// Case-insensitive match for the M30 `auto` type value (mirrors the enum
// parser's own case-insensitivity for the canonical names).
bool is_auto_value(const std::string& value) {
    if (value.size() != 4) {
        return false;
    }
    const char* expected = "auto";
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != expected[i]) {
            return false;
        }
    }
    return true;
}

// Consumes `args[i]` (a recognized flag) plus its following value argument
// (`args[i+1]`) if present; returns the value, or records `error_no_value`
// and returns std::nullopt when the flag is the last argument (no value
// follows). Either way the caller should advance past the flag itself; the
// value (if consumed) is skipped by the caller's loop increment.
std::optional<std::string> take_value(const std::vector<std::string>& args, const std::size_t i,
                                       const char* flag_name, std::vector<std::string>& errors) {
    if (i + 1 >= args.size()) {
        errors.push_back(std::string("cartridge_cli: ") + flag_name + " requires a value argument");
        return std::nullopt;
    }
    return args[i + 1];
}

}  // namespace

ParsedCartridgeCli parse_cartridge_cli(const std::vector<std::string>& args) {
    ParsedCartridgeCli parsed;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--cart1") {
            if (auto value = take_value(args, i, "--cart1", parsed.errors)) {
                parsed.slot1.path = *value;
                ++i;
            }
        } else if (arg == "--cart2") {
            if (auto value = take_value(args, i, "--cart2", parsed.errors)) {
                parsed.slot2.path = *value;
                ++i;
            }
        } else if (arg == "--cart1-type") {
            if (auto value = take_value(args, i, "--cart1-type", parsed.errors)) {
                ++i;
                if (is_auto_value(*value)) {
                    // M30: `auto` == "as if the flag were omitted" (last
                    // occurrence wins, like every repeated flag here).
                    parsed.slot1.type = devices::cartridge::CartridgeMapperType::Mirrored;
                    parsed.slot1.type_was_explicit = false;
                } else if (const auto type = parse_cartridge_mapper_type(*value)) {
                    parsed.slot1.type = *type;
                    parsed.slot1.type_was_explicit = true;
                } else {
                    parsed.errors.push_back("cartridge_cli: unrecognized --cart1-type value: '" + *value + "'");
                }
            }
        } else if (arg == "--cart2-type") {
            if (auto value = take_value(args, i, "--cart2-type", parsed.errors)) {
                ++i;
                if (is_auto_value(*value)) {
                    parsed.slot2.type = devices::cartridge::CartridgeMapperType::Mirrored;
                    parsed.slot2.type_was_explicit = false;
                } else if (const auto type = parse_cartridge_mapper_type(*value)) {
                    parsed.slot2.type = *type;
                    parsed.slot2.type_was_explicit = true;
                } else {
                    parsed.errors.push_back("cartridge_cli: unrecognized --cart2-type value: '" + *value + "'");
                }
            }
        } else if (arg == "--softwaredb") {
            // M30 (backlog G2): identification-database override.
            if (auto value = take_value(args, i, "--softwaredb", parsed.errors)) {
                parsed.softwaredb_path = *value;
                ++i;
            }
        }
        // Any other argument is not this parser's concern (mode flags,
        // positional parity-trace args, etc.) -- left untouched, order-
        // independent scanning (A-M19-4).
    }

    return parsed;
}

}  // namespace sony_msx::machine
