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

#include "peripherals/msx_key_names.h"

#include <array>

namespace sony_msx::peripherals {

namespace {

struct KeyNameBinding {
    std::string_view name;
    int row;
    int column;
};

}  // namespace

std::optional<std::pair<int, int>> key_name_to_row_col(const std::string_view name) {
    // Re-expression of src/frontend/sdl3_input_mapper.cpp's `kMap` array
    // literal (72 entries, rows 0-8), re-keyed by the SDL_Scancode
    // enumerator's suffix string instead of the SDL_Scancode value. Every
    // (row, column) pair below was copied verbatim, in the same
    // order/grouping, from that file (never independently re-derived) --
    // see the header doc comment / R-M27-4. A dedicated SDL3-gated
    // cross-consistency test proves the two tables agree.
    static const std::array<KeyNameBinding, 72> kMap{{
        // Row 0: digits 0-7
        {"0", 0, 0},
        {"1", 0, 1},
        {"2", 0, 2},
        {"3", 0, 3},
        {"4", 0, 4},
        {"5", 0, 5},
        {"6", 0, 6},
        {"7", 0, 7},
        // Row 1: 8 9 - = \ [ ] ;
        {"8", 1, 0},
        {"9", 1, 1},
        {"MINUS", 1, 2},
        {"EQUALS", 1, 3},
        {"BACKSLASH", 1, 4},
        {"LEFTBRACKET", 1, 5},
        {"RIGHTBRACKET", 1, 6},
        {"SEMICOLON", 1, 7},
        // Row 2: ":"/"*" (col0, mapped to Right-Ctrl -> "RCTRL", mirrors
        // scancode_map()'s SDL_SCANCODE_RCTRL binding) ' , . / ` A B
        {"RCTRL", 2, 0},
        {"APOSTROPHE", 2, 1},
        {"COMMA", 2, 2},
        {"PERIOD", 2, 3},
        {"SLASH", 2, 4},
        {"GRAVE", 2, 5},
        {"A", 2, 6},
        {"B", 2, 7},
        // Row 3: C D E F G H I J
        {"C", 3, 0},
        {"D", 3, 1},
        {"E", 3, 2},
        {"F", 3, 3},
        {"G", 3, 4},
        {"H", 3, 5},
        {"I", 3, 6},
        {"J", 3, 7},
        // Row 4: K L M N O P Q R
        {"K", 4, 0},
        {"L", 4, 1},
        {"M", 4, 2},
        {"N", 4, 3},
        {"O", 4, 4},
        {"P", 4, 5},
        {"Q", 4, 6},
        {"R", 4, 7},
        // Row 5: S T U V W X Y Z
        {"S", 5, 0},
        {"T", 5, 1},
        {"U", 5, 2},
        {"V", 5, 3},
        {"W", 5, 4},
        {"X", 5, 5},
        {"Y", 5, 6},
        {"Z", 5, 7},
        // Row 6: SHIFT CTRL GRAPH CAPS CODE F1 F2 F3
        {"LSHIFT", 6, 0},
        {"LCTRL", 6, 1},
        {"LALT", 6, 2},
        {"CAPSLOCK", 6, 3},
        {"LGUI", 6, 4},
        {"F1", 6, 5},
        {"F2", 6, 6},
        {"F3", 6, 7},
        // Row 7: F4 F5 ESC TAB STOP BS SELECT RETURN
        {"F4", 7, 0},
        {"F5", 7, 1},
        {"ESCAPE", 7, 2},
        {"TAB", 7, 3},
        {"END", 7, 4},
        {"BACKSPACE", 7, 5},
        {"PAGEUP", 7, 6},
        {"RETURN", 7, 7},
        // Row 8: SPACE HOME INS DEL LEFT UP DOWN RIGHT
        {"SPACE", 8, 0},
        {"HOME", 8, 1},
        {"INSERT", 8, 2},
        {"DELETE", 8, 3},
        {"LEFT", 8, 4},
        {"UP", 8, 5},
        {"DOWN", 8, 6},
        {"RIGHT", 8, 7},
    }};

    for (const KeyNameBinding& binding : kMap) {
        if (binding.name == name) {
            return std::make_pair(binding.row, binding.column);
        }
    }
    return std::nullopt;
}

}  // namespace sony_msx::peripherals
