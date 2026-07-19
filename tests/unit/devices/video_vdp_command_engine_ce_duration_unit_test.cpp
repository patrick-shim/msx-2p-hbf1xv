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

// Suite: Devices_VdpCommandEngineCeDuration_Unit
// (M44, DEF-M44-CMDSYNC Phase 2a, DEC-0069; docs/m44-phase2a-planner-package.md
//  §4.4 AC-5/AC-6/AC-6-nt/AC-7)
//
// Phase 2a gives each atomic VDP command a real reported S#2-bit0 CE busy
// DURATION so software that busy-waits on CE (the split-screen title's HUD blit loop) is
// paced. This test covers the DETERMINISTIC, clock-free contract of that
// mechanism -- the split-screen-title A/B (the calibrated per-mode factor) is separate
// evidence, not asserted here.
//
// Cases:
//   AC-5  Pure duration (engine-level, clock-free): last_cmd_duration_tstates()
//         equals ceil((per_unit*(tmp_nx*tmp_ny - 1) + per_line*(tmp_ny - 1)) / 6)
//         for HMMV(48/px,56/line), LMMV(96,64), LMMM(120,64), HMMM(88,64),
//         YMMM(64,0). (VDP command-timing hardware-parity correction: the per-
//         line end-of-line overhead, fact sheet §8, was added; pre-correction it
//         was omitted.) A minimal 1x1 command, ABRT, POINT and event-driven LMMC
//         all report 0; LINE/SRCH report > 0.
//   AC-6  CE window (VDP-level, fake absolute clock): CE reads 1 while
//         now < cmd_busy_until and flips to 0 once now reaches it; a busy-wait
//         loop that advances the clock per poll exits after exactly the paced
//         number of polls; the window is an ABSOLUTE u64 that survives a vsync.
//   AC-6-nt  Non-tautology: a 0-duration command (1x1) makes CE clear on the
//         first poll (reproducing the pre-fix un-paced loop) -- proving the
//         duration is load-bearing; and CE genuinely CLEARS after the window
//         (the guard that a constant-CE=1 mutation would fail).
//   AC-7  Strict-superset / default-inert: with NO clock attached CE is the
//         exact pre-fix instant-clear (engine bit only); with a clock attached
//         but command timing SUSPENDED (the debug-write exclusion) CE also
//         reports the pre-fix instant-clear.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"

namespace {

using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void expect_eq_u64(const std::uint64_t got, const std::uint64_t want, const char* case_name) {
    if (got != want) {
        std::cerr << "Case failed: " << case_name << " (got " << got << ", want " << want << ")\n";
        ++g_failures;
    }
}

// Fully decoupled fake clock: total_ is the ABSOLUTE monotonic count the CE
// busy window compares against; since_vsync_ feeds the raster derivation (kept
// small = vblank so the active-display slot factor never perturbs the base-
// duration math these cases assert).
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
void set_dx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_sx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 0, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_sy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 2, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 3, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 12, v); }
void set_arg(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 13, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }
void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }

// Mirror of the engine's pure duration model for exact-value assertions, keyed on
// the KNOWN clipped tmp_nx/tmp_ny. VDP command-timing parity: per-unit cost for
// every unit-but-one, PLUS the per-line end-of-line overhead for every row-but-one
// (fact sheet §8, openMSX end-of-line Delta minus per-unit Delta), one ceil at the
// end -- byte-identical to vdp_command_engine.cpp's rect_duration_tstates.
std::uint64_t rect_duration(const int ticks, const int per_line, const unsigned tnx,
                            const unsigned tny) {
    const std::uint64_t units = static_cast<std::uint64_t>(tnx) * static_cast<std::uint64_t>(tny);
    if (units == 0) return 0;
    const std::uint64_t vc = static_cast<std::uint64_t>(ticks) * (units - 1) +
                             static_cast<std::uint64_t>(per_line) * (tny - 1);
    return (vc + 5) / 6;
}

bool ce_bit(const V9958Vdp& vdp) { return (vdp.peek_status_register(2) & 0x01) != 0; }

}  // namespace

