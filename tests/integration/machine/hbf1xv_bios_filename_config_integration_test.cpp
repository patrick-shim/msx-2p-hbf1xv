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

// Suite: Machine_Hbf1xvBiosFilenameConfig_Integration
//
// Proves the config-fed role-keyed BIOS FILENAMES flow through the machine into
// the actual RomAssetLoader lookup path (Hbf1xvMachine::set_bios_filenames() ->
// load_rom_assets() -> RomAssetLoader::load()) -- so a user can point at
// differently-named BIOS files via config. Asset-INDEPENDENT + deterministic:
// it drives an EMPTY temp asset root and reads the missing-asset diagnostics
// (which name the RESOLVED path root/filename), so it passes with or without the
// real bios/*.rom present. Non-tautological: it asserts the CUSTOM filename
// appears in the loader diagnostic AND the default filename does NOT (the
// literal was truly replaced), and a positive-load case where the custom-named
// file EXISTS at the right size loads WITHOUT a diagnostic.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/emulator_config.h"
#include "machine/hbf1xv_machine.h"

namespace {

namespace fs = std::filesystem;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
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
    using sony_msx::machine::EmulatorConfig;
    using sony_msx::machine::Hbf1xvMachine;

    const fs::path base = fs::temp_directory_path() / "hbf1xv_bios_filenames";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);

    // --- Case A: DEFAULT filenames on an EMPTY root -> the loader diagnostics
    //     name the strict-spec default filenames (regression baseline). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(base);
        machine.cold_boot();
        const std::vector<std::string>& diag = machine.rom_diagnostics();
        expect(any_contains(diag, "f1xvbios.rom"), "Default_DiagNamesDefaultBios");
        expect(any_contains(diag, "f1xvdisk.rom"), "Default_DiagNamesDefaultDisk");
        // The machine reports its effective filename set (default here).
        expect(machine.bios_filenames().bios == "f1xvbios.rom", "Default_AccessorBios");
    }

    // --- Case B: CUSTOM filenames -> the loader looks up the CONFIG-fed names,
    //     so the diagnostics name the custom files and NO LONGER name the
    //     hardcoded defaults for the overridden roles (the literal was replaced). ---
    {
        EmulatorConfig::BiosRoms names;  // start from the spec defaults
        names.bios = "renamed-bios.rom";
        names.disk = "renamed-disk.rom";

        Hbf1xvMachine machine;
        machine.set_bios_filenames(names);
        machine.set_asset_root(base);
        machine.cold_boot();
        const std::vector<std::string>& diag = machine.rom_diagnostics();

        expect(any_contains(diag, "renamed-bios.rom"), "Custom_DiagNamesCustomBios");
        expect(any_contains(diag, "renamed-disk.rom"), "Custom_DiagNamesCustomDisk");
        expect(!any_contains(diag, "f1xvbios.rom"), "Custom_DiagNoLongerNamesDefaultBios");
        expect(!any_contains(diag, "f1xvdisk.rom"), "Custom_DiagNoLongerNamesDefaultDisk");
        // A NON-overridden role still uses its strict-spec default filename.
        expect(any_contains(diag, "f1xvext.rom"), "Custom_UnoverriddenRoleKeepsDefault");
        expect(machine.bios_filenames().bios == "renamed-bios.rom", "Custom_AccessorReflectsSet");
    }

    // --- Case C: POSITIVE LOAD -- a custom-named BIOS file that EXISTS at the
    //     exact expected 0x8000 window size loads WITHOUT a diagnostic, proving
    //     the custom name is really resolved + read (not merely echoed). The
    //     other (absent) default-named roles still produce diagnostics. ---
    {
        const fs::path custom_bios = base / "present-bios.rom";
        {
            std::ofstream out(custom_bios, std::ios::binary | std::ios::trunc);
            const std::vector<char> bytes(0x8000, '\0');  // exact BIOS window size
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }

        EmulatorConfig::BiosRoms names;
        names.bios = "present-bios.rom";  // present @ correct size in `base`

        Hbf1xvMachine machine;
        machine.set_bios_filenames(names);
        machine.set_asset_root(base);
        machine.cold_boot();
        const std::vector<std::string>& diag = machine.rom_diagnostics();

        // The present, correctly-sized custom BIOS loads cleanly: NO diagnostic
        // names it (neither ABSENT nor SIZE_MISMATCH).
        expect(!any_contains(diag, "present-bios.rom"), "Positive_CustomBiosLoadedNoDiag");
        // A still-absent default-named role (SUB) is still reported -- confirming
        // the loader ran and only the custom BIOS resolved.
        expect(any_contains(diag, "f1xvext.rom"), "Positive_AbsentSubStillReported");
    }

    fs::remove_all(base, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvBiosFilenameConfig_Integration cases passed\n";
    return 0;
}
