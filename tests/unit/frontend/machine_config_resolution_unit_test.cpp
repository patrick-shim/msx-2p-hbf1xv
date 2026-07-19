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

#include "devices/video/vdp_vram.h"
#include "frontend/config_runtime.h"
#include "frontend/sdl3_cli.h"
#include "machine/emulator_config.h"

// Suite: Frontend_MachineConfigResolution_Unit (M50-S3, docs/m50-planner-
// package.md §6-S3/§4.4, AC-S3-1/AC-S3-2/AC-4).
//
// Pure, SDL-free tests for the MACHINE-sizing/path RESOLUTION seam
// (resolve_machine_config): precedence CLI > XML > built-in default across the
// BIOS dir + 7 role-keyed filenames, FM-PAC ROM/SRAM, and softwaredb path, plus
// the VRAM validated-to-128 (§4.4) contract. Non-tautological: config-driven
// cases parse REAL XML through EmulatorConfig::parse(), CLI-override cases parse
// REAL argv through parse_sdl3_cli(), and every case asserts a specific
// effective value (never that the resolver echoes its own input).

namespace {

using sony_msx::frontend::parse_sdl3_cli;
using sony_msx::frontend::ParsedSdl3Cli;
using sony_msx::frontend::ResolvedMachineConfig;
using sony_msx::frontend::resolve_machine_config;
using sony_msx::machine::EmulatorConfig;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

EmulatorConfig parse_config(const std::string& xml) {
    std::vector<std::string> warnings;
    return EmulatorConfig::parse(xml, warnings);
}

// Parse XML and also return the collected warnings (for the VRAM-invalid case).
EmulatorConfig parse_config_warn(const std::string& xml, std::vector<std::string>& warnings) {
    return EmulatorConfig::parse(xml, warnings);
}

bool any_contains(const std::vector<std::string>& lines, const std::string& needle) {
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    // ================================================================
    // Part A -- NO config loaded (built-in defaults): the resolved machine
    // config is BYTE-IDENTICAL to pre-M50 (the determinism/anti-drift guarantee).
    // ================================================================
    {
        const EmulatorConfig def;  // all built-in defaults
        const ParsedSdl3Cli parsed = parse_sdl3_cli({});  // no CLI flags
        const ResolvedMachineConfig r = resolve_machine_config(def, parsed);

        expect(r.bios_dir == "bios", "NoConfig_BiosDirDefault");
        expect(r.bios_roms.bios == "f1xvbios.rom", "NoConfig_BiosFilenameDefault");
        expect(r.bios_roms.firmware == "f1xvfirm.rom", "NoConfig_FirmwareFilenameDefault");
        expect(r.fmpac_autoload_rom == "roms/fmpac.rom", "NoConfig_FmPacRomDefault");
        // The two OPTIONAL consumer seams stay std::nullopt with no config, so the
        // downstream (SRAM auto-derive / softwaredb default) path is unchanged.
        expect(!r.fmpac_sram.has_value(), "NoConfig_FmPacSramNulloptAutoDerive");
        expect(!r.softwaredb.has_value(), "NoConfig_SoftwaredbNulloptDefault");
        expect(r.vram_kb == 128, "NoConfig_Vram128");
        // M64: the in-window file-dialog default directories.
        expect(r.cartridge_dir == "roms", "NoConfig_CartridgeDirDefaultRoms");
        expect(r.disk_dir == "disks", "NoConfig_DiskDirDefaultDisks");
    }

    // ================================================================
    // Part B -- CONFIG-DRIVEN (XML sets each field to a NON-default value): the
    // XML value takes effect (AC-3/AC-S3-1). Parses real XML end to end.
    // ================================================================
    {
        const EmulatorConfig cfg = parse_config(
            "<hbf1xv-config version=\"1\">\n"
            "  <machine>\n"
            "    <bios dir=\"/opt/msx/roms\">\n"
            "      <rom role=\"bios\" file=\"my-bios.rom\"/>\n"
            "      <rom role=\"disk\" file=\"my-disk.rom\"/>\n"
            "      <rom role=\"firmware\" file=\"my-firm.rom\"/>\n"
            "    </bios>\n"
            "    <fmpac rom=\"carts/fmpac-custom.rom\" sram=\"saves/fmpac.sram\"/>\n"
            "    <cartridge dir=\"my/carts\"/>\n"
            "    <disk dir=\"my/floppies\"/>\n"
            "    <softwaredb path=\"db/mydb.xml\"/>\n"
            "  </machine>\n"
            "</hbf1xv-config>\n");
        const ParsedSdl3Cli parsed = parse_sdl3_cli({});  // no CLI overrides
        const ResolvedMachineConfig r = resolve_machine_config(cfg, parsed);

        expect(r.bios_dir == "/opt/msx/roms", "Xml_BiosDirApplied");
        expect(r.bios_roms.bios == "my-bios.rom", "Xml_BiosFilenameApplied");
        expect(r.bios_roms.disk == "my-disk.rom", "Xml_DiskFilenameApplied");
        expect(r.bios_roms.firmware == "my-firm.rom", "Xml_FirmwareFilenameApplied");
        // An UNSPECIFIED role keeps its built-in default filename.
        expect(r.bios_roms.sub == "f1xvext.rom", "Xml_UnspecifiedRoleKeepsDefault");
        expect(r.fmpac_autoload_rom == "carts/fmpac-custom.rom", "Xml_FmPacRomApplied");
        expect(r.fmpac_sram.has_value() && *r.fmpac_sram == "saves/fmpac.sram",
               "Xml_FmPacSramApplied");
        expect(r.softwaredb.has_value() && *r.softwaredb == "db/mydb.xml",
               "Xml_SoftwaredbApplied");
        // M64: the dialog default dirs carry the XML values (no CLI flag rung).
        expect(r.cartridge_dir == "my/carts", "Xml_CartridgeDirApplied");
        expect(r.disk_dir == "my/floppies", "Xml_DiskDirApplied");
    }

    // ================================================================
    // Part C -- PRECEDENCE CLI > XML (AC-4/AC-S3-1). A CLI --bios-dir /
    // --fmpac-sram / --softwaredb each WINS over the XML value.
    // ================================================================
    {
        const EmulatorConfig cfg = parse_config(
            "<hbf1xv-config version=\"1\">\n"
            "  <machine>\n"
            "    <bios dir=\"xml-bios-dir\"/>\n"
            "    <fmpac rom=\"xml-fmpac.rom\" sram=\"xml.sram\"/>\n"
            "    <softwaredb path=\"xml-db.xml\"/>\n"
            "  </machine>\n"
            "</hbf1xv-config>\n");
        const ParsedSdl3Cli parsed = parse_sdl3_cli(
            {"--bios-dir", "cli-bios-dir", "--fmpac-sram", "cli.sram", "--softwaredb", "cli-db.xml"});
        const ResolvedMachineConfig r = resolve_machine_config(cfg, parsed);

        expect(r.bios_dir == "cli-bios-dir", "Precedence_CliBiosDirWinsOverXml");
        expect(r.fmpac_sram.has_value() && *r.fmpac_sram == "cli.sram",
               "Precedence_CliFmPacSramWinsOverXml");
        expect(r.softwaredb.has_value() && *r.softwaredb == "cli-db.xml",
               "Precedence_CliSoftwaredbWinsOverXml");
        // No CLI flag for the FM-PAC auto-load ROM path -> the XML value still wins
        // over the built-in default (XML > default holds where no CLI override).
        expect(r.fmpac_autoload_rom == "xml-fmpac.rom", "Precedence_FmPacRomXmlOverDefault");
    }

    // ================================================================
    // Part D -- PRECEDENCE CLI > default when NO config was loaded (the XML rung
    // is absent): the CLI still wins over the built-in default.
    // ================================================================
    {
        const EmulatorConfig def;  // no config loaded
        const ParsedSdl3Cli parsed =
            parse_sdl3_cli({"--bios-dir", "only-cli-dir", "--softwaredb", "only-cli-db.xml"});
        const ResolvedMachineConfig r = resolve_machine_config(def, parsed);

        expect(r.bios_dir == "only-cli-dir", "Precedence_CliBiosDirOverDefault");
        expect(r.softwaredb.has_value() && *r.softwaredb == "only-cli-db.xml",
               "Precedence_CliSoftwaredbOverDefault");
        // Unspecified-by-CLI fields still fall back to the built-in default.
        expect(!r.fmpac_sram.has_value(), "Precedence_FmPacSramUnsetStaysNullopt");
    }

    // ================================================================
    // Part E -- VRAM validated-to-128 (§4.4): a config vram@kb != 128 WARNS
    // (naming the key), the model clamps to 128, resolve_machine_config carries
    // 128, and the effective device VRAM constant is 128 KB. Non-tautological:
    // asserts the parser warning text AND the clamped value AND the constexpr.
    // ================================================================
    {
        std::vector<std::string> warnings;
        const EmulatorConfig cfg = parse_config_warn(
            "<hbf1xv-config version=\"1\">\n"
            "  <machine><vram kb=\"64\"/></machine>\n"
            "</hbf1xv-config>\n",
            warnings);
        expect(any_contains(warnings, "machine/vram@kb"), "Vram_InvalidWarnsNamingKey");
        expect(cfg.vram_kb == 128, "Vram_InvalidClampedTo128InModel");

        const ParsedSdl3Cli parsed = parse_sdl3_cli({});
        const ResolvedMachineConfig r = resolve_machine_config(cfg, parsed);
        expect(r.vram_kb == 128, "Vram_ResolvedStays128");
        // The strict HB-F1XV device VRAM is 128 KB regardless of any config value
        // (there is no runtime-sizing seam; the config field is documentary).
        expect(sony_msx::devices::video::VdpVram::kVramBytes == 128u * 1024u,
               "Vram_DeviceConstantIs128KB");
    }

    // A config vram@kb == 128 is accepted with NO warning (the valid boundary).
    {
        std::vector<std::string> warnings;
        const EmulatorConfig cfg = parse_config_warn(
            "<hbf1xv-config version=\"1\">\n"
            "  <machine><vram kb=\"128\"/></machine>\n"
            "</hbf1xv-config>\n",
            warnings);
        expect(!any_contains(warnings, "machine/vram@kb"), "Vram_128NoWarning");
        expect(cfg.vram_kb == 128, "Vram_128Applied");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineConfigResolution_Unit cases passed\n";
    return 0;
}
