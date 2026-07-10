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
#include <cstdint>
#include <utility>
#include <vector>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::memory {

// A read-only ROM window on the CPU memory map (M13-S1).
//
// A RomDevice presents a byte image at a fixed placement inside its (sub)slot's
// 64 KB view, described by the XML `<mem base=.. size=..>` for the device
// (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml). The slot fabric
// (chipset::SlotBus) resolves which (primary, sub, page) cell answers an access
// and hands the device the full 16-bit CPU address; the device maps that to its
// image offset `address - base` when the address is inside [base, base + size),
// and returns open-bus 0xFF outside that window. Writes are ignored (ROM).
//
// The window model also expresses the WD2793 DISK ROM `rom_visibility`
// (Sony_HB-F1XV.xml:174-175: base 0x4000 size 0x4000, "ROM only visible in page
// 1", "no mirroring"): the device window is exactly page 1, so a read outside
// page 1 returns 0xFF by construction, even though the fabric only routes the
// page-1 cell to this device.
//
// Behaviour reference (read only, never copied — GPL isolation, guardrails):
// references/openmsx-21.0/src/memory/Rom.cc / RomBlocks.cc — direct image
// index reads, unmapped regions reading the bus, writes ignored (inherent to
// a mask-ROM device).
class RomDevice final : public core::MemoryDevice {
public:
    static constexpr core::BusData kOpenBus = 0xFF;

    // Construct a window at [base, base + size) with a 0xFF-filled image.
    RomDevice(std::uint16_t base, std::uint32_t size);

    // Construct a window at [base, base + size) over `image`; `image.size()`
    // must equal `size` (the caller — the machine-side asset loader — guarantees
    // this and applies the missing-asset 0xFF-fill policy). If the sizes differ
    // the image is resized to `size` (truncated or 0xFF-padded) so the device is
    // always deterministic.
    RomDevice(std::uint16_t base, std::uint32_t size, std::vector<std::uint8_t> image);

    // Replace the backing image (e.g. re-load assets at cold_boot). The image is
    // normalized to the configured window size (truncate / 0xFF-pad).
    void set_image(std::vector<std::uint8_t> image);

    [[nodiscard]] std::uint16_t base() const { return base_; }
    [[nodiscard]] std::uint32_t size() const { return size_; }
    [[nodiscard]] const std::vector<std::uint8_t>& image() const { return image_; }

    // MemoryDevice contract.
    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // True when `address` falls inside the ROM window [base, base + size).
    [[nodiscard]] bool in_window(core::BusAddress address) const;

private:
    void normalize_image();

    std::uint16_t base_;
    std::uint32_t size_;
    std::vector<std::uint8_t> image_;
};

}  // namespace sony_msx::devices::memory
