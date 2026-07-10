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
    // RomKonami.cc:61-67: [0x4000..0x6000) is fixed at bank 0 -- writes only
    // trigger at addr >= 0x6000, so slot 2 (and its mirror, slot 0) never
    // changes after reset(), while slot 1 tracks slot 3's live value via the
    // page-3 mirror branch (R-M19-3). Full derivation: class doc in
    // cartridge_konami_rom.h.
    if (address >= 0x6000 && address < 0xC000) {
        bank_switch((address >> 13) & 0x07, value);
    }
}

}  // namespace sony_msx::devices::cartridge
