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

// Suite: Devices_V9958Ie0RegisterWriteIrq_Unit (M36 Bug B)
//
// Proves the V9958 re-evaluates the /INT line on the REGISTER WRITE that toggles
// the interrupt-enable bits IE0 (R#1 bit5) and IE1 (R#0 bit4) -- not only at
// VBlank/line-match and the S#0/S#1 status reads. This is the fix for the YS II
// building-interior crash: the game's VBlank ISR repeatedly writes R#1=0x40
// (display on, IE0 OFF) at pc=A461; on real hardware clearing IE0 immediately
// de-asserts /INT so the ISR's trailing EI cannot re-fire. Without the
// register-write re-evaluation, our /INT stayed latched (F still set) and the EI
// re-entered the ISR forever -> nested-VBLANK stack overflow.
//
// Grounded EXACTLY in openMSX changeRegister
// (references/openmsx-21.0/src/video/VDP.cc:1177-1198):
//   R#0 IE1 clear -> irqHorizontal.reset()  (set edge armed by scheduleHScan)
//   R#1 IE0 set   -> irqVertical.set() ONLY if statusReg0 & 0x80 (F pending)
//                    (openMSX's documented Andonis/Zanac case)
//   R#1 IE0 clear -> irqVertical.reset()
//
// NON-VACUITY: with the fix reverted, the "IntDeasserts" cases FAIL (a held
// /INT survives the IE0/IE1 clear because change_register only recomputed the
// display mode) -- confirmed by the developer before delivery.

#include <cstdint>
#include <iostream>

#include "devices/video/irq_line.h"
#include "devices/video/v9958_vdp.h"

namespace {

using sony_msx::devices::video::IrqLine;
using sony_msx::devices::video::V9958Vdp;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Records the last pushed /INT level and counts edge transitions.
struct StubIrq final : IrqLine {
    bool level = false;
    int transitions = 0;
    void set_irq(bool asserted) override {
        level = asserted;
        ++transitions;
    }
};

// Writes control register `reg` via the real #99 two-write latch protocol
// (data byte, then command byte with bit7 set). No status read, so the F flag
// is never cleared as a side effect -- mirrors the game's ISR path.
void set_register(V9958Vdp& vdp, std::uint8_t reg, std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Destructive S#0 read through #99 (via the R#15 pointer) -- clears F, releases
// the vertical /INT (the existing M14 semantics, kept unchanged by Bug B).
std::uint8_t read_status0(V9958Vdp& vdp) {
    set_register(vdp, 15, 0);
    return vdp.io_read(0x99);
}

}  // namespace

int main() {
    // ------------------------------------------------------------------------
    // Case group 1: the exact YS II scenario -- IE0 cleared by an R#1 write
    // while the F flag is still pending must DE-ASSERT /INT immediately.
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);

        // Display on + IE0 (the game's steady display value, R#1 = 0x60).
        // NOTE: F is status bit7 -- we mask it (& 0x80) rather than compare the
        // whole byte, because with the display enabled (BL=bit6) the sprite
        // engine also contributes live S#0 5S/C/sprite-number bits (the bare
        // all-zero VRAM parks many sprites on line 0). Bug B is a /INT-line +
        // F-flag behavior, orthogonal to those sprite bits.
        set_register(vdp, 1, 0x60);
        vdp.on_vsync();  // F set, vertical /INT asserted (IE0 on)
        expect((vdp.peek_status_register(0) & 0x80) != 0, "Ie0Clear_Pre_FSet");
        expect(vdp.irq_active() && irq.level, "Ie0Clear_Pre_IntAsserted");

        // The ISR write from the watch-log: R#1 = 0x40 (display on, IE0 OFF).
        // F is STILL set (no S#0 read happened), yet /INT must drop -- THE FIX.
        set_register(vdp, 1, 0x40);
        expect(!vdp.irq_active() && !irq.level, "Ie0Clear_FStillPending_IntDeasserts");
        // The register write must NOT clear the F flag itself (only an S#0 read
        // does) -- de-assertion is a pure /INT-line effect.
        expect((vdp.peek_status_register(0) & 0x80) != 0, "Ie0Clear_FRemainsSet_NotClearedByRegWrite");

