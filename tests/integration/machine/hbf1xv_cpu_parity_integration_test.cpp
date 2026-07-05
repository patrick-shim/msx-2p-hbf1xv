#include <array>
#include <cstdint>
#include <iostream>

#include "devices/cpu/z80a_cpu.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvCpuParity_Integration  (M12-S6)
//
// Exercises the M12 CPU parity behaviors END-TO-END through the real M11
// SystemBus (slot decode + I/O bus + S1985 M1 wait) — not a fake bus. Covers:
//   1. WZ path: LD A,(nn) then BIT n,(HL) -> X/Y from WZ hi byte via real reads.
//   2. SCF-Q genuine-Zilog move-from-A (the documented A-4 divergence from
//      openMSX's OR-form): after CP sets F.Y with A=0, SCF yields X/Y = 0.
//   3. Block + NMOS carry: OUTI to a real I/O port (0xFC mapper), asserting
//      B/HL/carry and that the I/O bus dispatched on port & 0xFF (readback).
//   4. Interrupt path: IM1 accept -> vector 0x0038, IFF1/IFF2 cleared, push
//      order, and the ACK cycle == datasheet 13 + one M1 wait = 14 (no double
//      count, ties to the M11 IM oracle).
//   5. RETI IFF restore and EI one-instruction delay across a real ISR.
//   6. Whole-program timing oracle (datasheet + machine M1 wait) + determinism.
//
// Determinism: fixed program bytes, fixed injected interrupt timing, exact
// register/memory/flag/cycle assertions, bounded guard loops (no wall-clock).

