#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvLdirProgram_Integration
//
// Drives a full ED block-transfer (LDIR) through the machine boundary: sets up
// HL/DE/BC via unprefixed 16-bit loads, copies a 3-byte block, then HALTs.
// Asserts destination memory, final pointer/counter registers, and a hardcoded
// deterministic T-state oracle covering LDIR repeat-vs-terminate timing. The
// stepping loop is bounded (no wall-clock) for determinism.

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

    // LD HL,0x4000 ; LD DE,0x5000 ; LD BC,0x0003 ; LDIR ; HALT
    const std::array<std::uint8_t, 12> program{
        0x21, 0x00, 0x40,  // 0x0000 LD HL,0x4000
        0x11, 0x00, 0x50,  // 0x0003 LD DE,0x5000
        0x01, 0x03, 0x00,  // 0x0006 LD BC,0x0003
        0xED, 0xB0,        // 0x0009 LDIR
        0x76,              // 0x000B HALT
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    // Deterministic source block copied into 0x5000..0x5002.
    const std::array<std::uint8_t, 3> source{0xAA, 0xBB, 0xCC};
    machine.load_memory(0x4000, source.data(), static_cast<std::uint32_t>(source.size()));

    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 64) {
        machine.step_cpu_instruction();
        ++guard;
    }

    if (!expect_true(machine.cpu().state().halted(),
            "LdirProgram_Executed_ReachesHaltState")) {
        return 1;
    }

    if (!expect_true(machine.read_memory(0x5000) == 0xAA &&
                         machine.read_memory(0x5001) == 0xBB &&
                         machine.read_memory(0x5002) == 0xCC,
            "LdirProgram_BlockCopied_DestinationMatchesSource")) {
        return 1;
    }

    if (!expect_true(machine.cpu().state().regs().hl == 0x4003 &&
                         machine.cpu().state().regs().de == 0x5003 &&
                         machine.cpu().state().regs().bc == 0x0000,
            "LdirProgram_PointersAdvanced_CounterExhausted")) {
        return 1;
    }

    // Cycle oracle, MSX-accurate (datasheet + S1985 +1-per-M1 wait, fact-sheet §8).
    // Datasheet: LD HL,nn(10) + LD DE,nn(10) + LD BC,nn(10) + LDIR [21+21+16] +
    // HALT(4) = 92. This core models LDIR's repeat as re-executed ED-B0 steps, so
    // each of the 3 LDIR iterations fetches ED + B0 (2 M1). M1 = 3 loads + 3*2 +
    // HALT = 10 -> machine total 92 + 10 = 102.
    if (!expect_true(machine.elapsed_cycles() == 102,
            "LdirProgram_SequenceExecuted_DeterministicCycleTotal")) {
        return 1;
    }

    return 0;
}
