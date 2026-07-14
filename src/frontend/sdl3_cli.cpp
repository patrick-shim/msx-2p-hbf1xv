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
#include <cstdlib>
#include <string>

#include "devices/chipset/mb670836_pause.h"

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

bool parse_speed_level(const std::string& value, int& out_level, std::vector<std::string>& errors,
                       const char* context) {
    int level = 0;
    const auto begin = value.data();
    const auto end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, level);
    if (ec != std::errc{} || ptr != end) {
        errors.push_back(std::string(context) + ": --speed value is not a valid integer: '" + value + "'");
        return false;
    }
    // Range policy mirrors --max-frames: an out-of-range level is a loud parse
    // error, NOT silently clamped (set_speed_level() clamps at the device, but
    // the CLI must reject a nonsensical request). kMaxSpeedLevel == 7.
    if (level < 0 || level > devices::chipset::Mb670836PauseController::kMaxSpeedLevel) {
        errors.push_back(std::string(context) + ": --speed value out of range [0.." +
                         std::to_string(devices::chipset::Mb670836PauseController::kMaxSpeedLevel) + "]: '" +
                         value + "'");
        return false;
    }
    out_level = level;
    return true;
}

bool parse_ram_kb(const std::string& value, int& out_kb, std::vector<std::string>& errors,
                  const char* context) {
    int kb = 0;
    const auto begin = value.data();
    const auto end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, kb);
    if (ec != std::errc{} || ptr != end) {
        errors.push_back(std::string(context) + ": --ram value is not a valid integer: '" + value + "'");
        return false;
    }
    // DEC-0061 enum policy: ONLY {64,128,256,512}KB are offered (4/8/16/32 mapper
    // segments -- all natively probeable within the S1985's 5-bit read-back;
    // >512 KB would need an external RAM-expansion cartridge). Any other value is
    // a loud parse error, NOT a silent clamp (mirrors --speed/--max-frames).
    if (kb != 64 && kb != 128 && kb != 256 && kb != 512) {
        errors.push_back(std::string(context) +
                         ": --ram value must be one of 64|128|256|512 (KB): '" + value + "'");
        return false;
    }
    out_kb = kb;
    return true;
}

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
        } else if (arg == "--config") {
            // M50-S2 (DEC-0077): force-load an externalized strict-XML config from
            // <path> in ANY mode (the determinism opt-in, §4.6). Only records the
            // path; the load + precedence application happen in config_runtime.h.
            if (auto value = take_value(args, i, "--config", parsed.errors)) {
                parsed.config_path = *value;
                ++i;
            }
        } else if (arg == "--border") {
            // M39-D (human preference revert): the active OPT-IN to the openMSX-
            // matching framed canvas (the default is now the bare Sony edge-to-
            // edge present). Last-wins vs a later --no-border on the linear scan.
            parsed.border_enabled = true;
            parsed.border_specified = true;  // M50-S2 precedence: user chose explicitly
        } else if (arg == "--no-border") {
            // M39-D: explicit OFF -- the bare edge-to-edge active-area present
            // (matches the default). Last-wins vs an earlier --border, since
            // this is a plain linear scan (mirrors the other boolean flags).
            parsed.border_enabled = false;
            parsed.border_specified = true;  // M50-S2 precedence: user chose explicitly
        } else if (arg == "--disk-writable") {
            parsed.disk_writable = true;  // M36-S-c: opt-in disk-save persistence
            parsed.disk_writable_specified = true;  // M50-S2 precedence
        } else if (arg == "--fast-disk") {
            // M46 (DEC-0071): tri-state -- explicit ON overrides --stock/the
            // resolver default. Last-wins vs a later --no-fast-disk (linear scan).
            parsed.fast_disk_opt = true;
        } else if (arg == "--no-fast-disk") {
            parsed.fast_disk_opt = false;  // M46: explicit OFF (accurate FDC timing)
        } else if (arg == "--no-fmpac") {
            parsed.no_fmpac = true;  // M46: opt out of the default FM-PAC slot-2 auto-load
        } else if (arg == "--stock") {
            parsed.stock = true;  // M46: one-shot authentic bare machine preset
        } else if (arg == "--stream-light") {
            parsed.stream_light = true;  // DEC-0052: F10 arms lightweight mode
        } else if (arg == "--fmpac-sram") {
            // M36 FM-PAC SRAM persistence: explicit override of the auto-derived
            // <fmpac-cart>.rom.sram default (mirrors headless --fmpac-sram).
            if (auto value = take_value(args, i, "--fmpac-sram", parsed.errors)) {
                parsed.fmpac_sram_path = *value;
                ++i;
            }
        } else if (arg == "--no-fmpac-sram") {
            parsed.fmpac_sram_disabled = true;  // M36: opt out of FM-PAC .sram persistence
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
        } else if (arg == "--record-input") {
            // Input RECORDER (DEC-0072): stream this live session's keystrokes +
            // F11 disk swaps to <path> as a replayable HBF1XV-INPUT-SCRIPT v1.
            if (auto value = take_value(args, i, "--record-input", parsed.errors)) {
                parsed.record_input_path = *value;
                ++i;
            }
        } else if (arg == "--snapshot") {
            if (auto value = take_value(args, i, "--snapshot", parsed.errors)) {
                parsed.snapshot_dir = *value;  // M36 Phase 3: snapshot output root
                ++i;
            }
        } else if (arg == "--swap-disk-frame") {
            // DEC-0072: scripted disk hot-swap at frame N (replay a recorded owner
            // script's "# SWAP_DISK frame=<N>" on hidden-window SDL3).
            if (auto value = take_value(args, i, "--swap-disk-frame", parsed.errors)) {
                parsed.swap_disk_frame = static_cast<std::uint32_t>(std::strtoul(value->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--fingerprint") {
            if (auto value = take_value(args, i, "--fingerprint", parsed.errors)) {
                parsed.fingerprint_path = *value;  // DEC-0072 per-frame CPU fingerprint CSV
                ++i;
            }
        } else if (arg == "--speed") {
            // M37 Slice D (DEC-0056): launch-time initial Speed Controller level.
            if (auto value = take_value(args, i, "--speed", parsed.errors)) {
                ++i;
                int level = 0;
                if (parse_speed_level(*value, level, parsed.errors, "sdl3_cli")) {
                    parsed.speed_level = level;
                }
            }
        } else if (arg == "--scale") {
            // M37 Slice E (DEC-0056): initial window scale (320N x 240N), N in [1,8].
            if (auto value = take_value(args, i, "--scale", parsed.errors)) {
                ++i;
                int scale = 0;
                const auto begin = value->data();
                const auto end = value->data() + value->size();
                const auto [ptr, ec] = std::from_chars(begin, end, scale);
                if (ec != std::errc{} || ptr != end) {
                    parsed.errors.push_back("sdl3_cli: --scale value is not a valid integer: '" + *value + "'");
                } else if (scale < 1 || scale > 8) {
                    parsed.errors.push_back("sdl3_cli: --scale value out of range [1..8]: '" + *value + "'");
                } else {
                    parsed.scale = scale;
                }
            }
        } else if (arg == "--persistence") {
            // Phosphor-persistence inter-frame blend weight (percent of the
            // previous presented frame retained), [0,100]. Absent -> OFF (0).
            // Out-of-range / non-numeric -> loud error (mirrors --scale).
            if (auto value = take_value(args, i, "--persistence", parsed.errors)) {
                ++i;
                int pct = 0;
                const auto begin = value->data();
                const auto end = value->data() + value->size();
                const auto [ptr, ec] = std::from_chars(begin, end, pct);
                if (ec != std::errc{} || ptr != end) {
                    parsed.errors.push_back("sdl3_cli: --persistence value is not a valid integer: '" +
                                            *value + "'");
                } else if (pct < 0 || pct > 100) {
                    parsed.errors.push_back("sdl3_cli: --persistence value out of range [0..100]: '" +
                                            *value + "'");
                } else {
                    parsed.persistence = pct;
                }
            }
        } else if (arg == "--persistence-mode") {
            // Phosphor blend mode: 'avg' (weighted mean, default) or 'peak'
            // (peak-hold-with-decay -- no sprite dimming). Value policy mirrors
            // --filter: an unrecognized value is a loud error; absent stays avg.
            if (auto value = take_value(args, i, "--persistence-mode", parsed.errors)) {
                ++i;
                if (*value == "avg" || *value == "average") {
                    parsed.persistence_mode = PhosphorMode::Average;
                    parsed.persistence_mode_specified = true;  // M50-S2 precedence
                } else if (*value == "peak") {
                    parsed.persistence_mode = PhosphorMode::Peak;
                    parsed.persistence_mode_specified = true;  // M50-S2 precedence
                } else {
                    parsed.errors.push_back("sdl3_cli: --persistence-mode value must be 'avg' or 'peak': '" +
                                            *value + "'");
                }
            }
        } else if (arg == "--filter") {
            // M37 Slice E (DEC-0056): texture scale mode; default (absent) Linear.
            if (auto value = take_value(args, i, "--filter", parsed.errors)) {
                ++i;
                if (*value == "nearest") {
                    parsed.filter = TextureFilter::Nearest;
                    parsed.filter_specified = true;  // M50-S2 precedence
                } else if (*value == "linear") {
                    parsed.filter = TextureFilter::Linear;
                    parsed.filter_specified = true;  // M50-S2 precedence
                } else {
                    parsed.errors.push_back("sdl3_cli: --filter value must be 'nearest' or 'linear': '" +
                                            *value + "'");
                }
            }
        } else if (arg == "--ram") {
            // M42 (DEC-0061): main-RAM size in KB, enum {64,128,256,512}. Absent
            // -> nullopt -> stock 64 KB. Shared validator = one policy across exes.
            if (auto value = take_value(args, i, "--ram", parsed.errors)) {
                ++i;
                int kb = 0;
                if (parse_ram_kb(*value, kb, parsed.errors, "sdl3_cli")) {
                    parsed.ram_kb = kb;
                }
            }
        } else if (arg == "--fullscreen") {
            parsed.fullscreen = true;  // M37 Slice E: start fullscreen (Alt+Enter toggles at runtime)
            parsed.fullscreen_specified = true;  // M50-S2 precedence
        } else if (arg == "--capture") {
            // M37 Slice F: gate the F10 live stream-capture hotkey; default OFF.
            // Value must be 'on' or 'off' (mirrors the --filter value policy).
            if (auto value = take_value(args, i, "--capture", parsed.errors)) {
                ++i;
                if (*value == "on") {
                    parsed.capture_enabled = true;
                    parsed.capture_specified = true;  // M50-S2 precedence
                } else if (*value == "off") {
                    parsed.capture_enabled = false;
                    parsed.capture_specified = true;  // M50-S2 precedence
                } else {
                    parsed.errors.push_back("sdl3_cli: --capture value must be 'on' or 'off': '" +
                                            *value + "'");
                }
            }
        }
        // Anything else isn't this parser's concern -- left untouched for
        // the cartridge-flag delegation below (order-independent scan,
        // mirrors cartridge_cli's A-M19-4).
    }

    parsed.cartridges = machine::parse_cartridge_cli(args);
    for (std::string& err : parsed.cartridges.errors) {
        parsed.errors.push_back(err);
    }

    return parsed;
}

ResolvedSessionDefaults resolve_session_defaults(const SessionDefaultsRequest& request) {
    // Precedence = specificity, NOT argv order (planner §2.4): an explicit
    // per-field flag always wins over --stock's field, which in turn wins over
    // the convenience default. The empty-CLI result is the ready-to-game config.
    ResolvedSessionDefaults resolved;

    // M50-S2 (§4.1): the final "else" of each chain is the XML BASE DEFAULT
    // (request.base_*), which defaults to the M46 convenience value -- so an
    // unspecified request stays byte-identical while a loaded config replaces the
    // fallback. Explicit per-field CLI still wins; --stock still forces the bare
    // machine over the XML base.
    resolved.ram_bytes = request.ram_kb.has_value()
                             ? static_cast<std::size_t>(*request.ram_kb) * 1024u  // explicit --ram wins
                         : request.stock ? 64u * 1024u                            // --stock preset
                                         : static_cast<std::size_t>(request.base_ram_kb) * 1024u;  // XML/conv base

    resolved.fast_disk = request.fast_disk_opt.has_value() ? *request.fast_disk_opt  // explicit wins
                         : request.stock                   ? false                   // --stock preset
                                                           : request.base_fast_disk;  // XML/conv base

    resolved.fmpac_autoload = request.no_fmpac ? false                       // explicit opt-out wins
                              : request.stock  ? false                       // --stock preset
                                               : request.base_fmpac_autoload;  // XML/conv base

    resolved.is_stock = request.stock;  // banner label only
    return resolved;
}

}  // namespace sony_msx::frontend
