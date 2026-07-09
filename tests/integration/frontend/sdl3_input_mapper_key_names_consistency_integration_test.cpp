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

#include <SDL3/SDL.h>

#include <array>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include "frontend/sdl3_input_mapper.h"
#include "peripherals/msx_key_names.h"

// Suite: Frontend_Sdl3InputMapperKeyNamesConsistency_Integration (M27-S7,
// "Production Hardening + Debug/Test Tooling" item 3, docs/m27-planner-
// package.md §2.4/§3 M27-S7). The HARD cross-consistency gate named by
// R-M27-4: for every one of the 71 entries in
// Sdl3InputMapper::scancode_map() (src/frontend/sdl3_input_mapper.cpp, M26),
// asserts peripherals::key_name_to_row_col(<the corresponding name>) equals
// Sdl3InputMapper::map_scancode(<the corresponding scancode>) EXACTLY --
// proving src/peripherals/msx_key_names.cpp's table (built headlessly,
// SDL3-independent) genuinely agrees with the real, already-tested SDL3
// scancode table, not merely asserted in prose. Every (scancode, name) pair
// below was independently re-derived by directly reading
// sdl3_input_mapper.cpp's own array literal this cycle (never copied from
// msx_key_names.cpp itself, which would make this check circular/vacuous).

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

struct Pair {
    SDL_Scancode scancode;
    std::string_view name;
};

}  // namespace

