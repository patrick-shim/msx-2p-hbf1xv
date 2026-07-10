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

#include "devices/fdc/sony_fdc.h"

namespace sony_msx::devices::fdc {

SonyFdc::SonyFdc(memory::RomDevice& rom, Wd2793& fdc, DiskDrive& drive, FdcClockSource& clock)
    : rom_(rom), fdc_(fdc), drive_(drive), clock_(clock) {}

void SonyFdc::reset() {
    // PhilipsFDC.cc:17-22 reset() writes 0x3FFC=0 and 0x3FFD=0.
    side_reg_ = 0;
    drive_reg_ = 0;
    drive_.set_side(0);
    write_drive_register(0);
}

core::BusData SonyFdc::mem_read(const core::BusAddress address) {
    switch (address & 0x3FFF) {
        case 0x3FF8:
            return fdc_.read_status();
        case 0x3FF9:
            return fdc_.read_track();
        case 0x3FFA:
            return fdc_.read_sector();
        case 0x3FFB:
            return fdc_.read_data();
        case 0x3FFC:
            return side_reg_;  // PhilipsFDC.cc:77-80
        case 0x3FFD: {
            // bit2 = 0 iff disk changed (DSKCHG); else pulled to 1 (PhilipsFDC.cc:35-41).
            // READING the register CLEARS the DSKCHG one-shot: openMSX's readMem
            // calls the MUTATING multiplexer.diskChanged() (PhilipsFDC.cc:37),
            // which resets the latch (DiskChanger.cc:95-100). take_disk_changed()
            // mirrors that. This is REQUIRED so a swapped medium reports "changed"
            // exactly once; otherwise DSKCHG stays asserted forever after any swap
            // and a game that re-checks the disk retries/aborts into DI;HALT
            // (M36 Bug B). The debug/snapshot peek path stays on the const,
            // non-clearing drive_.disk_changed() (mirrors const peekMem :90).
            std::uint8_t res = static_cast<std::uint8_t>(drive_reg_ & ~0x04);
            if (!drive_.take_disk_changed()) {
                res |= 0x04;
            }
            return res;
        }
        case 0x3FFE:
            return 0xFF;  // unused (PhilipsFDC.cc:95-97)
        case 0x3FFF: {
            // ACTIVE-LOW: value starts 0xFF, IRQ clears bit6, DTRQ clears bit7
            // (PhilipsFDC.cc:42-55). NOT the fact-sheet's inferred active-high map.
            std::uint8_t value = 0xFF;
            if (fdc_.intrq()) {
                value &= ~0x40;
            }
            if (fdc_.drq()) {
                value &= ~0x80;
            }
            return value;
        }
        default:
            return rom_.mem_read(address);  // DISK ROM elsewhere in page 1
    }
}

void SonyFdc::mem_write(const core::BusAddress address, const core::BusData value) {
    switch (address & 0x3FFF) {
        case 0x3FF8:
            fdc_.write_command(value);
            break;
        case 0x3FF9:
            fdc_.write_track(value);
            break;
        case 0x3FFA:
            fdc_.write_sector(value);
            break;
        case 0x3FFB:
            fdc_.write_data(value);
            break;
        case 0x3FFC:
            side_reg_ = value;
            drive_.set_side(value & 1u);  // PhilipsFDC.cc:145-149
            break;
        case 0x3FFD:
            write_drive_register(value);  // PhilipsFDC.cc:151-172
            break;
        default:
            break;  // DISK ROM is read-only; register-window writes above
    }
}

void SonyFdc::write_drive_register(const std::uint8_t value) {
    drive_reg_ = value;
    // Drive select bits1..0: 00/10 -> drive A (present), 01 -> drive B, 11 -> NONE
    // (PhilipsFDC.cc:158-169). Single physical drive: only A is present/ready.
    const std::uint8_t sel = value & 0x03;
    const bool drive_a = (sel == 0x00) || (sel == 0x02);
    drive_.set_available(drive_a);
    // bit7 = motor on (delayed motor-off timer runs off emulated cycles).
    drive_.set_motor((value & 0x80) != 0, clock_.cpu_cycles());
}

}  // namespace sony_msx::devices::fdc
