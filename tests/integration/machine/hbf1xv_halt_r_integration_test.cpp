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

#include <array>
#include <cstdint>
#include <iostream>

#include "devices/cpu/z80a_cpu.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvHaltR_Integration  (M23-S1, closes backlog C2 in
// full / DEC-0004)
//
// Machine-level companion to tests/unit/devices/z80a_halt_r_unit_test.cpp.
// Deliberately a NEW file rather than an edit to
// hbf1xv_interrupt_ack_integration_test.cpp: that file is one of the planner
// package's NAMED, zero-tolerance oracle suites (§4/§6 item 4) required to
// show a byte-identical `git diff` against the v1.0.22 tag, so this new
// scenario -- several halted idle steps (R visibly incrementing, 5T each at
// the machine level) followed by an interrupt wake at the IM1-ack timing
// (13T; corrected from the earlier 14T guess once an openMSX A/B settled that
// the S1985 M1 wait does NOT apply to the interrupt-acknowledge M1) -- is
// proven here.

namespace {

using sony_msx::devices::cpu::InterruptMode;

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();
    machine.cpu().set_interrupt_mode(InterruptMode::Im1);
    machine.cpu().state().set_iff1(true);

    const std::array<std::uint8_t, 1> main_program{0x76};  // HALT
    machine.load_memory(0x0000, main_program.data(), static_cast<std::uint32_t>(main_program.size()));

    // HALT opcode's own M1 fetch: unaffected by this milestone (already-correct
    // datasheet 4 + M1 wait 1 = 5T).
    const std::uint32_t t_halt = machine.step_cpu_instruction();
    if (!expect_true(t_halt == 5 && machine.cpu().state().halted(), "HaltOpcode_CostsFiveTstates")) {
        return 1;
    }
    const std::uint8_t r_after_halt = machine.cpu().state().regs().r;

    // Several halted idle steps: each is 4 (bare) + 1 (S1985 M1 wait) = 5T at
    // the machine level (M23-S1 closes this; was 4T pre-M23), and R visibly
    // increments once per step (low 7 bits, bit 7 preserved).
    constexpr int kIdleSteps = 5;
    bool idle_ok = true;
    for (int i = 0; i < kIdleSteps; ++i) {
        const std::uint8_t r_before = machine.cpu().state().regs().r;
        const std::uint32_t t_idle = machine.step_cpu_instruction();
        const std::uint8_t r_after = machine.cpu().state().regs().r;
        idle_ok = idle_ok && (t_idle == 5) &&
                  (static_cast<std::uint8_t>((r_after - r_before) & 0x7F) == 1);
    }
    if (!expect_true(idle_ok, "HaltedIdle_SeveralSteps_EachFiveTstatesRIncrementsByOne")) {
        return 1;
    }
    const std::uint8_t r_before_irq = machine.cpu().state().regs().r;
    if (!expect_true(static_cast<std::uint8_t>((r_before_irq - r_after_halt) & 0x7F) == kIdleSteps,
            "HaltedIdle_SeveralSteps_RefreshRegisterAdvancesByStepCount")) {
        return 1;
    }

    // Interrupt wake: IM1-ack timing is 13T = 13 bare + 0 M1 wait. The ack special
    // M1 asserts /M1 + /IORQ (not the opcode-fetch /M1 + /MREQ the S1985 gates its
    // +1 wait on), so no S1985 wait is billed -- proving zero coupling between the
    // HALT-R fix and interrupt-accept timing (openMSX CC_IRQ1 = 7+3+3 = 13; live
    // openMSX A/B measured a 13T accept).
    machine.cpu().request_maskable_interrupt();
    const std::uint32_t t_wake = machine.step_cpu_instruction();
    if (!expect_true(t_wake == 13 && !machine.cpu().state().halted() &&
                         machine.cpu().state().regs().pc == 0x0038,
            "HaltedIdle_InterruptWake_Im1AckThirteenTstatesUnaffected")) {
        return 1;
    }

    return 0;
}
