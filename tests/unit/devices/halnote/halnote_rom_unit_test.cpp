#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/halnote/halnote_rom.h"

// Suite: Devices_HalnoteRom_Unit  (M20-S1, backlog B4/B6)
//
// HalnoteRom's main 8-slot window (reusing devices::cartridge::
// CartridgeRomWindow verbatim, A-M20-10), the 4 main bank-switch registers
// (window-slots 2-5), the SRAM-enable gate wired to a REAL
// devices::memory::BatteryBackedSram store (A-M20-11), and reset(). Byte-exact
// per references/openmsx-21.0/src/memory/RomHalnote.cc (behaviour reference
// only, never copied -- GPL isolation); see docs/m20-planner-package.md
// A-M20-1..A-M20-13 for the full, independently re-derived grounding.
//
// The sub-bank shadow mechanism (M20-S2) is covered separately in
// halnote_subbank_unit_test.cpp.

namespace {

using sony_msx::devices::halnote::HalnoteRom;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::size_t kBankSize = 0x2000;  // 8 KB, matches CartridgeRomWindow::kBankSize

// A 128-bank (1 MB) synthetic image where bank N's every byte equals N mod
// 256 -- a clearly distinguishable marker per bank (mirrors the M19
// CartridgeRomWindow unit test's own marker-image technique).
std::vector<std::uint8_t> make_marker_image() {
    std::vector<std::uint8_t> image(HalnoteRom::kImageBytes);
    const std::size_t num_banks = HalnoteRom::kImageBytes / kBankSize;
    for (std::size_t bank = 0; bank < num_banks; ++bank) {
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::size_t i = 0; i < kBankSize; ++i) {
            image[bank * kBankSize + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- reset() establishes the documented layout. ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        // set_image() re-applies reset_bank_state(), so the constructed
        // device is already in the documented post-reset layout.
        expect(halnote.window().num_blocks() == 128, "Reset_NumBlocks_Is128");
        expect(halnote.window().block_mask() == 127, "Reset_BlockMask_DefaultNotKonami");
        expect(!halnote.window().slot_mapped(0), "Reset_Slot0_Unmapped");
        expect(!halnote.window().slot_mapped(1), "Reset_Slot1_Unmapped");
        expect(halnote.window().slot_mapped(2) && halnote.window().slot_bank(2) == 0, "Reset_Slot2_Bank0");
        expect(halnote.window().slot_mapped(3) && halnote.window().slot_bank(3) == 0, "Reset_Slot3_Bank0");
        expect(halnote.window().slot_mapped(4) && halnote.window().slot_bank(4) == 0, "Reset_Slot4_Bank0");
        expect(halnote.window().slot_mapped(5) && halnote.window().slot_bank(5) == 0, "Reset_Slot5_Bank0");
        expect(!halnote.window().slot_mapped(6), "Reset_Slot6_Unmapped");
        expect(!halnote.window().slot_mapped(7), "Reset_Slot7_Unmapped");
        expect(!halnote.sram_enabled(), "Reset_SramDisabled");
        expect(!halnote.sub_mapper_enabled(), "Reset_SubMapperDisabled");
        expect(halnote.sub_bank(0) == 0 && halnote.sub_bank(1) == 0, "Reset_SubBanksZero");
        // Explicit call, not just construction, re-establishes the same layout.
        halnote.reset();
        expect(halnote.window().slot_mapped(2) && halnote.window().slot_bank(2) == 0, "Reset_Explicit_Slot2Bank0");
    }

    // --- Each of the 4 main bank-switch trigger addresses lands on its
    //     CORRECT window-slot (A-M20-3: 2/3/4/5, NOT the header comment's
    //     simplified 0/1/2/3). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());

        struct Trigger {
            std::uint16_t address;
            int slot;
        };
        const Trigger triggers[4] = {
            {0x4FFF, 2},
            {0x6FFF, 3},
            {0x8FFF, 4},
            {0xAFFF, 5},
        };
        for (const auto& t : triggers) {
            halnote.mem_write(t.address, 0x07);  // in-range bank, bit7 clear
            expect(halnote.window().slot_mapped(t.slot) && halnote.window().slot_bank(t.slot) == 7,
                   "BankSwitch_TriggerAddress_LandsOnCorrectSlot");
            const std::uint16_t region_base = static_cast<std::uint16_t>(t.slot * kBankSize);
            expect(halnote.mem_read(region_base) == 7, "BankSwitch_ReadBackMarkerAtRegionBase");
        }
    }

    // --- A-M20-5: the bit-0x80 double-duty effect on the bank-2 trigger
    //     address -- a SINGLE write both enables SRAM AND sets that same
    //     window-slot's visible ROM bank (via the mask fallback, since
    //     0x85 = 133 >= 128 -> 133 & 127 = 5). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        halnote.mem_write(0x4FFF, 0x85);
        expect(halnote.sram_enabled(), "DoubleDuty_Bank2Write0x85_EnablesSram");
        expect(halnote.window().slot_mapped(2) && halnote.window().slot_bank(2) == 5,
               "DoubleDuty_Bank2Write0x85_SetsWindowSlot2ToBank5");
        expect(halnote.mem_read(0x4000) == 5, "DoubleDuty_Bank2Write0x85_ReadBackConfirmsBank5");
    }