        // Re-enabling IE0 while F is STILL pending must RE-ASSERT /INT
        // (openMSX's Andonis/Zanac case, VDP.cc:1189-1194).
        set_register(vdp, 1, 0x60);
        expect(vdp.irq_active() && irq.level, "Ie0Reenable_FStillPending_IntReasserts");

        // The normal release path is unchanged: an S#0 read clears F + releases.
        const std::uint8_t s0 = read_status0(vdp);
        expect((s0 & 0x80) != 0, "ReadS0_ReturnsF");
        expect(!vdp.irq_active() && !irq.level, "ReadS0_ReleasesInt");
        expect((vdp.peek_status_register(0) & 0x80) == 0, "ReadS0_ClearsF");
    }

    // ------------------------------------------------------------------------
    // Case group 2: re-enabling IE0 when NO F is pending must NOT assert /INT
    // (the re-assert is gated on the F flag, VDP.cc:1192).
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        // No on_vsync() -> F never set.
        set_register(vdp, 1, 0x00);  // IE0 off
        set_register(vdp, 1, 0x20);  // IE0 on, but F not pending
        expect(!vdp.irq_active() && !irq.level, "Ie0Enable_NoFPending_NoAssert");
    }

    // ------------------------------------------------------------------------
    // Case group 3: an R#1 write that leaves IE0 SET (only other bits change)
    // must NOT disturb a held vertical /INT -- the fix reacts to the IE0 CHANGE
    // edge only, never to any/every R#1 write (universal, not YS-II-specific).
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 1, 0x20);  // IE0 on
        vdp.on_vsync();              // F set, /INT asserted
        expect(vdp.irq_active() && irq.level, "Ie0Unchanged_Pre_IntAsserted");
        // Toggle a NON-IE0 bit (bit6/BL display) while IE0 stays on: 0x20 -> 0x60.
        set_register(vdp, 1, 0x60);
        expect(vdp.irq_active() && irq.level, "Ie0Unchanged_OtherBitToggled_IntStaysAsserted");
    }

    // ------------------------------------------------------------------------
    // Case group 4: the symmetric IE1 (R#0 bit4) path -- clearing IE1 by an
    // R#0 write while a line /INT is held must de-assert it (VDP.cc:1182).
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 0, 0x10);  // IE1 on
        vdp.on_line_match();          // horizontal /INT asserted
        expect(vdp.irq_active() && irq.level, "Ie1Clear_Pre_IntAsserted");
        expect((vdp.peek_status_register(1) & 0x01) != 0, "Ie1Clear_Pre_S1FHSet");

        set_register(vdp, 0, 0x00);  // IE1 off -> de-assert -- THE FIX (symmetric)
        expect(!vdp.irq_active() && !irq.level, "Ie1Clear_IntDeasserts");
    }

    // ------------------------------------------------------------------------
    // Case group 5: normal VBlank path is behavior-UNCHANGED when IE0 stays on
    // across a frame -- /INT holds until the S#0 read, no spurious drop.
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 1, 0x20);  // IE0 on (F not yet set -> no assert)
        expect(!vdp.irq_active() && !irq.level, "NormalVBlank_EnableBeforeF_NoAssert");
        vdp.on_vsync();
        expect(vdp.irq_active() && irq.level, "NormalVBlank_Vsync_Asserts");
        // No further register writes: /INT holds until the status read.
        expect(vdp.irq_active() && irq.level, "NormalVBlank_HoldsUntilRead");
        read_status0(vdp);
        expect(!vdp.irq_active() && !irq.level, "NormalVBlank_ReadS0_Releases");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_V9958Ie0RegisterWriteIrq_Unit cases passed\n";
    return 0;
}
