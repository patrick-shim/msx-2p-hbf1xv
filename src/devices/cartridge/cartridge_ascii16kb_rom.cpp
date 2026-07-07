#include "devices/cartridge/cartridge_ascii16kb_rom.h"

namespace sony_msx::devices::cartridge {

namespace {
constexpr std::uint32_t k16kBankSize = 0x4000;
}  // namespace

bool CartridgeAscii16kbRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && (size % k16kBankSize) == 0;
}

CartridgeAscii16kbRom::CartridgeAscii16kbRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    reset();
}

void CartridgeAscii16kbRom::set_logical_bank(const int bank, const unsigned block16k) {
    const int low_slot = 2 * bank;
    const int high_slot = 2 * bank + 1;
    window_.set_bank(low_slot, 2 * block16k);
    window_.set_bank(high_slot, 2 * block16k + 1);
}

void CartridgeAscii16kbRom::reset() {
    // RomAscii16kB.cc:24-27. Both middle banks start at the SAME image bank
    // (0), not sequential 0/1 -- a genuine quirk of the real hardware/openMSX
    // model, not a bug to "fix" (R-M19-2).
    window_.set_unmapped(0);
    window_.set_unmapped(1);
    set_logical_bank(1, 0);
    set_logical_bank(2, 0);
    window_.set_unmapped(6);
    window_.set_unmapped(7);
}

core::BusData CartridgeAscii16kbRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeAscii16kbRom::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomAscii16kB.cc:30-36: only 0x6000 <= addr < 0x7800 AND bit 0x0800 clear
    // (excludes 0x6800-6FFF and 0x7800-7FFF).
    if (address >= 0x6000 && address < 0x7800 && (address & 0x0800) == 0) {
        const int bank = ((address >> 12) & 0x01) + 1;
        set_logical_bank(bank, value);
    }
}

}  // namespace sony_msx::devices::cartridge
