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

    // MSX-accurate machine T-states = datasheet + S1985 +1-per-M1 wait (M11,
    // fact-sheet §8). EI = 4+1, NOP = 4+1, IM1 acknowledge = 13 + 1 (the ack M1).
    if (!expect_true(machine.step_cpu_instruction() == 5,
            "EiInstruction_BootstrapProgram_ExpectedTiming")) {
        return 1;
    }

    machine.cpu().request_maskable_interrupt();

    if (!expect_true(machine.step_cpu_instruction() == 5 && machine.cpu().state().regs().pc == 0x0002,
            "MaskableInterrupt_AfterEi_DeferredForSingleInstruction")) {
        return 1;
    }

    if (!expect_true(machine.step_cpu_instruction() == 14 && machine.cpu().state().regs().pc == 0x0038,
            "MaskableInterrupt_AfterDelayTaken_Im1VectorUsed")) {
        return 1;
    }

    // 5 + 5 + 14 = 24 (datasheet 21 + three M1 waits).
    if (!expect_true(machine.elapsed_cycles() == 24,
            "BootstrapSequence_ThreeCpuSteps_DeterministicCycleTotal")) {
        return 1;
    }

    return 0;
}