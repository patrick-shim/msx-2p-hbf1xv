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

#include "devices/kanji/kanji_font_rom.h"

#include <utility>

namespace sony_msx::devices::kanji {

KanjiFontRom::KanjiFontRom() : image_(kImageSize, 0xFF) {}

void KanjiFontRom::reset() {
    adr1_ = kResetAdr1;
    adr2_ = kResetAdr2;
}

void KanjiFontRom::set_image(std::vector<std::uint8_t> image) {
    image.resize(kImageSize, 0xFF);
    image_ = std::move(image);
}

core::BusData KanjiFontRom::io_read(const core::BusAddress port) {
    switch (port & 0x03) {
    case 1: {  // #D9 -- JIS1 data read + auto-increment (MSXKanji.cc:59-60,79)
        const core::BusData result = image_[adr1_ & (kImageSize - 1)];
        adr1_ = (adr1_ & ~0x1Fu) | ((adr1_ + 1) & 0x1Fu);
        return result;
    }
    case 3: {  // #DB -- JIS2 data read + auto-increment (MSXKanji.cc:62-63,82-84)
        const core::BusData result = image_[adr2_ & (kImageSize - 1)];
        adr2_ = (adr2_ & ~0x1Fu) | ((adr2_ + 1) & 0x1Fu);
        return result;
    }
    default:  // #D8, #DA -- open-bus, no side effect (MSXKanji.cc:54-58,73-76)
        return 0xFF;
    }
}

void KanjiFontRom::io_write(const core::BusAddress port, const core::BusData value) {
    const auto masked = static_cast<std::uint32_t>(value) & 0x3Fu;
    switch (port & 0x03) {
    case 0:  // #D8 -- adr1_ low-address byte (bits 5-10)
        adr1_ = (adr1_ & 0x1F800u) | (masked << 5);
        break;
    case 1:  // #D9 -- adr1_ high-address byte (bits 11-16)
        adr1_ = (adr1_ & 0x007E0u) | (masked << 11);
        break;
    case 2:  // #DA -- adr2_ low-address byte (bits 5-10)
        adr2_ = (adr2_ & 0x3F800u) | (masked << 5);
        break;
    case 3:  // #DB -- adr2_ high-address byte (bits 11-16); mask 0x207E0
             // preserves bit 17 (the JIS2-half selector, 0x20000).
        adr2_ = (adr2_ & 0x207E0u) | (masked << 11);
        break;
    default:
        break;
    }
}

}  // namespace sony_msx::devices::kanji
