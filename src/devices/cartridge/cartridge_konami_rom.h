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

#pragma once

#include <cstddef>
#include <vector>

#include "devices/cartridge/cartridge_mapper_device.h"
#include "devices/cartridge/cartridge_rom_window.h"

namespace sony_msx::devices::cartridge {

// `Konami` mapper type, NO SCC. Grounded in
// openMSX 21.0: src/memory/RomKonami.cc (behaviour reference only,
// never copied -- GPL isolation). The single most common real-world MSX
// MegaROM scheme, used by many classic Konami cartridge titles (including
// a stealth-action cartridge title). The SCC-chip-bearing sibling is CartridgeKonamiScc
// (cartridge_konami_scc_rom.h).
//
// Construction: set_block_mask(31) -- "Konami mapper is 256kB in size, even
// if ROM is smaller" (RomKonami.cc:24). An image exceeding 256 KB is loaded
// anyway (never rejected, matching openMSX's own non-fatal warning,
// RomKonami.cc:27-31) -- and since the mask is only a FALLBACK for
// out-of-range requests, every byte-value bank request 0..255
// against a >256KB image is used UNMASKED (the mask never engages).
// Deliberately does NOT call reset() in the constructor, matching
// RomKonami.cc:33-35 exactly ("Do not call reset() here... there will be a
// reset() at power up anyway"); CartridgeSlot::load() calls the
// newly constructed mapper's reset() explicitly before installing it as
// active -- a documented no-op-until-loaded state, not a bug.
//
// reset(): bank_switch(2,0); bank_switch(3,1); bank_switch(4,2);
// bank_switch(5,3) (RomKonami.cc:54-59).
// bank_switch(page, block): window.set_bank(page, block); if page==2 or 3,
// ALSO window.set_bank(page-2, block) (mirrors into window-slots 0/1); if
// page==4 or 5, ALSO window.set_bank(page+2, block) (mirrors into
// window-slots 6/7) -- RomKonami.cc:38-52.
// mem_write: only when 0x6000 <= addr < 0xC000; bank_switch(addr>>13, val)
// (RomKonami.cc:61-67). QUIRK (deliberately preserved): slot 2
// (0x4000-0x5FFF) is never a write target -- writes only trigger at addr >=
// 0x6000 ("[0x4000..0x6000) is fixed at segment 0", RomKonami.cc:63) -- so
// bank_switch(2,...) only ever runs from reset(), leaving slots 0 and 2
// (mirrored together via the page-2=0 branch) permanently fixed at bank 0
// for the session. Slot 1, however, is mirrored by bank_switch(3,...)
// (page-2=1), which IS re-invoked on every page-3 write (0x6000-0x7FFF) --
// so slot 1 tracks slot 3's LIVE value, same as slot 6/4 and slot 7/5.
class CartridgeKonamiRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()). Does NOT call reset()
    // (see class comment) -- the caller (CartridgeSlot::load) must reset()
    // before relying on any documented bank layout.
    explicit CartridgeKonamiRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Konami;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    // Debug-snapshot seam: exposes the bank window for generic bank-state dumps.
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    void bank_switch(int page, unsigned block);

    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
