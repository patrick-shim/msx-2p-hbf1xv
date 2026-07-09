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

namespace {

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

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Devices_Z80AUnprefixed_Unit
    ArrayBus bus;
    sony_msx::devices::cpu::CpuBusClient bus_client(bus);
    sony_msx::devices::cpu::Z80aCpu cpu(bus_client);
    cpu.reset();

    bus.memory[0x0000] = 0x3E;  // LD A,0x2A
    bus.memory[0x0001] = 0x2A;
    bus.memory[0x0002] = 0x3C;  // INC A
    bus.memory[0x0003] = 0xAF;  // XOR A
    bus.memory[0x0004] = 0x76;  // HALT

    const std::uint32_t t0 = cpu.step();
    if (!expect_true(t0 == 7 && cpu.state().regs().a() == 0x2A && cpu.state().regs().pc == 0x0002,
            "LdAImmediate_ValidProgram_ExpectedRegisterAndTiming")) {
        return 1;
    }

    const std::uint32_t t1 = cpu.step();
    if (!expect_true(t1 == 4 && cpu.state().regs().a() == 0x2B,
            "IncA_AfterLoad_IncrementedWithExpectedTiming")) {
        return 1;
    }

    const std::uint32_t t2 = cpu.step();
    if (!expect_true(t2 == 4 && cpu.state().regs().a() == 0x00 &&
            (cpu.state().regs().f() & sony_msx::devices::cpu::Z80aState::kFlagZero) != 0,
            "XorA_SameOperand_ZeroFlagSet")) {
        return 1;
    }

    const std::uint32_t t3 = cpu.step();
    if (!expect_true(t3 == 4 && cpu.state().halted() && cpu.state().regs().pc == 0x0005,
            "Halt_OpcodeExecuted_HaltedStateEntered")) {
        return 1;
    }

    const std::uint16_t pc_before_halt_idle = cpu.state().regs().pc;
    const std::uint32_t t4 = cpu.step();
    if (!expect_true(t4 == 4 && cpu.state().regs().pc == pc_before_halt_idle,
            "Halt_IdleStepWithoutInterrupt_ConsumesDeterministicTstates")) {
        return 1;
    }

    if (!expect_true(cpu.state().total_tstates() == 23,
            "TotalTStates_ExecutedSequence_ExpectedAccumulatedTiming")) {
        return 1;
    }

    return 0;
}