int main() {
    // ================= AC-5: pure duration (clock-free, engine-level) =========
    {
        V9958Vdp vdp;  // no clock attached: the engine still computes the pure duration
        select_graphic4(vdp);

        // HMMV (48 / byte, 56 / line). NX=6 -> tmp_nx=3 bytes; NY=2 -> tmp_ny=2.
        // VDP cycles = 48*(6-1) + 56*(2-1) = 240+56 = 296 -> ceil(296/6) = 50.
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), rect_duration(48, 56, 3, 2),
                      "Ac5_Hmmv_PureDuration");
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), 50, "Ac5_Hmmv_PureDuration_Literal50");

        // LMMV (96 / pixel, 64 / line). NX=10 -> tmp_nx=10; NY=4 -> tmp_ny=4.
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 10); set_ny(vdp, 4); set_col(vdp, 0x07);
        set_cmd(vdp, 0x80);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), rect_duration(96, 64, 10, 4),
                      "Ac5_Lmmv_PureDuration");

        // LMMM (120 / pixel, 64 / line). sx=0,dx=0,NX=8,NY=3 -> tmp_nx=8, tmp_ny=3.
        set_sx(vdp, 0); set_sy(vdp, 0); set_dx(vdp, 0); set_dy(vdp, 0);
        set_nx(vdp, 8); set_ny(vdp, 3);
        set_cmd(vdp, 0x90);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), rect_duration(120, 64, 8, 3),
                      "Ac5_Lmmm_PureDuration");

        // HMMM (88 / byte, 64 / line). sx=0,dx=20,NX=4,NY=1 -> tmp_nx=2, tmp_ny=1
        // (single row -> per-line term is 0, value unchanged from pre-correction).
        set_sx(vdp, 0); set_sy(vdp, 0); set_dx(vdp, 20); set_dy(vdp, 0);
        set_nx(vdp, 4); set_ny(vdp, 1);
        set_cmd(vdp, 0xD0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), rect_duration(88, 64, 2, 1),
                      "Ac5_Hmmm_PureDuration");

        // YMMM (64 / byte, 0 / line -- "does not take extra time"). dx=0 -> tmp_nx
        // clips to full width 128 bytes; NY=1.
        set_dx(vdp, 0); set_sy(vdp, 0); set_dy(vdp, 5); set_ny(vdp, 1);
        set_cmd(vdp, 0xE0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), rect_duration(64, 0, 128, 1),
                      "Ac5_Ymmm_PureDuration");
    }

    // AC-5: minimal / instant commands all report 0.
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        // 1x1 HMMV: NX=2 -> tmp_nx=1 byte, NY=1 -> tmp_ny=1. work = 1*1-1 = 0.
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 2); set_ny(vdp, 1); set_col(vdp, 0x11);
        set_cmd(vdp, 0xC0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), 0, "Ac5_Hmmv1x1_ZeroDuration");

        // ABRT.
        set_cmd(vdp, 0x00);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), 0, "Ac5_Abrt_ZeroDuration");

        // POINT.
        set_sx(vdp, 0); set_sy(vdp, 0);
        set_cmd(vdp, 0x40);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), 0, "Ac5_Point_ZeroDuration");

        // Event-driven LMMC (paced by its own held CE + TR handshake).
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 4); set_ny(vdp, 2);
        set_cmd(vdp, 0xB0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), 0, "Ac5_Lmmc_ZeroDuration");
    }

    // AC-5: LINE and SRCH report a non-zero duration (major-axis / searched px).
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 40); set_ny(vdp, 0); set_col(vdp, 0x0F);
        set_cmd(vdp, 0x70);  // LINE
        expect(vdp.cmd_engine().last_cmd_duration_tstates() > 0, "Ac5_Line_NonZeroDuration");
    }
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_sx(vdp, 6); set_sy(vdp, 0); set_col(vdp, 0x05); set_arg(vdp, 0x02);
        set_cmd(vdp, 0x60);  // SRCH (nothing matches -> searches to the border)
        expect(vdp.cmd_engine().last_cmd_duration_tstates() > 0, "Ac5_Srch_NonZeroDuration");
    }

    // ================= AC-6: CE window paced by an absolute fake clock =========
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;  // vblank -> the active-display factor is NOT applied
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);

        // HMMV NX=6 NY=2 -> base duration 50; armed at now=0 -> cmd_busy_until=50.
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);

        clk.total_ = 0;  expect(ce_bit(vdp), "Ac6_Ce_BusyAtStart");
        clk.total_ = 49; expect(ce_bit(vdp), "Ac6_Ce_BusyJustBeforeExpiry");
        clk.total_ = 50; expect(!ce_bit(vdp), "Ac6_Ce_ClearsAtExpiry");
        clk.total_ = 51; expect(!ce_bit(vdp), "Ac6_Ce_StaysClearPastExpiry");
    }

    // AC-6: a busy-wait-on-CE loop exits after exactly the paced number of polls.
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);  // duration 50

        int polls = 0;
        while (ce_bit(vdp) && polls < 1000) {
            ++polls;
            clk.total_ += 1;  // the busy-wait loop burns one emulated cycle per poll
        }
        expect(polls == 50, "Ac6_BusyWaitLoop_ExitsAfter50Polls");
    }

    // AC-6: the window is an ABSOLUTE u64 that correctly survives a vsync.
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 60'000;   // well past a single 59'736-cycle NTSC frame
        clk.since_vsync_ = 0;  // vblank at arm time -> base duration
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);  // cmd_busy_until = 60'000 + 50 = 60'050

        // Simulate a vsync mid-window: the frame-relative counter resets, the
        // absolute counter keeps climbing -- CE must stay busy until the
        // absolute expiry regardless of the reset.
        clk.since_vsync_ = 0;
        clk.total_ = 60'020; expect(ce_bit(vdp), "Ac6_Ce_SurvivesVsync_StillBusy");
        clk.total_ = 60'050; expect(!ce_bit(vdp), "Ac6_Ce_SurvivesVsync_ClearsAtAbsoluteExpiry");
    }

    // ================= AC-6-nt: non-tautology ==================================
    {
        // A 0-duration command (1x1) arms cmd_busy_until == now -> CE is 0 on the
        // FIRST poll, reproducing the pre-fix UN-paced loop. If the duration were
        // not load-bearing this would be indistinguishable from the 50-poll case
        // above -> the two together prove the duration drives the pacing.
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 100;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 2); set_ny(vdp, 1); set_col(vdp, 0x11);
        set_cmd(vdp, 0xC0);  // 1x1 HMMV -> base duration 0

        int polls = 0;
        while (ce_bit(vdp) && polls < 1000) {
            ++polls;
            clk.total_ += 1;
        }
        expect(polls == 0, "Ac6nt_ZeroDuration_ClearsOnFirstPoll");
    }
    {
        // The "clears after the window" assertion is itself the guard against a
        // constant-CE=1 mutation (dropping the `> now` compare): a real command
        // reports CE busy for a bounded interval and then DEFINITIVELY clears.
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 10); set_ny(vdp, 4); set_col(vdp, 0x07);
        set_cmd(vdp, 0x80);  // LMMV NX=10 NY=4 -> duration 656 (96*39 + 64*3 = 3936 -> ceil/6)
        clk.total_ = 623; expect(ce_bit(vdp), "Ac6nt_Lmmv_BusyBeforeExpiry");
        clk.total_ = 100'000; expect(!ce_bit(vdp), "Ac6nt_Lmmv_ClearsFarPastExpiry");
    }

    // ================= AC-7: default-inert / strict superset ===================
    {
        // No clock attached: CE is the EXACT pre-fix instant-clear (engine bit
        // only) -- byte-identical to the pre-Phase-2a behavior.
        V9958Vdp vdp;  // no clock
        select_graphic4(vdp);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        expect(!ce_bit(vdp), "Ac7_NoClock_CeInstantClear");
        // And the pure duration is still computed (proves the engine is inert to
        // the clock, not that the model was skipped).
        expect(vdp.cmd_engine().last_cmd_duration_tstates() == 50, "Ac7_NoClock_PureDurationStillComputed");
    }
    {
        // Clock attached but command timing SUSPENDED (the debug-write exclusion):
        // the window is NOT armed -> CE reports the pre-fix instant-clear.
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = 0;
        vdp.attach_clock_source(&clk);
        select_graphic4(vdp);

        vdp.set_command_timing_suspended(true);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        expect(!ce_bit(vdp), "Ac7_Suspended_CeInstantClear");

        // Un-suspend and re-issue: NOW the window arms and CE pages busy -- proving
        // the suspend flag was the sole cause of the inert behavior above.
        vdp.set_command_timing_suspended(false);
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        expect(ce_bit(vdp), "Ac7_Unsuspended_CeArmsBusy");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
