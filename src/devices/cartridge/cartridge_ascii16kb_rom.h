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

// `Ascii16kB` mapper type. Grounded in
// openMSX 21.0: src/memory/RomAscii16kB.cc (header comment + code
// lines 16-45; behaviour reference only, never copied -- GPL isolation).
// Used by many 16 KB-bank cartridge titles.
//
// Composes the shared 8 KB-granularity CartridgeRomWindow via logical 16 KB
// bank window-slot PAIRS, same convention as Generic16kB.
//
// Load-time validation: image size must be a positive multiple of
// 0x4000 (16 KB).
//
// reset(): bank 0 unmapped; bank 1 = image bank 0; bank 2 = image bank 0
// (CRITICAL QUIRK, deliberately preserved, not "fixed": BOTH middle banks
// start IDENTICAL, not sequential 0/1 -- RomAscii16kB.cc:24-27); bank 3
// unmapped.
// mem_write: only when 0x6000 <= addr < 0x7800 AND (addr & 0x0800) == 0
// (excludes 0x6800-6FFF and 0x7800-7FFF); region = ((addr>>12)&1)+1
// (0x6xxx->1, 0x7xxx->2); sets that logical bank's two window-slots to
// 2*val / 2*val+1 (RomAscii16kB.cc:30-36).
class CartridgeAscii16kbRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()).
    explicit CartridgeAscii16kbRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Ascii16kB;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    // Debug-snapshot seam: exposes the bank window for generic bank-state dumps.
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    void set_logical_bank(int bank, unsigned block16k);

    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
