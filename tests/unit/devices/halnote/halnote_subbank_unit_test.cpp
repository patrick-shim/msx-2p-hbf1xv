#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/halnote/halnote_rom.h"

// Suite: Devices_HalnoteSubbank_Unit  (M20-S2, backlog B6)
//
// The Halnote sub-mapper-enable gate (bank-3 write, bit 0x80), the 2 sub-bank
// registers (0x77FF/0x7FFF, 2 KB granularity), and the 0x7000-0x7FFF read
// override -- layered onto the M20-S1 main window (halnote_rom_unit_test.cpp).
// Byte-exact per references/openmsx-21.0/src/memory/RomHalnote.cc:63-97
// (behaviour reference only, never copied -- GPL isolation). Covers R-M20-2/
// R-M20-3, the task's own flagged "single most subtle detail" in this
// milestone: the shadow covers ONLY 0x7000-0x7FFF, never 0x6000-0x6FFF (the
// same window-slot 3, but outside the shadow's narrower range).

namespace {

using sony_msx::devices::halnote::HalnoteRom;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::size_t kBankSize = 0x2000;      // 8 KB main bank
constexpr std::size_t kSubBankSize = 0x800;    // 2 KB sub-bank
constexpr std::size_t kSubBankRegionBase = 0x80000;  // last 512 KB of the 1 MB image

// A 128-bank (1 MB) image where:
//   - every byte of main bank N (0..127) equals N (mod 256) -- the M20-S1
//     marker convention, reused here so the main-bank-3 window content is
//     independently distinguishable from any sub-bank shadow content.
//   - additionally, every byte of sub-bank S (0..255, the last 512 KB
//     re-addressed as 256x2KB) is overwritten with a DIFFERENT, disjoint
//     marker family (0x80 | (S & 0x7F)) so a shadowed read can never be
//     confused with a plain main-bank read even where the numeric ranges
//     could otherwise coincide.
std::vector<std::uint8_t> make_marker_image() {
    std::vector<std::uint8_t> image(HalnoteRom::kImageBytes);
    const std::size_t num_banks = HalnoteRom::kImageBytes / kBankSize;
    for (std::size_t bank = 0; bank < num_banks; ++bank) {
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::size_t i = 0; i < kBankSize; ++i) {
            image[bank * kBankSize + i] = marker;
        }
    }
    const std::size_t num_sub_banks = (HalnoteRom::kImageBytes - kSubBankRegionBase) / kSubBankSize;  // 256
    for (std::size_t sub = 0; sub < num_sub_banks; ++sub) {
        const std::uint8_t marker = static_cast<std::uint8_t>(0x80 | (sub & 0x7F));
        for (std::size_t i = 0; i < kSubBankSize; ++i) {
            image[kSubBankRegionBase + sub * kSubBankSize + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- Sub-bank register writes ALWAYS land (A-M20-7), regardless of
    //     sub_mapper_enabled_ -- write while disabled, then enable, and
    //     confirm the PREVIOUSLY-written value is already in effect (not
    //     reset to 0 by the later enable). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        expect(!halnote.sub_mapper_enabled(), "Precondition_SubMapperDisabledByDefault");

        halnote.mem_write(0x77FF, 10);  // sub-bank 0, while disabled
        halnote.mem_write(0x7FFF, 20);  // sub-bank 1, while disabled
        expect(halnote.sub_bank(0) == 10, "WriteWhileDisabled_SubBank0_TakesEffectImmediately");
        expect(halnote.sub_bank(1) == 20, "WriteWhileDisabled_SubBank1_TakesEffectImmediately");

        // Enable the sub-mapper (bank-3 trigger, bit7 set; low bits 0 so
        // window slot 3 resolves to bank 0, irrelevant here).
        halnote.mem_write(0x6FFF, 0x80);
        expect(halnote.sub_mapper_enabled(), "Enable_SubMapperFlagSet");
        expect(halnote.sub_bank(0) == 10 && halnote.sub_bank(1) == 20,
               "Enable_DoesNotResetPreviouslyWrittenSubBanks");

        const std::size_t expected_offset0 = kSubBankRegionBase + 10 * kSubBankSize;
        const std::uint8_t expected_marker0 = static_cast<std::uint8_t>(0x80 | (10 & 0x7F));
        expect(halnote.window().image()[expected_offset0] == expected_marker0,
               "Enable_SubBank0Value_AlreadyReflectedInImageLookup");
        expect(halnote.mem_read(0x7000) == expected_marker0,
               "Enable_Read0x7000_ShowsSubBank0SetBeforeEnable");
    }

    // --- The exact boundary (R-M20-2, the task's flagged subtlest risk):
    //     enabling makes 0x7000-0x77FF/0x7800-0x7FFF show the shadow content,
    //     while 0x6000-0x6FFF (same window-slot 3, OUTSIDE the shadow's
    //     narrower range) stays on the normal bank-3 window. ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());

        // Give window-slot 3 (0x6000-0x7FFF) a distinguishable, non-zero main
        // bank so it is never confused with sub-bank marker bytes (main-bank
        // markers are 0-127; sub-bank markers are 0x80-0xFF, but assert this
        // explicitly rather than relying on the disjoint ranges alone).
        halnote.mem_write(0x6FFF, 0x07);  // bit7 clear: sub-mapper stays disabled, bank -> 7
        expect(!halnote.sub_mapper_enabled(), "Boundary_SubMapperStillDisabledAfterBank7Write");
        expect(halnote.window().slot_bank(3) == 7, "Boundary_WindowSlot3_IsBank7");

        // Disabled: BOTH 0x6000-0x6FFF and 0x7000-0x7FFF read the SAME
        // normal bank-3 window content (marker 7).
        expect(halnote.mem_read(0x6000) == 7, "Disabled_Read0x6000_NormalBank3Window");
        expect(halnote.mem_read(0x6FFE) == 7, "Disabled_Read0x6FFE_NormalBank3Window");
        expect(halnote.mem_read(0x7000) == 7, "Disabled_Read0x7000_StillNormalBank3Window");
        expect(halnote.mem_read(0x77FF) == 7, "Disabled_Read0x77FF_StillNormalBank3Window");
        expect(halnote.mem_read(0x7800) == 7, "Disabled_Read0x7800_StillNormalBank3Window");
        expect(halnote.mem_read(0x7FFF) == 7, "Disabled_Read0x7FFF_StillNormalBank3Window");

        // Program distinguishing sub-bank register values, THEN enable.
        halnote.mem_write(0x77FF, 3);    // sub-bank 0 -> covers 0x7000-0x77FF
        halnote.mem_write(0x7FFF, 200);  // sub-bank 1 -> covers 0x7800-0x7FFF
        halnote.mem_write(0x6FFF, 0x87);  // bit7 SET: enable sub-mapper, bank -> 7 (0x87 & 0x7F = 7)
        expect(halnote.sub_mapper_enabled(), "Boundary_SubMapperNowEnabled");
        expect(halnote.window().slot_bank(3) == 7, "Boundary_WindowSlot3_StillBank7AfterEnable");

        const std::uint8_t marker_sub0 = static_cast<std::uint8_t>(0x80 | (3 & 0x7F));
        const std::uint8_t marker_sub1 = static_cast<std::uint8_t>(0x80 | (200 & 0x7F));

        // 0x7000-0x77FF now shows sub-bank 0's shadow content.
        expect(halnote.mem_read(0x7000) == marker_sub0, "Enabled_Read0x7000_ShowsSubBank0Shadow");
        expect(halnote.mem_read(0x77FF) == marker_sub0, "Enabled_Read0x77FF_LastByteOfSubBank0_ShowsShadow");
        // 0x7800-0x7FFF now shows sub-bank 1's shadow content.
        expect(halnote.mem_read(0x7800) == marker_sub1, "Enabled_Read0x7800_ShowsSubBank1Shadow");
        expect(halnote.mem_read(0x7FFF) == marker_sub1, "Enabled_Read0x7FFF_LastByteOfSubBank1_ShowsShadow");

        // 0x6000-0x6FFF is UNCHANGED -- never shadowed, always the normal
        // bank-3 window (still marker 7), even though the sub-mapper is now
        // enabled and window-slot 3 is the SAME slot backing both ranges.
        expect(halnote.mem_read(0x6000) == 7, "Enabled_Read0x6000_StillNormalBank3Window_NeverShadowed");
        expect(halnote.mem_read(0x6FFE) == 7, "Enabled_Read0x6FFE_StillNormalBank3Window_NeverShadowed");
        expect(halnote.mem_read(0x6FFF) == 7, "Enabled_Read0x6FFF_LastByteBeforeShadow_StillNormalWindow");

        // Disabling reverts 0x7000-0x7FFF to the normal bank-3 window.
        halnote.mem_write(0x6FFF, 0x07);  // bit7 clear -> disable, bank stays 7
        expect(!halnote.sub_mapper_enabled(), "Disable_SubMapperFlagCleared");
        expect(halnote.mem_read(0x7000) == 7, "Disabled_Again_Read0x7000_RevertsToNormalBank3Window");
        expect(halnote.mem_read(0x7FFF) == 7, "Disabled_Again_Read0x7FFF_RevertsToNormalBank3Window");
        // The sub-bank registers themselves are untouched by disabling.
        expect(halnote.sub_bank(0) == 3 && halnote.sub_bank(1) == 200,
               "Disable_DoesNotResetSubBankRegisterValues");
    }

    // --- Every sub-bank byte value 0-255 resolves in-bounds with no masking
    //     needed (256 sub-banks * 2 KB == exactly the last 512 KB, A-M20-8). ---
    {
        HalnoteRom halnote;
        halnote.set_image(make_marker_image());
        halnote.mem_write(0x6FFF, 0x80);  // enable sub-mapper (bank stays 0)
        bool all_in_range = true;
        for (int value = 0; value <= 255; ++value) {
            halnote.mem_write(0x77FF, static_cast<std::uint8_t>(value));
            const std::uint8_t expected = static_cast<std::uint8_t>(0x80 | (value & 0x7F));
            if (halnote.mem_read(0x7000) != expected) {
                all_in_range = false;
            }
            halnote.mem_write(0x7FFF, static_cast<std::uint8_t>(value));
            if (halnote.mem_read(0x7800) != expected) {
                all_in_range = false;
            }
        }
        expect(all_in_range, "EverySubBankByteValue_ResolvesInBounds_NoMaskingNeeded");
    }

    // --- Two-run determinism across the sub-bank mechanism. ---
    {
        auto run = [] {
            HalnoteRom halnote;
            halnote.set_image(make_marker_image());
            halnote.mem_write(0x77FF, 17);
            halnote.mem_write(0x7FFF, 240);
            halnote.mem_write(0x6FFF, 0x99);  // enable + bank (0x99 & 0x7F = 0x19 = 25)
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0x6000; addr < 0x8000; addr += 0x0100) {
                reads.push_back(halnote.mem_read(static_cast<std::uint16_t>(addr)));
            }
            return reads;
        };
        expect(run() == run(), "TwoRunDeterminism_SubBankMechanism_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_HalnoteSubbank_Unit cases passed\n";
    return 0;
}
