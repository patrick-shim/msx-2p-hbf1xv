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

// Suite: Machine_Hbf1xvM15Devices_Integration  (M15-S6)
//
// Cross-boundary coverage for the M15 devices wired onto the M11 IoBus: a real
// CPU program drives the PSG (OUT #A0/#A1 then IN #A2, YM readback); the S1985
// #AC mirror equals #A8; the keyboard matrix is read through PPI #AA/#A9; the
// joystick is read through PSG R14/R15; the RTC answers a REDCLK-style #B4/#B5
// sequence gated by #F5; and the S1985 backup RAM survives a .sram round-trip
// across cold_boot. No wall-clock; all loops bounded; two runs deterministic.

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>

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

    // --- Real CPU program: PSG OUT #A0/#A1 then IN #A2 (YM readback). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 16> program{
            0x3E, 0x01, 0xD3, 0xA0,  // LD A,1    ; OUT (0xA0),A  (select R1)
            0x3E, 0xFF, 0xD3, 0xA1,  // LD A,0xFF ; OUT (0xA1),A  (write R1=0xFF)
            0xDB, 0xA2,              // IN  A,(0xA2)              (read R1 back)
            0x32, 0x00, 0x90,        // LD (0x9000),A
            0x76,                    // HALT
            0x00,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        for (int steps = 0; steps < 32 && !machine.cpu().state().halted(); ++steps) {
            machine.step_cpu_instruction();
        }
        expect(machine.cpu().state().halted(), "PsgProgram_ReachesHalt");
        // YM2149 reads back the written 0xFF (an AY would mask R1 to 0x0F).
        expect(machine.read_memory(0x9000) == 0xFF, "PsgOutIn_YmReadback_0xFF");
        expect(machine.psg().register_value(1) == 0xFF, "PsgRegister1_StoredAsWritten");
    }

    // --- S1985 PPI mirror: IN(#AC) == IN(#A8) (engine-detection oracle). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0xA8, 0x5A);  // drive slot select (preserved path)
        expect(machine.debug_io_read(0xAC) == machine.debug_io_read(0xA8),
               "PpiMirror_Ac_EqualsA8");
        // Restore a sane slot map so nothing downstream is disturbed.
        machine.debug_io_write(0xA8, 0x00);
    }

    // --- Keyboard matrix read through the PPI over the bus. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.keyboard().set_key(4, 6, true);  // row 4, column 6 -> 0xBF
        // Program the MSX PPI mode (A out, B in, C out), select row 4, read #A9.
        machine.debug_io_write(0xAB, 0x82);
        machine.debug_io_write(0xAA, 0x04);
        expect(machine.debug_io_read(0xA9) == static_cast<std::uint8_t>(0xFF & ~(1u << 6)),
               "Keyboard_Row4_ReadThroughPpi");
        // Idle row reads all-high.
        machine.debug_io_write(0xAA, 0x05);
        expect(machine.debug_io_read(0xA9) == 0xFF, "Keyboard_IdleRow_AllHigh");
    }

    // --- Joystick read through the PSG (R15 select, R14 read) over the bus. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        sony_msx::peripherals::JoystickPorts::PortState p2;
        p2.left = true;  // bit2 on port 2
        machine.joystick().set_port(1, p2);
        machine.debug_io_write(0xA0, 15);    // select R15
        machine.debug_io_write(0xA1, 0x40);  // R15 bit6 -> port 2
        machine.debug_io_write(0xA0, 14);    // select R14
        expect(machine.debug_io_read(0xA2) == static_cast<std::uint8_t>(0xFF & ~0x04),
               "Joystick_Port2Left_ReadThroughPsgR14");
    }

    // --- RTC REDCLK-style #B4/#B5 sequence, gated by #F5. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // #F5 defaults to CLOCK-IC enabled. Select block 2 (mode reg 13).
        machine.debug_io_write(0xB4, 13);
        machine.debug_io_write(0xB5, 0x0A);  // timer-en (0x08) + block 2
        machine.debug_io_write(0xB4, 0);     // latch reg 0
        expect(machine.debug_io_read(0xB5) == 0xFA, "Rtc_Block2Reg0_ValidCmosOverBus");
        // Disable the CLOCK-IC via #F5 bit7 -> RTC data path inert.
        machine.debug_io_write(0xF5, 0x00);
        expect(machine.debug_io_read(0xB5) == 0xFF, "Rtc_F5Disabled_OpenBusOverBus");
        machine.debug_io_write(0xF5, 0x80);  // re-enable
        expect(machine.debug_io_read(0xB5) == 0xFA, "Rtc_F5ReEnabled_DataAgain");
    }

    // --- S1985 backup RAM: write over the #40-#4F bus, .sram round-trip. ---
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "sony_msx_m15_machine_backup.sram";
        std::error_code ec;
        std::filesystem::remove(path, ec);

        {
            Hbf1xvMachine machine;
            machine.set_backup_ram_path(path);
            machine.cold_boot();  // absent file -> zero
            // Select device ID 0xFE, write 16 bytes via address/data registers.
            machine.debug_io_write(0x40, 0xFE);
            for (int i = 0; i < 16; ++i) {
                machine.debug_io_write(0x41, static_cast<std::uint8_t>(i));
                machine.debug_io_write(0x42, static_cast<std::uint8_t>(0x30 + i));
            }
            // Read one back over the bus.
            machine.debug_io_write(0x41, 5);
            expect(machine.debug_io_read(0x42) == 0x35, "BackupRam_WriteReadOverBus");
            expect(machine.flush_backup_ram(), "BackupRam_FlushToSram");
        }
        {
            Hbf1xvMachine machine;
            machine.set_backup_ram_path(path);
            machine.cold_boot();  // loads the .sram
            machine.debug_io_write(0x40, 0xFE);
            machine.debug_io_write(0x41, 9);
            expect(machine.debug_io_read(0x42) == static_cast<std::uint8_t>(0x30 + 9),
                   "BackupRam_ReloadedFromSram");
        }
        std::filesystem::remove(path, ec);
    }

    // --- Determinism: two fresh machines read identical device state. ---
    {
        auto probe = [] {
            Hbf1xvMachine machine;
            machine.cold_boot();
            machine.debug_io_write(0xB4, 0);
            const std::uint8_t rtc0 = machine.debug_io_read(0xB5);
            const std::uint8_t key = machine.debug_io_read(0xA9);
            return static_cast<int>(rtc0) * 256 + key;
        };
        expect(probe() == probe(), "Determinism_TwoMachinesIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
