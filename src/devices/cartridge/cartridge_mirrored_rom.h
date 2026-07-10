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

// `Mirrored` MVP mapper type (M19-S2), grounds the MIRRORED case of
// references/openmsx-21.0/src/memory/RomPlain.cc (behaviour reference only,
// never copied -- GPL isolation).
//
// Load-time validation (A-M19-7, a deliberate STRICTER divergence from
// openMSX's own short-image 0xFF-padding, RomBlocks.cc:28-39): the image size
// must be a positive multiple of 0x2000 (8 KB) and <= 0x10000 (64 KB) --
// RomPlain.cc:39-43's own check ("must be smaller than or equal to 64kB and
// must be a multiple of 8kB"). An invalid size is REJECTED, never
// padded/truncated/fabricated.
//
// Placement (A-M19-8, RomPlain's guessLocation heuristic is deliberately NOT
// needed/ported: this machine's external cartridge slots have no narrower
// `<mem>` sub-window to place inside -- the whole 64 KB window IS the slot):
// window-slot `s` (0-7) reads image bank `s mod nrBlocks` -- a full,
// unconditional mirror across the whole 64 KB window. No bank-switch
// registers: writes are ignored (read-only cartridge, matching a plain mask
// ROM).
class CartridgeMirroredRom final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()).
    explicit CartridgeMirroredRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::Mirrored;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    // M36 Phase 3 snapshot: generic bank-state dump seam (planner §2.4 item 13).
    [[nodiscard]] const CartridgeRomWindow* rom_window() const override { return &window_; }

private:
    CartridgeRomWindow window_;
};

}  // namespace sony_msx::devices::cartridge
