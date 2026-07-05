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
    // Suite: Devices_Z80AInterrupts_Unit

    {
        ArrayBus bus;
        sony_msx::devices::cpu::CpuBusClient bus_client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(bus_client);
        cpu.reset();

        bus.memory[0x0000] = 0xFB;      // EI
        bus.memory[0x0001] = 0x00;      // NOP
        bus.memory[0x0038] = 0xED;      // RETI
        bus.memory[0x0039] = 0x4D;

        const std::uint32_t t0 = cpu.step();
        if (!expect_true(t0 == 4 && cpu.state().iff1() && cpu.state().ei_delay_active(),
                "Ei_Executed_InterruptEnabledWithDelay")) {
            return 1;
        }

        cpu.request_maskable_interrupt();
        const std::uint32_t t1 = cpu.step();
        if (!expect_true(t1 == 4 && cpu.state().regs().pc == 0x0002,
                "MaskableInterrupt_AfterEiDelayedByOneInstruction_NotTakenImmediately")) {
            return 1;
        }

        const std::uint32_t t2 = cpu.step();
        if (!expect_true(t2 == 13 && cpu.state().regs().pc == 0x0038 && cpu.state().regs().sp == 0xFFFD,
                "Im1Interrupt_PendingAndEnabled_VectorTakenAndStackUpdated")) {
            return 1;
        }

        const std::uint32_t t3 = cpu.step();
        if (!expect_true(t3 == 14 && cpu.state().regs().pc == 0x0002 && cpu.state().regs().sp == 0xFFFF,
                "Reti_FromIm1Handler_ReturnsToInterruptedProgram")) {
            return 1;
        }
    }

    {
        ArrayBus bus;
        sony_msx::devices::cpu::CpuBusClient bus_client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(bus_client);
        cpu.reset();

        cpu.state().regs().pc = 0x0100;
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        bus.memory[0x0066] = 0xED;      // RETN
        bus.memory[0x0067] = 0x45;

        cpu.request_nmi();
        const std::uint32_t t0 = cpu.step();
        if (!expect_true(t0 == 11 && cpu.state().regs().pc == 0x0066 && !cpu.state().iff1(),
                "Nmi_Pending_ForcedVectorAndIff1Cleared")) {
            return 1;
        }

        const std::uint32_t t1 = cpu.step();
        if (!expect_true(t1 == 14 && cpu.state().regs().pc == 0x0100 && cpu.state().iff1(),
                "Retn_AfterNmi_ReturnsAndRestoresIff1FromIff2")) {
            return 1;
        }
    }

    {
        ArrayBus bus;
        sony_msx::devices::cpu::CpuBusClient bus_client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(bus_client);
        cpu.reset();

        cpu.state().set_interrupt_mode(sony_msx::devices::cpu::InterruptMode::Im2);
        cpu.state().set_iff1(true);
        cpu.state().regs().i = 0x80;
        cpu.state().regs().pc = 0x2222;

        bus.memory[0x80FF] = 0x34;
        bus.memory[0x8100] = 0x12;

        cpu.request_maskable_interrupt();
        const std::uint32_t t0 = cpu.step();
        if (!expect_true(t0 == 19 && cpu.state().regs().pc == 0x1234 && cpu.state().regs().sp == 0xFFFD,
                "Im2Interrupt_ValidVectorTable_ComputedTargetTaken")) {
            return 1;
        }
    }

    return 0;
}