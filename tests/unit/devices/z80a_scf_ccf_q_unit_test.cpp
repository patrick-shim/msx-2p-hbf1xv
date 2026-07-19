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

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AScfCcfQ_Unit
//
// Proves the genuine-Zilog NMOS SCF/CCF undocumented X/Y rule with the Q latch:
//   X = bit 3, Y = bit 5 of ((Q ^ F) | A)
// where Q is the flag byte latched by the PREVIOUS instruction, or 0 if that
// instruction did not modify flags (incl. EX AF,AF', POP AF, and interrupt
// acceptance). Fact-sheet §8 (Patrik-Rak). Deliberately diverges from openMSX's
// (F | A) OR-form in the "previous instruction modified flags"
// case: after a flag op, Q == F so (Q ^ F) == 0 and X/Y are MOVED from A;
// after a non-flag op, Q == 0 so X/Y are OR'd from F into A.

namespace {

namespace cpu = sony_msx::devices::cpu;
using State = cpu::Z80aState;

class ArrayBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress address) override {
        return memory[address];
    }
    void write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        memory[address] = value;
    }
    std::array<std::uint8_t, 65536> memory{};
};

constexpr std::uint8_t kXY = static_cast<std::uint8_t>(State::kFlagX | State::kFlagY);

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

int g_failures = 0;

}  // namespace

int main() {
    // --- SCF after a flag-modifying op: X/Y MOVED from A (Q == F) ----------
    // Predecessor CP 0x20 sets F.Y (X/Y of CP come from the operand) while A
    // stays 0x00. Because Q == F, (Q^F)==0, so SCF sources X/Y purely from A
    // (0x00) -> no X/Y. The (F|A) OR-form would incorrectly set Y from F.
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x00);
        bus.memory[0x0000] = 0xFE;  // CP 0x20  (flag-modifying; F.Y set from operand)
        bus.memory[0x0001] = 0x20;
        bus.memory[0x0002] = 0x37;  // SCF
        cpu.step();                 // CP
        const std::uint8_t f_after_cp = cpu.state().regs().f();
        cpu.step();                 // SCF
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true((f_after_cp & State::kFlagY) != 0 && (f & kXY) == 0 &&
                             (f & State::kFlagCarry) != 0,
                         "Scf_AfterFlagOp_XYMovedFromA")) {
            return 1;
        }
    }

    // --- SCF after a flag-modifying op with A carrying X/Y: moved from A ----
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x28);  // X and Y bits set in A
        bus.memory[0x0000] = 0xB7;       // OR A (flag op; result 0x28 -> F.XY set)
        bus.memory[0x0001] = 0x37;       // SCF
        cpu.step();
        cpu.step();
        const std::uint8_t f = cpu.state().regs().f();
        // Q == F, so X/Y = A & XY = 0x28.
        if (!expect_true((f & kXY) == 0x28 && (f & State::kFlagCarry) != 0,
                         "Scf_AfterFlagOp_XYFromAccumulator")) {
            return 1;
        }
    }

    // --- SCF after POP AF (non-flag op): X/Y OR'd from F (Q == 0) ----------
    // POP AF loads F.Y=1, A=0x00 and does NOT latch Q (writes AF wholesale).
    // SCF then computes (0 ^ F | A) so Y survives from F even though A is 0.
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().sp = 0x8000;
        bus.memory[0x8000] = 0x20;  // popped F = 0x20 (Y set)
        bus.memory[0x8001] = 0x00;  // popped A = 0x00
        bus.memory[0x0000] = 0xF1;  // POP AF
        bus.memory[0x0001] = 0x37;  // SCF
        cpu.step();                 // POP AF -> Q stays 0
        cpu.step();                 // SCF
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true((f & State::kFlagY) != 0 && (f & State::kFlagX) == 0 &&
                             (f & State::kFlagCarry) != 0,
                         "Scf_AfterPopAf_XYOrredFromF")) {
            return 1;
        }
    }

    // --- SCF after EX AF,AF' (non-flag op): Q == 0 -> OR-from-F -------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().af_shadow = 0x0020;  // swapped-in A=0x00, F=0x20 (Y)
        bus.memory[0x0000] = 0x08;              // EX AF,AF'
        bus.memory[0x0001] = 0x37;              // SCF
        cpu.step();                             // EX AF,AF' -> Q stays 0
        cpu.step();                             // SCF
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true((f & State::kFlagY) != 0 && (f & State::kFlagX) == 0,
                         "Scf_AfterExAfAf_QZeroOrFromF")) {
            return 1;
        }
    }

    // --- SCF as first ISR instruction: Q == 0 after interrupt accept -------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().regs().set_a(0x00);
        cpu.state().regs().set_f(0x20);  // F.Y set (preserved through the accept)
        cpu.state().regs().sp = 0x9000;
        cpu.state().regs().pc = 0x0100;
        bus.memory[0x0038] = 0x37;  // SCF at the IM1 handler
        cpu.request_maskable_interrupt();
        cpu.step();  // interrupt accept -> Q reset to 0, F unchanged
        cpu.step();  // SCF
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true((f & State::kFlagY) != 0 && (f & State::kFlagCarry) != 0,
                         "Scf_FirstIsrInstruction_QZeroOrFromF")) {
            return 1;
        }
    }

    // --- CCF after a flag op: X/Y moved from A; H = old carry --------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x00);
        bus.memory[0x0000] = 0xFE;  // CP 0x20 (sets F.Y from operand; also C set: 0-0x20 borrows)
        bus.memory[0x0001] = 0x20;
        bus.memory[0x0002] = 0x3F;  // CCF
        cpu.step();                 // CP -> C set, Y set, Q = F
        cpu.step();                 // CCF
        const std::uint8_t f = cpu.state().regs().f();
        // Q == F -> X/Y from A (0x00) -> none. CCF: old C was set -> new C clear, H = old C = set.
        if (!expect_true((f & kXY) == 0 && (f & State::kFlagHalfCarry) != 0 &&
                             (f & State::kFlagCarry) == 0,
                         "Ccf_AfterFlagOp_XYMovedFromA_HIsOldCarry")) {
            return 1;
        }
    }

    // --- CCF after POP AF (non-flag op): Q == 0 -> OR-from-F ---------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().sp = 0x8000;
        bus.memory[0x8000] = 0x20;  // popped F = 0x20 (Y set, C clear)
        bus.memory[0x8001] = 0x00;  // popped A = 0x00
        bus.memory[0x0000] = 0xF1;  // POP AF
        bus.memory[0x0001] = 0x3F;  // CCF
        cpu.step();
        cpu.step();
        const std::uint8_t f = cpu.state().regs().f();
        // Q == 0 -> Y OR'd from F; old C clear -> new C set.
        if (!expect_true((f & State::kFlagY) != 0 && (f & State::kFlagCarry) != 0,
                         "Ccf_AfterPopAf_QZeroOrFromF")) {
            return 1;
        }
    }

    return g_failures;
}
