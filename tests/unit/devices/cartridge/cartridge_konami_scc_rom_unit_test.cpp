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

#include "devices/cartridge/cartridge_konami_scc_rom.h"

// Suite: Devices_CartridgeKonamiSccRom_Unit
//
// Grounds openMSX 21.0: src/memory/RomKonamiSCC.cc (behaviour
// reference only, never copied -- GPL isolation) and the
// Konami SCC fact sheet §2. Asserts specifically the facts
// that DIFFER from the plain Konami mapper, so a
// copy-paste inversion cannot pass silently:
//   - mirroring direction is the OPPOSITE (pages 2/3 -> slots 6/7,
//     pages 4/5 -> slots 0/1);
//   - the write-decode lower bound is 0x5000 (not 0x6000) and page 2
//     (0x4000-0x5FFF) IS live-switchable via 0x5000-0x57FF;
//   - the block mask is the image-derived default (no 256 kB override);
//   - the SCC enable latch + register window exist at all.

namespace {

using sony_msx::devices::cartridge::CartridgeKonamiScc;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeRomWindow;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> make_marker_image(const unsigned num_banks) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_banks) * CartridgeRomWindow::kBankSize);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        const auto marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[static_cast<std::size_t>(bank) * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }
    return image;
}

CartridgeKonamiScc make_reset_mapper(const unsigned num_banks = 16) {
    CartridgeKonamiScc rom(make_marker_image(num_banks));
    rom.reset();
    return rom;
}

}  // namespace

