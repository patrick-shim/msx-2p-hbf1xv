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

// M56 (DEC-0084, planner §3.1 / §9 test 1): the SDL-free classification of an
// SDL file-dialog callback's `filelist` argument, extracted as a pure function
// so it is unit-testable OUTSIDE the SONY_MSX_ENABLE_SDL3 guard (the same
// SDL-free-frontend-logic pattern as menu_captures_event in menu_model.h). The
// SDL_dialog contract (references/sdl3/include/SDL3/SDL_dialog.h:81-88):
//   * filelist == NULL      -> an error occurred (SDL_GetError()).
//   * filelist[0] == NULL   -> the user cancelled / chose nothing.
//   * filelist[0] != NULL   -> one or more chosen UTF-8 paths.

namespace sony_msx::frontend {

enum class DialogResultKind { Error, Cancel, Selection };

// `filelist_null` == (filelist == nullptr); `first_null` == (filelist[0] ==
// nullptr), only meaningful when !filelist_null.
constexpr DialogResultKind classify_dialog_filelist(const bool filelist_null,
                                                    const bool first_null) {
    if (filelist_null) {
        return DialogResultKind::Error;
    }
    if (first_null) {
        return DialogResultKind::Cancel;
    }
    return DialogResultKind::Selection;
}

}  // namespace sony_msx::frontend
