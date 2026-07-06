#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvIndexedProgram_Integration
//
// Drives a DD-prefixed (IX) program through the machine boundary: loads IX,
// stores two bytes via (IX+d) displacement writes, reads one back into A, adds
// the second, then HALTs. Asserts destination memory, IX, the accumulator, and
// a hardcoded deterministic T-state oracle covering indexed instruction timing.
// The stepping loop is bounded (no wall-clock) for determinism.

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
    // Authentic reset boots slot-0 BIOS (M13-S4 #A8=0); page flat RAM in for this
    // CPU-over-RAM program (M11 R-1/R-2).
    machine.map_flat_ram();

    // LD IX,0x4000 ; LD (IX+0),0xAB ; LD (IX+1),0xCD ;
    // LD A,(IX+0)  ; ADD A,(IX+1)   ; HALT
    const std::array<std::uint8_t, 19> program{
        0xDD, 0x21, 0x00, 0x40,  // 0x0000 LD IX,0x4000        (14T)
        0xDD, 0x36, 0x00, 0xAB,  // 0x0004 LD (IX+0),0xAB      (19T)
        0xDD, 0x36, 0x01, 0xCD,  // 0x0008 LD (IX+1),0xCD      (19T)
        0xDD, 0x7E, 0x00,        // 0x000C LD A,(IX+0)         (19T)
        0xDD, 0x86, 0x01,        // 0x000F ADD A,(IX+1)        (19T)
        0x76,                    // 0x0012 HALT                (4T)
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 64) {
        machine.step_cpu_instruction();
        ++guard;
    }

    if (!expect_true(machine.cpu().state().halted(),
            "IndexedProgram_Executed_ReachesHaltState")) {
        return 1;
    }

    if (!expect_true(machine.cpu().state().regs().ix == 0x4000,
            "IndexedProgram_IndexRegister_Loaded")) {
        return 1;
    }

    if (!expect_true(machine.read_memory(0x4000) == 0xAB && machine.read_memory(0x4001) == 0xCD,
            "IndexedProgram_DisplacementStores_WriteThroughIndex")) {
        return 1;
    }

    // A = (IX+0) + (IX+1) = 0xAB + 0xCD = 0x178 -> 0x78.
    if (!expect_true(machine.cpu().state().regs().a() == 0x78,
            "IndexedProgram_IndexedAdd_AccumulatorWrapped")) {
        return 1;
    }

    // Cycle oracle, MSX-accurate (datasheet + S1985 +1-per-M1 wait, fact-sheet §8).
    // Datasheet: 14 + 19 + 19 + 19 + 19 + 4 = 94. Each DD-prefixed instruction has
    // 2 M1 cycles (DD + sub-opcode); HALT has 1: M1 = 5*2 + 1 = 11 -> 94 + 11 = 105.
    if (!expect_true(machine.elapsed_cycles() == 105,
            "IndexedProgram_SequenceExecuted_DeterministicCycleTotal")) {
        return 1;
    }

    return 0;
}
