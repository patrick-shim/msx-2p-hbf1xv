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

// Suite: Machine_Hbf1xvM48CpuVramStealInvariance_Integration
// (M48 Slice 2, DEC-0075; docs/m48-planner-package.md §3 Slice-2 DoD + §4.1
//  AC-4 [HARD, CPU byte-identical] + §6 R1)
//
// Slice 2 adds a CPU-priority VRAM access-slot STEAL: each CPU #98 access during
// a busy command lengthens the command's S#2-bit0 CE busy window. It is
// ONE-DIRECTIONAL -- the CPU access is NEVER stalled/delayed/dropped (the
// HB-F1XV does not wire /WAIT; fact-sheet §1/§7). This test proves that
// invariance in the REAL machine path (real Z80 core, real I/O bus, real
// scheduler), not just at the device level.
//
// Two byte-for-byte-identical Z80 programs are run -- both set up GRAPHIC4, issue
// a command, then hammer `OUT (#98),A` -- differing ONLY in the R#46 command
// operand:
//   * BUSY program: R#46 = 0xC0 (HMMV, large NX/NY) -> arms a long CE window, so
//     every subsequent OUT (#98) STEALS a slot (the timing overlay is active).
//   * IDLE program: R#46 = 0x00 (STOP/abort) -> no busy window, no steal.
// Because the two instruction streams are identical (one immediate operand byte
// differs), the CPU T-state cost of every instruction MUST be identical. Case 1
// asserts that lockstep, per-instruction AND cumulative -- so if the steal ever
// perturbed CPU timing, it would show. This is the machine-path witness for
// AC-4, complementing the M23 non-gating fixtures' pre/post byte-identity.
//
// Case 2 proves the steal is genuinely ACTIVE in the machine path (non-vacuous):
// the BUSY program's CE window, captured just after the command is armed, grows
// by exactly K * slot_cost(154) = K * 2 T-states over the K OUT (#98) writes
// (display OFF during early stepping -> the 154-slot regime; slot_cost(154) =
// ceil(1368/(6*154)) = 2, re-derived here from the tier-1 constants).

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/video/vdp_access_timing.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::machine::Hbf1xvMachine;
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

