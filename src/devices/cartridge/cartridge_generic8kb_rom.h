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

// `Generic8kB` mapper type. Grounded in
// openMSX 21.0: src/memory/RomGeneric8kB.cc:7-36 (behaviour
// reference only, never copied -- GPL isolation). openMSX's own label: "a
// generic ROM type that does not exist in real ROMs" (RomInfo.cc:17-19).
//
// Load-time validation: image size must be a positive multiple of
// 0x2000 (8 KB).
//
// reset(): window-slots 0,1 unmapped; slots 2,3,4,5 = image banks 0,1,2,3;
// slots 6,7 unmapped (RomGeneric8kB.cc:13-22).
// mem_write: writable at ANY address -- slot = addr >> 13 (0-7); the full
// byte value is the requested bank index (RomGeneric8kB.cc:24-27).
class CartridgeGeneric8kbRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()).
    explicit CartridgeGeneric8kbRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Generic8kB;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    // Debug-snapshot seam: exposes the bank window for generic bank-state dumps.
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
