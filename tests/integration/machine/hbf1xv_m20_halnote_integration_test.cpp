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

// Suite: Machine_Hbf1xvM20Halnote_Integration  (M20-S3, closes backlog B4 AND
// B6 together)
//
// Machine-level wiring of HalnoteRom (the Halnote-mapped MSX-JE firmware ROM)
// at primary slot 0, secondary slot 3, ALL 4 CPU pages -- the M13 BIOS ROM at
// slot 0-0 must stay byte-for-byte unchanged (regression guard). Exercises the
// full main-bank + SRAM + sub-bank protocol over the REAL system bus at the
// real slot address, and the real bios/f1xvfirm.rom asset load. SRAM
// persistence round-trips across two independent Hbf1xvMachine instances via
// a shared file path (mirrors the M15 S1985 backup-RAM persistence pattern
// exactly, A-M20-12).
//
// CRITICAL slot-routing discipline (A-M20-13, DEC-0016): every #FFFF/#A8 hex
// constant here is independently re-derived from SlotBus's own formulas
// (src/devices/chipset/slot_bus.cpp: primary_for_page, sub_for_page,
// write_ffff), never copied from a prior milestone's worked example. At least
// one case explicitly asserts the RESOLVED (primary, sub, page) triple via
// slot_expanded()/debug_sub_slot_register() before relying on it in a larger
// sequence.
//
// (Zero-regression across the full M1-M19 suite is confirmed separately by
// the full `ctest` run captured in the M20 implementation report, not
// re-asserted here.)

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

using sony_msx::machine::Hbf1xvMachine;

// After cold_boot(), #A8 == 0x00 (X1, every page's primary field == 0) and
// primary slot 0 is expanded (wire_bus()'s set_expanded(0, true)). A single
// debug_bus_write(0xFFFF, ...) therefore ALWAYS lands in
// sub_slot_register_[primary_for_page(3)] == sub_slot_register_[0]
// (SlotBus::write_ffff: primary = primary_for_page(3); the write targets
// whichever primary currently occupies PAGE 3, never the page being
// logically configured -- A-M20-13's own hard-won lesson).
//
// 0xFF == 0b11_11_11_11 -> every 2-bit page field (bits[1:0] page0,
// bits[3:2] page1, bits[5:4] page2, bits[7:6] page3) independently decodes
// to secondary slot 3, so ALL FOUR pages resolve to (primary 0, sub 3) at
// once -- the entire Halnote 64 KB window becomes directly reachable via
// plain debug_bus_read/debug_bus_write.
void route_all_pages_to_halnote(Hbf1xvMachine& machine) {
    machine.debug_bus_write(0xFFFF, 0xFF);
}

// 0x00 -> every page field decodes to secondary slot 0 (RAM mapper at slot
// 3-0; slot 0-0 = BIOS ROM only occupies pages 0-1, the routing this helper
// restores for the BIOS-ROM regression guard).
void route_all_pages_to_secondary_zero(Hbf1xvMachine& machine) {
    machine.debug_bus_write(0xFFFF, 0x00);
}

std::array<std::uint8_t, 64> read_bios_rom_sample(Hbf1xvMachine& machine) {
    route_all_pages_to_secondary_zero(machine);
    std::array<std::uint8_t, 64> sample{};
    for (std::size_t i = 0; i < sample.size(); ++i) {
        sample[i] = machine.debug_bus_read(static_cast<std::uint16_t>(0x0000 + i));
    }
    return sample;
}

}  // namespace

