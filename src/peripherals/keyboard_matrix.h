#pragma once

#include <array>
#include <cstdint>

#include "devices/chipset/ppi_8255.h"

namespace sony_msx::peripherals {

// MSX 11x8 keyboard matrix (M15-S4, backlog C6).
//
// The matrix is INVERTED (0 = pressed) and read through PPI port B (#A9) for the
// row selected by PPI port C bits 0-3 (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §3/§10; openMSX
// references/openmsx-21.0/src/MSXPPI.cc:88-95 — behaviour reference, never
// copied). Implements devices::chipset::KeyboardRowSource so the PPI reads rows
// through the injected interface.
//
// Determinism: idle = no keys pressed -> every row reads 0xFF (A-M15-5). Live
// key state is set by tests/API; real input events are a frontend concern
// (backlog C9). Key ghosting is NOT modelled (deterministic direct-injection per
// fact-sheet §10; an optional later refinement).
class KeyboardMatrix final : public devices::chipset::KeyboardRowSource {
public:
    static constexpr int kRows = 11;
    static constexpr int kColumns = 8;

    void reset();

    // Set/clear a key at (row, column). Out-of-range is ignored.
    void set_key(int row, int column, bool pressed);
    [[nodiscard]] bool key(int row, int column) const;

    // devices::chipset::KeyboardRowSource — inverted (0 = pressed), idle 0xFF.
    [[nodiscard]] std::uint8_t keyboard_row(int row) const override;

private:
    // Stored INVERTED: bit = 0 means pressed. Idle = 0xFF.
    std::array<std::uint8_t, kRows> rows_{};
};

}  // namespace sony_msx::peripherals
