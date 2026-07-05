#include <array>
#include <cstdint>
#include <functional>
#include <iostream>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AWzMemptr_Unit  (M12-S3, gaps #3 / #4 / #5 / #35)
//
// Proves the internal WZ/MEMPTR register is tracked at every fact-sheet §4 rule
// site and that BIT n,(HL) / BIT n,(IX+d) source undocumented X/Y from the WZ
// high byte. WZ is not directly software-visible, so most cases assert the raw
// regs().wz value after the instruction (deterministic internal state), and the
// BIT cases assert the observable X/Y consequence. Grounding: fact-sheet §4;
// openMSX CPUCore.cc setMemPtr sites (cited per case).

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
    // --- LD A,(nn): WZ = nn+1 (openMSX:2781) -------------------------------
    run("LdAxnn_WzIsNnPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0x3A;  // LD A,(0x4000)
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x40;
            b.memory[0x4000] = 0x99;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x4001; });

    // --- LD (nn),A: WZ = (A<<8)|((nn+1)&0xFF) (openMSX:2752) ----------------
    run("LdxnnA_WzIsAHiNnLoPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x7E);
            b.memory[0x0000] = 0x32;  // LD (0x40FF),A
            b.memory[0x0001] = 0xFF;
            b.memory[0x0002] = 0x40;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x7E00;  // (0x7E<<8)|((0xFF+1)&0xFF)=0x7E00
        });

    // --- LD A,(BC): WZ = BC+1 (openMSX:2773) -------------------------------
    run("LdAxBc_WzIsBcPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x1234;
            b.memory[0x1234] = 0x11;
            b.memory[0x0000] = 0x0A;  // LD A,(BC)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x1235; });

    // --- LD (DE),A: WZ = (A<<8)|((DE+1)&0xFF) ------------------------------
    run("LdxDeA_WzIsAHiDeLoPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x42);
            c.state().regs().de = 0x2010;
            b.memory[0x0000] = 0x12;  // LD (DE),A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x4211; });

    // --- LD HL,(nn): WZ = nn+1 (openMSX:2808) ------------------------------
    run("LdHlxnn_WzIsNnPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0x2A;  // LD HL,(0x5000)
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x50;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x5001; });

    // --- JP nn: WZ = nn (openMSX:3926) -------------------------------------
    run("JpNn_WzIsTarget",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0xC3;  // JP 0x1357
            b.memory[0x0001] = 0x57;
            b.memory[0x0002] = 0x13;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x1357 && c.state().regs().pc == 0x1357;
        });

    // --- JP cc,nn NOT taken: WZ still = nn (openMSX:3926) -------------------
    run("JpCcNn_NotTaken_WzStillTarget",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(0x00);  // Z clear -> JP Z not taken
            b.memory[0x0000] = 0xCA;       // JP Z,0x2468
            b.memory[0x0001] = 0x68;
            b.memory[0x0002] = 0x24;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x2468 && c.state().regs().pc == 0x0003;
        });

    // --- CALL nn: WZ = nn (openMSX:3866) -----------------------------------
    run("CallNn_WzIsTarget",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().sp = 0xFFFF;
            b.memory[0x0000] = 0xCD;  // CALL 0x0080
            b.memory[0x0001] = 0x80;
            b.memory[0x0002] = 0x00;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x0080 && c.state().regs().pc == 0x0080;
        });

    // --- CALL cc,nn NOT taken: WZ = nn -------------------------------------
    run("CallCcNn_NotTaken_WzStillTarget",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_f(0x00);  // Z clear -> CALL Z not taken
            b.memory[0x0000] = 0xCC;       // CALL Z,0x1000
            b.memory[0x0001] = 0x00;
            b.memory[0x0002] = 0x10;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x1000 && c.state().regs().pc == 0x0003;
        });

    // --- JR d taken: WZ = destination (openMSX:3971) -----------------------
    run("JrD_WzIsDestination",
        [](ArrayBus& b, cpu::Z80aCpu&) {
            b.memory[0x0000] = 0x18;  // JR +4
            b.memory[0x0001] = 0x04;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x0006 && c.state().regs().pc == 0x0006;
        });

    // --- DJNZ taken: WZ = destination; DJNZ falling through: WZ unchanged ---
    run("Djnz_Taken_WzIsDestination",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x02);
            b.memory[0x0000] = 0x10;  // DJNZ +2
            b.memory[0x0001] = 0x02;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x0004 && c.state().regs().pc == 0x0004;
        });

    run("Djnz_FallThrough_WzUnchanged",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_b(0x01);   // B->0, no branch
            c.state().regs().wz = 0xBEEF;   // sentinel
            b.memory[0x0000] = 0x10;        // DJNZ +2
            b.memory[0x0001] = 0x02;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0xBEEF && c.state().regs().pc == 0x0002;
        });

    // --- ADD HL,rr: WZ = HL+1 before add (openMSX:3247) --------------------
    run("AddHlBc_WzIsHlPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x8000;
            c.state().regs().bc = 0x0001;
            b.memory[0x0000] = 0x09;  // ADD HL,BC
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x8001 && c.state().regs().hl == 0x8001;
        });

    // --- ADC HL,rr: WZ = HL+1 before op (openMSX:3799) ---------------------
    run("AdcHlDe_WzIsHlPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().hl = 0x4000;
            c.state().regs().de = 0x0010;
            c.state().regs().set_f(0x00);
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x5A;  // ADC HL,DE
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x4001; });

    // --- IN A,(n): WZ = ((A<<8)|n)+1 (openMSX:4026) ------------------------
    run("InAxn_WzIsPortPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x12);
            b.memory[0x0000] = 0xDB;  // IN A,(0x34)
            b.memory[0x0001] = 0x34;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x1235;  // (0x1234)+1
        });

    // --- OUT (n),A: WZ = (A<<8)|((n+1)&0xFF) (openMSX:4052) -----------------
    run("OutxnA_WzIsAHiPortLoPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().set_a(0x56);
            b.memory[0x0000] = 0xD3;  // OUT (0x78),A
            b.memory[0x0001] = 0x78;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x5679; });

    // --- IN r,(C): WZ = BC+1 (openMSX:4008) --------------------------------
    run("InRc_WzIsBcPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x00A0;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x40;  // IN B,(C)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x00A1; });

    // --- OUT (C),r: WZ = BC+1 (openMSX:4042) -------------------------------
    run("OutCr_WzIsBcPlusOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x10FF;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0x79;  // OUT (C),A
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x1100; });

    // --- (IX+d) access: WZ = IX+d (openMSX:2800) ---------------------------
    run("LdRxIxd_WzIsIndexPlusD",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x4000;
            b.memory[0x4005] = 0x77;
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0x7E;  // LD A,(IX+5)
            b.memory[0x0002] = 0x05;
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x4005 && c.state().regs().a() == 0x77;
        });

    // --- INI: WZ = BC+1 taken before B decrement (openMSX:4126) ------------
    run("Ini_WzIsBcPlusOneBeforeDec",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x05C0;
            c.state().regs().hl = 0x4000;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA2;  // INI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x05C1; });

    // --- OUTI: WZ = BC+1 taken AFTER B decrement (openMSX:4160) ------------
    run("Outi_WzIsBcPlusOneAfterDec",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().bc = 0x05C0;  // B decrements to 0x04
            c.state().regs().hl = 0x4000;
            b.memory[0x4000] = 0x00;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA3;  // OUTI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            return c.state().regs().wz == 0x04C1;  // (0x04C0)+1
        });

    // --- CPI: WZ = WZ+1 (openMSX:4061) -------------------------------------
    run("Cpi_WzIncrementsByOne",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().wz = 0x1000;  // predecessor value
            c.state().regs().bc = 0x0002;
            c.state().regs().hl = 0x4000;
            c.state().regs().set_a(0x00);
            b.memory[0x4000] = 0x00;
            b.memory[0x0000] = 0xED;
            b.memory[0x0001] = 0xA1;  // CPI
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) { return c.state().regs().wz == 0x1001; });

    // --- BIT n,(HL): observable X/Y from WZ hi byte (openMSX:3420) ---------
    // Chain LD A,(nn) [sets WZ=nn+1] then BIT n,(HL) and read X/Y.
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu processor(client);
        processor.reset();
        processor.state().regs().hl = 0x4000;
        bus.memory[0x4000] = 0x00;  // bit clear -> Z|PV|H set
        // WZ high byte set to 0x28 (bits 3 and 5 set) directly to isolate the path.
        processor.state().regs().wz = 0x2800;
        bus.memory[0x0000] = 0xCB;
        bus.memory[0x0001] = 0x46;  // BIT 0,(HL)
        const std::uint32_t t = processor.step();
        const std::uint8_t f = processor.state().regs().f();
        const bool ok = t == 12 &&
                        f == (State::kFlagZero | State::kFlagHalfCarry | State::kFlagParityOverflow |
                              State::kFlagX | State::kFlagY);
        if (!ok) {
            std::cerr << "Case failed: BitNHl_UndocXYFromWzHiByte\n";
            ++g_failures;
        }
    }

    // --- BIT n,(HL) after a real WZ-setting predecessor (LD A,(nn)) --------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu processor(client);
        processor.reset();
        // LD A,(0x2820) -> WZ = 0x2821 -> hi byte 0x28 -> X and Y set.
        bus.memory[0x0000] = 0x3A;
        bus.memory[0x0001] = 0x20;
        bus.memory[0x0002] = 0x28;
        bus.memory[0x2820] = 0x00;
        processor.step();  // LD A,(nn); WZ = 0x2821
        processor.state().regs().hl = 0x4000;
        bus.memory[0x4000] = 0x00;
        bus.memory[0x0003] = 0xCB;
        bus.memory[0x0004] = 0x46;  // BIT 0,(HL)
        processor.step();
        const std::uint8_t f = processor.state().regs().f();
        // WZ hi = 0x28 -> X (bit3) and Y (bit5) set.
        const bool ok = (f & State::kFlagX) != 0 && (f & State::kFlagY) != 0;
        if (!ok) {
            std::cerr << "Case failed: BitNHl_XYFromWzAfterLdAnnPredecessor\n";
            ++g_failures;
        }
    }

    // --- BIT n,(IX+d): X/Y from WZ hi = (IX+d) hi (openMSX:3426) -----------
    run("BitNxIxd_UndocXYFromIndexHiByte",
        [](ArrayBus& b, cpu::Z80aCpu& c) {
            c.state().regs().ix = 0x2800;  // IX+d = 0x2820 -> hi 0x28 (bits 3 and 5)
            b.memory[0x2820] = 0x00;       // bit clear
            b.memory[0x0000] = 0xDD;
            b.memory[0x0001] = 0xCB;
            b.memory[0x0002] = 0x20;  // d = 0x20
            b.memory[0x0003] = 0x46;  // BIT 0,(IX+d)
        },
        [](ArrayBus&, cpu::Z80aCpu& c, std::uint32_t) {
            const std::uint8_t f = c.state().regs().f();
            return c.state().regs().wz == 0x2820 && (f & State::kFlagX) != 0 &&
                   (f & State::kFlagY) != 0;
        });

    return g_failures == 0 ? 0 : 1;
}
