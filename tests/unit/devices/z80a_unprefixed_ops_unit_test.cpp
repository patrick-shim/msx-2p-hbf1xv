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
#include <functional>
#include <iostream>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AUnprefixedOps_Unit
//
// Deterministic per-instruction vectors for the completed unprefixed table
// (M9-S2). Each case asserts resulting register/flag byte (S,Z,Y,H,X,P/V,N,C),
// memory effects, PC/SP, and T-state cycle counts including taken vs not-taken
// conditional variants.

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
    // --- 8-bit ALU: ADD / ADC / SUB / SBC / AND / OR / XOR / CP -------------
    run("AddAReg_HalfCarryBoundary_HFlagSetTiming4",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x3A);
            c.state().regs().set_b(0x06);
            b.memory[0x0000] = 0x80;  // ADD A,B
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x40 && c.state().regs().f() == State::kFlagHalfCarry;
        });

    run("AddAReg_SignedOverflow_PvSignHalfSet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x7F);
            c.state().regs().set_b(0x01);
            b.memory[0x0000] = 0x80;  // ADD A,B
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagHalfCarry | State::kFlagParityOverflow);
        });

    run("AdcAImm_CarryInAcrossNibble_HalfCarrySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x0F);
            c.state().regs().set_f(State::kFlagCarry);
            b.memory[0x0000] = 0xCE;  // ADC A,n
            b.memory[0x0001] = 0x00;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0x10 && c.state().regs().f() == State::kFlagHalfCarry;
        });

    run("SubImm_Borrow_SignHalfCarrySubtractYSet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x10);
            b.memory[0x0000] = 0xD6;  // SUB n
            b.memory[0x0001] = 0x20;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0xF0 && c.state().regs().f() == 0xA3;
        });

    run("CpImm_Equal_ZeroSubtractSetAccumulatorUnchanged",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x10);
            b.memory[0x0000] = 0xFE;  // CP n
            b.memory[0x0001] = 0x10;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0x10 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagSubtract);
        });

    run("CpImm_UndocumentedFlags_FromOperandNotResult",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            b.memory[0x0000] = 0xFE;  // CP n
            b.memory[0x0001] = 0x28;  // X and Y set in operand
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            // F = S|Y|H|X|N|C = 0xBB; X/Y (0x28) sourced from the operand, not the result.
            return t == 7 && c.state().regs().a() == 0x00 && c.state().regs().f() == 0xBB;
        });

    run("AndImm_MaskLowNibble_HalfAndParitySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0xFF);
            b.memory[0x0000] = 0xE6;  // AND n
            b.memory[0x0001] = 0x0F;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0x0F &&
                   c.state().regs().f() == (State::kFlagHalfCarry | State::kFlagParityOverflow | State::kFlagX);
        });

    run("OrImm_ZeroResult_ZeroAndParitySet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            b.memory[0x0000] = 0xF6;  // OR n
            b.memory[0x0001] = 0x00;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagParityOverflow);
        });

    // --- INC / DEC on (HL) with memory + timing ----------------------------
    run("IncMemHl_SevenFToEighty_SignHalfOverflowTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x7F;
            b.memory[0x0000] = 0x34;  // INC (HL)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && b.memory[0x4000] == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagHalfCarry | State::kFlagParityOverflow);
        });

    run("DecMemHl_ZeroToFF_SignHalfSubtractXYTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x00;
            b.memory[0x0000] = 0x35;  // DEC (HL)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && b.memory[0x4000] == 0xFF && c.state().regs().f() == 0xBA;
        });

    // --- 16-bit ADD HL,rp / INC / DEC --------------------------------------
    run("AddHlDe_HalfCarryFromBit11_HFlagTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x0FFF;
            c.state().regs().de = 0x0001;
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x19;  // ADD HL,DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().hl == 0x1000 && c.state().regs().f() == State::kFlagHalfCarry;
        });

    run("AddHlHl_Overflow_CarryHalfXYSet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0xFFFF;
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x29;  // ADD HL,HL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().hl == 0xFFFE && c.state().regs().f() == 0x39;
        });

    run("IncBc_WrapAround_NoFlagChangeTiming6",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0xFFFF;
            c.state().regs().set_f(0xFF);
            b.memory[0x0000] = 0x03;  // INC BC
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 6 && c.state().regs().bc == 0x0000 && c.state().regs().f() == 0xFF;
        });

    // --- DAA / CPL / SCF / CCF ---------------------------------------------
    run("Daa_LowNibbleAdjust_AddsSixHalfSet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x0A);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x27;  // DAA
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x10 && c.state().regs().f() == State::kFlagHalfCarry;
        });

    run("Daa_InvalidHighAndLow_WrapWithCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x9A);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x27;  // DAA
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x00 && c.state().regs().f() == 0x55;
        });

    run("Cpl_Complement_HalfSubtractSet",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x3C);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x2F;  // CPL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0xC3 &&
                   c.state().regs().f() == (State::kFlagHalfCarry | State::kFlagSubtract);
        });

    run("Scf_SetCarry_ClearsHalfSubtract",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x37;  // SCF
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().f() == State::kFlagCarry;
        });

    run("Ccf_ComplementCarry_HalfBecomesOldCarry",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(State::kFlagCarry);
            b.memory[0x0000] = 0x3F;  // CCF
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().f() == State::kFlagHalfCarry;
        });

    // --- Exchange group -----------------------------------------------------
    run("ExDeHl_Swap_RegistersExchanged",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().de = 0x1234;
            c.state().regs().hl = 0x5678;
            b.memory[0x0000] = 0xEB;  // EX DE,HL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().de == 0x5678 && c.state().regs().hl == 0x1234;
        });

    run("ExAfAf_Swap_ShadowExchanged",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().af = 0x1122;
            c.state().regs().af_shadow = 0x3344;
            b.memory[0x0000] = 0x08;  // EX AF,AF'
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().af == 0x3344 && c.state().regs().af_shadow == 0x1122;
        });

    run("Exx_Swap_AllBcDeHlExchanged",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x1111;
            c.state().regs().de = 0x2222;
            c.state().regs().hl = 0x3333;
            c.state().regs().bc_shadow = 0xAAAA;
            c.state().regs().de_shadow = 0xBBBB;
            c.state().regs().hl_shadow = 0xCCCC;
            b.memory[0x0000] = 0xD9;  // EXX
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().bc == 0xAAAA && c.state().regs().de == 0xBBBB &&
                   c.state().regs().hl == 0xCCCC && c.state().regs().bc_shadow == 0x1111;
        });

    run("ExSpHl_Swap_StackAndHlExchangedTiming19",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFE;
            c.state().regs().hl = 0x1234;
            b.memory[0xFFFE] = 0x78;
            b.memory[0xFFFF] = 0x56;
            b.memory[0x0000] = 0xE3;  // EX (SP),HL
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 19 && c.state().regs().hl == 0x5678 && b.memory[0xFFFE] == 0x34 &&
                   b.memory[0xFFFF] == 0x12;
        });

    // --- Relative jumps / DJNZ (taken vs not-taken timing) ------------------
    run("JrNz_ConditionFalse_NotTakenTiming7",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(State::kFlagZero);
            b.memory[0x0000] = 0x20;  // JR NZ,d
            b.memory[0x0001] = 0x05;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().pc == 0x0002;
        });

    run("JrNz_ConditionTrue_TakenTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x20;  // JR NZ,d
            b.memory[0x0001] = 0x05;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().pc == 0x0007;
        });

    run("Jr_Backward_NegativeDisplacementTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            b.memory[0x0000] = 0x18;  // JR d
            b.memory[0x0001] = 0xFE;  // -2
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().pc == 0x0000;
        });

    run("Djnz_CounterNonZero_TakenTiming13",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x02);
            b.memory[0x0000] = 0x10;  // DJNZ d
            b.memory[0x0001] = 0x05;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 13 && c.state().regs().b() == 0x01 && c.state().regs().pc == 0x0007;
        });

    run("Djnz_CounterReachesZero_NotTakenTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x01);
            b.memory[0x0000] = 0x10;  // DJNZ d
            b.memory[0x0001] = 0x05;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().b() == 0x00 && c.state().regs().pc == 0x0002;
        });

    // --- CALL / RET (taken vs not-taken) and stack --------------------------
    run("CallImm_Unconditional_PushesReturnAndJumps",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFF;
            b.memory[0x0000] = 0xCD;  // CALL nn
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x80;
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 17 && c.state().regs().pc == 0x8000 && c.state().regs().sp == 0xFFFD &&
                   b.memory[0xFFFD] == 0x03 && b.memory[0xFFFE] == 0x00;
        });

    run("CallNz_ConditionFalse_NotTakenTiming10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFF;
            c.state().regs().set_f(State::kFlagZero);
            b.memory[0x0000] = 0xC4;  // CALL NZ,nn
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x80;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().pc == 0x0003 && c.state().regs().sp == 0xFFFF;
        });

    run("Ret_Unconditional_PopsReturnTiming10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            b.memory[0xFFFD] = 0x34;
            b.memory[0xFFFE] = 0x12;
            b.memory[0x0000] = 0xC9;  // RET
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().pc == 0x1234 && c.state().regs().sp == 0xFFFF;
        });

    run("RetNz_ConditionTrue_TakenTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            c.state().regs().set_f(0x00);
            b.memory[0xFFFD] = 0x34;
            b.memory[0xFFFE] = 0x12;
            b.memory[0x0000] = 0xC0;  // RET NZ
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().pc == 0x1234 && c.state().regs().sp == 0xFFFF;
        });

    run("RetNz_ConditionFalse_NotTakenTiming5",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            c.state().regs().set_f(State::kFlagZero);
            b.memory[0x0000] = 0xC0;  // RET NZ
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 5 && c.state().regs().pc == 0x0001 && c.state().regs().sp == 0xFFFD;
        });

    run("PushBc_StackWriteOrder_HighThenLow",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFF;
            c.state().regs().bc = 0x1234;
            b.memory[0x0000] = 0xC5;  // PUSH BC
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().sp == 0xFFFD && b.memory[0xFFFD] == 0x34 &&
                   b.memory[0xFFFE] == 0x12;
        });

    run("PopDe_StackRead_ValueRestoredTiming10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            b.memory[0xFFFD] = 0x34;
            b.memory[0xFFFE] = 0x12;
            b.memory[0x0000] = 0xD1;  // POP DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().de == 0x1234 && c.state().regs().sp == 0xFFFF;
        });

    run("Rst18_Restart_PushesReturnAndVectorTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFF;
            b.memory[0x0000] = 0xDF;  // RST 18h
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().pc == 0x0018 && c.state().regs().sp == 0xFFFD &&
                   b.memory[0xFFFD] == 0x01;
        });

    // --- 16-bit loads / indirect loads / SP loads --------------------------
    run("LdMemNnHl_StoreWord_LittleEndianTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0xABCD;
            b.memory[0x0000] = 0x22;  // LD (nn),HL
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x40;
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && b.memory[0x4000] == 0xCD && b.memory[0x4001] == 0xAB;
        });

    run("LdHlMemNn_LoadWord_LittleEndianTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            b.memory[0x4000] = 0xCD;
            b.memory[0x4001] = 0xAB;
            b.memory[0x0000] = 0x2A;  // LD HL,(nn)
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x40;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && c.state().regs().hl == 0xABCD;
        });

    run("LdMemDeA_StoreAccumulator_Timing7",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x5A);
            c.state().regs().de = 0x4321;
            b.memory[0x0000] = 0x12;  // LD (DE),A
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 7 && b.memory[0x4321] == 0x5A;
        });

    run("LdSpHl_TransferPointer_Timing6",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x8000;
            b.memory[0x0000] = 0xF9;  // LD SP,HL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 6 && c.state().regs().sp == 0x8000;
        });

    run("JpHl_IndirectBranch_Timing4",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x1234;
            b.memory[0x0000] = 0xE9;  // JP (HL)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().pc == 0x1234;
        });

    run("JpZ_ConditionFalse_FallThroughTiming10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(0x00);  // Z clear
            b.memory[0x0000] = 0xCA;  // JP Z,nn
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x80;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().pc == 0x0003;
        });

    run("JpZ_ConditionTrue_BranchTakenTiming10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(State::kFlagZero);
            b.memory[0x0000] = 0xCA;  // JP Z,nn
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x80;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 10 && c.state().regs().pc == 0x8000;
        });

    // --- LD r,r' register-transfer matrix with (HL) timing -----------------
    run("LdRegFromReg_BFromC_Timing4",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_c(0x7E);
            b.memory[0x0000] = 0x41;  // LD B,C
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().b() == 0x7E;
        });

    run("LdRegFromMemHl_AFromHl_Timing7",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x99;
            b.memory[0x0000] = 0x7E;  // LD A,(HL)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 7 && c.state().regs().a() == 0x99;
        });

    run("LdMemHlFromReg_HlFromB_Timing7",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().set_b(0x42);
            b.memory[0x0000] = 0x70;  // LD (HL),B
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 7 && b.memory[0x4000] == 0x42;
        });

    run("LdMemHlImm_StoreImmediate_Timing10",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x0000] = 0x36;  // LD (HL),n
            b.memory[0x0001] = 0x7B;
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 10 && b.memory[0x4000] == 0x7B;
        });

    // --- Accumulator rotates (RLCA/RRCA/RLA/RRA) ---------------------------
    run("Rlca_HighBitToCarry_ResultRotatedLeft",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x80);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0x07;  // RLCA
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x01 && c.state().regs().f() == State::kFlagCarry;
        });

    run("Rra_CarryInToBit7_ResultRotatedRight",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(State::kFlagCarry);
            b.memory[0x0000] = 0x1F;  // RRA
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 4 && c.state().regs().a() == 0x80 && c.state().regs().f() == State::kFlagCarry;
        });

    // --- Port I/O deterministic stubs (backing not modeled this slice) -----
    run("OutImmA_PortWriteStub_AccumulatorUnchangedTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x5A);
            b.memory[0x0000] = 0xD3;  // OUT (n),A
            b.memory[0x0001] = 0x10;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().a() == 0x5A && c.state().regs().pc == 0x0002;
        });

    run("InAImm_OpenBusStub_ReadsFFTiming11",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            b.memory[0x0000] = 0xDB;  // IN A,(n)
            b.memory[0x0001] = 0x10;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 11 && c.state().regs().a() == 0xFF && c.state().regs().pc == 0x0002;
        });

    return g_failures == 0 ? 0 : 1;
}
