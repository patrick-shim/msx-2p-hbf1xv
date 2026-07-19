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

#include <optional>
#include <string_view>
#include <utility>

namespace sony_msx::peripherals {

// Symbolic key-name -> (row, column) lookup for the MSX 11x8 keyboard matrix.
//
// Every entry below re-expresses `Sdl3InputMapper::scancode_map()`'s own
// already-tested array literal (src/frontend/sdl3_input_mapper.cpp) --
// rows 0-8 only (the same numeric-keypad-rows-9-10-excluded boundary that
// table already established, `sdl3_input_mapper.h:33-34`, inherited here for
// consistency, never silently
// extended). Row 2 column 0 (the MSX ":"/"*" key) is bound to "RCTRL", mirroring
// `scancode_map()`'s SDL_SCANCODE_RCTRL binding (Right-Ctrl -> ":",
// Shift+Right-Ctrl -> "*").
//
// Names are the literal `SDL_Scancode` enumerator suffix for every entry
// with a direct SDL3 counterpart (e.g. "MINUS" <-> SDL_SCANCODE_MINUS,
// "LSHIFT" <-> SDL_SCANCODE_LSHIFT), enabling a mechanical
// cross-consistency test
// (`tests/integration/frontend/
// sdl3_input_mapper_key_names_consistency_integration_test.cpp`) against
// `sdl3_input_mapper.cpp`'s own table, never an independently re-derived
// value. This header is intentionally SDL3-independent (no
// `SDL_Scancode` type appears here) so it stays headless-buildable, per
// the `src/peripherals/`-is-frontend-independent boundary rule.
[[nodiscard]] std::optional<std::pair<int, int>> key_name_to_row_col(std::string_view name);

// Exact inverse of key_name_to_row_col(): the (row, column) -> symbolic key-name
// lookup, sharing the SAME single-source 72-entry table. std::nullopt for any
// (row, column) that has no bound key (e.g. the intentionally-unmapped MSX
// ":"/"*" cell at row 2 column 0, or any out-of-range coordinate). Added for the
// input RECORDER (src/frontend/input_recorder path): the SDL3 mapper resolves a
// scancode to (row, column), and this converts that back to the KEY= name the
// HBF1XV-INPUT-SCRIPT v1 parser resolves through key_name_to_row_col() -- so a
// recorded key round-trips through replay to the identical matrix cell.
[[nodiscard]] std::optional<std::string_view> row_col_to_key_name(int row, int column);

}  // namespace sony_msx::peripherals
