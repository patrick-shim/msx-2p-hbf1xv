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

std::uint8_t KeyboardMatrix::keyboard_row(const int row) const {
    if (row < 0 || row >= kRows) {
        return 0xFF;
    }
    return rows_[static_cast<std::size_t>(row)];
}

}  // namespace sony_msx::peripherals
