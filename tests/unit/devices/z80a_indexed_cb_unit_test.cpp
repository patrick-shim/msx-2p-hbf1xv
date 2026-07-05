#include <array>
#include <cstdint>
#include <functional>
#include <iostream>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AIndexedCb_Unit
//
// Deterministic vectors for the DDCB/FDCB indexed-bit family (M9-S4):
// rotate/shift (RLC/RRC/RL/RR/SLA/SRA/SLL/SRL) and RES/SET on (IX+d)/(IY+d)
// including the undocumented register-copy variants (z != 6), plus
// BIT n,(IX+d) whose undocumented X/Y flags derive from the high byte of the
// computed address. Byte order is DD/FD CB <displacement> <sub-opcode>. Each
// case asserts memory, the copied register, the full flag byte where defined,
// and exact T-states (rotate/shift/RES/SET (IX+d) = 23T, BIT n,(IX+d) = 20T).

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

using SetupFn = std::function<void(ArrayBus&, cpu::Z80aCpu&)>;
using CheckFn = std::function<bool(ArrayBus&, cpu::Z80aCpu&, std::uint32_t)>;

int g_failures = 0;

void run(const char* name, const SetupFn& setup, const CheckFn& check) {
    ArrayBus bus;
    cpu::CpuBusClient client(bus);
    cpu::Z80aCpu processor(client);
    processor.reset();
    setup(bus, processor);
    const std::uint32_t t = processor.step();
    if (!check(bus, processor, t)) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Rotate/shift on (IX+d) with undocumented register copy -------------
    run("RlcIxDispCopyB_MemoryAndRegister_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            b.memory[0x4000] = 0x80;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x00;  // displacement 0
            b.memory[0x0003] = 0x00;  // RLC (IX+0) -> copy to B (z=0)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4000] == 0x01 && c.state().regs().b() == 0x01 &&
                   c.state().regs().f() == State::kFlagCarry;
        });

    run("SlaIxDispCopyD_ShiftLeftCarry_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            b.memory[0x4001] = 0x81;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x01;  // displacement 1
            b.memory[0x0003] = 0x22;  // SLA (IX+1) -> copy to D (z=2)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4001] == 0x02 && c.state().regs().d() == 0x02 &&
                   c.state().regs().f() == State::kFlagCarry;
        });

    run("RlcIxDispNoCopy_ZEqualsSix_MemoryOnly_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            b.memory[0x4000] = 0x80;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x06;  // RLC (IX+0), z=6: no register copy
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4000] == 0x01 && c.state().regs().b() == 0x00 &&
                   c.state().regs().f() == State::kFlagCarry;
        });

    run("RrcIyDispNegativeCopyE_SignAndCarry_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().iy = 0x5002;
            b.memory[0x5001] = 0x01;  // (IY-1)
            b.memory[0x0000] = 0xFD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0xFF;  // displacement -1
            b.memory[0x0003] = 0x0B;  // RRC (IY-1) -> copy to E (z=3)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x5001] == 0x80 && c.state().regs().e() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagCarry);
        });

    // --- BIT n,(IX+d): X/Y from address high byte, no register copy ---------
    run("BitZeroIxDisp_XYFromAddressHighByte_Timing20",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x2800;  // high byte 0x28 -> bit3 and bit5 set
            c.state().regs().set_f(0x00);
            b.memory[0x2800] = 0x00;  // tested bit 0 clear
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x46;  // BIT 0,(IX+0)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 20 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagHalfCarry |
                                            State::kFlagParityOverflow | State::kFlagX | State::kFlagY);
        });

    // --- RES / SET on (IX+d) with register copy, flags unaffected -----------
    run("Res7IxDispCopyC_ClearsBit_FlagsUnaffectedTiming23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0xFF;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0xB9;  // RES 7,(IX+0) -> copy to C (z=1)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4000] == 0x7F && c.state().regs().c() == 0x7F &&
                   c.state().regs().f() == 0x00;
        });

    run("Set0IxDispCopyA_SetsBit_FlagsUnaffectedTiming23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x00;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0xC7;  // SET 0,(IX+0) -> copy to A (z=7)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4000] == 0x01 && c.state().regs().a() == 0x01 &&
                   c.state().regs().f() == 0x00;
        });

    return g_failures == 0 ? 0 : 1;
}
