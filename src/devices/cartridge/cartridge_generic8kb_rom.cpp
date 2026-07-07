#include "devices/cartridge/cartridge_generic8kb_rom.h"

namespace sony_msx::devices::cartridge {

bool CartridgeGeneric8kbRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && (size % CartridgeRomWindow::kBankSize) == 0;
}

CartridgeGeneric8kbRom::CartridgeGeneric8kbRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    reset();
}

void CartridgeGeneric8kbRom::reset() {
    // RomGeneric8kB.cc:13-22.
    window_.set_unmapped(0);
    window_.set_unmapped(1);
    for (int i = 2; i < 6; ++i) {
        window_.set_bank(i, static_cast<unsigned>(i - 2));
    }
    window_.set_unmapped(6);
    window_.set_unmapped(7);
}

core::BusData CartridgeGeneric8kbRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeGeneric8kbRom::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomGeneric8kB.cc:24-27: writable at ANY address; slot = addr >> 13.
    const int slot = (address >> 13) & 0x07;
    window_.set_bank(slot, value);
}

}  // namespace sony_msx::devices::cartridge
