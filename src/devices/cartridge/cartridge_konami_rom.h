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

// `Konami` MVP mapper type, NO SCC (M19-S2), grounds
// references/openmsx-21.0/src/memory/RomKonami.cc (behaviour reference only,
// never copied -- GPL isolation). The single most common real-world MSX
// MegaROM scheme (Nemesis, Penguin Adventure, Metal Gear, The Maze of
// Galious, ...). KonamiSCC (the SCC-chip-bearing sibling) is explicitly OUT
// of scope (backlog G1, a new audio device, not an incremental mapper).
//
// Construction: set_block_mask(31) -- "Konami mapper is 256kB in size, even
// if ROM is smaller" (RomKonami.cc:24). If the loaded image exceeds 256 KB
// this is loaded anyway (never rejected, matching openMSX's own non-fatal
// warning, RomKonami.cc:27-31) -- and because the mask is only a FALLBACK for
// out-of-range requests (A-M19-6), every byte-value bank request 0..255
// against a >256KB image is used UNMASKED (the mask never engages).
// Deliberately does NOT call reset() in the constructor, matching
// RomKonami.cc:33-35 exactly ("Do not call reset() here... there will be a
// reset() at power up anyway"); CartridgeSlot::load() (M19-S3) calls the
// newly constructed mapper's reset() explicitly before installing it as
// active, so this is a documented, deliberate no-op-until-loaded state, never
// an unresolved gap.
//
// reset(): bank_switch(2,0); bank_switch(3,1); bank_switch(4,2);
// bank_switch(5,3) (RomKonami.cc:54-59).
// bank_switch(page, block): window.set_bank(page, block); if page==2 or 3,
// ALSO window.set_bank(page-2, block) (mirrors into window-slots 0/1); if
// page==4 or 5, ALSO window.set_bank(page+2, block) (mirrors into
// window-slots 6/7) -- RomKonami.cc:38-52.
// mem_write: only when 0x6000 <= addr < 0xC000; bank_switch(addr>>13, val)
// (RomKonami.cc:61-67). CRITICAL QUIRK (deliberately preserved, corrected
// during M19 QA): window-slot 2 (0x4000-0x5FFF) is never a write target
// (writes only trigger at addr >= 0x6000, "[0x4000..0x6000) is fixed at
// segment 0", RomKonami.cc:63), so bank_switch(2,...) is ONLY ever invoked
// by reset() -- meaning window-slots 0 AND 2 (mirrored together via
// bank_switch(2,...)'s page-2=0 branch) are BOTH permanently fixed at bank 0
// for the entire session. Window-slot 1, however, is mirrored by
// bank_switch(3,...) (page-2=1), which IS re-invoked on every write to page
// 3 (addr 0x6000-0x7FFF) -- so slot 1 is NOT fixed; it tracks slot 3's LIVE
// value, exactly as slot 6 tracks slot 4 and slot 7 tracks slot 5 (R-M19-3).
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
    // M36 Phase 3 snapshot: generic bank-state dump seam (planner §2.4 item 13).
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    void bank_switch(int page, unsigned block);

    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
