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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/emulator_config.h"

// Suite: Machine_EmulatorConfig_Unit (M50-S1, docs/m50-planner-package.md
// §4.3/§7 AC-S1). Non-tautological parser + validator tests: the "valid full
// config" fixture sets EVERY knob to a value DIFFERENT from the compiled
// default (so a parser that ignored the XML and returned defaults would FAIL),
// and each per-key error case asserts BOTH that a WARNING names the offending
// key AND that the effective value is the built-in default AND that a
// co-present VALID key still applies (so it is not "everything defaulted").
//
// Deterministic oracle: identical XML text -> identical EmulatorConfig + the
// same ordered warning strings, every run (pure parse, no clock/host state).

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Does any warning contain `needle` (used to assert a warning NAMES the key)?
bool any_warning_contains(const std::vector<std::string>& warnings, const std::string& needle) {
    for (const std::string& w : warnings) {
        if (w.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    using sony_msx::machine::EmulatorConfig;

    const EmulatorConfig kDefaults;  // compiled built-in defaults

    // ========================================================================
    // 1. Valid FULL config -> every field parses to the given (non-default!)
    //    value, and ZERO warnings. (AC-S1-1; non-tautology: all values differ
    //    from the compiled defaults.)
    // ========================================================================
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<hbf1xv-config version=\"1\">\n"
            "  <defaults>\n"
            "    <fast-disk enabled=\"false\"/>\n"           // default true
            "    <fmpac autoload=\"false\" slot=\"1\"/>\n"   // default true / 2
            "    <border enabled=\"true\"/>\n"               // default false
            "    <disk-writable enabled=\"true\"/>\n"        // M52 (DEC-0079): default now true; XML also true
            "    <volume percent=\"55\"/>\n"                 // M52 (DEC-0079): default 100
            "    <speed level=\"5\"/>\n"                     // default 0
            "    <video scale=\"6\" filter=\"nearest\"/>\n"  // default 3 / linear
            "    <persistence percent=\"40\" mode=\"peak\"/>\n"  // default 0 / avg
            "    <fullscreen enabled=\"true\"/>\n"           // default false
            "    <capture enabled=\"true\"/>\n"              // default false
            "  </defaults>\n"
            "  <machine>\n"
            "    <ram kb=\"128\"/>\n"                        // default 512
            "    <vram kb=\"128\"/>\n"                       // strict 128
            "    <bios dir=\"custombios\">\n"                // default bios
            "      <rom role=\"bios\"         file=\"my-bios.rom\"/>\n"
            "      <rom role=\"sub\"          file=\"my-sub.rom\"/>\n"
            "      <rom role=\"kanji-driver\" file=\"my-kdr.rom\"/>\n"
            "      <rom role=\"disk\"         file=\"my-disk.rom\"/>\n"
            "      <rom role=\"fm-music\"     file=\"my-mus.rom\"/>\n"
            "      <rom role=\"kanji-font\"   file=\"my-kfn.rom\"/>\n"
            "      <rom role=\"firmware\"     file=\"my-firm.rom\"/>\n"
            "    </bios>\n"
            "    <fmpac rom=\"custom/fm.rom\" sram=\"custom/fm.sram\"/>\n"
            "    <cartridge dir=\"customcarts\"/>\n"        // M64: default roms
            "    <disk dir=\"customdisks\"/>\n"             // M64: default disks
            "    <softwaredb path=\"custom/sw.xml\"/>\n"
            "  </machine>\n"
            "</hbf1xv-config>\n";

        std::vector<std::string> warnings;
        const EmulatorConfig cfg = EmulatorConfig::parse(xml, warnings);

        expect(warnings.empty(), "FullValid_NoWarnings");
        expect(cfg.fast_disk == false, "FullValid_FastDisk");
        expect(cfg.fmpac_autoload == false, "FullValid_FmpacAutoload");
        expect(cfg.fmpac_slot == 1, "FullValid_FmpacSlot");
        expect(cfg.border_enabled == true, "FullValid_Border");
        expect(cfg.disk_writable == true, "FullValid_DiskWritable");
        expect(cfg.master_volume == 55, "FullValid_MasterVolume");  // M52 (DEC-0079)
        expect(cfg.speed_level == 5, "FullValid_Speed");
        expect(cfg.video_scale == 6, "FullValid_Scale");
        expect(cfg.video_filter == "nearest", "FullValid_Filter");
        expect(cfg.persistence_percent == 40, "FullValid_PersistencePercent");
        expect(cfg.persistence_mode == "peak", "FullValid_PersistenceMode");
        expect(cfg.fullscreen == true, "FullValid_Fullscreen");
        expect(cfg.capture_enabled == true, "FullValid_Capture");
        expect(cfg.ram_kb == 128, "FullValid_RamKb");
        expect(cfg.vram_kb == 128, "FullValid_VramKb");
        expect(cfg.bios_dir == "custombios", "FullValid_BiosDir");
        expect(cfg.bios_roms.bios == "my-bios.rom", "FullValid_RomBios");
        expect(cfg.bios_roms.sub == "my-sub.rom", "FullValid_RomSub");
        expect(cfg.bios_roms.kanji_driver == "my-kdr.rom", "FullValid_RomKanjiDriver");
        expect(cfg.bios_roms.disk == "my-disk.rom", "FullValid_RomDisk");
        expect(cfg.bios_roms.fm_music == "my-mus.rom", "FullValid_RomFmMusic");
        expect(cfg.bios_roms.kanji_font == "my-kfn.rom", "FullValid_RomKanjiFont");
        expect(cfg.bios_roms.firmware == "my-firm.rom", "FullValid_RomFirmware");
        expect(cfg.fmpac_rom == "custom/fm.rom", "FullValid_FmpacRom");
        expect(cfg.fmpac_sram == "custom/fm.sram", "FullValid_FmpacSram");
        expect(cfg.cartridge_dir == "customcarts", "FullValid_CartridgeDir");  // M64
        expect(cfg.disk_dir == "customdisks", "FullValid_DiskDir");            // M64
        expect(cfg.softwaredb_path == "custom/sw.xml", "FullValid_SoftwareDb");

        // Non-tautology guard: this parsed config MUST differ from the defaults.
        expect(!(cfg == kDefaults), "FullValid_DiffersFromDefaults");
    }

    // ========================================================================
    // 2. Per-key validation. Each fixture pairs ONE invalid key with ONE valid
    //    co-present key; asserts (a) a WARNING names the bad key, (b) the bad
    //    field == its built-in default, (c) the valid co-present key applied.
    //    (AC-S1-2)
    // ========================================================================

    // Bad bool (fast-disk) + valid co-present border.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<fast-disk enabled=\"maybe\"/>"
            "<border enabled=\"true\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/fast-disk@enabled"), "BadBool_WarnNamesKey");
        expect(cfg.fast_disk == kDefaults.fast_disk, "BadBool_KeepsDefault");
        expect(cfg.border_enabled == true, "BadBool_CoPresentValidApplied");
    }

    // fmpac slot out of range (3) + valid co-present autoload.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<fmpac autoload=\"false\" slot=\"3\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/fmpac@slot"), "BadSlot_WarnNamesKey");
        expect(cfg.fmpac_slot == kDefaults.fmpac_slot, "BadSlot_KeepsDefault");
        expect(cfg.fmpac_autoload == false, "BadSlot_CoPresentAutoloadApplied");
    }

    // speed level out of range (9) + valid co-present video scale.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<speed level=\"9\"/>"
            "<video scale=\"4\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/speed@level"), "BadSpeed_WarnNamesKey");
        expect(cfg.speed_level == kDefaults.speed_level, "BadSpeed_KeepsDefault");
        expect(cfg.video_scale == 4, "BadSpeed_CoPresentScaleApplied");
    }

    // video scale out of range (0) + bad filter, both on the same element.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<video scale=\"0\" filter=\"fuzzy\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/video@scale"), "BadScale_WarnNamesKey");
        expect(any_warning_contains(w, "defaults/video@filter"), "BadFilter_WarnNamesKey");
        expect(cfg.video_scale == kDefaults.video_scale, "BadScale_KeepsDefault");
        expect(cfg.video_filter == kDefaults.video_filter, "BadFilter_KeepsDefault");
    }

    // persistence percent out of range (150) + bad mode.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<persistence percent=\"150\" mode=\"loud\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/persistence@percent"), "BadPercent_WarnNamesKey");
        expect(any_warning_contains(w, "defaults/persistence@mode"), "BadMode_WarnNamesKey");
        expect(cfg.persistence_percent == kDefaults.persistence_percent, "BadPercent_KeepsDefault");
        expect(cfg.persistence_mode == kDefaults.persistence_mode, "BadMode_KeepsDefault");
    }

    // M52 (DEC-0079): <volume percent> valid boundaries + out-of-range + bad type
    // + missing -> warn+keep-default(100). Mirrors the persistence@percent policy.
    {
        std::vector<std::string> w0;
        const auto c0 = EmulatorConfig::parse(
            "<hbf1xv-config><defaults><volume percent=\"0\"/></defaults></hbf1xv-config>", w0);
        expect(w0.empty() && c0.master_volume == 0, "Volume0_Parses_NoWarn");
        std::vector<std::string> w100;
        const auto c100 = EmulatorConfig::parse(
            "<hbf1xv-config><defaults><volume percent=\"100\"/></defaults></hbf1xv-config>", w100);
        expect(w100.empty() && c100.master_volume == 100, "Volume100_Parses_NoWarn");
        // Out-of-range (150) + a valid co-present sibling -> warn names key, keeps 100.
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<volume percent=\"150\"/>"
            "<border enabled=\"true\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/volume@percent"), "BadVolume_WarnNamesKey");
        expect(cfg.master_volume == kDefaults.master_volume, "BadVolume_KeepsDefault100");
        expect(cfg.border_enabled == true, "BadVolume_CoPresentValidApplied");
        // Non-numeric percent -> warn + default.
        std::vector<std::string> wn;
        const auto cn = EmulatorConfig::parse(
            "<hbf1xv-config><defaults><volume percent=\"loud\"/></defaults></hbf1xv-config>", wn);
        expect(any_warning_contains(wn, "defaults/volume@percent"), "BadVolumeType_WarnNamesKey");
        expect(cn.master_volume == 100, "BadVolumeType_KeepsDefault100");
        // Omitted <volume> -> default 100, no warning.
        std::vector<std::string> wo;
        const auto co = EmulatorConfig::parse(
            "<hbf1xv-config><defaults><border enabled=\"true\"/></defaults></hbf1xv-config>", wo);
        expect(wo.empty() && co.master_volume == 100, "VolumeOmitted_Default100_NoWarn");
    }

    // ram bad enum (96) + valid co-present vram.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><machine>"
            "<ram kb=\"96\"/>"
            "<vram kb=\"128\"/>"
            "</machine></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "machine/ram@kb"), "BadRam_WarnNamesKey");
        expect(cfg.ram_kb == kDefaults.ram_kb, "BadRam_KeepsDefault");
        expect(cfg.vram_kb == 128, "BadRam_CoPresentVramApplied");
    }

    // vram not 128 (256) -> validated-to-128, warns naming key.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><machine><vram kb=\"256\"/></machine></hbf1xv-config>", w);
        expect(any_warning_contains(w, "machine/vram@kb"), "BadVram_WarnNamesKey");
        expect(cfg.vram_kb == 128, "BadVram_ClampedTo128");
    }

    // bad bios rom role -> warns naming key; valid roles on the same <bios> apply.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><machine><bios dir=\"bios\">"
            "<rom role=\"nonsense\" file=\"x.rom\"/>"
            "<rom role=\"disk\" file=\"good-disk.rom\"/>"
            "</bios></machine></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "machine/bios/rom@role"), "BadRole_WarnNamesKey");
        expect(cfg.bios_roms.disk == "good-disk.rom", "BadRole_ValidRoleApplied");
        expect(cfg.bios_roms.bios == kDefaults.bios_roms.bios, "BadRole_UnsetRolesDefault");
    }

    // M64: unknown attribute on <cartridge> -> warns naming machine/cartridge@
    // <attr>; the known dir attr on the co-present <disk> still applies.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><machine>"
            "<cartridge dir=\"mycarts\" bogus=\"1\"/>"
            "<disk dir=\"mydisks\"/>"
            "</machine></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "machine/cartridge@bogus"),
               "CartridgeUnknownAttr_WarnNamesKey");
        expect(cfg.cartridge_dir == "mycarts", "CartridgeUnknownAttr_KnownAttrStillApplies");
        expect(cfg.disk_dir == "mydisks", "CartridgeUnknownAttr_CoPresentDiskApplied");
    }

    // M64: <cartridge>/<disk> absent -> the built-in dialog defaults
    // "roms"/"disks" hold (and an unrelated machine key parses cleanly).
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><machine><ram kb=\"128\"/></machine></hbf1xv-config>", w);
        expect(w.empty(), "DialogDirsAbsent_NoWarnings");
        expect(cfg.cartridge_dir == "roms", "DialogDirsAbsent_CartridgeDirDefaultRoms");
        expect(cfg.disk_dir == "disks", "DialogDirsAbsent_DiskDirDefaultDisks");
    }

    // Unknown key (element) -> warns naming it; other keys still apply.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<frobnicate value=\"1\"/>"
            "<border enabled=\"true\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "frobnicate"), "UnknownKey_WarnNamesKey");
        expect(cfg.border_enabled == true, "UnknownKey_OtherKeysApply");
        expect(cfg == [&] {
            EmulatorConfig e = kDefaults;
            e.border_enabled = true;
            return e;
        }(), "UnknownKey_OnlyBorderChanged");
    }

    // Unknown attribute on a known element -> warns naming <elem>@<attr>.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse(
            "<hbf1xv-config><defaults>"
            "<speed level=\"2\" bogus=\"7\"/>"
            "</defaults></hbf1xv-config>",
            w);
        expect(any_warning_contains(w, "defaults/speed@bogus"), "UnknownAttr_WarnNamesKey");
        expect(cfg.speed_level == 2, "UnknownAttr_KnownAttrStillApplies");
    }

    // ========================================================================
    // 3. Structural failures -> ONE whole-file warning + all built-in defaults,
    //    never a throw. (AC-S1-3)
    // ========================================================================

    // Binary/garbage with no XML at all.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse("this is not xml at all %%%\x01\x02", w);
        expect(cfg == kDefaults, "Garbage_ReturnsDefaults");
        expect(any_warning_contains(w, "not a valid hbf1xv-config"), "Garbage_WholeFileWarning");
        expect(w.size() == 1, "Garbage_ExactlyOneWarning");
    }

    // Wrong root element.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse("<wrongroot><ram kb=\"128\"/></wrongroot>", w);
        expect(cfg == kDefaults, "WrongRoot_ReturnsDefaults");
        expect(any_warning_contains(w, "not a valid hbf1xv-config"), "WrongRoot_WholeFileWarning");
        expect(cfg.ram_kb == kDefaults.ram_kb, "WrongRoot_NoLeakage");
    }

    // Empty input -> whole-file fallback + a warning.
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse("", w);
        expect(cfg == kDefaults, "Empty_ReturnsDefaults");
        expect(!w.empty(), "Empty_HasWarning");
    }

    // ========================================================================
    // 4. Built-in-default config == compiled defaults (round-trip identity of
    //    an empty-but-valid root produces exactly the compiled defaults, no
    //    warnings). (AC-S1-1 / A-1 base)
    // ========================================================================
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::parse("<hbf1xv-config version=\"1\"></hbf1xv-config>", w);
        expect(cfg == kDefaults, "EmptyRoot_EqualsCompiledDefaults");
        expect(w.empty(), "EmptyRoot_NoWarnings");

        // Self-closing empty root is equally valid.
        std::vector<std::string> w2;
        const auto cfg2 = EmulatorConfig::parse("<hbf1xv-config/>", w2);
        expect(cfg2 == kDefaults, "SelfCloseRoot_EqualsCompiledDefaults");
        expect(w2.empty(), "SelfCloseRoot_NoWarnings");
    }

    // Confirm the compiled defaults are exactly the documented built-in values
    // (proves the model's defaults equal the schema's built-in-default column).
    {
        expect(kDefaults.fast_disk == true && kDefaults.fmpac_autoload == true &&
                   kDefaults.fmpac_slot == 2 && kDefaults.border_enabled == false &&
                   // M52 (DEC-0079, §4.5 enumerated flip): disk-writable default
                   // false -> true (SDL3 convenience; headless stays OFF). master
                   // volume default 100 (unity).
                   kDefaults.disk_writable == true && kDefaults.master_volume == 100 &&
                   kDefaults.speed_level == 0 &&
                   kDefaults.video_scale == 3 && kDefaults.video_filter == "linear" &&
                   kDefaults.persistence_percent == 0 && kDefaults.persistence_mode == "avg" &&
                   kDefaults.fullscreen == false && kDefaults.capture_enabled == false &&
                   kDefaults.ram_kb == 512 && kDefaults.vram_kb == 128 &&
                   kDefaults.bios_dir == "bios",
               "CompiledDefaults_MatchSchemaDefaults_A");
        expect(kDefaults.bios_roms.bios == "f1xvbios.rom" &&
                   kDefaults.bios_roms.sub == "f1xvext.rom" &&
                   kDefaults.bios_roms.kanji_driver == "f1xvkdr.rom" &&
                   kDefaults.bios_roms.disk == "f1xvdisk.rom" &&
                   kDefaults.bios_roms.fm_music == "f1xvmus.rom" &&
                   kDefaults.bios_roms.kanji_font == "f1xvkfn.rom" &&
                   kDefaults.bios_roms.firmware == "f1xvfirm.rom" &&
                   kDefaults.fmpac_rom == "roms/fmpac.rom" &&
                   kDefaults.fmpac_sram == "roms/fmpac.rom.sram" &&
                   // M64: the in-window file-dialog default directories.
                   kDefaults.cartridge_dir == "roms" && kDefaults.disk_dir == "disks" &&
                   kDefaults.softwaredb_path ==
                       "references/openmsx-21.0/share/softwaredb.xml",
               "CompiledDefaults_MatchSchemaDefaults_B");
    }

    // ========================================================================
    // 5. load_from_file: absent -> defaults + warning; present -> parse-equals.
    // ========================================================================
    {
        std::vector<std::string> w;
        const auto cfg = EmulatorConfig::load_from_file(
            "this/path/genuinely/does/not/exist/sony_msx_hbf1xv.xml", w);
        expect(cfg == kDefaults, "AbsentFile_ReturnsDefaults");
        expect(!w.empty(), "AbsentFile_HasWarning");
    }
    {
        const std::string xml =
            "<hbf1xv-config><machine><ram kb=\"256\"/></machine></hbf1xv-config>";
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "sony_msx_m50_config_fixtures";
        std::filesystem::create_directories(dir);
        const std::filesystem::path path = dir / "cfg.xml";
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << xml;
        }

        std::vector<std::string> wf;
        const auto from_file = EmulatorConfig::load_from_file(path.string(), wf);
        std::vector<std::string> wt;
        const auto from_text = EmulatorConfig::parse(xml, wt);
        expect(from_file == from_text, "LoadFromFile_EqualsParseText");
        expect(from_file.ram_kb == 256, "LoadFromFile_ValueApplied");
        expect(wf.empty() && wt.empty(), "LoadFromFile_NoWarnings");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_EmulatorConfig_Unit cases passed\n";
    return 0;
}