int main() {
    const std::filesystem::path bios_dir = SONY_MSX_BIOS_DIR;

    // --- Case 1: the resolved (primary, sub, page) triple, asserted
    //     EXPLICITLY before relying on it (A-M20-13's verification action). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(bios_dir);
        machine.cold_boot();

        route_all_pages_to_halnote(machine);
        expect(machine.slot_expanded(0), "Routing_PrimarySlot0_IsExpanded");
        const std::uint8_t sub_reg = machine.debug_sub_slot_register(0);
        expect(sub_reg == 0xFF, "Routing_SubSlotRegister0_Is0xFF_AfterWrite");
        bool every_page_is_sub3 = true;
        for (int page = 0; page < 4; ++page) {
            const int resolved_sub = (sub_reg >> (2 * page)) & 0x03;
            if (resolved_sub != 3) {
                every_page_is_sub3 = false;
            }
        }
        expect(every_page_is_sub3, "Routing_EveryPage_ResolvesToSecondarySlot3");

        // Restore and re-confirm the OTHER routing this test also relies on.
        route_all_pages_to_secondary_zero(machine);
        expect(machine.debug_sub_slot_register(0) == 0x00, "Routing_RestoreToSecondaryZero_Confirmed");
    }

    // --- Case 2: the real bios/f1xvfirm.rom asset loads (size/asset gate --
    //     no missing-asset diagnostic recorded for it). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(bios_dir);
        machine.cold_boot();
        bool firmware_diagnostic_found = false;
        for (const auto& diagnostic : machine.rom_diagnostics()) {
            if (diagnostic.find("f1xvfirm.rom") != std::string::npos) {
                firmware_diagnostic_found = true;
            }
        }
        expect(!firmware_diagnostic_found, "AssetLoad_F1xvfirmRom_NoMissingAssetDiagnostic");
        expect(machine.halnote().window().num_blocks() == 128, "AssetLoad_RealFirmware_128Blocks");
        expect(machine.halnote().window().block_mask() == 127, "AssetLoad_RealFirmware_DefaultBlockMask");
    }

    // --- Case 3: the M13 BIOS ROM at slot 0-0 is confirmed byte-for-byte
    //     UNCHANGED by populating slot 0-3 with Halnote (regression guard,
    //     mirrors the M17 fmmusic_rom_ guard). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(bios_dir);
        machine.cold_boot();

        const std::array<std::uint8_t, 64> before = read_bios_rom_sample(machine);

        // Exercise Halnote heavily in between (bank switch + SRAM enable +
        // sub-mapper enable), then read the BIOS sample again.
        route_all_pages_to_halnote(machine);
        machine.debug_bus_write(0x4FFF, 0x81);  // enable SRAM, bank(2) -> 1
        machine.debug_bus_write(0x6FFF, 0x82);  // enable sub-mapper, bank(3) -> 2
        machine.debug_bus_write(0x8FFF, 0x03);  // bank(4) -> 3
        machine.debug_bus_write(0xAFFF, 0x04);  // bank(5) -> 4
        machine.debug_bus_write(0x0000, 0x5A);  // SRAM write
        machine.debug_bus_write(0x77FF, 0x07);  // sub-bank 0
        machine.debug_bus_write(0x7FFF, 0x09);  // sub-bank 1

        const std::array<std::uint8_t, 64> after = read_bios_rom_sample(machine);
        expect(before == after, "BiosRomGuard_Slot00_UnchangedByHalnoteTraffic");

        // Positive sanity check: this guard is genuinely reading real ROM
        // content, not a degenerate all-same-byte sample.
        bool all_same = true;
        for (std::size_t i = 1; i < before.size(); ++i) {
            if (before[i] != before[0]) {
                all_same = false;
                break;
            }
        }
        expect(!all_same, "BiosRomGuard_SampleIsNonDegenerate");
    }

    // --- Case 4: full main-bank + SRAM + sub-bank protocol over the REAL
    //     system bus at the real (primary 0, secondary 3) slot address,
    //     structurally cross-checked against machine.halnote()'s own window/
    //     image accessors (content-agnostic: the real firmware's actual
    //     bytes are unknown/un-fabricated, so assertions compare the BUS
    //     read-back to the INDEPENDENTLY-COMPUTED expected offset into the
    //     same image, never to a hardcoded byte value). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(bios_dir);
        machine.cold_boot();
        route_all_pages_to_halnote(machine);

        const auto& window = machine.halnote().window();
        const auto& image = window.image();

        // Main bank-switch: write bank(4) (region 0x8000-0x9FFF, trigger
        // 0x8FFF) to block 3; the bus read at the region base must equal the
        // image byte at offset 3*0x2000.
        machine.debug_bus_write(0x8FFF, 0x03);
        expect(machine.halnote().window().slot_bank(4) == 3, "BusProtocol_Bank4Write_ResolvesToBlock3");
        expect(machine.debug_bus_read(0x8000) == image[3 * 0x2000 + 0],
               "BusProtocol_Bank4Read_MatchesComputedImageOffset");
        expect(machine.debug_bus_read(0x9FFF) == image[3 * 0x2000 + 0x1FFF],
               "BusProtocol_Bank4Read_LastByteMatchesComputedImageOffset");

        // SRAM: enable via the bank-2 trigger (0x4FFF), bit7 set; write/read
        // over the bus at both SRAM-region boundaries.
        machine.debug_bus_write(0x4FFF, 0x80);
        expect(machine.halnote().sram_enabled(), "BusProtocol_SramEnableBitSet_FlagReflectsIt");
        machine.debug_bus_write(0x0000, 0x5A);
        machine.debug_bus_write(0x3FFF, 0xA5);
        expect(machine.debug_bus_read(0x0000) == 0x5A, "BusProtocol_SramWriteReadBack_FirstByte");
        expect(machine.debug_bus_read(0x3FFF) == 0xA5, "BusProtocol_SramWriteReadBack_LastByte");
        expect(machine.halnote().sram().read(0x0000) == 0x5A,
               "BusProtocol_SramWrite_VisibleViaDirectSramAccessor");

        // Sub-bank shadow: enable via the bank-3 trigger (0x6FFF), bit7 set;
        // write both sub-bank registers over the bus; the shadowed region
        // (0x7000-0x7FFF) must match the computed sub-bank image offset,
        // while 0x6000-0x6FFF (same window-slot 3, outside the narrower
        // shadow range) must still match the PLAIN window read.
        machine.debug_bus_write(0x6FFF, 0x81);  // enable sub-mapper, bank(3) -> 1
        expect(machine.halnote().sub_mapper_enabled(), "BusProtocol_SubMapperEnableBitSet_FlagReflectsIt");
        machine.debug_bus_write(0x77FF, 0x05);  // sub-bank 0
        machine.debug_bus_write(0x7FFF, 0x0A);  // sub-bank 1

        const std::size_t sub0_offset = 0x80000 + static_cast<std::size_t>(5) * 0x800;
        const std::size_t sub1_offset = 0x80000 + static_cast<std::size_t>(10) * 0x800;
        expect(machine.debug_bus_read(0x7000) == image[sub0_offset],
               "BusProtocol_SubBank0Shadow_MatchesComputedOffset");
        expect(machine.debug_bus_read(0x77FF) == image[sub0_offset + 0x7FF],
               "BusProtocol_SubBank0Shadow_LastByteMatchesComputedOffset");
        expect(machine.debug_bus_read(0x7800) == image[sub1_offset],
               "BusProtocol_SubBank1Shadow_MatchesComputedOffset");
        expect(machine.debug_bus_read(0x7FFF) == image[sub1_offset + 0x7FF],
               "BusProtocol_SubBank1Shadow_LastByteMatchesComputedOffset");

        // The R-M20-2 boundary: 0x6000-0x6FFF is NEVER shadowed -- it must
        // match the PLAIN window read (window-slot 3's resolved bank == 1,
        // set moments ago by the 0x6FFF trigger write above).
        expect(window.slot_bank(3) == 1, "BusProtocol_WindowSlot3_IsBank1");
        expect(machine.debug_bus_read(0x6000) == image[1 * 0x2000 + 0],
               "BusProtocol_0x6000_NeverShadowed_MatchesPlainWindowRead");
        expect(machine.debug_bus_read(0x6FFF) == image[1 * 0x2000 + 0x0FFF],
               "BusProtocol_0x6FFF_LastByteBeforeShadow_NeverShadowed");

        // window-slots 6/7 (0xC000-0xFFFF) stay permanently 0xFF regardless
        // of the bank-switch/SRAM/sub-bank traffic just exercised above.
        expect(machine.debug_bus_read(0xC000) == 0xFF, "BusProtocol_UpperQuarterStart_AlwaysOpenBus");
        expect(machine.debug_bus_read(0xFFFE) == 0xFF, "BusProtocol_UpperQuarterNearEnd_AlwaysOpenBus");
        machine.debug_bus_write(0xC000, 0x77);
        expect(machine.debug_bus_read(0xC000) == 0xFF, "BusProtocol_UpperQuarterWrite_NeverTakesEffect");
    }

    // --- Case 5: SRAM persistence round-trips across two independent
    //     Hbf1xvMachine instances via a shared file path (A-M20-12). ---
    {
        const std::filesystem::path sram_path =
            std::filesystem::temp_directory_path() / "sony_msx_m20_halnote_sram_test.bin";
        std::error_code ec;
        std::filesystem::remove(sram_path, ec);

        {
            Hbf1xvMachine writer;
            writer.set_asset_root(bios_dir);
            writer.set_halnote_sram_path(sram_path);
            writer.cold_boot();
            route_all_pages_to_halnote(writer);
            writer.debug_bus_write(0x4FFF, 0x80);  // enable SRAM
            writer.debug_bus_write(0x0000, 0x11);
            writer.debug_bus_write(0x2000, 0x22);
            writer.debug_bus_write(0x3FFF, 0x33);
            expect(writer.flush_halnote_sram(), "Persistence_Flush_Succeeds");
        }
        {
            Hbf1xvMachine reader;
            reader.set_asset_root(bios_dir);
            reader.set_halnote_sram_path(sram_path);
            reader.cold_boot();
            route_all_pages_to_halnote(reader);
            reader.debug_bus_write(0x4FFF, 0x80);  // enable SRAM to read it back over the bus
            expect(reader.debug_bus_read(0x0000) == 0x11, "Persistence_RoundTrip_FirstByte");
            expect(reader.debug_bus_read(0x2000) == 0x22, "Persistence_RoundTrip_MiddleByte");
            expect(reader.debug_bus_read(0x3FFF) == 0x33, "Persistence_RoundTrip_LastByte");
        }

        std::filesystem::remove(sram_path, ec);
    }

    // --- Case 6: unset SRAM path yields deterministic zero after EVERY
    //     cold_boot() (mirrors the M15 "absent file -> deterministic zero,
    //     M11 golden preserved" pattern). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(bios_dir);
        expect(machine.halnote_sram_path().empty(), "UnsetPath_DefaultEmpty");

        machine.cold_boot();
        route_all_pages_to_halnote(machine);
        machine.debug_bus_write(0x4FFF, 0x80);
        expect(machine.debug_bus_read(0x0000) == 0x00, "UnsetPath_FirstColdBoot_SramIsZero");
        machine.debug_bus_write(0x0000, 0x99);
        expect(machine.debug_bus_read(0x0000) == 0x99, "UnsetPath_WriteTakesEffect");

        // A SECOND cold_boot() (no path configured) must revert to zero --
        // never carrying over the previous boot's SRAM content.
        machine.cold_boot();
        route_all_pages_to_halnote(machine);
        machine.debug_bus_write(0x4FFF, 0x80);
        expect(machine.debug_bus_read(0x0000) == 0x00, "UnsetPath_SecondColdBoot_SramResetToZero");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM20Halnote_Integration cases passed\n";
    return 0;
}
