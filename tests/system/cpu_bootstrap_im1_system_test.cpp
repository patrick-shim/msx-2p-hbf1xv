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

#include "machine/hbf1xv_machine.h"

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: System_CpuBootstrapIm1_System
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // bootstrap program runs from RAM page 0

    if (!expect_true(machine.cpu().state().interrupt_mode() == sony_msx::devices::cpu::InterruptMode::Im1,
            "ColdBoot_InitialState_DefaultsToIm1")) {
        return 1;
    }

    if (!expect_true(!machine.cpu().state().iff1() && !machine.cpu().state().iff2(),
            "ColdBoot_InitialState_InterruptFlipFlopsDisabled")) {
        return 1;
    }

    const std::array<std::uint8_t, 4> program{
        0xFB,  // EI
        0x00,  // NOP (EI delay window)
        0x00,  // Next instruction replaced by interrupt service entry
        0x00,
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    // MSX-accurate machine T-states = Z80 datasheet + the S1985's +1 wait per
    // M1 cycle. EI = 4+1, NOP = 4+1. IM1 acknowledge = 13 with NO M1 wait:
    // the ack special M1 is /M1 + /IORQ, which the S1985's opcode-fetch-gated
    // (/M1 + /MREQ) wait generator does not stretch (openMSX CC_IRQ1 = 7+3+3 = 13;
    // live openMSX A/B: 11T JP + 13T accept = 24T).
    if (!expect_true(machine.step_cpu_instruction() == 5,
            "EiInstruction_BootstrapProgram_ExpectedTiming")) {
        return 1;
    }

    machine.cpu().request_maskable_interrupt();

    if (!expect_true(machine.step_cpu_instruction() == 5 && machine.cpu().state().regs().pc == 0x0002,
            "MaskableInterrupt_AfterEi_DeferredForSingleInstruction")) {
        return 1;
    }

    if (!expect_true(machine.step_cpu_instruction() == 13 && machine.cpu().state().regs().pc == 0x0038,
            "MaskableInterrupt_AfterDelayTaken_Im1VectorUsed")) {
        return 1;
    }

    // 5 + 5 + 13 = 23 (datasheet 21 + two M1 waits: EI + NOP; the ack takes none).
    if (!expect_true(machine.elapsed_cycles() == 23,
            "BootstrapSequence_ThreeCpuSteps_DeterministicCycleTotal")) {
        return 1;
    }

    return 0;
}