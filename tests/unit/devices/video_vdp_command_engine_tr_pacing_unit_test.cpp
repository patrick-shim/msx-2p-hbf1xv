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

// Suite: Devices_VdpCommandEngineTrPacing_Unit
//
// VDP command-timing hardware-parity correction (S#2 bit7 TR pacing). On real
// HW the transfer-ready bit DROPS after each serviced LMMC/HMMC/LMCM unit while
// the VDP consumes that unit's VRAM access slot, then RE-RAISES. Pre-correction
// TR was set once and stayed 1, so a poll-TR loop never blocked. Grounding: the
// fact sheet §8 handshake note + the per-unit cost of the byte/dot analog
// command (openMSX punts on transfer per-unit timing; TIER-1 gives no figure).
//
// Determinism: the window is an ABSOLUTE u64 compared against cpu_total_cycles()
// (a fully decoupled fake clock here) -- identical polls produce identical TR.
//
// Cases:
//   TR-1  After a serviced HMMC byte, an S#2 read WITHIN the per-unit window sees
//         TR=0; a read at/after the expiry sees TR=1 again (drop-then-rearm).
//   TR-2  A poll-TR loop that advances the clock exits after exactly the paced
//         count; a loop that advances PAST the window per poll never blocks
//         (models the normal, slower-than-a-VRAM-slot CPU poll loop).
//   TR-3  LMCM: an S#7 read that advances the transfer drops TR for the window.
//   TR-4  Strict-superset / no-clock inertness: with NO clock attached TR is the
//         exact pre-correction always-1-while-active bit; a COL write is ALWAYS
//         serviced (VRAM updated) regardless of the TR window -> no data drop.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_command_address.h"

namespace {

using sony_msx::devices::video::graphic7_command_address;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Fully decoupled fake clock (mirrors the CE-duration test): total_ is the
// absolute count the TR window compares against; since_vsync_ = 0 (vblank) so
// the CE active-display factor is never involved.
class FakeClock final : public VdpClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_tstates_since_vsync() const override { return since_vsync_; }
    [[nodiscard]] std::uint64_t cpu_total_cycles() const override { return total_; }
    std::uint64_t total_ = 0;
    std::uint64_t since_vsync_ = 0;
};

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}
void set_cmd_reg(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}
void set_sx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 0, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_sy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 2, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 3, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }
void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }
void select_graphic7(V9958Vdp& vdp) { set_register(vdp, 0, 0x0E); }

// S#2 bit7 (TR) via the real status-read path (the raster VR/HR bits are also
// live but never touch bit7).
bool tr_bit(const V9958Vdp& vdp) { return (vdp.peek_status_register(2) & 0x80) != 0; }

}  // namespace

int main() {
    // ================= TR-1: HMMC drop-then-rearm on the byte cadence =========
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic7(vdp);  // 1 pixel/byte
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 4); set_ny(vdp, 1);
        set_cmd(vdp, 0xF0);  // HMMC (byte transfer, per-unit cost 8 T-states)

        // Before any byte: TR is up (VDP ready for the first byte).
        expect(tr_bit(vdp), "Tr1_Hmmc_ReadyBeforeFirstByte");

        // Service one byte via a COL/R#44 write; the VDP consumes a VRAM slot ->
        // TR drops for the per-unit window (8 T-states).
        clk.total_ = 100;
        set_cmd_reg(vdp, 12, 0xAA);  // arms tr_busy_until = 100 + 8 = 108
        clk.total_ = 100; expect(!tr_bit(vdp), "Tr1_Hmmc_TrDropsRightAfterByte");
        clk.total_ = 107; expect(!tr_bit(vdp), "Tr1_Hmmc_TrStillLowInWindow");
        clk.total_ = 108; expect(tr_bit(vdp), "Tr1_Hmmc_TrRearmsAtExpiry");
        clk.total_ = 200; expect(tr_bit(vdp), "Tr1_Hmmc_TrStaysUpPastExpiry");
    }

    // ================= TR-2: poll cadence ====================================
    {
        // A poll-TR loop that advances ONE cycle per poll blocks for exactly the
        // per-unit window (8 cycles) after a serviced byte.
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic7(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 4); set_ny(vdp, 1);
        set_cmd(vdp, 0xF0);

        set_cmd_reg(vdp, 12, 0x11);  // one byte serviced at total=0 -> tr_busy_until=8
        int polls = 0;
        while (!tr_bit(vdp) && polls < 1000) {
            ++polls;
            clk.total_ += 1;
        }
        expect(polls == 8, "Tr2_Hmmc_PollLoopBlocksExactlyPerUnit");
    }
    {
        // A poll loop that advances MORE than the window per poll (a normal, slow
        // CPU poll+write loop) never observes TR low -> zero blocking, exactly as
        // on real HW where the VRAM slot completes before the CPU re-polls.
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic7(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 4); set_ny(vdp, 1);
        set_cmd(vdp, 0xF0);

        clk.total_ = 40;
        set_cmd_reg(vdp, 12, 0x22);  // tr_busy_until = 48
        clk.total_ = 40 + 30;        // 70 >= 48 -> already re-raised
        expect(tr_bit(vdp), "Tr2_Hmmc_SlowPollerNeverBlocks");
    }

    // ================= TR-3: LMCM read cadence ===============================
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);
        set_sx(vdp, 0); set_sy(vdp, 0); set_nx(vdp, 4); set_ny(vdp, 1);
        set_cmd(vdp, 0xA0);  // LMCM (pixel read, per-unit cost 15 T-states)

        expect(tr_bit(vdp), "Tr3_Lmcm_ReadyBeforeFirstRead");
        clk.total_ = 500;
        (void)vdp.peek_status_register(2);  // a plain S#2 poll must NOT advance LMCM
        expect(tr_bit(vdp), "Tr3_Lmcm_PlainPollDoesNotConsume");

        // An S#7 read advances one pixel and drops TR for the 15-cycle window.
        set_register(vdp, 15, 7);
        (void)vdp.io_read(0x99);  // read S#7 -> tr_busy_until = 500 + 15 = 515
        clk.total_ = 514; expect(!tr_bit(vdp), "Tr3_Lmcm_TrLowAfterS7Read");
        clk.total_ = 515; expect(tr_bit(vdp), "Tr3_Lmcm_TrRearmsAtExpiry");
    }

    // ================= TR-4: no-clock inertness + never-drop-data =============
    {
        // No clock attached: TR is the EXACT pre-correction always-1-while-active
        // bit, and every COL write still lands in VRAM (the window gates only the
        // status-read bit, never the write).
        V9958Vdp vdp;  // no clock
        select_graphic7(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 2); set_ny(vdp, 1);
        set_cmd(vdp, 0xF0);  // HMMC

        expect(tr_bit(vdp), "Tr4_NoClock_TrUpAtStart");
        set_cmd_reg(vdp, 12, 0xAA);
        expect(tr_bit(vdp), "Tr4_NoClock_TrStaysUp_PreCorrectionBehavior");
        expect(vdp.vram().read(graphic7_command_address(0, 0)) == 0xAA,
               "Tr4_NoClock_ByteZeroWritten");
        set_cmd_reg(vdp, 12, 0xBB);
        expect(vdp.vram().read(graphic7_command_address(1, 0)) == 0xBB,
               "Tr4_NoClock_ByteOneWritten_NoDataDrop");
        expect(!vdp.cmd_engine().ce(), "Tr4_NoClock_CeClearsAfterLastByte");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
