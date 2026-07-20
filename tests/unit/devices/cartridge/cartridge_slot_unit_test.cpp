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
#include <vector>

#include "devices/cartridge/cartridge_slot.h"

// Suite: Devices_CartridgeSlot_Unit
//
// The ONE device actually attached to slot_bus_ for an external cartridge
// bay: empty-slot open-bus regression guard, load/unload/reset
// dispatch across all 6 MVP types, size-invalid-load-leaves-prior-state-
// untouched, and two-run determinism.

namespace {

using sony_msx::devices::cartridge::CartridgeLoadResult;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeSlot;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> make_marker_image(const unsigned num_banks, const std::uint32_t bank_size) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_banks) * bank_size);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < bank_size; ++i) {
            image[static_cast<std::size_t>(bank) * bank_size + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- Empty slot: byte-for-byte the pre-cartridge open-bus default (0xFF). ---
    {
        CartridgeSlot slot(1);
        expect(!slot.loaded(), "Construct_Empty_NotLoaded");
        expect(slot.mem_read(0x0000) == 0xFF, "Empty_MemRead_AnyAddress_OpenBus");
        expect(slot.mem_read(0x8000) == 0xFF, "Empty_MemRead_MidAddress_OpenBus");
        expect(slot.mem_read(0xFFFF) == 0xFF, "Empty_MemRead_TopAddress_OpenBus");
        slot.mem_write(0x4000, 0x77);  // must be a no-op
        expect(slot.mem_read(0x4000) == 0xFF, "Empty_MemWrite_NoOp_StillOpenBus");
        slot.reset();  // must be a no-op on an empty slot (never crashes, never loads)
        expect(!slot.loaded(), "Empty_Reset_NoOp_StaysEmpty");
        expect(slot.primary_slot_number() == 1, "Construct_PrimarySlotNumber_Recorded");
    }

    // --- load() for each of the 6 types switches the active mapper. ---
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Mirrored, make_marker_image(2, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Mirrored_Ok");
        expect(slot.loaded() && slot.mapper_type() == CartridgeMapperType::Mirrored, "Load_Mirrored_ActiveMapper");
        expect(slot.mem_read(0x0000) == 0, "Load_Mirrored_ObservableThroughSlot");
    }
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Generic8kB, make_marker_image(4, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Generic8kB_Ok");
        expect(slot.mem_read(0x4000) == 0, "Load_Generic8kB_ResetDefault_Slot2Bank0");
        slot.mem_write(0x4000, 3);
        expect(slot.mem_read(0x4000) == 3, "Load_Generic8kB_BankSwitchObservableThroughSlot");
    }
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Generic16kB, make_marker_image(8, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Generic16kB_Ok");
        expect(slot.mem_read(0x4000) == 0, "Load_Generic16kB_ResetDefault");
    }
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Ascii8kB, make_marker_image(4, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Ascii8kB_Ok");
        expect(slot.mem_read(0x4000) == 0, "Load_Ascii8kB_ResetDefault");
    }
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Ascii16kB, make_marker_image(8, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Ascii16kB_Ok");
        expect(slot.mem_read(0x4000) == 0, "Load_Ascii16kB_ResetDefault");
    }
    {
        CartridgeSlot slot(1);
        const auto result = slot.load(CartridgeMapperType::Konami, make_marker_image(32, 0x2000));
        expect(result == CartridgeLoadResult::Ok, "Load_Konami_Ok");
        expect(slot.mem_read(0x4000) == 0, "Load_Konami_ResetDefault_Slot2Bank0");
        expect(slot.mem_read(0x6000) == 1, "Load_Konami_ResetDefault_Slot3Bank1");
    }

    // --- A size-invalid load returns the documented error and leaves the
    //     slot's PRIOR state completely untouched (never partially applied). ---
    {
        CartridgeSlot slot(1);
        const auto ok = slot.load(CartridgeMapperType::Generic8kB, make_marker_image(4, 0x2000));
        expect(ok == CartridgeLoadResult::Ok, "InvalidLoad_Setup_FirstLoadOk");
        slot.mem_write(0x4000, 2);  // perturb state away from reset default
        expect(slot.mem_read(0x4000) == 2, "InvalidLoad_Setup_StateDeliberatelyPerturbed");

        std::vector<std::uint8_t> bad_image(0x1234);  // not a multiple of 0x2000
        const auto bad = slot.load(CartridgeMapperType::Ascii8kB, bad_image);
        expect(bad == CartridgeLoadResult::ImageSizeInvalidForMapperType, "InvalidLoad_ReturnsDocumentedError");
        expect(slot.mapper_type() == CartridgeMapperType::Generic8kB, "InvalidLoad_PriorMapperTypeUnchanged");
        expect(slot.mem_read(0x4000) == 2, "InvalidLoad_PriorPerturbedStateUntouched_NeverPartiallyApplied");
    }

    // --- unload() reverts to the identical empty-slot default. ---
    {
        CartridgeSlot slot(1);
        expect(slot.load(CartridgeMapperType::Mirrored, make_marker_image(2, 0x2000)) == CartridgeLoadResult::Ok, "Unload_Setup_LoadSucceeded");
        expect(slot.loaded(), "Unload_Setup_Loaded");
        slot.unload();
        expect(!slot.loaded(), "Unload_NoLongerLoaded");
        expect(slot.mem_read(0x0000) == 0xFF, "Unload_RevertsToOpenBus");
    }

    // --- reset() reinitializes bank state without unloading. ---
    {
        CartridgeSlot slot(1);
        expect(slot.load(CartridgeMapperType::Generic8kB, make_marker_image(4, 0x2000)) == CartridgeLoadResult::Ok, "Reload_Setup_FirstLoadSucceeded");
        slot.mem_write(0x4000, 3);
        expect(slot.mem_read(0x4000) == 3, "Reset_Setup_BankSwitchedAway");
        slot.reset();
        expect(slot.loaded(), "Reset_StillLoaded_NeverUnloads");
        expect(slot.mapper_type() == CartridgeMapperType::Generic8kB, "Reset_MapperTypeUnchanged");
        expect(slot.mem_read(0x4000) == 0, "Reset_BankStateRevertsToDocumentedDefault");
    }

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeSlot slot(2);
            expect(slot.load(CartridgeMapperType::Ascii8kB, make_marker_image(4, 0x2000)) == CartridgeLoadResult::Ok, "Reload_Setup_SecondLoadSucceeded");
            slot.mem_write(0x6000, 2);
            slot.mem_write(0x7800, 1);
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x800) {
                reads.push_back(slot.mem_read(static_cast<std::uint16_t>(addr)));
            }
            return reads;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeSlot_Unit cases passed\n";
    return 0;
}
