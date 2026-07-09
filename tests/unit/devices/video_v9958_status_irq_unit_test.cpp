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

// Suite: Devices_V9958StatusIrq_Unit
//
// Deterministic unit coverage for the V9958 status registers S#0..S#9 and the
// VBlank + line interrupt model with the level-held /INT seam (M14-S4). Grounded
// in VDP.cc:290-296 (reset), 402-415 (raise), 903-986 (peek/read + flag reset),
// and fact-sheet §4/§7. The /INT level = vertical OR horizontal, released only on
// the corresponding status-register read.

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

// Records the last pushed /INT level and counts transitions.
struct StubIrq final : IrqLine {
    bool level = false;
    int transitions = 0;
    void set_irq(bool asserted) override {
        level = asserted;
        ++transitions;
    }
};

void set_register(V9958Vdp& vdp, std::uint8_t reg, std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Read status register `reg` destructively through #99 (via the R#15 pointer).
std::uint8_t read_status(V9958Vdp& vdp, std::uint8_t reg) {
    set_register(vdp, 15, reg);
    return vdp.io_read(0x99);
}

}  // namespace

int main() {
    // --- Reset values (non-destructive peek). ---
    {
        V9958Vdp vdp;
        expect(vdp.peek_status_register(0) == 0x00, "Reset_S0_Is00");
        expect(vdp.peek_status_register(1) == 0x04, "Reset_S1_Is04_V9958Id2");  // ID#=2, LPS/FL=0
        expect(vdp.peek_status_register(2) == 0x0C, "Reset_S2_Is0C_UndocBits");
        expect(vdp.peek_status_register(3) == 0x00, "Reset_S3_Idle");
        expect(vdp.peek_status_register(4) == 0xFE, "Reset_S4_IdleMask");
        expect(vdp.peek_status_register(5) == 0x00, "Reset_S5_Idle");
        expect(vdp.peek_status_register(6) == 0xFC, "Reset_S6_IdleMask");
        expect(vdp.peek_status_register(7) == 0x00, "Reset_S7_Idle");
        expect(vdp.peek_status_register(8) == 0x00, "Reset_S8_Idle");
        expect(vdp.peek_status_register(9) == 0xFE, "Reset_S9_IdleMask");
        expect(vdp.peek_status_register(10) == 0xFF, "Reset_S10_NonExistent_OpenBus");
    }

    // --- VBlank: IE0 gates the /INT assert; reading S#0 clears F + releases. ---
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 1, 0x20);  // R#1 IE0 enabled

        vdp.on_vsync();
        expect(vdp.peek_status_register(0) == 0x80, "VBlank_SetsF");
        expect(vdp.irq_active() && irq.level, "VBlank_IE0_AssertsInt");

        const std::uint8_t s0 = read_status(vdp, 0);
        expect(s0 == 0x80, "VBlank_ReadS0_ReturnsF");
        expect(vdp.peek_status_register(0) == 0x00, "VBlank_ReadS0_ClearsF");
        expect(!vdp.irq_active() && !irq.level, "VBlank_ReadS0_ReleasesInt");
    }

    // --- VBlank with IE0 disabled: F still set, but /INT NOT asserted. ---
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        // R#1 IE0 stays 0.
        vdp.on_vsync();
        expect(vdp.peek_status_register(0) == 0x80, "VBlank_NoIE0_StillSetsF");
        expect(!vdp.irq_active() && !irq.level, "VBlank_NoIE0_NoInt");
    }

    // --- Line interrupt: IE1 gates FH; reading S#1 clears FH + releases. ---
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 0, 0x10);  // R#0 IE1 enabled

        vdp.on_line_match();
        expect(vdp.peek_status_register(1) == 0x05, "Line_IE1_SetsFH");  // 0x04 | FH
        expect(vdp.irq_active() && irq.level, "Line_IE1_AssertsInt");

        const std::uint8_t s1 = read_status(vdp, 1);
        expect(s1 == 0x05, "Line_ReadS1_ReturnsFH");
        expect(vdp.peek_status_register(1) == 0x04, "Line_ReadS1_ClearsFH");
        expect(!vdp.irq_active() && !irq.level, "Line_ReadS1_ReleasesInt");
    }

    // --- Line interrupt with IE1 disabled: no FH, no /INT. ---
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        vdp.on_line_match();
        expect(vdp.peek_status_register(1) == 0x04, "Line_NoIE1_NoFH");
        expect(!vdp.irq_active() && !irq.level, "Line_NoIE1_NoInt");
    }

    // --- /INT = vertical OR horizontal: the line holds until BOTH are cleared. ---
    {
        V9958Vdp vdp;
        StubIrq irq;
        vdp.set_irq_line(&irq);
        set_register(vdp, 0, 0x10);  // IE1
        set_register(vdp, 1, 0x20);  // IE0

        vdp.on_vsync();
        vdp.on_line_match();
        expect(vdp.irq_active() && irq.level, "WiredOr_BothSources_Asserted");

        read_status(vdp, 0);  // clears vertical only
        expect(vdp.irq_active() && irq.level, "WiredOr_ReadS0_HorizontalStillHolds");

        read_status(vdp, 1);  // clears horizontal
        expect(!vdp.irq_active() && !irq.level, "WiredOr_ReadS1_LineReleased");
    }

    // --- S#2 EO field bit toggles each frame boundary. ---
    {
        V9958Vdp vdp;
        expect(vdp.peek_status_register(2) == 0x0C, "S2_EO_InitialClear");
        vdp.on_vsync();
        expect(vdp.peek_status_register(2) == 0x0E, "S2_EO_TogglesToSetAfterFrame");
        vdp.on_vsync();
        expect(vdp.peek_status_register(2) == 0x0C, "S2_EO_TogglesBackAfterTwoFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
