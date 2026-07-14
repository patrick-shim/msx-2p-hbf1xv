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

#include "frontend/config_runtime.h"

#include <fstream>

namespace sony_msx::frontend {

namespace {

bool file_readable(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return in.good();
}

}  // namespace

bool config_should_load(const bool interactive_sdl3, const bool explicit_config_path) {
    // The whole determinism rule (§4.6) in one line: AUTO-load only for a
    // genuinely interactive SDL3 launch; --config forces a load in any mode.
    // Everything else (headless, --debug-session, parity, SDL3 --hidden-window)
    // stays on the no-config path.
    return interactive_sdl3 || explicit_config_path;
}

machine::EmulatorConfig load_config_with_search(const std::optional<std::string>& explicit_path,
                                                const std::vector<std::string>& auto_search_paths,
                                                std::vector<std::string>& warnings) {
    // --config <path> (opt-in): force-load; load_from_file already emits ONE
    // WARNING naming the path on an unreadable file + returns built-in defaults.
    if (explicit_path.has_value()) {
        return machine::EmulatorConfig::load_from_file(*explicit_path, warnings);
    }

    // Auto-search order (§4.6): first existing wins.
    for (const std::string& candidate : auto_search_paths) {
        if (file_readable(candidate)) {
            return machine::EmulatorConfig::load_from_file(candidate, warnings);
        }
    }

    // None found -> the expected zero-config first-run behavior (§4.2): ONE
    // WARNING that names what was searched, then built-in defaults.
    std::string searched;
    for (std::size_t i = 0; i < auto_search_paths.size(); ++i) {
        searched += auto_search_paths[i];
        if (i + 1 < auto_search_paths.size()) {
            searched += ", ";
        }
    }
    warnings.push_back("config: WARNING no hbf1xv-config.xml found (searched: " + searched +
                       "); using built-in defaults");
    return machine::EmulatorConfig{};
}

ResolvedRuntimeConfig resolve_runtime_config(const machine::EmulatorConfig& cfg,
                                             const ParsedSdl3Cli& parsed) {
    ResolvedRuntimeConfig r;

    // --- Session knobs via the SHARED resolver, with the XML values as the base
    //     defaults it falls back to (precedence CLI > XML > built-in default).
    //     The single resolve_session_defaults() keeps the RAM/fast-disk/FM-PAC
    //     precedence policy in one place (no duplication). ---
    SessionDefaultsRequest req;
    req.ram_kb = parsed.ram_kb;
    req.fast_disk_opt = parsed.fast_disk_opt;
    req.no_fmpac = parsed.no_fmpac;
    req.stock = parsed.stock;
    req.base_ram_kb = cfg.ram_kb;
    req.base_fast_disk = cfg.fast_disk;
    req.base_fmpac_autoload = cfg.fmpac_autoload;
    const ResolvedSessionDefaults sd = resolve_session_defaults(req);
    r.ram_bytes = sd.ram_bytes;
    r.fast_disk = sd.fast_disk;
    r.fmpac_autoload = sd.fmpac_autoload;
    r.is_stock = sd.is_stock;
    // FM-PAC auto-load slot: no CLI flag exists (implicit), so it is XML > default.
    r.fmpac_autoload_slot = cfg.fmpac_slot;

    // --- Presentation knobs: CLI-if-explicitly-specified else XML(cfg) else the
    //     built-in default (which is cfg's own default when nothing was loaded). ---

    // Plain-bool knobs (explicit-tracking bool tells CLI-specified from default).
    r.border_enabled = parsed.border_specified ? parsed.border_enabled : cfg.border_enabled;
    r.disk_writable = parsed.disk_writable_specified ? parsed.disk_writable : cfg.disk_writable;
    r.fullscreen = parsed.fullscreen_specified ? parsed.fullscreen : cfg.fullscreen;
    r.capture_enabled = parsed.capture_specified ? parsed.capture_enabled : cfg.capture_enabled;

    // optional<int> knobs (has_value() == CLI-specified).
    r.speed_level = parsed.speed_level.has_value() ? *parsed.speed_level : cfg.speed_level;
    r.scale = parsed.scale.has_value() ? *parsed.scale : cfg.video_scale;
    r.persistence = parsed.persistence.has_value() ? *parsed.persistence : cfg.persistence_percent;

    // enum knobs (explicit-tracking bool). The XML string maps to the enum.
    r.filter = parsed.filter_specified
                   ? parsed.filter
                   : (cfg.video_filter == "nearest" ? TextureFilter::Nearest : TextureFilter::Linear);
    r.persistence_mode = parsed.persistence_mode_specified
                             ? parsed.persistence_mode
                             : (cfg.persistence_mode == "peak" ? PhosphorMode::Peak
                                                               : PhosphorMode::Average);
    return r;
}

ResolvedMachineConfig resolve_machine_config(const machine::EmulatorConfig& cfg,
                                             const ParsedSdl3Cli& parsed) {
    ResolvedMachineConfig r;
    const machine::EmulatorConfig def;  // built-in defaults, for the "XML differs
                                        // from default" test on the optional fields.

    // --- BIOS dir: CLI --bios-dir > XML > built-in "bios". ---
    r.bios_dir = parsed.bios_dir.has_value() ? *parsed.bios_dir : cfg.bios_dir;

    // --- The 7 role-keyed BIOS filenames: no per-file CLI flag -> XML > built-in.
    //     cfg carries either the XML-specified names or the built-in spec set, so
    //     with no config loaded this is byte-identical to the pre-M50 literals. ---
    r.bios_roms = cfg.bios_roms;

    // --- FM-PAC auto-load ROM path: no dedicated CLI flag (an explicit --slot2
    //     occupies the bay and the auto-load skips) -> XML > built-in. ---
    r.fmpac_autoload_rom = cfg.fmpac_rom;

    // --- FM-PAC SRAM path: CLI --fmpac-sram > XML > auto-derive. Surface an XML
    //     value ONLY when it differs from the built-in default, so the no-config
    //     path stays std::nullopt (auto-derive) byte-identically. ---
    if (parsed.fmpac_sram_path.has_value()) {
        r.fmpac_sram = parsed.fmpac_sram_path;
    } else if (cfg.fmpac_sram != def.fmpac_sram) {
        r.fmpac_sram = cfg.fmpac_sram;
    }

    // --- softwaredb path: CLI --softwaredb > XML > kDefaultSoftwareDbPath. Same
    //     differs-from-default rule keeps the no-config path std::nullopt (the
    //     byte-identical CartridgeIdentificationSession default + the quieter
    //     was_explicit=false unavailable message). ---
    if (parsed.cartridges.softwaredb_path.has_value()) {
        r.softwaredb = parsed.cartridges.softwaredb_path;
    } else if (cfg.softwaredb_path != def.softwaredb_path) {
        r.softwaredb = cfg.softwaredb_path;
    }

    // --- VRAM: validated-to-128 in the parser; no runtime sizing seam (§4.4).
    //     cfg.vram_kb is always 128 here; carry it, never resize the VDP. ---
    r.vram_kb = cfg.vram_kb;
    return r;
}

}  // namespace sony_msx::frontend
