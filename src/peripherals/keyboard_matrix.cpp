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

#include "peripherals/keyboard_matrix.h"

namespace sony_msx::peripherals {

void KeyboardMatrix::reset() {
    // Idle = nothing pressed -> all columns high (inverted).
    rows_.fill(0xFF);
}

void KeyboardMatrix::set_key(const int row, const int column, const bool pressed) {
    if (row < 0 || row >= kRows || column < 0 || column >= kColumns) {
        return;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << column);
    if (pressed) {
        rows_[static_cast<std::size_t>(row)] &= static_cast<std::uint8_t>(~mask);  // 0 = pressed
    } else {
        rows_[static_cast<std::size_t>(row)] |= mask;
    }
}

bool KeyboardMatrix::key(const int row, const int column) const {
    if (row < 0 || row >= kRows || column < 0 || column >= kColumns) {
        return false;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << column);
    return (rows_[static_cast<std::size_t>(row)] & mask) == 0;
}

void KeyboardMatrix::attach_rensha_turbo(const RenshaTurbo* source) {
    rensha_ = source;
}

std::uint8_t KeyboardMatrix::keyboard_row(const int row) const {
    if (row < 0 || row >= kRows) {
        return 0xFF;
    }
    std::uint8_t value = rows_[static_cast<std::size_t>(row)];
    // Ren-Sha Turbo autofire (M25, A-M25-7): row 8 bit0 = SPACE. Unattached
    // leaves value exactly as computed above -- byte-for-byte identical to
    // pre-M25 behavior (regression guard, M25-S3). OR-only: can only force a
    // 0 bit (pressed) to 1 (a periodic release), never the reverse (R-M25-6).
    if (row == 8 && rensha_ != nullptr) {
        value = static_cast<std::uint8_t>(value | rensha_->keyboard_row8_or_mask());
    }
    return value;
}

}  // namespace sony_msx::peripherals
