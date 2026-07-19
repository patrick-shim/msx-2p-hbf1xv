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

// Suite: Devices_Z80ANmosInterrupt_Unit
//
// Covers the NMOS interrupt-edge behaviors:
//  - RETI copies IFF2 -> IFF1 exactly like RETN (fact-sheet §5; openMSX retn()
//    is also reti(), CPUCore.cc:3911-3915).
//  - The NMOS LD A,I / LD A,R P/V interrupt bug: when a maskable interrupt will
//    be accepted at the instruction boundary, P/V reads 0 despite IFF2 = 1
//    (fact-sheet §5; openMSX prevWasLDAI fix-up, CPUCore.cc:2476-2496).
//  - EI one-instruction delay edges (EI;RET, EI;EI) regression.
//  - DI/EI/RETN IFF matrix, and IM/NMI acceptance bare T-states unchanged.

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

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    // --- RETI restores IFF1 from IFF2 (gap #30) ----------------------------
    // Accept an IM1 interrupt (IFF1/IFF2 cleared), then inside the handler set
    // IFF2 back (as EI would leave it) and RETI: IFF1 must be restored from IFF2.
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        cpu.state().regs().pc = 0x0200;
        cpu.state().regs().sp = 0xFFFF;
        bus.memory[0x0038] = 0xED;  // RETI
        bus.memory[0x0039] = 0x4D;
        cpu.request_maskable_interrupt();
        cpu.step();  // accept -> IFF1=IFF2=0, pc=0x0038
        // Model an ISR that re-enabled IFF2 (leaving IFF1 still 0) before RETI.
        cpu.state().set_iff2(true);
        const std::uint32_t t = cpu.step();  // RETI
        if (!expect_true(t == 14 && cpu.state().regs().pc == 0x0200 && cpu.state().iff1() &&
                             cpu.state().iff2(),
                         "Reti_CopiesIff2ToIff1")) {
            return 1;
        }
    }

    // --- RETI with IFF2 clear leaves IFF1 clear (regression parity) --------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().sp = 0xFFFD;
        cpu.state().set_iff1(true);   // will be overwritten from IFF2
        cpu.state().set_iff2(false);
        bus.memory[0xFFFD] = 0x00;
        bus.memory[0xFFFE] = 0x90;
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x4D;  // RETI
        const std::uint32_t t = cpu.step();
        if (!expect_true(t == 14 && cpu.state().regs().pc == 0x9000 && !cpu.state().iff1(),
                         "Reti_Iff2Clear_Iff1Cleared")) {
            return 1;
        }
    }

    // --- LD A,I clears P/V when an interrupt will be accepted (gap #31) -----
    // Realistic pattern: EI ; LD A,I with an IRQ pending. The EI-delay lets
    // LD A,I run (the IRQ is deferred one instruction), and the IRQ is then
    // accepted at the boundary immediately after LD A,I — the exact race that
    // triggers the NMOS P/V bug. openMSX notes the quirk is independent of a
    // preceding EI (CPUCore.cc:2490-2493).
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().i = 0x40;  // non-zero, bit7 clear
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0xFB;  // EI  (sets IFF1/IFF2, arms one-instruction delay)
        bus.memory[0x0001] = 0xED;
        bus.memory[0x0002] = 0x57;  // LD A,I
        cpu.step();                 // EI
        cpu.request_maskable_interrupt();  // pending; deferred by the EI delay
        const std::uint32_t t = cpu.step();  // LD A,I executes (IRQ deferred)
        const std::uint8_t f = cpu.state().regs().f();
        // A=0x40 -> S/Z clear; IFF2=1 but the NMOS bug forces P/V=0 because the
        // IRQ is accepted at the next boundary.
        if (!expect_true(t == 9 && cpu.state().regs().a() == 0x40 &&
                             (f & State::kFlagParityOverflow) == 0,
                         "LdAI_InterruptPending_PvClearedByNmosBug")) {
            return 1;
        }
    }

    // --- LD A,I without a pending interrupt: P/V reflects IFF2 (no bug) -----
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().i = 0x40;
        cpu.state().set_iff2(true);
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x57;  // LD A,I
        const std::uint32_t t = cpu.step();
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true(t == 9 && (f & State::kFlagParityOverflow) != 0,
                         "LdAI_NoInterrupt_PvReflectsIff2")) {
            return 1;
        }
    }

    // --- LD A,R also exhibits the bug (gap #31) ----------------------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_f(0x00);
        bus.memory[0x0000] = 0xFB;  // EI
        bus.memory[0x0001] = 0xED;
        bus.memory[0x0002] = 0x5F;  // LD A,R
        cpu.step();                 // EI
        cpu.request_maskable_interrupt();
        cpu.step();                 // LD A,R (IRQ deferred by EI delay)
        const std::uint8_t f = cpu.state().regs().f();
        if (!expect_true((f & State::kFlagParityOverflow) == 0,
                         "LdAR_InterruptPending_PvClearedByNmosBug")) {
            return 1;
        }
    }

    // --- EI one-instruction delay: EI ; RET -> IRQ not taken until after RET
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().regs().sp = 0xFFFE;
        bus.memory[0xFFFE] = 0x00;
        bus.memory[0xFFFF] = 0x40;  // RET target 0x4000
        bus.memory[0x0000] = 0xFB;  // EI
        bus.memory[0x0001] = 0xC9;  // RET
        cpu.step();                 // EI (delay armed)
        cpu.request_maskable_interrupt();
        const std::uint32_t t_ret = cpu.step();  // RET executes; IRQ still deferred
        const bool ret_ok = t_ret == 10 && cpu.state().regs().pc == 0x4000;
        bus.memory[0x0038] = 0x00;               // NOP handler
        const std::uint32_t t_irq = cpu.step();  // now the IRQ is accepted
        if (!expect_true(ret_ok && t_irq == 13 && cpu.state().regs().pc == 0x0038,
                         "EiRet_OneInstructionDelay_IrqAfterRet")) {
            return 1;
        }
    }

    // --- EI ; EI : delay re-armed; IRQ still deferred one instruction ------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().regs().sp = 0xFFFF;
        bus.memory[0x0000] = 0xFB;  // EI
        bus.memory[0x0001] = 0xFB;  // EI
        bus.memory[0x0002] = 0x00;  // NOP
        bus.memory[0x0038] = 0x00;  // handler
        cpu.step();                 // EI #1 (delay armed)
        cpu.request_maskable_interrupt();
        const std::uint32_t t_ei2 = cpu.step();  // EI #2: IRQ deferred again
        const bool ei2_ok = t_ei2 == 4 && cpu.state().regs().pc == 0x0002 &&
                            cpu.state().maskable_interrupt_pending();
        const std::uint32_t t_after = cpu.step();  // NOP boundary -> IRQ accepted
        if (!expect_true(ei2_ok && t_after == 13 && cpu.state().regs().pc == 0x0038,
                         "EiEi_DelayReArmed_IrqAfterSecond")) {
            return 1;
        }
    }

    // --- DI / EI / RETN IFF matrix -----------------------------------------
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_iff1(true);
        cpu.state().set_iff2(true);
        bus.memory[0x0000] = 0xF3;  // DI
        cpu.step();
        const bool di_ok = !cpu.state().iff1() && !cpu.state().iff2();

        cpu.state().regs().pc = 0x0010;
        bus.memory[0x0010] = 0xFB;  // EI
        cpu.step();
        const bool ei_ok = cpu.state().iff1() && cpu.state().iff2() && cpu.state().ei_delay_active();

        // RETN with IFF2=1, IFF1=0 restores IFF1.
        cpu.state().regs().pc = 0x0020;
        cpu.state().set_iff1(false);
        cpu.state().set_iff2(true);
        cpu.state().regs().sp = 0xFFFD;
        bus.memory[0xFFFD] = 0x00;
        bus.memory[0xFFFE] = 0x50;
        bus.memory[0x0020] = 0xED;
        bus.memory[0x0021] = 0x45;  // RETN
        cpu.step();
        const bool retn_ok = cpu.state().regs().pc == 0x5000 && cpu.state().iff1();

        if (!expect_true(di_ok && ei_ok && retn_ok, "DiEiRetn_IffMatrix")) {
            return 1;
        }
    }

    // --- Bare interrupt-acceptance T-states unchanged (no double-count) -----
    {
        ArrayBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().set_iff1(true);
        cpu.state().regs().sp = 0xFFFF;
        cpu.request_maskable_interrupt();
        const std::uint32_t t_im1 = cpu.step();  // IM1 = 13 bare

        cpu::Z80aCpu cpu2(client);
        cpu2.reset();
        cpu2.state().set_interrupt_mode(cpu::InterruptMode::Im2);
        cpu2.state().set_iff1(true);
        cpu2.state().regs().i = 0x80;
        cpu2.state().regs().sp = 0xFFFF;
        bus.memory[0x80FF] = 0x00;
        bus.memory[0x8100] = 0x70;
        cpu2.request_maskable_interrupt();
        const std::uint32_t t_im2 = cpu2.step();  // IM2 = 19 bare

        cpu::Z80aCpu cpu3(client);
        cpu3.reset();
        cpu3.state().regs().sp = 0xFFFF;
        cpu3.request_nmi();
        const std::uint32_t t_nmi = cpu3.step();  // NMI = 11 bare

        if (!expect_true(t_im1 == 13 && t_im2 == 19 && t_nmi == 11,
                         "InterruptAck_BareTStates_Unchanged")) {
            return 1;
        }
    }

    return 0;
}