    // --- The SAME double-duty effect on the bank-3 (sub-mapper-enable) trigger. ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        halnote.mem_write(0x6FFF, 0x85);
        expect(halnote.sub_mapper_enabled(), "DoubleDuty_Bank3Write0x85_EnablesSubMapper");
        expect(halnote.window().slot_mapped(3) && halnote.window().slot_bank(3) == 5,
               "DoubleDuty_Bank3Write0x85_SetsWindowSlot3ToBank5");
    }

    // --- Enabling SRAM does not disturb the sub-mapper flag and vice versa
    //     (the two enable bits are independent, even though they share the
    //     "bit 0x80 of a bank-switch write" mechanism on DIFFERENT registers). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        halnote.mem_write(0x4FFF, 0x80);  // enable SRAM only
        expect(halnote.sram_enabled() && !halnote.sub_mapper_enabled(),
               "IndependentFlags_SramEnableAlone_LeavesSubMapperDisabled");
        halnote.mem_write(0x6FFF, 0x80);  // enable sub-mapper only
        expect(halnote.sram_enabled() && halnote.sub_mapper_enabled(),
               "IndependentFlags_SubMapperEnable_LeavesSramEnabledUnchanged");
    }

    // --- SRAM read/write only when enabled; disabled reads are open-bus and
    //     disabled writes are no-ops -- cross-checked against direct
    //     sram().read()/write() calls (A-M20-6). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());

        // Disabled: mem_read is open-bus at every SRAM-region boundary; a
        // mem_write attempt must NOT reach the underlying store.
        expect(halnote.mem_read(0x0000) == 0xFF, "SramDisabled_Read0x0000_OpenBus");
        expect(halnote.mem_read(0x1FFF) == 0xFF, "SramDisabled_Read0x1FFF_OpenBus");
        expect(halnote.mem_read(0x2000) == 0xFF, "SramDisabled_Read0x2000_OpenBus");
        expect(halnote.mem_read(0x3FFF) == 0xFF, "SramDisabled_Read0x3FFF_OpenBus");
        halnote.mem_write(0x0000, 0xAB);
        expect(halnote.sram().read(0x0000) == 0x00, "SramDisabled_WriteIgnored_UnderlyingStoreUnchanged");

        // Enable SRAM (bank-2 trigger, bit7 set; low bits 0 -> window slot 2
        // resolves to bank 0, irrelevant to this case).
        halnote.mem_write(0x4FFF, 0x80);
        expect(halnote.sram_enabled(), "SramEnabled_FlagSet");

        halnote.mem_write(0x0000, 0x11);
        halnote.mem_write(0x1FFF, 0x22);
        halnote.mem_write(0x2000, 0x33);
        halnote.mem_write(0x3FFF, 0x44);
        expect(halnote.mem_read(0x0000) == 0x11 && halnote.sram().read(0x0000) == 0x11,
               "SramEnabled_Write0x0000_MatchesDirectSram");
        expect(halnote.mem_read(0x1FFF) == 0x22 && halnote.sram().read(0x1FFF) == 0x22,
               "SramEnabled_Write0x1FFF_MatchesDirectSram");
        expect(halnote.mem_read(0x2000) == 0x33 && halnote.sram().read(0x2000) == 0x33,
               "SramEnabled_Write0x2000_MatchesDirectSram");
        expect(halnote.mem_read(0x3FFF) == 0x44 && halnote.sram().read(0x3FFF) == 0x44,
               "SramEnabled_Write0x3FFF_MatchesDirectSram");

        // Direct sram() writes are visible through mem_read while enabled.
        halnote.sram().write(0x0100, 0x99);
        expect(halnote.mem_read(0x0100) == 0x99, "SramEnabled_DirectSramWrite_VisibleViaMemRead");

        // Disable again (bit7 clear): mem_read reverts to open-bus, but the
        // underlying store content is untouched (survives re-enable).
        halnote.mem_write(0x4FFF, 0x00);
        expect(!halnote.sram_enabled(), "SramReDisabled_FlagClear");
        expect(halnote.mem_read(0x0000) == 0xFF, "SramReDisabled_Read0x0000_OpenBus");
        expect(halnote.sram().read(0x0000) == 0x11, "SramReDisabled_UnderlyingStoreSurvives");
    }

    // --- window-slots 6/7 (CPU 0xC000-0xFFFF) are permanently unmapped --
    //     no write path ever reaches them (A-M20-9/R-M20-9). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        bool always_open_bus = true;
        for (std::uint32_t addr = 0xC000; addr <= 0xFFFF; addr += 0x0100) {
            halnote.mem_write(static_cast<std::uint16_t>(addr), static_cast<std::uint8_t>(addr & 0xFF));
            if (halnote.mem_read(static_cast<std::uint16_t>(addr)) != 0xFF) {
                always_open_bus = false;
            }
        }
        // Also sweep the very last address explicitly.
        halnote.mem_write(0xFFFF, 0x42);
        if (halnote.mem_read(0xFFFF) != 0xFF) {
            always_open_bus = false;
        }
        expect(always_open_bus, "UpperQuarter_0xC000To0xFFFF_AlwaysOpenBus");
    }

    // --- set_image() normalizes a wrong-sized image (truncate/pad, never
    //     throws) and preserves sram_ content UNTOUCHED. ---
    {
        HalnoteRom halnote;
        // Enable + populate SRAM before any set_image() call.
        halnote.mem_write(0x4FFF, 0x80);
        halnote.mem_write(0x0000, 0x5A);
        halnote.mem_write(0x3FFF, 0xA5);

        // Too-short image.
        std::vector<std::uint8_t> too_short(100, 0x11);
        halnote.set_image(too_short);
        expect(halnote.window().num_blocks() == 128, "SetImage_TooShort_NormalizedTo128Blocks");
        expect(halnote.window().block_mask() == 127, "SetImage_TooShort_BlockMaskDefault");
        // Re-applies the bank/flag reset -- SRAM enable is cleared again.
        expect(!halnote.sram_enabled(), "SetImage_ResetsBankFlagState");
        // But the underlying SRAM STORE content survives untouched.
        expect(halnote.sram().read(0x0000) == 0x5A, "SetImage_TooShort_SramContentUntouched_Byte0");
        expect(halnote.sram().read(0x3FFF) == 0xA5, "SetImage_TooShort_SramContentUntouched_LastByte");

        // Too-long image.
        std::vector<std::uint8_t> too_long(HalnoteRom::kImageBytes * 2, 0x22);
        halnote.set_image(too_long);
        expect(halnote.window().num_blocks() == 128, "SetImage_TooLong_NormalizedTo128Blocks");
        expect(halnote.sram().read(0x0000) == 0x5A, "SetImage_TooLong_SramContentStillUntouched");

        // Exactly-sized image (the real-asset case): behaves identically.
        halnote.set_image(make_marker_image());
        expect(halnote.window().num_blocks() == 128, "SetImage_ExactSize_128Blocks");
        expect(halnote.window().block_mask() == 127, "SetImage_ExactSize_BlockMask127");
        expect(halnote.mem_read(0x4000) == 0, "SetImage_ExactSize_Bank2ResetToBank0");
    }

    // --- Two-run determinism: identical write sequences on two independent
    //     instances produce byte-identical bank/SRAM/sub-bank state. ---
    {
        auto run = [] {
            HalnoteRom halnote;
            halnote.set_image(make_marker_image());
            halnote.mem_write(0x4FFF, 0x93);  // enable SRAM + bank (0x93 & 0x7F = 0x13 = 19)
            halnote.mem_write(0x6FFF, 0x05);
            halnote.mem_write(0x8FFF, 0x2A);
            halnote.mem_write(0xAFFF, 0x40);
            halnote.mem_write(0x0000, 0x77);
            halnote.mem_write(0x3FFF, 0x88);
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x0400) {
                reads.push_back(halnote.mem_read(static_cast<std::uint16_t>(addr)));
            }
            return reads;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_HalnoteRom_Unit cases passed\n";
    return 0;
}
