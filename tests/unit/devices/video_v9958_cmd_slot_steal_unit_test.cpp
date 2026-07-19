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

// Suite: Devices_V9958CmdSlotSteal_Unit
// (M48 Slice 2, DEC-0075; docs/m48-planner-package.md §2.2/§3/§4.1 AC-3/AC-4/
//  AC-5/AC-6 + EG-8 non-tautology)
//
// M48 Slice 2 models the fact-sheet §8-line-163 CPU-priority rule: "CPU VRAM
// access takes priority over the command engine and steals slots -- with
// sprites on, a HMMV can be cut ~2x by simultaneous OUT (#98),A." Each CPU #98
// VRAM data-port access (io_read/io_write, port & 3 == 0) that occurs while a
// command is BUSY extends that command's S#2-bit0 CE busy window
// (cmd_busy_until_cycles_) by ONE stolen slot's worth of time =
// slot_cost_tstates(effective_slots_per_line()) T-states. It is ONE-DIRECTIONAL:
// the CPU access itself is NEVER stalled/delayed/dropped (the HB-F1XV does not
// wire /WAIT; fact-sheet §1/§7) -- only the command's reported CE window grows.
//
// EVERY expected delta here is RE-DERIVED from the tier-1 constants
// (154/88/31, 1368, 6) via an independent test-side ceil, NEVER read back from
// the implementation (EG-8). The per-slot cost is slot_cost(S) = ceil(1368/(6S))
// -> 154->2, 88->3, 31->8. Discrimination / non-tautology:
//   - the three regimes must steal 2 / 3 / 8 T-states/access (an adversarial
//     88<->31 swap, or a wrong-regime mutation, fails a specific case);
//   - a mutation that DROPS the steal add leaves cmd_busy unchanged and fails
//     every steal-amount case;
//   - a mutation that steals REGARDLESS of the busy check fails the
//     window-closed adversarial cases.
//
// Cases:
//   A  Steal amount (WRITE path), display OFF (S=154) -> +N*2, re-derived.
//   B  Steal amount (READ path),  display OFF (S=154) -> +N*2 (both #98 dirs).
//   C  Regime sensitivity: sprites OFF (S=88) -> +N*3; sprites ON (S=31) -> +N*8.
//      The three per-access costs are pairwise distinct (2<3<8), so a regime/
//      count mutation fails.
//   D  Adversarial: a #98 access AFTER the window has closed (now >= expiry)
//      does NOT extend it (no phantom contention) -- write AND read paths.
//   E  CPU-invariance at the device level: (1) the returned #98 read byte is
//      IDENTICAL whether or not a command is busy (the steal does not alter the
//      VRAM data path); (2) the clock is NEVER advanced by a #98 access (the VDP
//      cannot stall the CPU) -- AC-4.
//   F  Determinism: identical inputs -> identical extended window across runs.
//   G  Inert paths: no clock attached, and command_timing_suspended_, both leave
//      cmd_busy_until untouched by a #98 access (AC-5/§5 headless+debug seam).

#include <cstdint>
#include <iostream>
#include <utility>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_access_timing.h"

namespace {

using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;
namespace vat = sony_msx::devices::video::vdp_access_timing;

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

// Independent (test-side) ceiling division -- NOT taken from the implementation.
constexpr std::uint64_t ceil_div(const std::uint64_t a, const std::uint64_t b) {
    return (a + b - 1) / b;
}
// The tier-1-re-derived per-access steal cost (EG-8): ceil(1368 / (6 * S)).
constexpr std::uint64_t rederived_slot_cost(const std::uint64_t s) { return ceil_div(1368, 6 * s); }

// Fully decoupled fake clock (same shape as the Slice-1 slot-factor test): total_
// is the ABSOLUTE monotonic count the CE window / steal busy-check compares
// against; since_vsync_ feeds the raster derivation (raster_display_line()).
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
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 12, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }

// A raster position squarely inside the active display region for LN=0 (192
// lines): non_active_lines = 70, so line 100 -> raster_display_line() == 30 >= 0.
constexpr std::uint64_t kActiveVsync = 100ull * vat::kCpuTstatesPerLine;  // line 100
// A vblank raster (tstates==0 == first bottom-border line) -> raster < 0.
constexpr std::uint64_t kVblankVsync = 0;

