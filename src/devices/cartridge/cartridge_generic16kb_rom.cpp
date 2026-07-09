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

#include "devices/cartridge/cartridge_generic16kb_rom.h"

namespace sony_msx::devices::cartridge {

namespace {
constexpr std::uint32_t k16kBankSize = 0x4000;
}  // namespace

bool CartridgeGeneric16kbRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && (size % k16kBankSize) == 0;
}

CartridgeGeneric16kbRom::CartridgeGeneric16kbRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    reset();
}

void CartridgeGeneric16kbRom::set_logical_bank(const int bank, const unsigned block16k) {
    const int low_slot = 2 * bank;
    const int high_slot = 2 * bank + 1;
    window_.set_bank(low_slot, 2 * block16k);
    window_.set_bank(high_slot, 2 * block16k + 1);
}

void CartridgeGeneric16kbRom::reset() {
    // RomGeneric16kB.cc:12-18: logical bank 0 unmapped; bank1 = image bank0;
    // bank2 = image bank1; bank3 unmapped.
    window_.set_unmapped(0);
    window_.set_unmapped(1);
    set_logical_bank(1, 0);
    set_logical_bank(2, 1);
    window_.set_unmapped(6);
    window_.set_unmapped(7);
}

core::BusData CartridgeGeneric16kbRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeGeneric16kbRom::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomGeneric16kB.cc:20-23: bank = addr >> 14 (0-3).
    const int bank = (address >> 14) & 0x03;
    set_logical_bank(bank, value);
}

}  // namespace sony_msx::devices::cartridge
