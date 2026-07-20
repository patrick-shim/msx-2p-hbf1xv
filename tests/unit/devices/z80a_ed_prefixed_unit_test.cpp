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

// Suite: Devices_Z80AEdPrefixed_Unit
//
// Deterministic vectors for the full ED-prefixed family (M9-S3):
// block transfer/search (LDI/LDD/LDIR/LDDR, CPI/CPD/CPIR/CPDR) with
// repeat-vs-terminate T-state timing, 16-bit ADC/SBC HL,rp, NEG (and a
// redundant alias), IN r,(C) / OUT (C),r, LD A,I / LD A,R / LD I,A / LD R,A,
// RRD/RLD, LD (nn),rp / LD rp,(nn), and preserved RETN/RETI/IM behavior.
// Each case asserts result bytes, full flag byte (S,Z,Y,H,X,P/V,N,C) where
// architecturally defined, memory effects, PC/SP, and exact T-state counts.

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
    // --- Block transfer: LDI / LDD / LDIR / LDDR ---------------------------
    run("Ldi_TransferByte_PreservesSZCUndocFromSumTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().de = 0x5000;
            c.state().regs().bc = 0x0002;
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0xFF);  // S,Z,C must survive; H,N cleared.
            b.memory[0x4000] = 0x88;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA0;  // LDI
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            // n = 0x88 + A(0x00) = 0x88 -> X(bit3)=1, Y(bit1)=0. BC!=0 -> PV.
            return t == 16 && b.memory[0x5000] == 0x88 && c.state().regs().hl == 0x4001 &&
                   c.state().regs().de == 0x5001 && c.state().regs().bc == 0x0001 &&
                   c.state().regs().f() == 0xCD;  // S|Z|C|PV|X
        });

    run("Ldd_TransferByte_DecrementsPointersTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4001;
            c.state().regs().de = 0x5001;
            c.state().regs().bc = 0x0002;
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x4001] = 0x88;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA8;  // LDD
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && b.memory[0x5001] == 0x88 && c.state().regs().hl == 0x4000 &&
                   c.state().regs().de == 0x5000 && c.state().regs().bc == 0x0001 &&
                   c.state().regs().f() == (State::kFlagParityOverflow | State::kFlagX);  // 0x0C
        });

    run("Ldir_MidLoop_RepeatsWith21TAndRewindsPc",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().de = 0x5000;
            c.state().regs().bc = 0x0003;
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x10;  // n=0x10 -> X=0, Y=0
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xB0;  // LDIR
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 21 && b.memory[0x5000] == 0x10 && c.state().regs().bc == 0x0002 &&
                   c.state().regs().pc == 0x0000 &&  // rewound to re-execute
                   c.state().regs().f() == State::kFlagParityOverflow;
        });

    run("Ldir_FinalIteration_Terminates16TAndAdvancesPc",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().de = 0x5000;
            c.state().regs().bc = 0x0001;
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x02;  // n=0x02 -> Y(bit1)=1
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xB0;  // LDIR
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && b.memory[0x5000] == 0x02 && c.state().regs().bc == 0x0000 &&
                   c.state().regs().pc == 0x0002 &&  // advanced past instruction
                   c.state().regs().f() == State::kFlagY;
        });

    // --- Block search: CPI / CPD / CPIR / CPDR -----------------------------
    run("Cpi_Match_ZeroSetHalfClearTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x42);
            c.state().regs().hl = 0x4000;
            c.state().regs().bc = 0x0002;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x42;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA1;  // CPI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && c.state().regs().a() == 0x42 && c.state().regs().hl == 0x4001 &&
                   c.state().regs().bc == 0x0001 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagSubtract |
                                            State::kFlagParityOverflow);  // 0x46
        });

    run("Cpd_NoMatch_DecrementsHlTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x42);
            c.state().regs().hl = 0x4005;
            c.state().regs().bc = 0x0002;
            c.state().regs().set_f(0x00);
            b.memory[0x4005] = 0x40;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA9;  // CPD
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            // result=0x02 -> Y(bit1)=1; PV(BC!=0); N.
            return t == 16 && c.state().regs().hl == 0x4004 && c.state().regs().bc == 0x0001 &&
                   c.state().regs().f() == (State::kFlagParityOverflow | State::kFlagSubtract |
                                            State::kFlagY);  // 0x26
        });

    run("Cpir_MatchEarly_Terminates16TEvenWithBcRemaining",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x42);
            c.state().regs().hl = 0x4000;
            c.state().regs().bc = 0x0005;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x42;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xB1;  // CPIR
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && c.state().regs().pc == 0x0002 && c.state().regs().bc == 0x0004 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagSubtract |
                                            State::kFlagParityOverflow);  // 0x46
        });

    run("Cpir_NoMatch_Repeats21TAndRewindsPc",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x42);
            c.state().regs().hl = 0x4000;
            c.state().regs().bc = 0x0003;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x99;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xB1;  // CPIR
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            // 0x42-0x99=0xA9 -> S; half-borrow -> H; n=0xA9-1=0xA8 -> X; PV(BC!=0); N.
            return t == 21 && c.state().regs().pc == 0x0000 && c.state().regs().bc == 0x0002 &&
                   c.state().regs().f() == 0x9E;  // S|H|X|PV|N
        });

    // --- 16-bit arithmetic: ADC HL,rp / SBC HL,rp --------------------------
    run("AdcHlHl_OverflowToZero_CarryPvZeroSetTiming15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x8000;
            c.state().regs().set_f(0x00);  // carry clear
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x6A;  // ADC HL,HL
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 15 && c.state().regs().hl == 0x0000 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagParityOverflow |
                                            State::kFlagCarry);  // 0x45
        });

    run("AdcHlDe_WithCarryIn_NoFlagsTiming15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x1000;
            c.state().regs().de = 0x0001;
            c.state().regs().set_f(State::kFlagCarry);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x5A;  // ADC HL,DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 15 && c.state().regs().hl == 0x1002 && c.state().regs().f() == 0x00;
        });

    run("SbcHlDe_Borrow_SignHalfCarryNegSetTiming15",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x0000;
            c.state().regs().de = 0x0001;
            c.state().regs().set_f(0x00);  // carry clear
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x52;  // SBC HL,DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            // result=0xFFFF -> S; half-borrow -> H; borrow -> C; N; XY from 0xFF -> Y,X.
            return t == 15 && c.state().regs().hl == 0xFFFF && c.state().regs().f() == 0xBB;
        });

    // --- NEG (documented + redundant alias) --------------------------------
    run("Neg_One_FullBorrowFlagsTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x44;  // NEG
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0xFF && c.state().regs().f() == 0xBB;
        });

    run("Neg_Zero_ZeroSubtractNoCarryTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x00);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x44;  // NEG
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x00 &&
                   c.state().regs().f() == (State::kFlagZero | State::kFlagSubtract);  // 0x42
        });

    run("Neg_Eighty_OverflowPreservedTiming8",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x80);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x44;  // NEG
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagParityOverflow |
                                            State::kFlagSubtract | State::kFlagCarry);  // 0x87
        });

    run("NegRedundantAlias_ED4C_BehavesAsNeg",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x01);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x4C;  // redundant NEG alias
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0xFF && c.state().regs().f() == 0xBB;
        });

    // --- I/O: IN r,(C) / IN (C) / OUT (C),r --------------------------------
    run("InBFromC_OpenBusFF_FlagsFromDataStoresBTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x00);
            c.state().regs().set_c(0x10);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x40;  // IN B,(C)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            // data=0xFF -> S; parity(0xFF)=even -> PV; XY from 0xFF -> Y,X. C preserved.
            return t == 12 && c.state().regs().b() == 0xFF && c.state().regs().pc == 0x0002 &&
                   c.state().regs().f() == 0xAC;  // S|Y|X|PV
        });

    run("InFlagOnly_ED70_NoRegisterWriteTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x11;  // must remain untouched (no (HL) write)
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x70;  // IN (C) / IN F,(C)
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && b.memory[0x4000] == 0x11 && c.state().regs().f() == 0xAC;
        });

    run("OutCFromB_DiscardedNoFlagChangeTiming12",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x5A);
            c.state().regs().set_c(0x10);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x41;  // OUT (C),B
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 12 && c.state().regs().b() == 0x5A && c.state().regs().pc == 0x0002 &&
                   c.state().regs().f() == 0x00;
        });

    // --- Interrupt/refresh registers ---------------------------------------
    run("LdIA_TransferAccumulatorNoFlagsTiming9",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x3C);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x47;  // LD I,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 9 && c.state().regs().i == 0x3C && c.state().regs().f() == 0x00;
        });

    run("LdRA_TransferAccumulatorOverwritesRefreshTiming9",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x7E);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x4F;  // LD R,A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 9 && c.state().regs().r == 0x7E;
        });

    run("LdAI_PvReflectsIff2SignSetTiming9",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().i = 0x80;
            c.state().set_iff2(true);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x57;  // LD A,I
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 9 && c.state().regs().a() == 0x80 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagParityOverflow);  // 0x84
        });

    run("LdAI_Iff2Clear_PvClear",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().i = 0x01;
            c.state().set_iff2(false);
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x57;  // LD A,I
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 9 && c.state().regs().a() == 0x01 && c.state().regs().f() == 0x00;
        });

    // --- RRD / RLD ---------------------------------------------------------
    run("Rrd_NibbleRotate_UpdatesAAndMemoryTiming18",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x84);
            c.state().regs().hl = 0x4000;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x20;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x67;  // RRD
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 18 && c.state().regs().a() == 0x80 && b.memory[0x4000] == 0x42 &&
                   c.state().regs().f() == State::kFlagSign;  // 0x80
        });

    run("Rld_NibbleRotate_UpdatesAAndMemoryTiming18",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x84);
            c.state().regs().hl = 0x4000;
            c.state().regs().set_f(0x00);
            b.memory[0x4000] = 0x20;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x6F;  // RLD
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 18 && c.state().regs().a() == 0x82 && b.memory[0x4000] == 0x04 &&
                   c.state().regs().f() == (State::kFlagSign | State::kFlagParityOverflow);  // 0x84
        });

    // --- 16-bit load ED forms: LD (nn),rp / LD rp,(nn) ---------------------
    run("LdMemNnBc_StoreWordLittleEndianTiming20",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x1234;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x43;  // LD (nn),BC
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x40;
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 20 && b.memory[0x4000] == 0x34 && b.memory[0x4001] == 0x12;
        });

    run("LdBcMemNn_LoadWordLittleEndianTiming20",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x4000] = 0x78;
            b.memory[0x4001] = 0x56;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x4B;  // LD BC,(nn)
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x40;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 20 && c.state().regs().bc == 0x5678;
        });

    run("LdMemNnSp_StoreStackPointerTiming20",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xABCD;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x73;  // LD (nn),SP
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x40;
        },
        [](ArrayBus& b, cpu::Z80aCpu&, std::uint32_t t) {
            return t == 20 && b.memory[0x4000] == 0xCD && b.memory[0x4001] == 0xAB;
        });

    run("LdDeMemNn_LoadWordTiming20",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x4000] = 0xEF;
            b.memory[0x4001] = 0xBE;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x5B;  // LD DE,(nn)
            b.memory[0x0002] = 0x00;
            b.memory[0x0003] = 0x40;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 20 && c.state().regs().de == 0xBEEF;
        });

    // --- Preserved RETN / RETI / IM (regression from S1) -------------------
    run("Retn_PopsPcRestoresIff1FromIff2Timing14",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            c.state().set_iff1(false);
            c.state().set_iff2(true);
            b.memory[0xFFFD] = 0x34;
            b.memory[0xFFFE] = 0x12;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x45;  // RETN
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 14 && c.state().regs().pc == 0x1234 && c.state().regs().sp == 0xFFFF &&
                   c.state().iff1();
        });

    run("Reti_PopsPcTiming14",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFD;
            b.memory[0xFFFD] = 0x00;
            b.memory[0xFFFE] = 0x80;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x4D;  // RETI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 14 && c.state().regs().pc == 0x8000 && c.state().regs().sp == 0xFFFF;
        });

    run("Im2_SetsInterruptModeTiming8",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x5E;  // IM 2
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().interrupt_mode() == cpu::InterruptMode::Im2;
        });

    // --- Block I/O deterministic core (open-bus data; timing/counter/mem) --
    run("Ini_WritesOpenBusDecrementsBTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().set_b(0x02);
            c.state().regs().set_c(0x10);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA2;  // INI
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && b.memory[0x4000] == 0xFF && c.state().regs().b() == 0x01 &&
                   c.state().regs().hl == 0x4001 && c.state().regs().pc == 0x0002;
        });

    run("Inir_MidLoop_Repeats21TAndRewindsPc",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().set_b(0x02);
            c.state().regs().set_c(0x10);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xB2;  // INIR
        },
        [](ArrayBus& b, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 21 && b.memory[0x4000] == 0xFF && c.state().regs().b() == 0x01 &&
                   c.state().regs().hl == 0x4001 && c.state().regs().pc == 0x0000;
        });

    run("Outi_ReadsMemDecrementsBTiming16",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().set_b(0x02);
            c.state().regs().set_c(0x10);
            b.memory[0x4000] = 0x99;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA3;  // OUTI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 16 && c.state().regs().b() == 0x01 && c.state().regs().hl == 0x4001 &&
                   c.state().regs().pc == 0x0002;
        });

    // --- Undefined ED opcode remains a deterministic 8T NOP ----------------
    run("UndefinedEdOpcode_NoOp8T",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x5A);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x00;  // undefined ED opcode
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t t) {
            return t == 8 && c.state().regs().a() == 0x5A && c.state().regs().pc == 0x0002;
        });

    return g_failures == 0 ? 0 : 1;
}
