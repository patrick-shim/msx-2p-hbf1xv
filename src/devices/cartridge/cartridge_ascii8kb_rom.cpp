#include "devices/cartridge/cartridge_ascii8kb_rom.h"

namespace sony_msx::devices::cartridge {

bool CartridgeAscii8kbRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && (size % CartridgeRomWindow::kBankSize) == 0;
}

CartridgeAscii8kbRom::CartridgeAscii8kbRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    reset();
}

void CartridgeAscii8kbRom::reset() {
    // RomAscii8kB.cc:24-33.
    window_.set_unmapped(0);
    window_.set_unmapped(1);
    for (int i = 2; i < 6; ++i) {
        window_.set_bank(i, 0);
    }
    window_.set_unmapped(6);
    window_.set_unmapped(7);
}

core::BusData CartridgeAscii8kbRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeAscii8kbRom::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomAscii8kB.cc:35-41: only 0x6000 <= addr < 0x8000.
    if (address >= 0x6000 && address < 0x8000) {
        const int region = ((address >> 11) & 0x03) + 2;
        window_.set_bank(region, value);
    }
}

}  // namespace sony_msx::devices::cartridge
