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

#include "devices/cartridge/cartridge_fmpac_rom.h"

#include <utility>

namespace sony_msx::devices::cartridge {

bool CartridgeFmPacRom::is_valid_image_size(const std::size_t size) {
    // 1..4 whole 16 KB banks. The real FM-PAC BIOS is exactly one 16 KB bank;
    // 64 KB 4-bank variants exist (MSXFmPac.cc:53-54 rom[bank*0x4000 + ...]).
    return size >= kBankSize && (size % kBankSize) == 0 && size <= 4 * kBankSize;
}

CartridgeFmPacRom::CartridgeFmPacRom(std::vector<std::uint8_t> image) : rom_(std::move(image)) {
    num_banks_ = rom_.empty() ? 1 : (rom_.size() / kBankSize);
    if (num_banks_ == 0) {
        num_banks_ = 1;
    }
    // Deliberately NO reset() here (M19 convention): CartridgeSlot::load()
    // resets once before installing.
}

void CartridgeFmPacRom::reset() {
    // MSXFmPac.cc:17-25 reset(): enable=0, sramEnabled=false, bank=0,
    // r1ffe=r1fff=0 (any non-magic value works). The battery SRAM is NOT
    // cleared -- it survives a power cycle (that is the whole point of a
    // battery-backed store; a fresh cold boot loads it from the .sram file).
    enable_ = 0;
    sram_enabled_ = false;
    bank_ = 0;
    r1ffe_ = 0;
    r1fff_ = 0;
    opll_.reset();
}

std::uint8_t CartridgeFmPacRom::rom_read(const std::uint16_t rel) const {
    // Bank masked against the ACTUAL bank count so a bank>available never reads
    // past a small image (planner §2.1). rel is already < 0x4000.
    const std::size_t index = (static_cast<std::size_t>(bank_ % num_banks_) * kBankSize) + rel;
    if (index >= rom_.size()) {
        return 0xFF;
    }
    return rom_[index];
}

core::BusData CartridgeFmPacRom::mem_read(const core::BusAddress address) {
    // PAGE-1 device only: open bus on pages 0/2/3 (a bare FM-PAC answers only
    // 0x4000-0x7FFF). This is what keeps the slot-3-0 memory-mapper reachable
    // on the other pages and coexists with #A8 primary-slot decode (AC-d4).
    if (address < 0x4000 || address >= 0x8000) {
        return 0xFF;
    }
    const std::uint16_t rel = static_cast<std::uint16_t>(address & 0x3FFF);
    switch (rel) {
        case 0x3FF6:
            return enable_;
        case 0x3FF7:
            return bank_;
        default:
            if (sram_enabled_) {
                if (rel < kSramWindow) {
                    return sram_.read(rel);
                }
                if (rel == 0x1FFE) {
                    return r1ffe_;  // always 0x4D while unlocked
                }
                if (rel == 0x1FFF) {
                    return r1fff_;  // always 0x69 while unlocked
                }
                return 0xFF;
            }
            return rom_read(rel);
    }
}

void CartridgeFmPacRom::mem_write(const core::BusAddress address, const core::BusData value) {
    if (address < 0x4000 || address >= 0x8000) {
        return;  // page-1 device: writes elsewhere are ignored
    }
    const std::uint16_t rel = static_cast<std::uint16_t>(address & 0x3FFF);
    const std::uint8_t v = static_cast<std::uint8_t>(value);
    switch (rel) {
        case 0x1FFE:
            // 'enable' bit4 gates the magic latches (MSXFmPac.cc:84-89): while
            // the force-reset bit is set the magic registers cannot be armed.
            if ((enable_ & 0x10) == 0) {
                r1ffe_ = v;
                check_sram_enable();
            }
            break;
        case 0x1FFF:
            if ((enable_ & 0x10) == 0) {
                r1fff_ = v;
                check_sram_enable();
            }
            break;
        case 0x3FF4:  // OPLL address port (memory-mapped)
            opll_.write_address(v);
            break;
        case 0x3FF5:  // OPLL data port (memory-mapped)
            opll_.write_data(v);
            break;
        case 0x3FF6:
            // MSXFmPac.cc:100-106. bit0 = I/O-port enable, bit4 = force-reset
            // of the magic latches. Setting bit4 clears r1ffe/r1fff (re-locking
            // the SRAM).
            enable_ = static_cast<std::uint8_t>(v & 0x11);
            if (enable_ & 0x10) {
                r1ffe_ = 0;
                r1fff_ = 0;
                check_sram_enable();
            }
            break;
        case 0x3FF7:
            // 16 KB ROM bank select (MSXFmPac.cc:107-113). 'enable' has no
            // effect on memory-mapped access.
            bank_ = static_cast<std::uint8_t>(v & 0x03);
            break;
        default:
            // SRAM byte write, ONLY while unlocked and within the addressable
            // 8190-byte window (MSXFmPac.cc:114-117). 'enable' does not gate
            // memory-mapped SRAM writes.
            if (sram_enabled_ && rel < kSramWindow) {
                sram_.write(rel, v);
            }
            break;
    }
}

void CartridgeFmPacRom::check_sram_enable() {
    // MSXFmPac.cc:137-144: both magic bytes required (AND, not either-or).
    sram_enabled_ = (r1ffe_ == 0x4D) && (r1fff_ == 0x69);
}

}  // namespace sony_msx::devices::cartridge
