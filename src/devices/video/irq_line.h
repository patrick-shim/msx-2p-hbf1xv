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

namespace sony_msx::devices::video {

// Thin, level-held interrupt-line sink (M14-S4).
//
// The V9958 owns its /INT line (open-drain, active-low on real hardware; here a
// simple boolean level = vertical OR horizontal). It drives a sink whenever the
// combined level CHANGES: set_irq(true) when it asserts, set_irq(false) when the
// last source releases (on the corresponding status-register read). The machine
// supplies an adapter that forwards this to the M12 Z80A's maskable-interrupt
// request/clear, REUSING the existing IM1 acceptance path unchanged.
//
// Keeping this as an abstract sink lets the VDP drive the CPU line without the
// VDP depending on the CPU class (dependency inversion; the concrete adapter
// lives in the machine layer). Grounding: fact-sheet §7 line 126 (/INT open-
// drain, active-low); openMSX drives the shared line via two IRQHelpers
// (references/openmsx-21.0/src/video/VDP.cc:402-415 — behavior reference only).
class IrqLine {
public:
    virtual ~IrqLine() = default;

    // Called only on a level transition of (vertical OR horizontal).
    virtual void set_irq(bool asserted) = 0;
};

}  // namespace sony_msx::devices::video
