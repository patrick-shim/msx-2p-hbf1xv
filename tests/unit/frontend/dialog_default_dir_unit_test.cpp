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

#include "frontend/dialog_default_dir.h"

// Suite: Frontend_DialogDefaultDir_Unit (M63, generalized at M64). The oracle
// for the pure, filesystem-free choose_dialog_dir() policy
// (frontend/dialog_default_dir.h) shared by ALL the in-window dialog
// launchers' default_location (Open Cartridge -> cartridge_dir, Open Disk(s) /
// New Blank Disk -> disk_dir, Machine > BIOS Folder... -> bios_dir): prefer
// the configured dir when it is a real directory, else the working directory,
// else "" (= SDL nullptr, no preference). The caller injects the
// "exists && is_directory" verdict, so no real directories are needed here.
// SDL-free -> registered OUTSIDE the SONY_MSX_ENABLE_SDL3 guard (counts in
// BOTH the ON and OFF denominators).

namespace {

using sony_msx::frontend::choose_dialog_dir;

int g_failures = 0;

void expect(const bool ok, const std::string& case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Case A: configured dir is a real directory -> preferred over cwd ----
    {
        const std::string r = choose_dialog_dir("D:\\emu\\bios", "D:\\emu", true);
        expect(r == "D:\\emu\\bios", "A_prefers_configured_when_directory");
    }
    {
        // Preferred even when cwd is empty (the cwd query failed).
        const std::string r = choose_dialog_dir("D:\\emu\\roms", "", true);
        expect(r == "D:\\emu\\roms", "A_prefers_configured_over_empty_cwd");
    }

    // --- Case B: configured dir NOT a directory -> falls back to cwd ---------
    {
        const std::string r = choose_dialog_dir("D:\\gone\\disks", "D:\\emu", false);
        expect(r == "D:\\emu", "B_falls_back_to_cwd_when_not_directory");
    }
    {
        // An empty configured dir with a (defensively wrong) true verdict still
        // falls back: an empty string can never be a preferred location.
        const std::string r = choose_dialog_dir("", "D:\\emu", true);
        expect(r == "D:\\emu", "B_empty_configured_falls_back_despite_flag");
    }

    // --- Case C: nothing resolvable -> "" (= SDL nullptr, no preference) -----
    {
        const std::string r = choose_dialog_dir("", "", false);
        expect(r.empty(), "C_both_empty_returns_no_preference");
    }
    {
        const std::string r = choose_dialog_dir("", "", true);
        expect(r.empty(), "C_both_empty_flag_true_returns_no_preference");
    }
    {
        // Non-directory configured dir + failed cwd -> "" as well.
        const std::string r = choose_dialog_dir("D:\\gone\\roms", "", false);
        expect(r.empty(), "C_not_directory_and_empty_cwd_returns_no_preference");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_DialogDefaultDir_Unit cases passed\n";
    return 0;
}
