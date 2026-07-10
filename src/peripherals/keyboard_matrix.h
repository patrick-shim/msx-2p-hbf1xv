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

#include <array>
#include <cstdint>

#include "devices/chipset/ppi_8255.h"
#include "peripherals/rensha_turbo.h"

namespace sony_msx::peripherals {

// MSX 11x8 keyboard matrix (M15-S4, backlog C6).
//
// The matrix is inverted (0 = pressed) and read through PPI port B (#A9) for
// the row selected by PPI port C bits 0-3 (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §3/§10; openMSX
// references/openmsx-21.0/src/MSXPPI.cc:88-95 — behaviour reference, never
// copied). Implements devices::chipset::KeyboardRowSource so the PPI reads
// rows through the injected interface.
//
// Determinism: idle = no keys pressed -> every row reads 0xFF (A-M15-5). Live
// key state is set by tests/API; real input events are a frontend concern
// (backlog C9). Key ghosting is not modelled (deterministic direct-injection
// per fact-sheet §10; an optional later refinement).
class KeyboardMatrix final : public devices::chipset::KeyboardRowSource {
public:
    static constexpr int kRows = 11;
    static constexpr int kColumns = 8;

    void reset();

    // Set/clear a key at (row, column). Out-of-range is ignored.
    void set_key(int row, int column, bool pressed);
    [[nodiscard]] bool key(int row, int column) const;

    // Inject the Ren-Sha Turbo autofire source backing row 8 bit0 (M25,
    // backlog C8, openMSX MSXPPI.cc:90-93 A-M25-7). nullptr (the default)
    // reproduces the exact pre-M25 behavior byte-for-byte -- a hard
    // regression guard, unit-tested explicitly.
    void attach_rensha_turbo(const RenshaTurbo* source);

    // devices::chipset::KeyboardRowSource — inverted (0 = pressed), idle 0xFF.
    [[nodiscard]] std::uint8_t keyboard_row(int row) const override;

private:
    // Stored INVERTED: bit = 0 means pressed. Idle = 0xFF.
    std::array<std::uint8_t, kRows> rows_{};
    const RenshaTurbo* rensha_ = nullptr;
};

}  // namespace sony_msx::peripherals
