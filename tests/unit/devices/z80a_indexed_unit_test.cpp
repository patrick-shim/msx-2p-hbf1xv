#include <array>
#include <cstdint>
#include <functional>
#include <iostream>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AIndexed_Unit
//
// Deterministic vectors for the DD/FD (IX/IY) indexed family (M9-S4):
// half-register operands (IXH/IXL/IYH/IYL), (IX+d)/(IY+d) displacement
// addressing with signed (incl. negative) displacements, 16-bit index loads,
// INC/DEC, ADD IX,rp, stack ops, EX (SP),IX, JP (IX), and prefix
// chaining/fallthrough (DD FD, DD DD, DD ED). Each case asserts the affected
// registers/memory, the full flag byte where flags are defined, PC/SP, and the
// exact T-state count (which includes the 4T prefix M1).

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
    // --- 16-bit index loads -------------------------------------------------
    run("LdIxNn_ImmediateLoad_Timing14",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x21;
            b.memory[0x0002] = 0x34;
            b.memory[0x0003] = 0x12;  // LD IX,0x1234
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 14 && c.state().regs().ix == 0x1234 && c.state().regs().pc == 0x0004;
        });

    run("LdIyNnDirect_Timing20_LoadsFromMemory",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x5000] = 0x34;
            b.memory[0x5001] = 0x12;
            b.memory[0x0000] = 0xFD;
            b.memory[0x0001] = 0x2A;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x50;  // LD IY,(0x5000)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 20 && c.state().regs().iy == 0x1234;
        });

    run("LdNnIx_StoresLittleEndian_Timing20",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0xBEEF;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x22;
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x50;  // LD (0x5000),IX
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 20 && b.memory[0x5000] == 0xEF && b.memory[0x5001] == 0xBE;
        });

    // --- Half-register ALU / INC / DEC --------------------------------------
    run("AddAIxl_CarryHalfZero_FullFlagsTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x00FF;  // IXL = 0xFF
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x85;  // ADD A,IXL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagHalfCarry | State::kFlagCarry);
        });

    run("IncIxh_NoCarryChange_Timing8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x12FF;  // IXH = 0x12
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x24;  // INC IXH
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().ix == 0x13FF && c.state().regs().f() == 0x00;
        });

    run("LdIxhIxl_RegisterTranslation_Timing8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x12FF;  // IXH=0x12, IXL=0xFF
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x65;  // LD H,L -> LD IXH,IXL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().ix == 0xFFFF;
        });

    // --- (IX+d) displacement addressing (incl. negative d) ------------------
    run("LdBFromIxNegativeDisp_Timing19",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4005;
            b.memory[0x4000] = 0x99;  // (IX-5)
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x46;
            b.memory[0x0002] = 0xFB;  // LD B,(IX-5)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 19 && c.state().regs().b() == 0x99 && c.state().regs().pc == 0x0003;
        });

    run("LdIxDispImmediate_Timing19",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x36;
            b.memory[0x0002] = 0x02;
            b.memory[0x0003] = 0x77;  // LD (IX+2),0x77
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 19 && b.memory[0x4002] == 0x77;
        });

    run("AddAFromIxDisp_YFlagFromResult_Timing19",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            c.state().regs().set_a(0x10);
            c.state().regs().set_f(0x00);
            b.memory[0x4001] = 0x22;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x86;
            b.memory[0x0002] = 0x01;  // ADD A,(IX+1)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 19 && c.state().regs().a() == 0x32 && c.state().regs().f() == State::kFlagY;
        });

    run("DecIxDispNegative_ZeroSubtract_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4010;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x01;  // (IX-16)
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x35;
            b.memory[0x0002] = 0xF0;  // DEC (IX-16)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && b.memory[0x4000] == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagSubtract);
        });

    run("LdIxDispFromRegister_RealHNotIxh_Timing19",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            c.state().regs().set_h(0x5A);  // real H is stored, IXH must be ignored
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x74;
            b.memory[0x0002] = 0x03;  // LD (IX+3),H
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 19 && b.memory[0x4003] == 0x5A;
        });

    // --- 16-bit index arithmetic / INC / DEC / stack ------------------------
    run("AddIxDe_HalfAndCarryClear_Timing15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x1000;
            c.state().regs().de = 0x0234;
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x19;  // ADD IX,DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 15 && c.state().regs().ix == 0x1234 && c.state().regs().f() == 0x00;
        });

    run("IncIx_SixteenBit_Timing10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x12FF;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x23;  // INC IX
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().ix == 0x1300;
        });

    run("PushIx_StackOrderTiming15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0xABCD;
            c.state().regs().sp = 0xFFFF;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xE5;  // PUSH IX
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 15 && c.state().regs().sp == 0xFFFD &&
                   b.memory[0xFFFE] == 0xAB && b.memory[0xFFFD] == 0xCD;
        });

    run("PopIx_StackOrderTiming14",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            b.memory[0xFFFD] = 0xCD;
            b.memory[0xFFFE] = 0xAB;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xE1;  // POP IX
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 14 && c.state().regs().ix == 0xABCD && c.state().regs().sp == 0xFFFF;
        });

    run("ExSpIx_SwapsStackAndIndex_Timing23",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x1234;
            c.state().regs().sp = 0x4000;
            b.memory[0x4000] = 0x78;
            b.memory[0x4001] = 0x56;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xE3;  // EX (SP),IX
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 23 && c.state().regs().ix == 0x5678 &&
                   b.memory[0x4000] == 0x34 && b.memory[0x4001] == 0x12;
        });

    run("JpIx_LoadsPcFromIndex_Timing8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x1234;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xE9;  // JP (IX)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().pc == 0x1234;
        });

    run("LdSpIx_Timing10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x8000;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xF9;  // LD SP,IX
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().sp == 0x8000;
        });

    // --- Prefix chaining / fallthrough --------------------------------------
    run("DdFdRelatch_LastPrefixWins_LoadsIy",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xFD;
            b.memory[0x0002] = 0x21;
            b.memory[0x0003] = 0x34;
            b.memory[0x0004] = 0x12;  // DD FD 21 nn -> LD IY,0x1234
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 18 && c.state().regs().iy == 0x1234 && c.state().regs().ix == 0x0000 &&
                   c.state().regs().pc == 0x0005;
        });

    run("DdDdFallthrough_IgnoredPrefixesThenIncA_Timing12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x41);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xDD;
            b.memory[0x0002] = 0x3C;  // DD DD INC A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().a() == 0x42 && c.state().regs().ix == 0x0000 &&
                   c.state().regs().pc == 0x0003;
        });

    run("DdEdFallthrough_EdExecutesUnmodified_NegTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xED;
            b.memory[0x0002] = 0x44;  // DD ED NEG
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().a() == 0xFF &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagHalfCarry | State::kFlagX |
                                            State::kFlagY | State::kFlagSubtract | State::kFlagCarry);
        });

    return g_failures == 0 ? 0 : 1;
}
