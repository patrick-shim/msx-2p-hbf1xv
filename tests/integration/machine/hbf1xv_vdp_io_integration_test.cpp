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

// Suite: Machine_Hbf1xvVdpIo_Integration
//
// Cross-boundary deterministic coverage for the V9958 wired onto the M11 IoBus
// (M14-S5). Verifies: a real CPU program fills VRAM through OUT (#98) with auto-
// increment and reads it back; the S1985 #9C-#9F mirror routes to the SAME VDP
// (mirror equivalence); VRAM is a device store separate from CPU DRAM; and a
// frame-boundary VBlank is ACCEPTED by the CPU through the M12 IM1 path and
// released on the S#0 status read (R-1). No wall-clock; all loops are bounded.

#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Mirror equivalence: fill via base ports, read back via the mirror. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // Set write address 0x0100 via #99, fill 16 bytes via #98.
        machine.debug_io_write(0x99, 0x00);
        machine.debug_io_write(0x99, 0x41);  // 0x0100, bit6=1 write
        for (int i = 0; i < 16; ++i) {
            machine.debug_io_write(0x98, static_cast<std::uint8_t>(0x50 + i));
        }
        // Read back through the S1985 mirror (#9D = #99, #9C = #98).
        machine.debug_io_write(0x9D, 0x00);
        machine.debug_io_write(0x9D, 0x01);  // 0x0100, bit6=0 read (read-ahead)
        bool mirror_ok = true;
        for (int i = 0; i < 16; ++i) {
            if (machine.debug_io_read(0x9C) != 0x50 + i) {
                mirror_ok = false;
            }
        }
        expect(mirror_ok, "Mirror9C9D_RouteToSameVdp_ReadBackRamp");
        expect(machine.vdp().vram().read(0x0100) == 0x50, "Mirror_WroteRealVram");
    }

    // --- Real CPU program drives the VDP over the SystemBus (OUT #99/#98). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        // LD A,00 / OUT(99) / LD A,40 / OUT(99)  -> write address 0x0000
        // then four LD A,n / OUT(98) auto-increment writes, then HALT.
        const std::array<std::uint8_t, 25> program{
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT (0x99),A  (addr low)
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT (0x99),A  (addr high, write)
            0x3E, 0xAA, 0xD3, 0x98,  // LD A,0xAA ; OUT (0x98),A
            0x3E, 0xBB, 0xD3, 0x98,  // LD A,0xBB ; OUT (0x98),A
            0x3E, 0xCC, 0xD3, 0x98,  // LD A,0xCC ; OUT (0x98),A
            0x3E, 0xDD, 0xD3, 0x98,  // LD A,0xDD ; OUT (0x98),A
            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        for (int steps = 0; steps < 32 && !machine.cpu().state().halted(); ++steps) {
            machine.step_cpu_instruction();
        }
        expect(machine.cpu().state().halted(), "CpuProgram_ReachesHalt");
        const auto& vram = machine.vdp().vram();
        expect(vram.read(0) == 0xAA && vram.read(1) == 0xBB && vram.read(2) == 0xCC &&
                   vram.read(3) == 0xDD,
               "CpuOut98_AutoIncrementFill_LandsInVram");

        // VRAM is a device store, NOT CPU-addressable memory: DRAM at the same
        // low addresses is unaffected by the VRAM writes (the program bytes remain).
        expect(machine.read_memory(0x0000) == 0x3E && machine.read_memory(0x0001) == 0x00,
               "Vram_SeparateFromDram_NoCpuAddressableWindow");
    }

    // --- VBlank accepted through IM1, released on the S#0 read (R-1). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        // Main: EI ; HALT ; HALT (halt again after the ISR returns).
        const std::array<std::uint8_t, 3> main_prog{0xFB, 0x76, 0x76};
        machine.load_memory(0x0000, main_prog.data(),
                            static_cast<std::uint32_t>(main_prog.size()));
        // ISR at the IM1 vector 0x0038: read S#0 (clears F + releases /INT),
        // store it, increment an acceptance counter, then EI ; RET.
        const std::array<std::uint8_t, 12> isr{
            0xF5,              // PUSH AF
            0xDB, 0x99,        // IN A,(0x99)      ; read S#0 (R#15=0) -> clears F
            0x32, 0x01, 0x90,  // LD (0x9001),A    ; store the S#0 value
            0x3A, 0x00, 0x90,  // LD A,(0x9000)    ; counter
            0x3C,              // INC A
            0x32, 0x00,        // LD (0x9000),A ... (completed below)
        };
        machine.load_memory(0x0038, isr.data(), static_cast<std::uint32_t>(isr.size()));
        // Finish the LD (0x9000),A high byte + POP AF ; EI ; RET.
        const std::array<std::uint8_t, 4> isr_tail{0x90, 0xF1, 0xFB, 0xC9};
        machine.load_memory(0x0038 + 12, isr_tail.data(),
                            static_cast<std::uint32_t>(isr_tail.size()));

        // Zero the acceptance counter deterministically (A-5 pattern puts 0 here,
        // but make it explicit and independent of the pattern).
        const std::uint8_t zero = 0x00;
        machine.load_memory(0x9000, &zero, 1);

        // Enable R#1 IE0 (VBlank interrupt) via the two-write #99 protocol.
        machine.debug_io_write(0x99, 0x20);
        machine.debug_io_write(0x99, 0x81);

        // Frame boundary raises VBlank; with IE0 the /INT asserts.
        machine.run_frame();
        expect(machine.vdp().irq_active(), "VBlank_FrameBoundary_AssertsInt");

        // Step the CPU: EI, then HALT, then the CPU accepts the interrupt.
        for (int steps = 0; steps < 40; ++steps) {
            machine.step_cpu_instruction();
        }

        expect(machine.read_memory(0x9000) == 0x01, "Interrupt_AcceptedExactlyOnce");
        expect(machine.read_memory(0x9001) == 0x80, "Isr_ReadS0_ObservedFSet");
        expect(machine.vdp().peek_status_register(0) == 0x00, "Isr_ReadS0_ClearedF");
        expect(!machine.vdp().irq_active(), "Interrupt_ReleasedAfterS0Read");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
