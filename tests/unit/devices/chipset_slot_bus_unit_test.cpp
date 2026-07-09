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

#include <cstdint>
#include <iostream>
#include <unordered_map>

#include "core/device_contracts.h"
#include "devices/chipset/slot_bus.h"

// Suite: Devices_ChipsetSlotBus_Unit
//
// M11-S2: primary (#A8) + secondary (#FFFF) slot decode over MemoryDevices.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// A tagged memory device: returns (tag | low-8-bits-of-address) on read so the
// resolved device AND the address it received are both observable; captures the
// last write.
class TagDevice final : public sony_msx::core::MemoryDevice {
public:
    explicit TagDevice(std::uint8_t tag) : tag_(tag) {}

    sony_msx::core::BusData mem_read(const sony_msx::core::BusAddress address) override {
        last_read_address = address;
        return static_cast<std::uint8_t>(tag_ | (address & 0x0F));
    }
    void mem_write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        store[address] = value;
        last_write_address = address;
        last_write_value = value;
    }

    std::uint8_t tag_;
    std::unordered_map<std::uint16_t, std::uint8_t> store;
    std::uint16_t last_read_address = 0xFFFF;
    std::uint16_t last_write_address = 0xFFFF;
    std::uint8_t last_write_value = 0x00;
};

}  // namespace

int main() {
    using sony_msx::devices::chipset::SlotBus;

    // --- #A8 readback equals last write; each page routes to its primary slot. ---
    {
        SlotBus bus;
        TagDevice slot0(0x10);
        TagDevice slot1(0x20);
        // Attach slot0 across pages 0-3 (sub 0), slot1 across pages 0-3 (sub 0).
        for (int page = 0; page < 4; ++page) {
            bus.attach(0, 0, page, &slot0);
            bus.attach(1, 0, page, &slot1);
        }
        // #A8 = 0b01_00_01_00: page0->slot0, page1->slot1, page2->slot0, page3->slot1.
        bus.set_primary_select(0b01000100);
        if (!expect_true(bus.primary_select() == 0b01000100, "PrimarySelect_Readback_EqualsLastWrite")) {
            return 1;
        }
        // page0 (0x0000) -> slot0 tag 0x10.
        if (!expect_true((bus.read(0x0000) & 0xF0) == 0x10, "Page0_RoutesToSelectedPrimarySlot0")) {
            return 1;
        }
        // page1 (0x4000) -> slot1 tag 0x20.
        if (!expect_true((bus.read(0x4000) & 0xF0) == 0x20, "Page1_RoutesToSelectedPrimarySlot1")) {
            return 1;
        }
        // page3 (0xC000) -> slot1.
        if (!expect_true((bus.read(0xD005) & 0xFF) == 0x25, "Page3_RoutesToSlot1_WithAddressOffset")) {
            return 1;
        }
        // Write routes to the resolved device with the full CPU address.
        bus.write(0x4002, 0x99);
        if (!expect_true(slot1.last_write_address == 0x4002 && slot1.last_write_value == 0x99,
                         "Write_RoutesToResolvedDevice_FullAddress")) {
            return 1;
        }
    }

    // --- Unmapped (primary,sub,page) triple: read 0xFF, write ignored. ---
    {
        SlotBus bus;
        bus.set_primary_select(0x00);  // all pages -> slot 0 (nothing attached)
        if (!expect_true(bus.read(0x1234) == 0xFF, "UnmappedTriple_Read_ReturnsOpenBus")) {
            return 1;
        }
        bus.write(0x1234, 0x55);  // must not crash / no effect
        if (!expect_true(bus.read(0x1234) == 0xFF, "UnmappedTriple_Write_Ignored")) {
            return 1;
        }
    }

    // --- Expanded page-3 slot: #FFFF sets sub-slot, reads back 0xFF ^ reg. ---
    {
        SlotBus bus;
        TagDevice s3s0(0x30);  // slot 3-0
        TagDevice s3s1(0x40);  // slot 3-1
        for (int page = 0; page < 4; ++page) {
            bus.attach(3, 0, page, &s3s0);
            bus.attach(3, 1, page, &s3s1);
        }
        bus.set_expanded(3, true);
        bus.set_primary_select(0xFF);  // all pages -> primary slot 3

        // Sub-slot register starts 0 -> all pages sub 0 -> s3s0.
        if (!expect_true((bus.read(0x0000) & 0xF0) == 0x30, "ExpandedSlot3_DefaultSub0_RoutesToSub0")) {
            return 1;
        }
        // Write #FFFF = 0b11_11_11_11 -> every page sub 3 (empty) ; readback 0xFF^0xFF=0x00.
        bus.write(0xFFFF, 0xFF);
        if (!expect_true(bus.read(0xFFFF) == 0x00, "Ffff_Readback_IsOnesComplementOfSubReg")) {
            return 1;
        }
        // Set page0 -> sub 1 (bits0-1 = 01), other pages sub 0.
        bus.write(0xFFFF, 0b00000001);
        if (!expect_true(bus.sub_slot_register(3) == 0b00000001, "Ffff_Write_StoresSubSlotRegister")) {
            return 1;
        }
        if (!expect_true((bus.read(0x0000) & 0xF0) == 0x40, "ExpandedSlot3_Page0Sub1_RoutesToSub1")) {
            return 1;
        }
        // page1 still sub 0 -> s3s0.
        if (!expect_true((bus.read(0x4000) & 0xF0) == 0x30, "ExpandedSlot3_Page1Sub0_RoutesToSub0")) {
            return 1;
        }
        if (!expect_true(bus.read(0xFFFF) == static_cast<std::uint8_t>(0xFF ^ 0x01),
                         "Ffff_Readback_ReflectsUpdatedReg")) {
            return 1;
        }
    }

    // --- Non-expanded page-3 slot: #FFFF is ordinary memory in the device. ---
    {
        SlotBus bus;
        TagDevice s0(0x50);
        for (int page = 0; page < 4; ++page) {
            bus.attach(0, 0, page, &s0);
        }
        bus.set_primary_select(0x00);  // page3 -> slot 0, not expanded
        bus.write(0xFFFF, 0xAB);
        if (!expect_true(s0.store[0xFFFF] == 0xAB && s0.last_write_address == 0xFFFF,
                         "NonExpandedFfff_Write_IsNormalMemory")) {
            return 1;
        }
        // Read #FFFF returns device value (tag|0x0F = 0x5F), NOT a 0xFF^reg readback.
        if (!expect_true((bus.read(0xFFFF) & 0xFF) == 0x5F, "NonExpandedFfff_Read_IsNormalMemory")) {
            return 1;
        }
    }

    return 0;
}
