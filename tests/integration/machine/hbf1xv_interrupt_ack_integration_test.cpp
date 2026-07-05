#include <array>
#include <cstdint>
#include <iostream>

#include "devices/cpu/z80a_cpu.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvInterruptAck_Integration
//
// Drives interrupt acknowledge end-to-end through the machine boundary for the
// M9-S5 IM0/IM2 fidelity work. Device bus vectors are injected as deterministic
// known values; the test asserts the ISR actually runs, the return address is
// correct, the stack is balanced on return, and a hardcoded T-state oracle
// holds. The stepping loop is bounded (no wall-clock) for determinism.

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
    // --- IM2: device-supplied vector low byte selects the ISR via the table. ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.cold_boot();
        machine.cpu().set_interrupt_mode(InterruptMode::Im2);
        machine.cpu().state().set_iff1(true);
        machine.cpu().state().regs().i = 0x40;

        // Main program: HALT at 0x0000 (interrupted before it retires nothing else).
        const std::array<std::uint8_t, 1> main_program{0x76};
        machine.load_memory(0x0000, main_program.data(),
                            static_cast<std::uint32_t>(main_program.size()));

        // ISR at 0x6000: LD A,0x55 ; RETI.
        const std::array<std::uint8_t, 4> isr{0x3E, 0x55, 0xED, 0x4D};
        machine.load_memory(0x6000, isr.data(), static_cast<std::uint32_t>(isr.size()));

        // Vector table entry at (0x40 << 8) | 0x08 = 0x4008 -> 0x6000 little-endian.
        const std::array<std::uint8_t, 2> table_entry{0x00, 0x60};
        machine.load_memory(0x4008, table_entry.data(),
                            static_cast<std::uint32_t>(table_entry.size()));

        machine.cpu().request_maskable_interrupt(0x08);

        int guard = 0;
        std::uint64_t cycles = 0;
        while (!machine.cpu().state().halted() && guard < 32) {
            cycles += machine.step_cpu_instruction();
            ++guard;
        }

        if (!expect_true(machine.cpu().state().halted() &&
                             machine.cpu().state().regs().a() == 0x55,
                "Im2Ack_IsrExecuted_AccumulatorSetAndHalted")) {
            return 1;
        }
        if (!expect_true(machine.cpu().state().regs().pc == 0x0001 &&
                             machine.cpu().state().regs().sp == 0xFFFF,
                "Im2Ack_Returned_StackBalancedAndResumed")) {
            return 1;
        }
        // Oracle, MSX-accurate (datasheet + S1985 +1-per-M1 wait, fact-sheet §8).
        // Datasheet: IM2 ack(19) + LD A,n(7) + RETI(14) + HALT(4) = 44. M1 cycles:
        // ack(1) + LD A,n(1) + RETI ED-4D(2) + HALT(1) = 5 -> 44 + 5 = 49.
        if (!expect_true(cycles == 49, "Im2Ack_Sequence_DeterministicCycleTotal")) {
            return 1;
        }
    }

    // --- IM0: device drives an RST 18h opcode onto the bus. -------------------
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.cold_boot();
        machine.cpu().set_interrupt_mode(InterruptMode::Im0);
        machine.cpu().state().set_iff1(true);

        // Main program: HALT at 0x0000.
        const std::array<std::uint8_t, 1> main_program{0x76};
        machine.load_memory(0x0000, main_program.data(),
                            static_cast<std::uint32_t>(main_program.size()));

        // ISR at RST 18h target 0x0018: LD B,0x99 ; RET.
        const std::array<std::uint8_t, 3> isr{0x06, 0x99, 0xC9};
        machine.load_memory(0x0018, isr.data(), static_cast<std::uint32_t>(isr.size()));

        machine.cpu().request_maskable_interrupt(0xDF);  // 0xDF = RST 18h

        int guard = 0;
        std::uint64_t cycles = 0;
        while (!machine.cpu().state().halted() && guard < 32) {
            cycles += machine.step_cpu_instruction();
            ++guard;
        }

        if (!expect_true(machine.cpu().state().halted() &&
                             machine.cpu().state().regs().b() == 0x99,
                "Im0Ack_BusRstOpcode_IsrExecuted")) {
            return 1;
        }
        if (!expect_true(machine.cpu().state().regs().pc == 0x0001 &&
                             machine.cpu().state().regs().sp == 0xFFFF,
                "Im0Ack_Returned_StackBalancedAndResumed")) {
            return 1;
        }
        // Oracle, MSX-accurate (datasheet + S1985 +1-per-M1 wait, fact-sheet §8).
        // Datasheet: IM0 ack RST(13) + LD B,n(7) + RET(10) + HALT(4) = 34. M1
        // cycles: ack(1) + LD B,n(1) + RET(1) + HALT(1) = 4 -> 34 + 4 = 38.
        if (!expect_true(cycles == 38, "Im0Ack_Sequence_DeterministicCycleTotal")) {
            return 1;
        }
    }

    return 0;
}
