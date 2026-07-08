#include "frontend/sdl3_cli.h"

#include <charconv>

namespace sony_msx::frontend {

namespace {

std::optional<std::string> take_value(const std::vector<std::string>& args, const std::size_t i,
                                       const char* flag_name, std::vector<std::string>& errors) {
    if (i + 1 >= args.size()) {
        errors.push_back(std::string("sdl3_cli: ") + flag_name + " requires a value argument");
        return std::nullopt;
    }
    return args[i + 1];
}

}  // namespace

ParsedSdl3Cli parse_sdl3_cli(const std::vector<std::string>& args) {
    ParsedSdl3Cli parsed;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--bios-dir") {
            if (auto value = take_value(args, i, "--bios-dir", parsed.errors)) {
                parsed.bios_dir = *value;
                ++i;
            }
        } else if (arg == "--disk") {
            if (auto value = take_value(args, i, "--disk", parsed.errors)) {
                parsed.disk_path = *value;
                ++i;
            }
        } else if (arg == "--max-frames") {
            if (auto value = take_value(args, i, "--max-frames", parsed.errors)) {
                ++i;
                std::uint32_t frames = 0;
                const auto begin = value->data();
                const auto end = value->data() + value->size();
                const auto [ptr, ec] = std::from_chars(begin, end, frames);
                if (ec != std::errc{} || ptr != end) {
                    parsed.errors.push_back("sdl3_cli: --max-frames value is not a valid non-negative integer: '" +
                                             *value + "'");
                } else {
                    parsed.max_frames = frames;
                }
            }
        } else if (arg == "--hidden-window") {
            parsed.hidden_window = true;
        }
        // Any other argument is not this parser's concern -- left untouched
        // for the cartridge-flag delegation below (order-independent
        // scanning, mirrors cartridge_cli's A-M19-4).
    }

    parsed.cartridges = machine::parse_cartridge_cli(args);
    for (std::string& err : parsed.cartridges.errors) {
        parsed.errors.push_back(err);
    }

    return parsed;
}

}  // namespace sony_msx::frontend