// Arm a GRAPHIC4 HMMV NX x NY under the chosen regime, with the clock parked at
// absolute 0 so the returned cmd_busy_until_cycles() IS the armed window length
// and now(0) < window => the command is BUSY. Register setup uses #99 writes
// only (port & 3 == 1) so NONE of it steals; the returned window is the clean
// pre-steal Slice-1 baseline.
void arm_graphic4_hmmv(V9958Vdp& vdp, FakeClock& clk, const std::uint64_t since_vsync,
                       const std::uint8_t r1, const std::uint8_t r8, const unsigned nx,
                       const unsigned ny) {
    clk.total_ = 0;
    clk.since_vsync_ = since_vsync;
    vdp.attach_clock_source(&clk);
    set_register(vdp, 0, 0x06);  // GRAPHIC4 (base 0x0C)
    set_register(vdp, 1, r1);    // BL = bit6; keeps M1=M2=0 -> stays GRAPHIC4
    set_register(vdp, 8, r8);    // SPD = bit1
    clk.total_ = 0;              // arm at absolute 0
    set_dx(vdp, 0);
    set_dy(vdp, 0);
    set_nx(vdp, nx);
    set_ny(vdp, ny);
    set_col(vdp, 0x5A);
    set_cmd(vdp, 0xC0);  // HMMV -> arms the CE window
}

}  // namespace

