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

// `Ascii8kB` MVP mapper type (M19-S2), grounds
// references/openmsx-21.0/src/memory/RomAscii8kB.cc (header comment lines
// 1-10 + code lines 18-52; behaviour reference only, never copied -- GPL
// isolation). Used by many Japanese-only cartridges (Valis, Dragon Slayer,
// Outrun, ...).
//
// Load-time validation (A-M19-7): image size must be a positive multiple of
// 0x2000 (8 KB).
//
// reset(): window-slots 0,1 unmapped; slots 2,3,4,5 ALL = image bank 0;
// slots 6,7 unmapped (RomAscii8kB.cc:24-33).
// mem_write: only when 0x6000 <= addr < 0x8000; region = ((addr>>11)&3)+2
// (0x6000-67FF->2, 0x6800-6FFF->3, 0x7000-77FF->4, 0x7800-7FFF->5),
// RomAscii8kB.cc:35-41.
class CartridgeAscii8kbRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()).
    explicit CartridgeAscii8kbRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Ascii8kB;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }

private:
    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
