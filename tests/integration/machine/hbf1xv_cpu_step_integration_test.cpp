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
    // Suite: Machine_Hbf1xvCpuStep_Integration
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    // Authentic reset boots slot-0 BIOS (M13-S4 #A8=0). This test runs a program
    // from RAM, so page the flat 64 KB mapper RAM in explicitly (M11 R-1/R-2).
    machine.map_flat_ram();

    if (!expect_true(machine.cpu().state().interrupt_mode() == sony_msx::devices::cpu::InterruptMode::Im1,
            "ColdBoot_DefaultInterruptMode_Im1")) {
        return 1;
    }

    const std::array<std::uint8_t, 5> program{
        0x3E, 0x2A,  // LD A,0x2A
        0x3C,        // INC A
        0x76,        // HALT
        0x00,        // NOP (not reached while halted)
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    // Machine T-states are MSX-accurate: datasheet + S1985 +1-per-M1 wait (M11,
    // fact-sheet §8). LD A,n = 7+1, INC A = 4+1, HALT = 4+1; the halted idle
    // step ALSO performs a phantom M1 opcode refetch (M23-S1, closes backlog
    // C2/DEC-0004), so it is 4+1 = 5T, not 4T. Grounded:
    // references/openmsx-21.0/src/cpu/Z80.hh:19-21 (HALT_STATES = 4 +
    // WAIT_CYCLES) and CPUCore.cc:2508-2511 (incR(advanceHalt(...))) -- the
    // SAME `halts` computation drives both the R-register increment and the
    // clock advance on real hardware, so they cannot be separated.
    const std::uint32_t t0 = machine.step_cpu_instruction();
    if (!expect_true(t0 == 8 && machine.cpu().state().regs().a() == 0x2A,
            "CpuStep_LdAImmediate_DeterministicRegisterUpdate")) {
        return 1;
    }

    const std::uint32_t t1 = machine.step_cpu_instruction();
    if (!expect_true(t1 == 5 && machine.cpu().state().regs().a() == 0x2B,
            "CpuStep_IncA_AppliesExpectedArithmetic")) {
        return 1;
    }

    const std::uint32_t t2 = machine.step_cpu_instruction();
    if (!expect_true(t2 == 5 && machine.cpu().state().halted(),
            "CpuStep_HaltInstruction_TransitionsToHaltedState")) {
        return 1;
    }

    const std::uint16_t pc_before_idle = machine.cpu().state().regs().pc;
    const std::uint32_t t3 = machine.step_cpu_instruction();
    // t3 == 5 (was 4 pre-M23): the halted idle step now correctly bills the
    // phantom M1 refetch's S1985 wait (M23-S1, DEC-0004, backlog C2 closes in
    // full). See the grounding comment above t0.
    if (!expect_true(t3 == 5 && machine.cpu().state().regs().pc == pc_before_idle,
            "CpuStep_HaltedNoInterrupt_StaysAtSameProgramCounter")) {
        return 1;
    }

    // 8 + 5 + 5 + 5 = 23 (was 22 pre-M23; datasheet 19 + FOUR M1 waits: the
    // three fetched ops plus the halted idle step's phantom refetch, A-M23-4).
    if (!expect_true(machine.elapsed_cycles() == 23,
            "CpuStep_SequenceExecuted_MachineCycleCounterTracksTstates")) {
        return 1;
    }

    return 0;
}