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

#include <array>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include "peripherals/msx_key_names.h"

// Suite: Peripherals_MsxKeyNames_Unit
//
// Every entry asserted here was copied verbatim from
// src/frontend/sdl3_input_mapper.cpp's own `kMap` array literal
// -- this file re-confirms the SAME 72 (name -> (row, column))
// facts, independently re-expressed by name. The SDL3-gated cross-
// consistency test (tests/integration/frontend/
// sdl3_input_mapper_key_names_consistency_integration_test.cpp) is the HARD
// gate proving the two tables actually agree against the real
// Sdl3InputMapper; this headless test proves msx_key_names.cpp's own table
// is internally self-consistent and complete.

namespace {

using sony_msx::peripherals::key_name_to_row_col;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

struct Expected {
    std::string_view name;
    int row;
    int column;
};

}  // namespace

int main() {
    // --- Case 1: every one of the 72 documented entries resolves to its
    // documented (row, column) -- the SAME facts src/frontend/
    // sdl3_input_mapper.cpp's kMap array literal records. ---
    static constexpr std::array<Expected, 72> kExpected{{
        {"0", 0, 0},        {"1", 0, 1},        {"2", 0, 2},        {"3", 0, 3},
        {"4", 0, 4},        {"5", 0, 5},        {"6", 0, 6},        {"7", 0, 7},
        {"8", 1, 0},        {"9", 1, 1},        {"MINUS", 1, 2},    {"EQUALS", 1, 3},
        {"BACKSLASH", 1, 4}, {"LEFTBRACKET", 1, 5}, {"RIGHTBRACKET", 1, 6}, {"SEMICOLON", 1, 7},
        {"RCTRL", 2, 0},    {"APOSTROPHE", 2, 1}, {"COMMA", 2, 2},   {"PERIOD", 2, 3},
        {"SLASH", 2, 4},    {"GRAVE", 2, 5},     {"A", 2, 6},        {"B", 2, 7},
        {"C", 3, 0},        {"D", 3, 1},         {"E", 3, 2},        {"F", 3, 3},
        {"G", 3, 4},        {"H", 3, 5},         {"I", 3, 6},        {"J", 3, 7},
        {"K", 4, 0},        {"L", 4, 1},         {"M", 4, 2},        {"N", 4, 3},
        {"O", 4, 4},        {"P", 4, 5},         {"Q", 4, 6},        {"R", 4, 7},
        {"S", 5, 0},        {"T", 5, 1},         {"U", 5, 2},        {"V", 5, 3},
        {"W", 5, 4},        {"X", 5, 5},         {"Y", 5, 6},        {"Z", 5, 7},
        {"LSHIFT", 6, 0},   {"LCTRL", 6, 1},     {"LALT", 6, 2},     {"CAPSLOCK", 6, 3},
        {"LGUI", 6, 4},     {"F1", 6, 5},        {"F2", 6, 6},       {"F3", 6, 7},
        {"F4", 7, 0},       {"F5", 7, 1},        {"ESCAPE", 7, 2},   {"TAB", 7, 3},
        {"END", 7, 4},      {"BACKSPACE", 7, 5}, {"PAGEUP", 7, 6},   {"RETURN", 7, 7},
        {"SPACE", 8, 0},    {"HOME", 8, 1},      {"INSERT", 8, 2},   {"DELETE", 8, 3},
        {"LEFT", 8, 4},     {"UP", 8, 5},        {"DOWN", 8, 6},     {"RIGHT", 8, 7},
    }};

    bool all_resolved = true;
    for (const Expected& e : kExpected) {
        const std::optional<std::pair<int, int>> got = key_name_to_row_col(e.name);
        if (!got.has_value() || got->first != e.row || got->second != e.column) {
            std::cerr << "  mismatch for \"" << e.name << "\": expected (" << e.row << "," << e.column << ")\n";
            all_resolved = false;
        }
    }
    expect(all_resolved, "AllDocumentedNames_ResolveToDocumentedRowColumn");
    expect(kExpected.size() == 72, "ExpectedTable_Has72Entries_MatchesScancodeMapSize");

    // --- Case 2: an unmapped/unknown name returns std::nullopt (regression
    // guard). Matrix cell (2,0) -- the MSX ":"/"*" key -- is now reachable
    // via the name "RCTRL" (see below); the literal name "COLON" was never a
    // table key (the table is keyed by SDL_Scancode enumerator suffixes, and
    // no SDL_SCANCODE_COLON exists), so it still resolves to nullopt. ---
    expect(!key_name_to_row_col("NOSUCHKEY").has_value(), "UnknownName_ReturnsNullopt");
    expect(!key_name_to_row_col("").has_value(), "EmptyName_ReturnsNullopt");
    expect(!key_name_to_row_col("COLON").has_value(), "ColonName_NotATableKey_ReturnsNullopt");
    // Matrix cell (2,0), the MSX ":"/"*" key, is reachable via "RCTRL"
    // (Right-Ctrl -> ":", Shift+Right-Ctrl -> "*").
    {
        const std::optional<std::pair<int, int>> rctrl = key_name_to_row_col("RCTRL");
        expect(rctrl.has_value() && rctrl->first == 2 && rctrl->second == 0,
               "RCTRL_ResolvesToRow2Column0_MsxColonStarKey");
    }
    // Case-sensitive: lowercase never matches (names are always uppercase).
    expect(!key_name_to_row_col("a").has_value(), "LowercaseName_CaseSensitive_ReturnsNullopt");
    // Numeric-keypad rows 9-10 are unmapped (an inherited boundary
    // from Sdl3InputMapper::scancode_map()).
    expect(!key_name_to_row_col("KP_0").has_value(), "NumericKeypadRow_OutOfScope_ReturnsNullopt");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Peripherals_MsxKeyNames_Unit cases passed\n";
    return 0;
}
