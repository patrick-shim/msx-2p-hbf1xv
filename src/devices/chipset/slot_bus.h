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

#include <array>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::chipset {

// Primary/secondary slot-decode fabric for the MSX memory map (M11-S2).
//
// Resolves a CPU memory access to the MemoryDevice occupying the selected
// (primary slot, sub-slot, page) and forwards it, or returns open-bus 0xFF when
// no device is attached. Two orthogonal selection mechanisms, per the S1985
// fact-sheet §3 and openMSX references/openmsx-21.0/src/cpu/MSXCPUInterface.cc
// (behaviour reference — read only, never copied here):
//
//   - Primary select: PPI port #A8, 2 bits per 16 KB page (bits 0-1 page0 ...
//     bits 6-7 page3). set_primary_select / primary_select.
//   - Secondary (sub-slot) select: memory-mapped #FFFF, present only when the
//     page-3 primary slot is *expanded*. Write sets that slot's sub-slot
//     register (2 bits per page); read returns 0xFF ^ subSlotRegister
//     (MSXCPUInterface.cc:209-210,769-770). When the page-3 primary slot is not
//     expanded, #FFFF is ordinary memory in the mapped device.
//
// HB-F1XV: only primary slot 3 is expanded; main RAM is the MemoryDevice at
// slot 3-0, pages 0-3 (fact-sheet §9). ROM/mapper population arrives in M12.
class SlotBus {
public:
    static constexpr int kSlots = 4;
    static constexpr int kPages = 4;

    // Clear the volatile selection state (primary + sub-slot registers) for a
    // cold boot. Static wiring (attached devices, expanded flags) is preserved.
    void reset();

    // Attach a MemoryDevice to a (primary, sub, page) cell. Nullptr detaches.
    void attach(int primary, int sub, int page, core::MemoryDevice* device);

    // Mark a primary slot as expanded (has sub-slots + an #FFFF register).
    void set_expanded(int primary, bool expanded);
    [[nodiscard]] bool is_expanded(int primary) const;

    // PPI #A8 primary-select byte (2 bits/page). Read/write.
    void set_primary_select(std::uint8_t value);
    [[nodiscard]] std::uint8_t primary_select() const;

    // Memory access with full slot resolution (incl. the #FFFF special case).
    [[nodiscard]] core::BusData read(core::BusAddress address);
    void write(core::BusAddress address, core::BusData value);

    // Direct #FFFF sub-slot register access for the page-3 primary slot when it
    // is expanded. Exposed for tests/wiring; read()/write() call these for
    // address 0xFFFF. When the slot is not expanded these fall through to normal
    // memory (handled inside read/write).
    void write_ffff(core::BusData value);
    [[nodiscard]] core::BusData read_ffff() const;

    // Sub-slot register value for a primary slot (2 bits/page). Test/debug view.
    [[nodiscard]] std::uint8_t sub_slot_register(int primary) const;

private:
    [[nodiscard]] int primary_for_page(int page) const;
    [[nodiscard]] int sub_for_page(int primary, int page) const;

    std::array<std::array<std::array<core::MemoryDevice*, kPages>, kSlots>, kSlots> devices_{};
    std::array<std::uint8_t, kSlots> sub_slot_register_{};
    std::array<bool, kSlots> expanded_{};
    std::uint8_t primary_select_ = 0;
};

}  // namespace sony_msx::devices::chipset
