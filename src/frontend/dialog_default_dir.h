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
// The pure default-directory pick shared by ALL the in-window dialog
// launchers (Open Cartridge -> config cartridge_dir, Open Disk(s)/New Blank
// Disk -> config disk_dir, Machine > BIOS Folder... -> config bios_dir).
// Filesystem-free (the caller injects the "exists && is_directory" verdict
// and the already-absolute path strings), so the selection policy is
// ctest-provable with no real directories -- the master_volume.h /
// dialog_result.h precedent.
//
// Policy: prefer the CONFIGURED directory when it is a real directory, so each
// dialog opens where its media already live; otherwise fall back to the
// emulator's working directory; "" means "no preference" (the caller passes
// the SDL_Show*Dialog a nullptr default_location, leaving the start directory
// to SDL).
// ---------------------------------------------------------------------------

// configured: the absolute-resolved configured directory ("" if unresolvable).
// cwd: the absolute-resolved working directory ("" if unresolvable).
// configured_is_directory: the injected "exists && is_directory" verdict.
[[nodiscard]] inline std::string choose_dialog_dir(const std::string& configured,
                                                   const std::string& cwd,
                                                   const bool configured_is_directory) {
    if (configured_is_directory && !configured.empty()) {
        return configured;
    }
    return cwd;  // may be "" = no preference (caller passes nullptr to SDL)
}

}  // namespace sony_msx::frontend