int main() {
    // --- Image-size validation: positive multiple of 0x2000; >512 kB
    //     ACCEPTED (documented real-chip ceiling; openMSX warns non-fatally,
    //     RomKonamiSCC.cc:28-34). ---
    expect(CartridgeKonamiScc::is_valid_image_size(0x2000), "IsValid_OneBank_Accepted");
    expect(CartridgeKonamiScc::is_valid_image_size(512 * 1024), "IsValid_512KB_Accepted");
    expect(CartridgeKonamiScc::is_valid_image_size(1024 * 1024),
           "IsValid_1MB_BeyondRealChipCeiling_StillAccepted");
    expect(!CartridgeKonamiScc::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeKonamiScc::is_valid_image_size(0x1000), "IsValid_NonMultiple_Rejected");

    expect(CartridgeKonamiScc(make_marker_image(4)).mapper_type() == CartridgeMapperType::KonamiSCC,
           "MapperType_ReportsKonamiSCC");

    // --- Constructor does NOT self-reset (the convention shared by the
    //     cartridge mappers: reset() is a separate, explicit step). ---
    {
        CartridgeKonamiScc rom(make_marker_image(16));
        expect(rom.mem_read(0x8000) == CartridgeRomWindow::kOpenBus,
               "Construct_NoImplicitReset_WindowStartsUnmapped");
        expect(!rom.scc_enabled(), "Construct_SccDisabled");
    }

    // --- reset(): banks 0,1,2,3 with the KonamiSCC-SPECIFIC mirrors
    //     (RomKonamiSCC.cc:44-58,60-68). ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        expect(rom.mem_read(0x4000) == 0, "Reset_Page2_Bank0");
        expect(rom.mem_read(0x6000) == 1, "Reset_Page3_Bank1");
        expect(rom.mem_read(0x8000) == 2, "Reset_Page4_Bank2");
        expect(rom.mem_read(0xA000) == 3, "Reset_Page5_Bank3");
        // Mirror direction is the OPPOSITE of plain Konami: the
        // 0x4000-region content appears at 0xC000+, the 0x8000-region
        // content at 0x0000+. (Plain Konami puts bank0/1 at 0x0000+ and
        // bank2/3 at 0xC000+ -- asserted the other way around here.)
        expect(rom.mem_read(0xC000) == 0, "Reset_Slot6_Mirrors0x4000Region_Bank0");
        expect(rom.mem_read(0xE000) == 1, "Reset_Slot7_Mirrors0x6000Region_Bank1");
        expect(rom.mem_read(0x0000) == 2, "Reset_Slot0_Mirrors0x8000Region_Bank2");
        expect(rom.mem_read(0x2000) == 3, "Reset_Slot1_Mirrors0xA000Region_Bank3");
    }

    // --- Bank switching: canonical addresses AND the full 0x800-wide decode
    //     windows (SCC fact-sheet §2 / §9.7 arbitration); mirrors track
    //     BOTH directions live. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        rom.mem_write(0x5000, 4);  // page 2 (canonical)
        expect(rom.mem_read(0x4000) == 4 && rom.mem_read(0xC000) == 4,
               "Write0x5000_Page2AndMirrorSlot6_BothSwitch");
        rom.mem_write(0x5400, 5);  // page 2 (mid-window, 0x800-wide decode)
        expect(rom.mem_read(0x4000) == 5 && rom.mem_read(0xC000) == 5,
               "Write0x5400_MidWindowDecode_Switches");
        rom.mem_write(0x57FF, 6);  // page 2 (window top)
        expect(rom.mem_read(0x4000) == 6, "Write0x57FF_WindowTop_Switches");
        rom.mem_write(0x5800, 7);  // OUTSIDE the decode window -> ignored
        expect(rom.mem_read(0x4000) == 6, "Write0x5800_OutsideWindow_Ignored");

        rom.mem_write(0x7000, 8);  // page 3
        expect(rom.mem_read(0x6000) == 8 && rom.mem_read(0xE000) == 8,
               "Write0x7000_Page3AndMirrorSlot7_BothSwitch");
        rom.mem_write(0x9000, 9);  // page 4 (also the enable latch -- 9 disables)
        expect(rom.mem_read(0x8000) == 9 && rom.mem_read(0x0000) == 9,
               "Write0x9000_Page4AndMirrorSlot0_BothSwitch");
        rom.mem_write(0xB000, 10);  // page 5
        expect(rom.mem_read(0xA000) == 10 && rom.mem_read(0x2000) == 10,
               "Write0xB000_Page5AndMirrorSlot1_BothSwitch");
        rom.mem_write(0xB400, 11);  // page 5 mid-window
        expect(rom.mem_read(0xA000) == 11 && rom.mem_read(0x2000) == 11,
               "Write0xB400_MidWindowDecode_Switches");
    }

    // --- Writes below 0x5000 and at/above 0xC000 are ignored (contrast
    //     plain Konami's 0x6000 bound: 0x5000-0x57FF IS live here). ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        rom.mem_write(0x4000, 12);  // below 0x5000 -> ignored
        rom.mem_write(0x0000, 12);  // ignored
        rom.mem_write(0xC000, 12);  // at upper bound -> ignored
        rom.mem_write(0xF000, 12);  // ignored (even though 0xF000&0x1800==0x1000)
        expect(rom.mem_read(0x4000) == 0 && rom.mem_read(0x8000) == 2 && rom.mem_read(0x6000) == 1 &&
                   rom.mem_read(0xA000) == 3,
               "WritesOutside0x5000To0xBFFF_AllIgnored");
    }

    // --- Block mask: image-derived default only (NO plain-Konami 31
    //     override): a 16-bank image resolves request 20 via mask 15 -> 4. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper(16);
        rom.mem_write(0x5000, 20);  // 20 >= 16 -> & 15 = 4
        expect(rom.mem_read(0x4000) == 4, "BlockMask_ImageDerivedDefault_WrapsAtImageSize");
    }

    // --- SCC enable semantics (fact-sheet §9.6): masked compare
    //     (value & 0x3F) == 0x3F -- 0x3F AND 0xBF enable; 0x3E disables;
    //     decode anywhere in 0x9000-0x97FF. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        expect(!rom.scc_enabled(), "Enable_ResetDefault_Disabled");
        rom.mem_write(0x9000, 0x3F);
        expect(rom.scc_enabled(), "Enable_0x3F_Enables");
        rom.mem_write(0x9000, 0x3E);
        expect(!rom.scc_enabled(), "Enable_0x3E_Disables");
        rom.mem_write(0x9000, 0xBF);
        expect(rom.scc_enabled(), "Enable_0xBF_MaskedCompare_AlsoEnables");
        rom.mem_write(0x9400, 0x00);  // mid-window disable
        expect(!rom.scc_enabled(), "Enable_MidWindow0x9400_DecodeWorks");
        rom.mem_write(0x97FF, 0x3F);  // window top enable
        expect(rom.scc_enabled(), "Enable_WindowTop0x97FF_DecodeWorks");
        rom.mem_write(0x9800, 0x3F);  // outside the 0x9000-0x97FF latch window
        // (0x9800 with SCC enabled is a wave write, never the latch.)
        expect(rom.scc_enabled(), "Enable_0x9800_NotTheLatchWindow");
    }

    // --- The 0x9000 write's BOTH-EFFECTS rule (RomKonamiSCC.cc:108-123):
    //     enable state AND page-4 bank both change on the same write. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper(16);
        rom.mem_write(0x9000, 0x3F);  // enables SCC AND requests bank 0x3F
        expect(rom.scc_enabled(), "BothEffects_EnableApplied");
        // 0x3F >= 16 banks -> mask 15 -> bank 15; visible via the page-4
        // ROM WINDOW's mirror at 0x0000 (page 4 itself now answers via ROM
        // only outside 0x9800-0x9FFF; 0x8000 still shows it directly too).
        expect(rom.mem_read(0x8000) == 15 && rom.mem_read(0x0000) == 15,
               "BothEffects_BankSwitchAppliedBySameWrite");
        // Disable via 0x3E: bank switches again (0x3E & 15 = 14).
        rom.mem_write(0x9000, 0x3E);
        expect(!rom.scc_enabled() && rom.mem_read(0x8000) == 14,
               "BothEffects_DisableWriteAlsoBankSwitches");
    }

    // --- SCC register window at 0x9800-0x9FFF: routing, the 0x9900 mirror,
    //     disabled-SCC ROM readback, and the ROM-mirror-pages-
    //     never-expose-the-SCC rule. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        // Disabled: 0x9800 reads ROM (page 4 = bank 2 after reset).
        expect(rom.mem_read(0x9800) == 2, "SccWindow_Disabled_ReadsRom");
        rom.mem_write(0x9800, 0x55);  // disabled -> not a wave write, not a bank reg
        expect(rom.mem_read(0x9800) == 2, "SccWindow_Disabled_WriteIgnored");

        rom.mem_write(0x9000, 0x3F);  // enable
        rom.mem_write(0x9800, 0x55);  // ch1 wave[0]
        expect(rom.mem_read(0x9800) == 0x55, "SccWindow_Enabled_WaveWriteReadBack");
        expect(rom.scc().wave(0, 0) == 0x55, "SccWindow_WriteLandsInOwnedChip");
        // The 256-byte map mirrors 8x across the 2 kB window (addr & 0xFF).
        expect(rom.mem_read(0x9900) == 0x55, "SccWindow_0x9900Mirror_SameRegister");
        rom.mem_write(0x9A20, 0x66);  // mirror write -> ch2 wave[0] (offset 0x20)
        expect(rom.mem_read(0x9820) == 0x66, "SccWindow_MirrorWrite_LandsInBaseMap");
        // Freq/vol block via the window: offset 0x80 -> ch1 period low.
        rom.mem_write(0x9880, 0x63);
        expect(rom.scc().period(0) == 0x63, "SccWindow_FreqVolBlock_Reachable");
        // Deform-range READ side effect fires through the mapper (read, not
        // peek): deform becomes 0xFF.
        expect(rom.mem_read(0x9FE0) == 0xFF, "SccWindow_DeformRead_ReturnsFF");
        expect(rom.scc().deform() == 0xFF, "SccWindow_DeformRead_SideEffectFires");

        // ROM mirror pages NEVER expose the SCC: 0x1800-0x1FFF (the mirror
        // of 0x9800-0x9FFF) serves the mirrored ROM bank (page-4 bank = the
        // 0x3F enable write's own bank switch: 0x3F & 15 = 15).
        expect(rom.mem_read(0x1800) == 15, "RomMirrorPages_NeverExposeSccWindow");
        // And writes there are outside [0x5000, 0xC000) -> ignored entirely.
        rom.mem_write(0x1800, 0x77);
        expect(rom.scc().wave(0, 0) == 0x55, "RomMirrorPages_WritesIgnored");
    }

    // --- reset() clears the enable latch and resets the owned chip. ---
    {
        CartridgeKonamiScc rom = make_reset_mapper();
        rom.mem_write(0x9000, 0x3F);
        rom.mem_write(0x9880, 0x63);
        rom.mem_write(0x988F, 0x1F);
        rom.reset();
        expect(!rom.scc_enabled(), "Reset_ClearsEnableLatch");
        expect(rom.scc().period(0) == 0 && rom.scc().enable_bits() == 0,
               "Reset_ResetsOwnedChip");
        expect(rom.mem_read(0x8000) == 2, "Reset_RestoresBankLayout");
    }

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeKonamiScc rom(make_marker_image(16));
            rom.reset();
            rom.mem_write(0x5000, 4);
            rom.mem_write(0x9000, 0x3F);
            rom.mem_write(0x9800, 0x11);
            rom.mem_write(0x9880, 0x2B);
            rom.mem_write(0x988F, 0x01);
            rom.scc().advance_cycles(1000);
            std::vector<std::int32_t> observed;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x0800) {
                observed.push_back(rom.mem_read(static_cast<std::uint16_t>(addr)));
            }
            observed.push_back(rom.scc().sample());
            return observed;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeKonamiSccRom_Unit cases passed\n";
    return 0;
}
