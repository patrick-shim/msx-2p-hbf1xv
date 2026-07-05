#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvCbProgram_Integration
//
// Runs a small deterministic routine that mixes completed unprefixed opcodes
// (LD immediate, DJNZ loop, LD (nn),A, HALT) with a CB-prefixed rotate
// (RLC A) through the machine boundary, asserting final register/memory state
// and the deterministic accumulated T-state total.

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
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();

    // LD A,0x01 ; LD B,0x03 ; loop: RLC A ; DJNZ loop ; LD (0x4000),A ; HALT
    const std::array<std::uint8_t, 12> program{
        0x3E, 0x01,        // 0x0000 LD A,0x01
        0x06, 0x03,        // 0x0002 LD B,0x03
        0xCB, 0x07,        // 0x0004 RLC A
        0x10, 0xFC,        // 0x0006 DJNZ -4 (back to 0x0004)
        0x32, 0x00, 0x40,  // 0x0008 LD (0x4000),A
        0x76,              // 0x000B HALT
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    // Step instruction-by-instruction until HALT is entered (bounded for determinism).
    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 64) {
        machine.step_cpu_instruction();
        ++guard;
    }

    if (!expect_true(machine.cpu().state().halted(),
            "CbLoopProgram_Executed_ReachesHaltState")) {
        return 1;
    }

    // A = 0x01 rotated left three times = 0x08.
    if (!expect_true(machine.cpu().state().regs().a() == 0x08,
            "CbLoopProgram_RlcAcrossLoop_AccumulatorRotatedThrice")) {
        return 1;
    }

    if (!expect_true(machine.read_memory(0x4000) == 0x08,
            "CbLoopProgram_StoreResult_MemoryHoldsRotatedValue")) {
        return 1;
    }

    if (!expect_true(machine.cpu().state().regs().b() == 0x00,
            "CbLoopProgram_DjnzLoop_CounterExhausted")) {
        return 1;
    }

    // Deterministic cycle oracle, MSX-accurate (datasheet + S1985 +1-per-M1 wait,
    // fact-sheet §8). Datasheet total = 7+7 + (8+13)+(8+13) + (8+8) + 13 + 4 = 89.
    // M1 cycles: LD A(1) + LD B(1) + 3x[RLC A(2) + DJNZ(1)] + LD(nn),A(1) + HALT(1)
    //   = 13, so machine total = 89 + 13 = 102.
    if (!expect_true(machine.elapsed_cycles() == 102,
            "CbLoopProgram_SequenceExecuted_DeterministicCycleTotal")) {
        return 1;
    }

    return 0;
}
