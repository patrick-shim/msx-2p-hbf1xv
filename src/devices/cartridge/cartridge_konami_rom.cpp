#include "devices/cartridge/cartridge_konami_rom.h"

namespace sony_msx::devices::cartridge {

bool CartridgeKonamiRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && (size % CartridgeRomWindow::kBankSize) == 0;
}

CartridgeKonamiRom::CartridgeKonamiRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    // RomKonami.cc:24: Konami mapper is 256kB (32 x 8kB banks) even if the
    // loaded ROM is smaller -- overrides the shared window's default mask.
    window_.set_block_mask(31);
    // Deliberately NO reset() call here (RomKonami.cc:33-35, class doc).
}

void CartridgeKonamiRom::bank_switch(const int page, const unsigned block) {
    window_.set_bank(page, block);
    // RomKonami.cc:42-51: mirror behaviour.
    if (page == 2 || page == 3) {
        window_.set_bank(page - 2, block);
    } else if (page == 4 || page == 5) {
        window_.set_bank(page + 2, block);
    }
}

void CartridgeKonamiRom::reset() {
    // RomKonami.cc:54-59.
    bank_switch(2, 0);
    bank_switch(3, 1);
    bank_switch(4, 2);
    bank_switch(5, 3);
}

core::BusData CartridgeKonamiRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeKonamiRom::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomKonami.cc:61-67: [0x4000..0x6000) is fixed at segment 0 -- writes
    // only trigger at addr >= 0x6000 (page>>13 in {3,4,5} only), so page 2 is
    // NEVER passed to bank_switch() again after reset(); window-slot 2 (and,
    // via the mirror, window-slot 0) is therefore permanently fixed at bank 0
    // for the whole session (R-M19-3, corrected). Window-slot 1 is NOT
    // fixed: it mirrors window-slot 3's LIVE value on every write to page 3
    // (0x6000-0x7FFF), since bank_switch(3, block) always re-applies the
    // page==3 mirror branch too -- see the class-level doc comment in
    // cartridge_konami_rom.h for the full derivation.
    if (address >= 0x6000 && address < 0xC000) {
        bank_switch((address >> 13) & 0x07, value);
    }
}

}  // namespace sony_msx::devices::cartridge
