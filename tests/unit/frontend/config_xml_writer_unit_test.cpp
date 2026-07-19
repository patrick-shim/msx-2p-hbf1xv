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

#include "frontend/config_xml_writer.h"
#include "machine/emulator_config.h"

// Suite: Frontend_ConfigXmlWriter_Unit (DEC-0095). The PERMANENT anti-drift
// oracle for the settings-persistence emitter.
//
// WHY THIS EXISTS: emulator_config_to_xml() (frontend) and
// EmulatorConfig::parse() (machine) are two hand-maintained halves of one
// schema, deliberately living in different layers. Nothing but a test keeps
// them synced -- add a knob to the config and forget the emitter, and the
// frontend silently rewrites the user's file with that knob MISSING, so it
// reverts to its default on the next launch (a real data-loss footgun now that
// v1.6.0 rewrites the file automatically).
//
// This REPLACES the retired file-based repo-root shipped-config round-trip test
// (DEC-0095-AMENDMENT-C): there is no longer a shipped config at the repository
// root -- the only runtime config lives beside the executable and is written by
// the emulator itself -- so the invariant moved in-memory, where it is stronger
// (it covers arbitrary configs, not just the one shipped file).
//
// THE INVARIANT (config_xml_writer.h): for every EmulatorConfig `cfg` reachable
// by parse(), parse(emulator_config_to_xml(cfg)) == cfg with EMPTY warnings.
// The mutated case is the load-bearing one: a knob the emitter drops comes back
// as its DEFAULT, so the comparison fails and names the drift.

namespace {

using sony_msx::frontend::emulator_config_to_xml;
using sony_msx::machine::EmulatorConfig;

int g_failures = 0;

void expect(const bool ok, const std::string& case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Emit `cfg`, parse it back, and assert BOTH halves of the invariant.
void expect_round_trip(const EmulatorConfig& cfg, const std::string& case_name) {
    const std::string xml = emulator_config_to_xml(cfg);
    std::vector<std::string> warnings;
    const EmulatorConfig parsed = EmulatorConfig::parse(xml, warnings);

    if (!warnings.empty()) {
        std::cerr << case_name << ": emitted XML produced " << warnings.size()
                  << " parser warning(s) -- the emitter wrote something parse() rejects:\n";
        for (const std::string& w : warnings) {
            std::cerr << "    " << w << "\n";
        }
    }
    expect(warnings.empty(), case_name + " / zero parser warnings");
    expect(parsed == cfg, case_name + " / round-trip equality");
}

// A config with EVERY externalized knob moved OFF its default, staying inside
// each field's valid range/enum so a faithful emitter round-trips with zero
// warnings. vram_kb is deliberately left at 128 (strict HB-F1XV spec: any other
// value is warned + clamped, so it is not "reachable by parse()").
EmulatorConfig mutated_config() {
    EmulatorConfig cfg;
    // (A) session / CLI-option defaults
    cfg.fast_disk = false;
    cfg.fmpac_autoload = false;
    cfg.fmpac_slot = 1;
    cfg.border_enabled = true;
    cfg.disk_writable = false;
    cfg.master_volume = 55;
    cfg.speed_level = 4;
    cfg.video_scale = 5;
    cfg.video_filter = "nearest";
    cfg.persistence_percent = 42;
    cfg.persistence_mode = "peak";
    cfg.fullscreen = true;
    cfg.capture_enabled = true;
    // (B) machine sizing + asset paths
    cfg.ram_kb = 128;
    cfg.bios_dir = "custom-bios";
    cfg.cartridge_dir = "mycarts";
    cfg.disk_dir = "mydisks";
    cfg.fmpac_rom = "roms/custom-fmpac.rom";
    cfg.fmpac_sram = "roms/custom-fmpac.rom.sram";
    cfg.softwaredb_path = "db/custom-softwaredb.xml";
    // the seven role-keyed BIOS filenames
    cfg.bios_roms.bios = "custom_bios.rom";
    cfg.bios_roms.sub = "custom_sub.rom";
    cfg.bios_roms.kanji_driver = "custom_kdr.rom";
    cfg.bios_roms.disk = "custom_disk.rom";
    cfg.bios_roms.fm_music = "custom_mus.rom";
    cfg.bios_roms.kanji_font = "custom_kfn.rom";
    cfg.bios_roms.firmware = "custom_firm.rom";
    return cfg;
}

}  // namespace

int main() {
    // 1. The compiled defaults round-trip. This is the direct successor to the
    //    retired shipped-file test: the document the emulator writes on a fresh
    //    interactive run parses back to the built-in defaults with no warnings.
    expect_round_trip(EmulatorConfig{}, "defaults");

    // 2. THE anti-drift case: every knob off its default. A field the emitter
    //    forgets returns as its default here and fails the equality above.
    expect_round_trip(mutated_config(), "all-knobs-mutated");

    // 3. Values needing XML escaping survive verbatim (the emitter escapes
    //    string/path attributes; an unescaped '&' or '"' would produce either a
    //    parser warning or a corrupted value).
    {
        EmulatorConfig cfg;
        cfg.bios_dir = R"(bios & "roms" <dir>)";
        cfg.cartridge_dir = "carts & 'more'";
        cfg.bios_roms.bios = R"(a&b"c.rom)";
        expect_round_trip(cfg, "xml-escaping");
    }

    // 4. The emitted defaults document the whole schema -- both sections and a
    //    representative knob from each are present in the raw text, so the file
    //    a user opens is editable rather than a bare stub.
    {
        const std::string xml = emulator_config_to_xml(EmulatorConfig{});
        const auto has = [&xml](const char* needle) {
            return xml.find(needle) != std::string::npos;
        };
        expect(has("<hbf1xv-config"), "documents / root element");
        expect(has("<defaults"), "documents / defaults section");
        expect(has("<machine"), "documents / machine section");
        expect(has("<bios"), "documents / bios dir + rom list");
        expect(has("<cartridge"), "documents / M64 cartridge dialog dir");
        expect(has("<disk"), "documents / M64 disk dialog dir");
        expect(!xml.empty() && xml.back() == '\n', "documents / newline-terminated");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "Frontend_ConfigXmlWriter_Unit: all cases passed\n";
    return 0;
}