namespace {

namespace cpu = sony_msx::devices::cpu;
using State = cpu::Z80aState;
using sony_msx::machine::Hbf1xvMachine;

constexpr std::uint8_t kX = State::kFlagX;
constexpr std::uint8_t kY = State::kFlagY;
constexpr std::uint8_t kC = State::kFlagCarry;
constexpr std::uint8_t kXY = static_cast<std::uint8_t>(kX | kY);

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

void run_to_halt(Hbf1xvMachine& machine, int guard = 64) {
    for (int i = 0; i < guard && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
}

}  // namespace

int main() {
    // --- 1. WZ -> BIT n,(HL) X/Y over the real memory read path -----------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // LD A,(0x2820) -> WZ = 0x2821 (hi 0x28: bits 3 and 5); LD HL,0x0200;
        // BIT 0,(HL) sources X/Y from WZ hi = 0x28 -> X and Y set; HALT.
        const std::array<std::uint8_t, 9> program{
            0x3A, 0x20, 0x28,  // LD A,(0x2820)
            0x21, 0x00, 0x02,  // LD HL,0x0200
            0xCB, 0x46,        // BIT 0,(HL)
            0x76,              // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint8_t operand = 0x00;
        machine.load_memory(0x0200, &operand, 1);
        const std::uint8_t at2820 = 0x11;
        machine.load_memory(0x2820, &at2820, 1);
        run_to_halt(machine);
        const std::uint8_t f = machine.cpu().state().regs().f();
        if (!expect_true(machine.cpu().state().regs().wz == 0x2821 && (f & kXY) == kXY,
                         "WzPath_BitNHl_XYFromWzHiByte")) {
            return 1;
        }
    }

    // --- 2. SCF-Q genuine-Zilog move-from-A (A-4 divergence) --------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // LD A,0x00 ; CP 0x20 (F.Y set from operand, Q=F, A=0) ; SCF ; HALT.
        // Genuine Zilog: X/Y moved from A(=0) -> 0. openMSX OR-form would set Y.
        const std::array<std::uint8_t, 5> program{
            0x3E, 0x00,  // LD A,0x00
            0xFE, 0x20,  // CP 0x20
            0x37,        // SCF
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint8_t halt = 0x76;
        machine.load_memory(0x0005, &halt, 1);
        run_to_halt(machine);
        const std::uint8_t f = machine.cpu().state().regs().f();
        if (!expect_true((f & kXY) == 0 && (f & kC) != 0, "ScfQ_MoveFromA_XYZeroCarrySet")) {
            return 1;
        }
    }

    // --- 3. OUTI to a real I/O port: carry + dispatch on port & 0xFF -------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // LD BC,0x02FC ; LD HL,0x0300 ; OUTI ; HALT.
        // (HL)=0xFF -> written to mapper port 0xFC. After: B=0x01, HL=0x0301,
        // k = data(0xFF) + newL(0x01) = 0x100 > 255 -> carry set (NMOS behavior).
        const std::array<std::uint8_t, 9> program{
            0x01, 0xFC, 0x02,  // LD BC,0x02FC
            0x21, 0x00, 0x03,  // LD HL,0x0300
            0xED, 0xA3,        // OUTI
            0x76,              // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint8_t data = 0xFF;
        machine.load_memory(0x0300, &data, 1);
        run_to_halt(machine);
        const std::uint8_t f = machine.cpu().state().regs().f();
        const bool state_ok = machine.cpu().state().regs().b() == 0x01 &&
                              machine.cpu().state().regs().hl == 0x0301 && (f & kC) != 0;
        if (!expect_true(state_ok, "Outi_RealPort_CarryAndCountersCorrect")) {
            return 1;
        }
    }

    // --- 3b. OUTI dispatch proven via mapper readback on 0xFC --------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // OUTI writes 0xFF to mapper port 0xFC; mapper stores val & 0x1F and reads
        // back 0x80 | (val & 0x1F) = 0x9F -> proves dispatch keyed on port & 0xFF.
        const std::array<std::uint8_t, 12> program{
            0x01, 0xFC, 0x02,  // LD BC,0x02FC
            0x21, 0x00, 0x03,  // LD HL,0x0300
            0xED, 0xA3,        // OUTI  -> port 0xFC
            0xDB, 0xFC,        // IN A,(0xFC)  -> readback
            0x76,              // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint8_t data = 0xFF;
        machine.load_memory(0x0300, &data, 1);
        run_to_halt(machine);
        if (!expect_true(machine.cpu().state().regs().a() == 0x9F,
                         "Outi_IoBusDispatchedOnPortLowByte")) {
            return 1;
        }
    }

    // --- 4 & 5. IM1 accept oracle, IFF clear, RETI restore, EI delay -------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // Main: EI ; NOP ; NOP ; HALT.
        const std::array<std::uint8_t, 4> main_program{0xFB, 0x00, 0x00, 0x76};
        machine.load_memory(0x0000, main_program.data(),
                            static_cast<std::uint32_t>(main_program.size()));
        // ISR at 0x0038: EI ; RETI.
        const std::array<std::uint8_t, 3> isr{0xFB, 0xED, 0x4D};
        machine.load_memory(0x0038, isr.data(), static_cast<std::uint32_t>(isr.size()));

        machine.step_cpu_instruction();  // EI (arms one-instruction delay)
        machine.cpu().request_maskable_interrupt();
        machine.step_cpu_instruction();  // NOP -> IRQ deferred by EI delay
        const bool deferred = machine.cpu().state().regs().pc == 0x0002 &&
                              machine.cpu().state().maskable_interrupt_pending();

        const std::uint64_t ack_cycles = machine.step_cpu_instruction();  // ACK
        const bool ack_ok = ack_cycles == 14 &&  // datasheet 13 + 1 M1 wait (no double count)
                            machine.cpu().state().regs().pc == 0x0038 &&
                            !machine.cpu().state().iff1() && !machine.cpu().state().iff2() &&
                            machine.cpu().state().regs().sp == 0xFFFD &&
                            machine.read_memory(0xFFFD) == 0x02 &&  // pushed return addr lo
                            machine.read_memory(0xFFFE) == 0x00;    // return addr hi

        machine.step_cpu_instruction();               // ISR: EI (re-enables IFF, arms delay)
        machine.step_cpu_instruction();               // ISR: RETI
        const bool reti_ok = machine.cpu().state().regs().pc == 0x0002 &&
                             machine.cpu().state().iff1() &&  // IFF1 restored from IFF2
                             machine.cpu().state().regs().sp == 0xFFFF;

        if (!expect_true(deferred, "EiDelay_IrqDeferredOneInstruction") ||
            !expect_true(ack_ok, "Im1Ack_VectorIffPushAndCycleOracle") ||
            !expect_true(reti_ok, "Reti_RestoresIff1AndReturns")) {
            return 1;
        }
    }

    // --- 6. Whole-program timing oracle + determinism ---------------------
    // Program: LD BC,nn(10) + LD HL,nn(10) + OUTI(16) + HALT(4) = 40 datasheet.
    // M1 cycles: 1 + 1 + 2(ED A3) + 1 = 5 -> +5 S1985 M1 wait -> 45 total.
    {
        const std::array<std::uint8_t, 9> program{
            0x01, 0xFC, 0x02, 0x21, 0x00, 0x03, 0xED, 0xA3, 0x76,
        };
        auto run_once = []() {
            Hbf1xvMachine machine;
            machine.cold_boot();
            const std::array<std::uint8_t, 9> prog{
                0x01, 0xFC, 0x02, 0x21, 0x00, 0x03, 0xED, 0xA3, 0x76,
            };
            machine.load_memory(0x0000, prog.data(), static_cast<std::uint32_t>(prog.size()));
            const std::uint8_t data = 0xFF;
            machine.load_memory(0x0300, &data, 1);
            run_to_halt(machine);
            return machine.elapsed_cycles();
        };
        static_cast<void>(program);
        const std::uint64_t c1 = run_once();
        const std::uint64_t c2 = run_once();
        if (!expect_true(c1 == 45 && c2 == 45,
                         "TimingOracle_DatasheetPlusM1Wait_Deterministic")) {
            std::cerr << "  observed c1=" << c1 << " c2=" << c2 << "\n";
            return 1;
        }
    }

    return 0;
}
