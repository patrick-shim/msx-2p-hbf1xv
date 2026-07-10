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
                parsed.disk_paths.push_back(*value);  // M35-S1: accumulate repeatable --disk
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
        } else if (arg == "--border") {
            parsed.border_enabled = true;
        } else if (arg == "--disk-writable") {
            parsed.disk_writable = true;  // M36-S-c: opt-in disk-save persistence
        } else if (arg == "--dump-state") {
            if (auto value = take_value(args, i, "--dump-state", parsed.errors)) {
                parsed.dump_state_filename = *value;
                ++i;
            }
        } else if (arg == "--trace-cpu") {
            if (auto value = take_value(args, i, "--trace-cpu", parsed.errors)) {
                parsed.trace_cpu_filename = *value;
                ++i;
            }
        } else if (arg == "--event-log") {
            if (auto value = take_value(args, i, "--event-log", parsed.errors)) {
                parsed.event_log_filename = *value;
                ++i;
            }
        } else if (arg == "--input-script") {
            if (auto value = take_value(args, i, "--input-script", parsed.errors)) {
                parsed.input_script_path = *value;
                ++i;
            }
        } else if (arg == "--snapshot") {
            if (auto value = take_value(args, i, "--snapshot", parsed.errors)) {
                parsed.snapshot_dir = *value;  // M36 Phase 3: snapshot output root
                ++i;
            }
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