int main() {
    using sony_msx::frontend::Sdl3InputMapper;
    using sony_msx::peripherals::key_name_to_row_col;

    // The FULL 71-entry correspondence, independently re-derived from
    // sdl3_input_mapper.cpp's own kMap array literal this cycle (same order,
    // same rows/columns implied by each SDL_Scancode -- verified via
    // Sdl3InputMapper::map_scancode() itself below, not merely asserted).
    static constexpr std::array<Pair, 71> kPairs{{
        {SDL_SCANCODE_0, "0"},        {SDL_SCANCODE_1, "1"},        {SDL_SCANCODE_2, "2"},
        {SDL_SCANCODE_3, "3"},        {SDL_SCANCODE_4, "4"},        {SDL_SCANCODE_5, "5"},
        {SDL_SCANCODE_6, "6"},        {SDL_SCANCODE_7, "7"},
        {SDL_SCANCODE_8, "8"},        {SDL_SCANCODE_9, "9"},        {SDL_SCANCODE_MINUS, "MINUS"},
        {SDL_SCANCODE_EQUALS, "EQUALS"}, {SDL_SCANCODE_BACKSLASH, "BACKSLASH"},
        {SDL_SCANCODE_LEFTBRACKET, "LEFTBRACKET"}, {SDL_SCANCODE_RIGHTBRACKET, "RIGHTBRACKET"},
        {SDL_SCANCODE_SEMICOLON, "SEMICOLON"},
        {SDL_SCANCODE_APOSTROPHE, "APOSTROPHE"}, {SDL_SCANCODE_COMMA, "COMMA"},
        {SDL_SCANCODE_PERIOD, "PERIOD"}, {SDL_SCANCODE_SLASH, "SLASH"}, {SDL_SCANCODE_GRAVE, "GRAVE"},
        {SDL_SCANCODE_A, "A"},        {SDL_SCANCODE_B, "B"},
        {SDL_SCANCODE_C, "C"},        {SDL_SCANCODE_D, "D"},        {SDL_SCANCODE_E, "E"},
        {SDL_SCANCODE_F, "F"},        {SDL_SCANCODE_G, "G"},        {SDL_SCANCODE_H, "H"},
        {SDL_SCANCODE_I, "I"},        {SDL_SCANCODE_J, "J"},
        {SDL_SCANCODE_K, "K"},        {SDL_SCANCODE_L, "L"},        {SDL_SCANCODE_M, "M"},
        {SDL_SCANCODE_N, "N"},        {SDL_SCANCODE_O, "O"},        {SDL_SCANCODE_P, "P"},
        {SDL_SCANCODE_Q, "Q"},        {SDL_SCANCODE_R, "R"},
        {SDL_SCANCODE_S, "S"},        {SDL_SCANCODE_T, "T"},        {SDL_SCANCODE_U, "U"},
        {SDL_SCANCODE_V, "V"},        {SDL_SCANCODE_W, "W"},        {SDL_SCANCODE_X, "X"},
        {SDL_SCANCODE_Y, "Y"},        {SDL_SCANCODE_Z, "Z"},
        {SDL_SCANCODE_LSHIFT, "LSHIFT"}, {SDL_SCANCODE_LCTRL, "LCTRL"}, {SDL_SCANCODE_LALT, "LALT"},
        {SDL_SCANCODE_CAPSLOCK, "CAPSLOCK"}, {SDL_SCANCODE_LGUI, "LGUI"},
        {SDL_SCANCODE_F1, "F1"},      {SDL_SCANCODE_F2, "F2"},      {SDL_SCANCODE_F3, "F3"},
        {SDL_SCANCODE_F4, "F4"},      {SDL_SCANCODE_F5, "F5"},      {SDL_SCANCODE_ESCAPE, "ESCAPE"},
        {SDL_SCANCODE_TAB, "TAB"},    {SDL_SCANCODE_END, "END"},    {SDL_SCANCODE_BACKSPACE, "BACKSPACE"},
        {SDL_SCANCODE_PAGEUP, "PAGEUP"}, {SDL_SCANCODE_RETURN, "RETURN"},
        {SDL_SCANCODE_SPACE, "SPACE"}, {SDL_SCANCODE_HOME, "HOME"}, {SDL_SCANCODE_INSERT, "INSERT"},
        {SDL_SCANCODE_DELETE, "DELETE"}, {SDL_SCANCODE_LEFT, "LEFT"}, {SDL_SCANCODE_UP, "UP"},
        {SDL_SCANCODE_DOWN, "DOWN"},  {SDL_SCANCODE_RIGHT, "RIGHT"},
    }};

    expect(kPairs.size() == Sdl3InputMapper::scancode_map().size(),
           "PairTable_SameSizeAs_Sdl3InputMapperScancodeMap_71Entries");

    bool all_agree = true;
    for (const Pair& p : kPairs) {
        const std::optional<std::pair<int, int>> from_scancode = Sdl3InputMapper::map_scancode(p.scancode);
        const std::optional<std::pair<int, int>> from_name = key_name_to_row_col(p.name);

        if (!from_scancode.has_value() || !from_name.has_value()) {
            std::cerr << "  missing mapping for name=\"" << p.name << "\" scancode=" << static_cast<int>(p.scancode)
                       << "\n";
            all_agree = false;
            continue;
        }
        if (*from_scancode != *from_name) {
            std::cerr << "  mismatch for \"" << p.name << "\": scancode->(" << from_scancode->first << ","
                       << from_scancode->second << ") vs name->(" << from_name->first << "," << from_name->second
                       << ")\n";
            all_agree = false;
        }
    }
    expect(all_agree, "AllSeventyOneEntries_ScancodeTable_AgreesWithKeyNameTable_ExactRowColumnMatch");

    // Every entry in Sdl3InputMapper::scancode_map() itself was covered
    // above (not merely a subset) -- a completeness check against the REAL
    // table's own size, confirming kPairs did not silently omit an entry.
    bool every_scancode_map_entry_covered = true;
    for (const auto& binding : Sdl3InputMapper::scancode_map()) {
        bool found = false;
        for (const Pair& p : kPairs) {
            if (p.scancode == binding.scancode) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "  scancode_map() entry not covered by this test's own table: scancode="
                       << static_cast<int>(binding.scancode) << "\n";
            every_scancode_map_entry_covered = false;
        }
    }
    expect(every_scancode_map_entry_covered, "EveryScancodeMapEntry_CoveredByThisTests_PairTable_NoneSilentlyOmitted");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3InputMapperKeyNamesConsistency_Integration cases passed\n";
    return 0;
}
