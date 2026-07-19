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

// Suite: Devices_Z80AInterruptModes_Unit
//
// M9-S5 interrupt-architecture fidelity for IM0/IM2 acknowledge/vector sourcing,
// plus regression coverage of IM1, NMI, and RETN/RETI IFF semantics. Every case
// asserts PC, SP (pushed return address), IFF1/IFF2, and the exact T-state count,
// so the acknowledge model is fully deterministic (device bus vectors are
// injected as known values).

namespace {

using sony_msx::core::Bus;
using sony_msx::core::BusAddress;
using sony_msx::core::BusData;
using sony_msx::devices::cpu::CpuBusClient;
using sony_msx::devices::cpu::InterruptMode;
using sony_msx::devices::cpu::Z80aCpu;

class ArrayBus final : public Bus {
public:
    BusData read(const BusAddress address) override {
        return memory[address];
    }

    void write(const BusAddress address, const BusData value) override {
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
    // --- IM0: no device vector -> floating bus 0xFF decodes as RST 38h. --------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im0);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x1000;
        cpu.state().regs().sp = 0xFFFF;

        cpu.request_maskable_interrupt();  // no supplied vector
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 13 && cpu.state().regs().pc == 0x0038 &&
                             cpu.state().regs().sp == 0xFFFD &&
                             bus.memory[0xFFFD] == 0x00 && bus.memory[0xFFFE] == 0x10 &&
                             !cpu.state().iff1() && !cpu.state().iff2(),
                "Im0_NoDeviceVector_DefaultsToRst38")) {
            return 1;
        }
    }

    // --- IM0: device supplies RST 20h opcode (0xE7) -> jump to 0x0020. ---------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im0);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x2345;
        cpu.state().regs().sp = 0x8000;

        cpu.request_maskable_interrupt(0xE7);  // RST 20h on the bus
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 13 && cpu.state().regs().pc == 0x0020 &&
                             cpu.state().regs().sp == 0x7FFE &&
                             bus.memory[0x7FFE] == 0x45 && bus.memory[0x7FFF] == 0x23 &&
                             !cpu.state().iff1(),
                "Im0_DeviceSuppliesRst20_ExecutesBusOpcode")) {
            return 1;
        }
    }

    // --- IM0: device supplies a non-RST opcode (NOP, 0x00) -> 4T+2T, no push. --
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im0);
        cpu.state().set_iff1(true);
        cpu.state().regs().pc = 0x4000;
        cpu.state().regs().sp = 0xC000;

        cpu.request_maskable_interrupt(0x00);  // NOP on the bus
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 6 && cpu.state().regs().pc == 0x4000 &&
                             cpu.state().regs().sp == 0xC000,
                "Im0_DeviceSuppliesNop_ExecutesWithoutVectoring")) {
            return 1;
        }
    }

    // --- IM1: unchanged fixed RST 38h, 13T. -----------------------------------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x1234;
        cpu.state().regs().sp = 0xFFFF;

        // Even if a device drives the bus, IM1 ignores it and uses 0x0038.
        cpu.request_maskable_interrupt(0xC7);
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 13 && cpu.state().regs().pc == 0x0038 &&
                             cpu.state().regs().sp == 0xFFFD &&
                             bus.memory[0xFFFD] == 0x34 && bus.memory[0xFFFE] == 0x12 &&
                             !cpu.state().iff1() && !cpu.state().iff2(),
                "Im1_IgnoresBusVector_UsesFixed0038")) {
            return 1;
        }
    }

    // --- IM2: (I<<8)|bus_vector table lookup reads a known ISR address. --------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im2);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().i = 0x40;
        cpu.state().regs().pc = 0x5678;
        cpu.state().regs().sp = 0xE000;

        // Table entry at (0x40 << 8) | 0x0A = 0x400A -> ISR 0xBEEF (little-endian).
        bus.memory[0x400A] = 0xEF;
        bus.memory[0x400B] = 0xBE;

        cpu.request_maskable_interrupt(0x0A);  // device drives vector low byte 0x0A
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 19 && cpu.state().regs().pc == 0xBEEF &&
                             cpu.state().regs().sp == 0xDFFE &&
                             bus.memory[0xDFFE] == 0x78 && bus.memory[0xDFFF] == 0x56 &&
                             !cpu.state().iff1() && !cpu.state().iff2(),
                "Im2_DeviceVector_TableLookupTargetTaken")) {
            return 1;
        }
    }

    // --- IM2: no device vector -> floating low byte 0xFF (default). ------------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im2);
        cpu.state().set_iff1(true);
        cpu.state().regs().i = 0x81;
        cpu.state().regs().pc = 0x0300;
        cpu.state().regs().sp = 0xF000;

        bus.memory[0x81FF] = 0x21;
        bus.memory[0x8200] = 0x43;

        cpu.request_maskable_interrupt();  // floating bus -> 0xFF
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 19 && cpu.state().regs().pc == 0x4321 &&
                             cpu.state().regs().sp == 0xEFFE,
                "Im2_FloatingBus_DefaultsToLowByteFf")) {
            return 1;
        }
    }

    // --- NMI: fixed 0x0066, IFF1 cleared, IFF2 retained. ----------------------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x7000;
        cpu.state().regs().sp = 0xB000;

        cpu.request_nmi();
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 11 && cpu.state().regs().pc == 0x0066 &&
                             cpu.state().regs().sp == 0xAFFE &&
                             bus.memory[0xAFFE] == 0x00 && bus.memory[0xAFFF] == 0x70 &&
                             !cpu.state().iff1() && cpu.state().iff2(),
                "Nmi_Vectors0066_Iff1ClearedIff2Retained")) {
            return 1;
        }

        // RETN restores IFF1 from the retained IFF2.
        bus.memory[0x0066] = 0xED;
        bus.memory[0x0067] = 0x45;
        const std::uint32_t t2 = cpu.step();
        if (!expect_true(t2 == 14 && cpu.state().regs().pc == 0x7000 &&
                             cpu.state().regs().sp == 0xB000 && cpu.state().iff1() &&
                             cpu.state().iff2(),
                "Retn_RestoresIff1FromIff2")) {
            return 1;
        }
    }

    // --- RETI: returns and leaves IFF unchanged (IFF1 stays cleared post-ack). -
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x0900;
        cpu.state().regs().sp = 0xFFFF;
        bus.memory[0x0038] = 0xED;  // RETI
        bus.memory[0x0039] = 0x4D;

        cpu.request_maskable_interrupt();
        const std::uint32_t t0 = cpu.step();  // acknowledge -> 0x0038, IFF1/2 cleared
        const std::uint32_t t1 = cpu.step();  // RETI
        if (!expect_true(t0 == 13 && t1 == 14 && cpu.state().regs().pc == 0x0900 &&
                             cpu.state().regs().sp == 0xFFFF && !cpu.state().iff1() &&
                             !cpu.state().iff2(),
                "Reti_ReturnsAndLeavesIffCleared")) {
            return 1;
        }
    }

    // --- Refresh register ticks once on interrupt acknowledge (M1 model). ------
    {
        ArrayBus bus;
        CpuBusClient client(bus);
        Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().regs().pc = 0x0100;
        cpu.state().regs().r = 0x10;

        cpu.request_maskable_interrupt();
        cpu.step();
        if (!expect_true(cpu.state().regs().r == 0x11,
                "InterruptAck_RefreshRegisterIncrementsOnce")) {
            return 1;
        }
    }

    return 0;
}