int main() {
    // ============ A: steal amount, WRITE path, display OFF (S=154) ============
    // now(0) < window; S=154 (vblank raster) -> each #98 write steals 2 T-states.
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kVblankVsync, /*r1=*/0x40, /*r8=*/0x00, /*nx=*/6, /*ny=*/2);
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        expect(w0 > clk.total_, "A_CommandBusyAtIssue");
        const std::uint64_t cost = rederived_slot_cost(154);  // == 2
        expect_eq_u64(cost, 2, "A_SlotCost154_Rederived_Is2");
        const int n = 5;
        for (int i = 0; i < n; ++i) {
            vdp.io_write(0x98, static_cast<std::uint8_t>(0x10 + i));
        }
        // N writes while busy -> window extended by exactly N*cost beyond the
        // Slice-1 baseline. A dropped-steal mutation leaves this == w0 and fails.
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0 + static_cast<std::uint64_t>(n) * cost,
                      "A_WritePath_Extended_By_N_times_2");
    }

    // ============ B: steal amount, READ path, display OFF (S=154) =============
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kVblankVsync, 0x40, 0x00, 6, 2);
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        const std::uint64_t cost = rederived_slot_cost(154);
        const int n = 4;
        for (int i = 0; i < n; ++i) {
            (void)vdp.io_read(0x98);
        }
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0 + static_cast<std::uint64_t>(n) * cost,
                      "B_ReadPath_Extended_By_N_times_2");
    }

    // ============ C: regime sensitivity (88 -> +3, 31 -> +8) ==================
    // Sprites OFF (SPD=1) -> S=88 -> 3 T-states/access.
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kActiveVsync, /*r1=*/0x40, /*r8=*/0x02, 6, 2);
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        const std::uint64_t cost = rederived_slot_cost(88);  // == 3
        expect_eq_u64(cost, 3, "C_SlotCost88_Rederived_Is3");
        const int n = 3;
        for (int i = 0; i < n; ++i) {
            vdp.io_write(0x98, 0x11);
        }
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0 + static_cast<std::uint64_t>(n) * cost,
                      "C_SpritesOff_Extended_By_N_times_3");
    }
    // Sprites ON (SPD=0) -> S=31 -> 8 T-states/access. An 88<->31 swap makes this
    // (or C above) fail -- the two costs (3 vs 8) are distinct.
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kActiveVsync, /*r1=*/0x40, /*r8=*/0x00, 6, 2);
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        const std::uint64_t cost = rederived_slot_cost(31);  // == 8
        expect_eq_u64(cost, 8, "C_SlotCost31_Rederived_Is8");
        const int n = 3;
        for (int i = 0; i < n; ++i) {
            vdp.io_write(0x98, 0x11);
        }
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0 + static_cast<std::uint64_t>(n) * cost,
                      "C_SpritesOn_Extended_By_N_times_8");
        // Distinctness of the three per-access costs (discrimination).
        expect(rederived_slot_cost(154) < rederived_slot_cost(88) &&
                   rederived_slot_cost(88) < rederived_slot_cost(31),
               "C_ThreeCosts_StrictlyOrdered_2_3_8");
    }

    // ============ D: adversarial -- window closed => no extension =============
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kVblankVsync, 0x40, 0x00, 6, 2);
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        // now == expiry -> NOT busy (cmd_busy > now is false) -> no steal.
        clk.total_ = w0;
        vdp.io_write(0x98, 0x22);
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0, "D_WriteAfterExpiry_NoExtension");
        // Well past expiry, read path -> still no steal.
        clk.total_ = w0 + 10000;
        (void)vdp.io_read(0x98);
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0, "D_ReadAfterExpiry_NoExtension");
    }

    // ============ E: CPU-invariance at the device level (AC-4) ================
    // (1) The returned #98 read byte is identical with vs without a busy command
    // (the steal does not touch the VRAM data path). (2) The clock total is NEVER
    // advanced by a #98 access -- the VDP cannot stall the CPU.
    {
        // Reference VDP: NO command busy. Seed a known byte at address 0, set the
        // read pointer to 0, then read #98 twice (first returns the read-ahead
        // prefetch of addr 0, i.e. the seeded byte).
        auto read_seq = [](const bool arm_busy) {
            V9958Vdp vdp;
            FakeClock clk;
            clk.total_ = 0;
            clk.since_vsync_ = kVblankVsync;
            vdp.attach_clock_source(&clk);
            set_register(vdp, 0, 0x06);  // GRAPHIC4
            // Seed VRAM[0] = 0xA5 via a #98 write (pointer auto-advances to 1).
            set_register(vdp, 14, 0x00);          // A16..A14 = 0
            vdp.io_write(0x99, 0x00);             // addr low = 0
            vdp.io_write(0x99, 0x40);             // addr high, bit6=1 (write access) -> ptr=0
            vdp.io_write(0x98, 0xA5);             // VRAM[0]=0xA5, ptr->1
            if (arm_busy) {
                // Arm a long busy command AFTER seeding, so the subsequent reads
                // occur while busy (they will steal, but must return the SAME
                // data). The fill is placed at DY=100 (rows 100..163 in GRAPHIC4,
                // 128 bytes/line -> addr >= 12800) so it CANNOT touch VRAM[0]/[1].
                clk.total_ = 0;
                set_dx(vdp, 0);
                set_dy(vdp, 100);
                set_nx(vdp, 64);
                set_ny(vdp, 64);
                set_col(vdp, 0x5A);
                set_cmd(vdp, 0xC0);  // HMMV, big -> long window
            }
            // Point the read pointer back at 0 (bit6=0 -> read access, issues the
            // read-ahead prefetch of VRAM[0]).
            vdp.io_write(0x99, 0x00);  // addr low = 0
            vdp.io_write(0x99, 0x00);  // addr high, bit6=0 (read) -> ptr=0, prefetch
            const std::uint64_t before = clk.total_;
            const std::uint8_t b0 = static_cast<std::uint8_t>(vdp.io_read(0x98));  // returns VRAM[0]
            const std::uint8_t b1 = static_cast<std::uint8_t>(vdp.io_read(0x98));  // returns VRAM[1]
            const std::uint64_t after = clk.total_;
            // (2) The VDP never advanced the clock across the #98 reads.
            expect_eq_u64(after, before, "E_Clock_NeverAdvancedByVdp");
            return std::pair<std::uint8_t, std::uint8_t>{b0, b1};
        };
        const auto ref = read_seq(/*arm_busy=*/false);
        const auto busy = read_seq(/*arm_busy=*/true);
        expect(ref.first == 0xA5, "E_ReadAhead_SeededByte_A5");
        // (1) Identical returned bytes whether or not a command is busy.
        expect(ref.first == busy.first && ref.second == busy.second,
               "E_ReadData_Identical_BusyVsIdle");
    }

    // ============ F: determinism =============================================
    {
        auto run = []() {
            V9958Vdp vdp;
            FakeClock clk;
            arm_graphic4_hmmv(vdp, clk, kActiveVsync, 0x40, 0x00, 6, 2);  // sprites on
            for (int i = 0; i < 7; ++i) {
                vdp.io_write(0x98, 0x11);
            }
            return vdp.cmd_busy_until_cycles();
        };
        const std::uint64_t a = run();
        const std::uint64_t b = run();
        expect_eq_u64(a, b, "F_Determinism_IdenticalInputsIdenticalWindow");
    }

    // ============ G: inert paths (no clock / suspended) ======================
    // No clock attached: the steal is inert (cmd_busy stays 0, no crash).
    {
        V9958Vdp vdp;  // no attach_clock_source
        set_register(vdp, 0, 0x06);
        vdp.io_write(0x98, 0x33);
        (void)vdp.io_read(0x98);
        expect_eq_u64(vdp.cmd_busy_until_cycles(), 0, "G_NoClock_Inert");
    }
    // Command timing suspended: even with a genuinely-armed busy window, a #98
    // access does NOT steal (the debug/non-perturbing seam stays byte-identical).
    {
        V9958Vdp vdp;
        FakeClock clk;
        arm_graphic4_hmmv(vdp, clk, kVblankVsync, 0x40, 0x00, 6, 2);  // window armed, busy
        const std::uint64_t w0 = vdp.cmd_busy_until_cycles();
        expect(w0 > clk.total_, "G_Suspended_Precondition_Busy");
        vdp.set_command_timing_suspended(true);
        vdp.io_write(0x98, 0x44);
        (void)vdp.io_read(0x98);
        expect_eq_u64(vdp.cmd_busy_until_cycles(), w0, "G_Suspended_NoSteal");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
