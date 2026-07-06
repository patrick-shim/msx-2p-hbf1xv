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
