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

#pragma once

#include <string>

namespace sony_msx::frontend {

// ---------------------------------------------------------------------------
// M63: the pure default-directory pick for the Machine > BIOS Folder... picker.
// Filesystem-free (the caller injects the "exists && is_directory" verdict and
// the already-absolute path strings), so the selection policy is ctest-provable
// with no real directories -- the master_volume.h / dialog_result.h precedent.
//
// Policy: prefer the CURRENT BIOS directory (config_.bios_dir) when it is a
// real directory, so the picker opens where the ROMs already live; otherwise
// fall back to the emulator's working directory; "" means "no preference"
// (the caller passes SDL_ShowOpenFolderDialog a nullptr default_location,
// today's behavior).
// ---------------------------------------------------------------------------

// bios_dir: the absolute-resolved current BIOS directory ("" if unresolvable).
// cwd: the absolute-resolved working directory ("" if unresolvable).
// bios_dir_is_directory: the injected "exists && is_directory" verdict.
[[nodiscard]] inline std::string choose_bios_dialog_dir(const std::string& bios_dir,
                                                        const std::string& cwd,
                                                        const bool bios_dir_is_directory) {
    if (bios_dir_is_directory && !bios_dir.empty()) {
        return bios_dir;
    }
    return cwd;  // may be "" = no preference (caller passes nullptr to SDL)
}

}  // namespace sony_msx::frontend