// --- Z80 program emitters ---------------------------------------------------
void emit_out99(std::vector<std::uint8_t>& p, const std::uint8_t v) {
    p.push_back(0x3E);  // LD A,n
    p.push_back(v);
    p.push_back(0xD3);  // OUT (n),A
    p.push_back(0x99);
}
void emit_out98(std::vector<std::uint8_t>& p, const std::uint8_t v) {
    p.push_back(0x3E);  // LD A,n
    p.push_back(v);
    p.push_back(0xD3);  // OUT (n),A
    p.push_back(0x98);
}
// Write VDP register `reg` = value via the #99 two-write latch protocol.
void emit_reg(std::vector<std::uint8_t>& p, const std::uint8_t reg, const std::uint8_t v) {
    emit_out99(p, v);
    emit_out99(p, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

constexpr int kNumOut98Writes = 8;  // K

// GRAPHIC4 + a command engine setup (DX/DY/NX/NY/COL) + R#46 = cmd_byte, then
// K OUT (#98) writes, then HALT. NX/NY are large so the BUSY (HMMV) variant's CE
// window stays open across all K writes. The IDLE variant uses cmd_byte=0x00
// (STOP) -> no window. The two differ ONLY in the single cmd_byte operand.
std::vector<std::uint8_t> build_program(const std::uint8_t cmd_byte) {
    std::vector<std::uint8_t> p;
    emit_reg(p, 0, 0x06);  // R#0 -> GRAPHIC4
    // Command-engine registers R#36..R#46 (see cmd_engine changeRegister map).
    emit_reg(p, 36, 0x00);  // DX low  = 0
    emit_reg(p, 37, 0x00);  // DX high = 0
    emit_reg(p, 38, 0x00);  // DY low  = 0
    emit_reg(p, 39, 0x00);  // DY high = 0
    emit_reg(p, 40, 0xC8);  // NX low  = 200
    emit_reg(p, 41, 0x00);  // NX high = 0
    emit_reg(p, 42, 0xC8);  // NY low  = 200
    emit_reg(p, 43, 0x00);  // NY high = 0
    emit_reg(p, 44, 0x5A);  // COL
    emit_reg(p, 46, cmd_byte);  // CMD (arms window when 0xC0 HMMV; inert for 0x00)
    for (int i = 0; i < kNumOut98Writes; ++i) {
        emit_out98(p, static_cast<std::uint8_t>(0x10 + i));
    }
    p.push_back(0x76);  // HALT
    return p;
}

void load_program(Hbf1xvMachine& machine, const std::vector<std::uint8_t>& program) {
    machine.cold_boot();
    machine.map_flat_ram();
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> busy_prog = build_program(0xC0);  // HMMV
    const std::vector<std::uint8_t> idle_prog = build_program(0x00);  // STOP
    // Byte-identical except the single R#46 command operand (structural guard: if
    // the two ever diverged in length/shape the lockstep proof would be invalid).
    expect(busy_prog.size() == idle_prog.size(), "Programs_SameLength");
    {
        int diff = 0;
        for (std::size_t i = 0; i < busy_prog.size(); ++i) {
            if (busy_prog[i] != idle_prog[i]) {
                ++diff;
            }
        }
        expect(diff == 1, "Programs_DifferByExactlyOneByte");
    }

    // ===== Case 1: CPU-timing invariance -- per-instruction lockstep =========
    {
        Hbf1xvMachine mb;  // steal ACTIVE (busy command)
        Hbf1xvMachine mi;  // steal INERT  (no command)
        load_program(mb, busy_prog);
        load_program(mi, idle_prog);

        std::uint64_t tb = 0;
        std::uint64_t ti = 0;
        bool per_step_identical = true;
        int steps = 0;
        for (int i = 0; i < 512 && !mb.cpu().state().halted(); ++i) {
            const std::uint32_t sb = mb.step_cpu_instruction();
            const std::uint32_t si = mi.step_cpu_instruction();
            if (sb != si) {
                per_step_identical = false;
            }
            tb += sb;
            ti += si;
            ++steps;
        }
        expect(mb.cpu().state().halted(), "C1_Busy_ReachesHalt");
        expect(mi.cpu().state().halted(), "C1_Idle_ReachesHalt");
        // Every instruction cost identical whether the steal is active or not.
        expect(per_step_identical, "C1_PerInstructionTstates_Identical_BusyVsIdle");
        // ...hence the cumulative total is byte-identical (AC-4, machine path).
        expect_eq_u64(tb, ti, "C1_CumulativeTstates_Identical_BusyVsIdle");
        expect(steps > 0, "C1_SteppedSomething");
        // Non-vacuous: the BUSY machine really did engage the timing overlay --
        // its CE window ends up strictly beyond the IDLE machine's (which never
        // armed a window). Both issued at the same absolute cycle, so this is a
        // direct witness that the busy path was exercised.
        expect(mb.vdp().cmd_busy_until_cycles() > mi.vdp().cmd_busy_until_cycles(),
               "C1_BusyPath_WindowExceedsIdle");
    }

    // ===== Case 2: steal fires in the machine path, correct amount ===========
    // Step the BUSY program; detect the exact instruction that arms a genuinely
    // busy window (cmd_busy_until > elapsed_cycles), capture it, run to HALT, and
    // assert the window grew by exactly K * slot_cost(154) = K*2 -- one stolen
    // slot per OUT (#98), none dropped, none phantom. Display is OFF during this
    // short run (no run_frame(); raster stays < 0), fixing the 154-slot regime.
    {
        Hbf1xvMachine machine;
        load_program(machine, busy_prog);

        std::uint64_t window_at_arm = 0;
        bool armed = false;
        for (int i = 0; i < 512 && !machine.cpu().state().halted(); ++i) {
            machine.step_cpu_instruction();
            if (!armed && machine.vdp().cmd_busy_until_cycles() > machine.elapsed_cycles()) {
                // The OUT (#99) that just wrote R#46 armed the window; no OUT (#98)
                // has executed yet, so this is the clean pre-steal Slice-1 baseline.
                window_at_arm = machine.vdp().cmd_busy_until_cycles();
                armed = true;
                break;
            }
        }
        expect(armed, "C2_CommandArmed_BusyWindowDetected");

        // Now run the remaining program (the K OUT (#98) writes + HALT).
        for (int i = 0; i < 512 && !machine.cpu().state().halted(); ++i) {
            machine.step_cpu_instruction();
        }
        expect(machine.cpu().state().halted(), "C2_ReachesHalt");

        const std::uint64_t window_final = machine.vdp().cmd_busy_until_cycles();
        const std::uint64_t slot_cost_154 = ceil_div(1368, 6 * 154);  // == 2 (re-derived)
        expect_eq_u64(slot_cost_154, 2, "C2_SlotCost154_Rederived_Is2");
        expect_eq_u64(slot_cost_154, static_cast<std::uint64_t>(vat::slot_cost_tstates(154)),
                      "C2_SlotCost154_MatchesHeaderHelper");
        // Exactly K steals of 2 T-states each -- proves the steal fired on every
        // #98 write in the machine path, at the correct 154-slot cost, and nothing
        // else touched the window (VRAM writes stay atomic; §5). A dropped steal
        // or a wrong regime makes this fail.
        expect_eq_u64(window_final,
                      window_at_arm + static_cast<std::uint64_t>(kNumOut98Writes) * slot_cost_154,
                      "C2_Window_Grew_By_K_times_2");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
