#include <array>
#include <cstdint>
#include <functional>
#include <iostream>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80ACbPrefixed_Unit
//
// Deterministic vectors for the full CB-prefixed family (M9-S2):
// RLC/RRC/RL/RR/SLA/SRA/SLL/SRL rotate-shift, plus BIT/RES/SET across
// registers and (HL). Each case asserts result byte, full flag byte,
// memory effects, and T-state cycle counts (register 8, (HL) rot/res/set 15,
// BIT n,(HL) 12).

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
    // --- Rotate / shift on registers ---------------------------------------
    run("RlcB_HighBitWraps_CarrySetTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x80);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x00;  // RLC B
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().b() == 0x01 && c.state().regs().f() == State::kFlagCarry;
        });

    run("RrcC_LowBitWraps_SignAndCarrySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_c(0x01);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x09;  // RRC C
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().c() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagCarry);
        });

    run("RlD_ThroughCarry_ZeroResultParityCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_d(0x80);
            c.state().regs().set_f(0x00);  // carry-in 0
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x12;  // RL D
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().d() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagParityOverflow | State::kFlagCarry);
        });

    run("RrE_CarryInToBit7_SignAndCarrySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_e(0x01);
            c.state().regs().set_f(State::kFlagCarry);  // carry-in 1
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x1B;  // RR E
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().e() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagCarry);
        });

    run("SlaH_ShiftLeft_ZeroResultCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_h(0x80);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x24;  // SLA H
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().h() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagParityOverflow | State::kFlagCarry);
        });

    run("SraL_ArithmeticShift_SignPreservedParityCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_l(0x81);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x2D;  // SRA L
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().l() == 0xC0 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagParityOverflow | State::kFlagCarry);
        });

    run("SllA_UndocumentedShift_SetsBitZeroCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x80);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x37;  // SLL A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x01 && c.state().regs().f() == State::kFlagCarry;
        });

    run("SrlA_LogicalShift_ZeroResultCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x3F;  // SRL A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagParityOverflow | State::kFlagCarry);
        });

    run("RlcMemHl_MemoryOperand_ResultWrittenTiming15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x80;
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x06;  // RLC (HL)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 15 && b.memory[0x4000] == 0x01 && c.state().regs().f() == State::kFlagCarry;
        });

    // --- BIT test group -----------------------------------------------------
    run("Bit7A_BitSet_SignAndHalfSetZeroClear",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x80);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x7F;  // BIT 7,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagHalfCarry);
        });

    run("Bit0A_BitClear_ZeroHalfParitySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x47;  // BIT 0,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagHalfCarry | State::kFlagParityOverflow);
        });

    run("Bit1D_UndocumentedFlags_FromTestedValue",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_d(0x28);  // bit1 clear, X/Y bits set
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x4A;  // BIT 1,D
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().f() == 0x7C;
        });

    run("Bit0A_CarryPreserved_UnaffectedByBitTest",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(State::kFlagCarry);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x47;  // BIT 0,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().f() == (State::kFlagHalfCarry | State::kFlagCarry);
        });

    run("Bit3MemHl_MemoryOperand_HalfSetTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x08;  // bit3 set
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x5E;  // BIT 3,(HL)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().f() == (State::kFlagHalfCarry | State::kFlagX);
        });

    // --- RES / SET group ----------------------------------------------------
    run("Res7A_ClearBit_NoFlagChangeTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0xFF);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0xBF;  // RES 7,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x7F && c.state().regs().f() == 0x00;
        });

    run("Res0MemHl_ClearBitInMemory_Timing15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0xFF;
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0x86;  // RES 0,(HL)
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 15 && b.memory[0x4000] == 0xFE;
        });

    run("Set0B_SetBit_NoFlagChangeTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0xC0;  // SET 0,B
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().b() == 0x01 && c.state().regs().f() == 0x00;
        });

    run("Set7MemHl_SetBitInMemory_Timing15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x00;
            b.memory[0x0000] = 0xCB;
            b.memory[0x0001] = 0xFE;  // SET 7,(HL)
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 15 && b.memory[0x4000] == 0x80;
        });

    return g_failures == 0 ? 0 : 1;
}
