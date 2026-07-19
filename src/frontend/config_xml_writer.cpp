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

#include "frontend/config_xml_writer.h"

#include <string>

namespace sony_msx::frontend {

namespace {

// Escape the 5 predefined XML entities in an attribute VALUE. The machine-layer
// tokenizer DECODES exactly these on parse (xml_decode_entities) but ships no
// encoder, so we provide the matching encode here. Applied to every string
// value (chiefly the asset paths, which may legitimately contain '&').
std::string xml_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

// The parser reads booleans case-insensitively as true|false (parse_bool_ci);
// we emit canonical lowercase.
const char* xml_bool(const bool v) { return v ? "true" : "false"; }

}  // namespace

std::string emulator_config_to_xml(const machine::EmulatorConfig& cfg) {
    const machine::EmulatorConfig::BiosRoms& roms = cfg.bios_roms;

    std::string x;
    x.reserve(1536);

    // A PI prolog + comments are all skipped by the tokenizer, so they are safe
    // documentation. This file is regenerated whenever the SDL3 frontend saves
    // settings; the <machine> section round-trips whatever was loaded, while the
    // <defaults> presentation knobs reflect the latest interactive state.
    x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    x += "<!-- Sony HB-F1XV emulator configuration (strict <hbf1xv-config> schema).\n";
    x += "     Auto-managed by the SDL3 frontend; CLI flags override every value here. -->\n";
    x += "<hbf1xv-config version=\"1\">\n";

    // ---- (A) <defaults>: session / CLI-option presentation knobs ----
    x += "  <defaults>\n";
    x += "    <fast-disk enabled=\"";
    x += xml_bool(cfg.fast_disk);
    x += "\"/>\n";
    x += "    <fmpac autoload=\"";
    x += xml_bool(cfg.fmpac_autoload);
    x += "\" slot=\"";
    x += std::to_string(cfg.fmpac_slot);
    x += "\"/>\n";
    x += "    <border enabled=\"";
    x += xml_bool(cfg.border_enabled);
    x += "\"/>\n";
    x += "    <disk-writable enabled=\"";
    x += xml_bool(cfg.disk_writable);
    x += "\"/>\n";
    x += "    <volume percent=\"";
    x += std::to_string(cfg.master_volume);
    x += "\"/>\n";
    x += "    <speed level=\"";
    x += std::to_string(cfg.speed_level);
    x += "\"/>\n";
    x += "    <video scale=\"";
    x += std::to_string(cfg.video_scale);
    x += "\" filter=\"";
    x += xml_escape(cfg.video_filter);
    x += "\"/>\n";
    x += "    <persistence percent=\"";
    x += std::to_string(cfg.persistence_percent);
    x += "\" mode=\"";
    x += xml_escape(cfg.persistence_mode);
    x += "\"/>\n";
    x += "    <fullscreen enabled=\"";
    x += xml_bool(cfg.fullscreen);
    x += "\"/>\n";
    x += "    <capture enabled=\"";
    x += xml_bool(cfg.capture_enabled);
    x += "\"/>\n";
    x += "  </defaults>\n";

    // ---- (B) <machine>: sizing + asset paths ----
    x += "  <machine>\n";
    x += "    <ram kb=\"";
    x += std::to_string(cfg.ram_kb);
    x += "\"/>\n";
    x += "    <vram kb=\"";
    x += std::to_string(cfg.vram_kb);
    x += "\"/>\n";
    x += "    <bios dir=\"";
    x += xml_escape(cfg.bios_dir);
    x += "\">\n";
    x += "      <rom role=\"bios\" file=\"";
    x += xml_escape(roms.bios);
    x += "\"/>\n";
    x += "      <rom role=\"sub\" file=\"";
    x += xml_escape(roms.sub);
    x += "\"/>\n";
    x += "      <rom role=\"kanji-driver\" file=\"";
    x += xml_escape(roms.kanji_driver);
    x += "\"/>\n";
    x += "      <rom role=\"disk\" file=\"";
    x += xml_escape(roms.disk);
    x += "\"/>\n";
    x += "      <rom role=\"fm-music\" file=\"";
    x += xml_escape(roms.fm_music);
    x += "\"/>\n";
    x += "      <rom role=\"kanji-font\" file=\"";
    x += xml_escape(roms.kanji_font);
    x += "\"/>\n";
    x += "      <rom role=\"firmware\" file=\"";
    x += xml_escape(roms.firmware);
    x += "\"/>\n";
    x += "    </bios>\n";
    x += "    <fmpac rom=\"";
    x += xml_escape(cfg.fmpac_rom);
    x += "\" sram=\"";
    x += xml_escape(cfg.fmpac_sram);
    x += "\"/>\n";
    x += "    <cartridge dir=\"";
    x += xml_escape(cfg.cartridge_dir);
    x += "\"/>\n";
    x += "    <disk dir=\"";
    x += xml_escape(cfg.disk_dir);
    x += "\"/>\n";
    x += "    <softwaredb path=\"";
    x += xml_escape(cfg.softwaredb_path);
    x += "\"/>\n";
    x += "  </machine>\n";

    x += "</hbf1xv-config>\n";
    return x;
}

}  // namespace sony_msx::frontend
