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

// `Generic16kB` MVP mapper type (M19-S2), grounds
// references/openmsx-21.0/src/memory/RomGeneric16kB.cc:6-24 (behaviour
// reference only, never copied -- GPL isolation).
//
// Composes the SAME shared 8 KB-granularity CartridgeRomWindow as every other
// MVP type (planner §2.1/§2.2): 4 LOGICAL 16 KB banks map onto window-slot
// PAIRS {0,1}, {2,3}, {4,5}, {6,7} (window-slot 2n = low half, 2n+1 = high
// half of logical bank n).
//
// Load-time validation (A-M19-7): image size must be a positive multiple of
// 0x4000 (16 KB).
//
// reset(): logical bank 0 unmapped; bank 1 = image bank 0; bank 2 = image
// bank 1; bank 3 unmapped (RomGeneric16kB.cc:12-18).
// mem_write: bank = addr >> 14 (0-3); sets that logical bank's two
// window-slots to image banks 2*val / 2*val+1 (RomGeneric16kB.cc:20-23).
class CartridgeGeneric16kbRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()).
    explicit CartridgeGeneric16kbRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Generic16kB;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    // M36 Phase 3 snapshot: generic bank-state dump seam (planner §2.4 item 13).
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    void set_logical_bank(int bank, unsigned block16k);

    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
