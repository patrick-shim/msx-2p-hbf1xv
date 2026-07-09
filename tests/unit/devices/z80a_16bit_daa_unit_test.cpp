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

// Suite: Devices_Z80A16BitDaa_Unit  (M12-S1, PRESENT-item regression floor for
// gaps #10 ADD HL,rr, #11 ADC/SBC HL,rr (SBC N=1), #2 X/Y from 16-bit high byte,
// #12 full-table DAA after add AND subtract). Grounding: fact-sheet §4;
// openMSX CPUCore.cc alu16 / daa.

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

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

int g_failures = 0;

}  // namespace

int main() {
    // --- ADD HL,BC: H from bit11, C from bit15, S/Z/PV preserved -----------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().hl = 0x0FFF;
        cpu.state().regs().bc = 0x0001;
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0x09;  // ADD HL,BC
        const std::uint32_t t = cpu.step();
        // 0x1000: H set (bit11 carry), no C, XY from hi 0x10 -> 0. F = H = 0x10.
        g_failures += expect_true(t == 11 && cpu.state().regs().hl == 0x1000 &&
                                      cpu.state().regs().f() == 0x10,
                                  "AddHlBc_HalfCarryFromBit11")
                          ? 0
                          : 1;
    }

    // --- ADD HL,HL: full 16-bit carry, S/Z/PV preserved --------------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().hl = 0x8000;
        cpu.state().regs().set_f(State::kFlagZero);  // Z preserved
        bus.memory[0x0000] = 0x29;  // ADD HL,HL
        const std::uint32_t t = cpu.step();
        // 0x10000 -> 0x0000, C set, no H. Z preserved. F = Z|C = 0x41.
        g_failures += expect_true(t == 11 && cpu.state().regs().hl == 0x0000 &&
                                      cpu.state().regs().f() == 0x41,
                                  "AddHlHl_CarryFromBit15_ZPreserved")
                          ? 0
                          : 1;
    }

    // --- ADC HL,DE: overflow + sign + half; N=0; X/Y from high byte --------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().hl = 0x7FFF;
        cpu.state().regs().de = 0x0001;
        cpu.state().regs().set_f(0x00);  // carry-in 0
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x5A;  // ADC HL,DE
        const std::uint32_t t = cpu.step();
        // 0x8000: S set, V set (pos+pos->neg), H set, C no, N=0, XY from 0x80 -> 0.
        // F = S|H|PV = 0x94.
        g_failures += expect_true(t == 15 && cpu.state().regs().hl == 0x8000 &&
                                      cpu.state().regs().f() == 0x94,
                                  "AdcHlDe_SignOverflowHalf_NClear")
                          ? 0
                          : 1;
    }

    // --- SBC HL,DE: N=1, borrow, X/Y from high byte ------------------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().hl = 0x0000;
        cpu.state().regs().de = 0x0001;
        cpu.state().regs().set_f(0x00);  // carry-in 0
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x52;  // SBC HL,DE
        const std::uint32_t t = cpu.step();
        // 0xFFFF: S set, H set, N set, C set, no PV, XY from 0xFF -> 0x28.
        // F = S|H|N|C|X|Y = 0x80|0x10|0x02|0x01|0x08|0x20 = 0xBB.
        g_failures += expect_true(t == 15 && cpu.state().regs().hl == 0xFFFF &&
                                      cpu.state().regs().f() == 0xBB,
                                  "SbcHlDe_SubtractFlagSet_XYFromHiByte")
                          ? 0
                          : 1;
    }

    // --- DAA after addition: low-nibble adjust adds 6, H set ---------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x0A);
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0x27;  // DAA
        cpu.step();
        g_failures += expect_true(cpu.state().regs().a() == 0x10 &&
                                      cpu.state().regs().f() == State::kFlagHalfCarry,
                                  "Daa_AfterAdd_LowNibbleAdjust")
                          ? 0
                          : 1;
    }

    // --- DAA after addition wrapping both nibbles: carry out ---------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x9A);
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0x27;  // DAA
        cpu.step();
        g_failures += expect_true(cpu.state().regs().a() == 0x00 && cpu.state().regs().f() == 0x55,
                                  "Daa_AfterAdd_WrapWithCarry")
                          ? 0
                          : 1;
    }

    // --- DAA after subtraction (N=1): low-nibble adjust subtracts 6 --------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x1B);
        cpu.state().regs().set_f(State::kFlagSubtract);  // N=1, H=0, C=0
        bus.memory[0x0000] = 0x27;  // DAA
        cpu.step();
        // 0x1B - 0x06 = 0x15; N kept, no H, no PV (parity 0x15 odd), no C. F = N.
        g_failures += expect_true(cpu.state().regs().a() == 0x15 &&
                                      cpu.state().regs().f() == State::kFlagSubtract,
                                  "Daa_AfterSub_LowNibbleAdjust")
                          ? 0
                          : 1;
    }

    // --- DAA after subtraction with borrow (N=1,C=1): subtracts 0x60 -------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x00);
        cpu.state().regs().set_f(static_cast<std::uint8_t>(State::kFlagSubtract | State::kFlagCarry));
        bus.memory[0x0000] = 0x27;  // DAA
        cpu.step();
        // 0x00 - 0x60 = 0xA0; N,S set, no H, PV (parity 0xA0 even), C set, Y from 0xA0.
        // F = N|S|PV|C|Y = 0x02|0x80|0x04|0x01|0x20 = 0xA7.
        g_failures += expect_true(cpu.state().regs().a() == 0xA0 && cpu.state().regs().f() == 0xA7,
                                  "Daa_AfterSub_WithBorrow")
                          ? 0
                          : 1;
    }

    return g_failures == 0 ? 0 : 1;
}
