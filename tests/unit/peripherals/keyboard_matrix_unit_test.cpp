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

#include <cstdint>
#include <iostream>

#include "peripherals/keyboard_matrix.h"

// Suite: Peripherals_KeyboardMatrix_Unit
//
// 11x8 inverted matrix (0 = pressed); idle rows read 0xFF. Grounding:
// the keyboard hardware fact sheet; openMSX 21.0: src/MSXPPI.cc:88-95.

namespace {

using sony_msx::peripherals::KeyboardMatrix;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

}  // namespace

int main() {
    // --- Idle: every row 0xFF. ---
    {
        KeyboardMatrix kb;
        kb.reset();
        bool all_idle = true;
        for (int row = 0; row < KeyboardMatrix::kRows; ++row) {
            if (kb.keyboard_row(row) != 0xFF) {
                all_idle = false;
            }
        }
        if (!expect_true(all_idle, "Idle_AllRows0xFF")) {
            return 1;
        }
    }

    // --- Pressing a key clears exactly its column bit on its row (inverted). ---
    {
        KeyboardMatrix kb;
        kb.reset();
        kb.set_key(3, 5, true);  // row 3, column 5
        if (!expect_true(kb.keyboard_row(3) == static_cast<std::uint8_t>(0xFF & ~(1u << 5)),
                         "Press_ClearsColumnBit")) {
            return 1;
        }
        if (!expect_true(kb.key(3, 5), "Press_KeyStateSet")) {
            return 1;
        }
        // Other rows unaffected.
        if (!expect_true(kb.keyboard_row(4) == 0xFF, "Press_OtherRowsUnaffected")) {
            return 1;
        }
        // Release restores the bit.
        kb.set_key(3, 5, false);
        if (!expect_true(kb.keyboard_row(3) == 0xFF, "Release_RestoresBit")) {
            return 1;
        }
    }

    // --- Out-of-range access is ignored (bounds), row read clamps to 0xFF. ---
    {
        KeyboardMatrix kb;
        kb.reset();
        kb.set_key(99, 0, true);  // ignored
        if (!expect_true(kb.keyboard_row(99) == 0xFF, "OutOfRange_ReadsIdle")) {
            return 1;
        }
    }

    return 0;
}
