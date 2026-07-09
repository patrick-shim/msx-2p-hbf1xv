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

// Symbolic key-name -> (row, column) lookup for the MSX 11x8 keyboard matrix
// (M27-S6, "Production Hardening + Debug/Test Tooling" item 3,
// docs/m27-planner-package.md §2.4).
//
// Every entry below is a direct RE-EXPRESSION of
// `Sdl3InputMapper::scancode_map()`'s own already-tested array literal
// (src/frontend/sdl3_input_mapper.cpp, M26) -- rows 0-8 only (the SAME
// disclosed numeric-keypad-rows-9-10-excluded boundary that table already
// established, `sdl3_input_mapper.h:33-34`, inherited here for consistency,
// never silently extended, per docs/m27-planner-package.md §1.2). Row 2
// column 0 (":") is likewise intentionally left unmapped for the identical
// reason `scancode_map()` omits it (no dedicated physical PC key).
//
// Names are the literal `SDL_Scancode` enumerator suffix for every entry
// that has a direct SDL3 counterpart (e.g. "MINUS" <-> SDL_SCANCODE_MINUS,
// "LSHIFT" <-> SDL_SCANCODE_LSHIFT) -- a deliberate design choice enabling a
// hard, mechanical M27-S7 cross-consistency test
// (`tests/integration/frontend/
// sdl3_input_mapper_key_names_consistency_integration_test.cpp`) against
// `sdl3_input_mapper.cpp`'s own table, never an independently re-derived
// value (R-M27-4). This header is intentionally SDL3-independent (no
// `SDL_Scancode` type appears here) so it stays headless-buildable, per
// A-M27-8's `src/peripherals/`-is-frontend-independent boundary rule.
[[nodiscard]] std::optional<std::pair<int, int>> key_name_to_row_col(std::string_view name);

}  // namespace sony_msx::peripherals
