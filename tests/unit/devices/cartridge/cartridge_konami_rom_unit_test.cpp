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

#include "devices/cartridge/cartridge_konami_rom.h"

// Suite: Devices_CartridgeKonamiRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomKonami.cc (behaviour
// reference only, never copied -- GPL isolation). No SCC (backlog G1).
//
// GROUNDING FINDING (surfaced explicitly, not silently "corrected" without
// disclosure -- mirrors the DEC-0012/§2.8 precedent of verifying prose
// against the literal source before trusting it): the task brief's own
// shorthand ("slots 0/1/2 stay PERMANENTLY fixed at bank 0") is an
// OVERSTATEMENT not supported by a literal reading of RomKonami.cc:38-52,
// 61-67. Precisely, per the actual algorithm:
//   - writeMem only ever calls bankSwitch(addr>>13, value) for addr in
//     [0x6000, 0xC000), i.e. addr>>13 in {3,4,5} ONLY -- page 2 is NEVER
//     reachable from any write address (0x4000-0x5FFF, "fixed at segment 0",
//     RomKonami.cc:63).
//   - bankSwitch(2, block) is therefore called EXACTLY ONCE, from reset()
//     (block=0) -- so window-slot 2 AND its mirror window-slot 0
//     (bankSwitch's "page==2" branch mirrors into page-2=0) are BOTH
//     genuinely fixed at bank 0 for the entire session.
//   - bankSwitch(3, block) mirrors into window-slot 1 (page-2=1) on EVERY
//     write to page 3 (addr 0x6000-0x7FFF) -- so slot 1 is NOT fixed, it
//     tracks slot 3's live value.
//   - Symmetrically, slot 6 tracks slot 4 (page 4, addr 0x8000-0x9FFF) and
//     slot 7 tracks slot 5 (page 5, addr 0xA000-0xBFFF) -- neither is fixed.
// This test asserts the CORRECT, literally-grounded behaviour (slots 0 and 2
// permanently fixed; slots 1/3, 4/6, 5/7 each paired-but-live), not the
// task brief's imprecise "slots 0/1/2 all fixed" shorthand. See the M19
// implementation report for the full citation.

namespace {

using sony_msx::devices::cartridge::CartridgeKonamiRom;
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
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[static_cast<std::size_t>(bank) * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- Load-time validation: positive multiple of 0x2000, no upper bound
    //     (an oversized image is loaded anyway, never rejected -- matches
    //     openMSX's own non-fatal warning, RomKonami.cc:27-31). ---
    expect(CartridgeKonamiRom::is_valid_image_size(0x2000), "IsValid_OneBank_Accepted");
    expect(CartridgeKonamiRom::is_valid_image_size(0x50000), "IsValid_320KB_OversizedForKonami_StillAccepted");
    expect(!CartridgeKonamiRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeKonamiRom::is_valid_image_size(0x1000), "IsValid_NonMultiple_Rejected");

    // --- Constructor deliberately does NOT reset() (RomKonami.cc:33-35): the
    //     window starts entirely unmapped until reset() is called explicitly. ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        expect(rom.mem_read(0x8000) == CartridgeRomWindow::kOpenBus,
               "Construct_NoImplicitReset_WindowStartsUnmapped");
    }

    // --- reset(): bank_switch(2,0); bank_switch(3,1); bank_switch(4,2);
    //     bank_switch(5,3) (RomKonami.cc:54-59), WITH the mirror applied. ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        rom.reset();
        expect(rom.mem_read(0x4000) == 0, "Reset_Slot2_Page4000_Bank0");
        expect(rom.mem_read(0x6000) == 1, "Reset_Slot3_Page6000_Bank1");
        expect(rom.mem_read(0x8000) == 2, "Reset_Slot4_Page8000_Bank2");
        expect(rom.mem_read(0xA000) == 3, "Reset_Slot5_PageA000_Bank3");
        // Mirrors established at reset time.
        expect(rom.mem_read(0x0000) == 0, "Reset_Slot0_MirrorsSlot2_Bank0");
        expect(rom.mem_read(0x2000) == 1, "Reset_Slot1_MirrorsSlot3_Bank1_NOT_Bank0");
        expect(rom.mem_read(0xC000) == 2, "Reset_Slot6_MirrorsSlot4_Bank2");
        expect(rom.mem_read(0xE000) == 3, "Reset_Slot7_MirrorsSlot5_Bank3");
    }

    // --- CORRECTED quirk (see file header finding): slots 0 AND 2 are the
    //     ones genuinely fixed at bank 0 forever -- writing to EVERY address
    //     in 0x6000-0xBFFF with varying values never moves them. ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        rom.reset();
        for (std::uint32_t addr = 0x6000; addr < 0xC000; addr += 0x0200) {
            rom.mem_write(static_cast<std::uint16_t>(addr), static_cast<std::uint8_t>((addr / 7) & 0x1F));
        }
        expect(rom.mem_read(0x0000) == 0, "AfterManyWrites_Slot0_StillFixedAtBank0");
        expect(rom.mem_read(0x4000) == 0, "AfterManyWrites_Slot2_StillFixedAtBank0");
    }

    // --- Slot 1 tracks slot 3's live value (mirror on EVERY page-3 write,
    //     NOT independently fixed -- the corrected finding above). ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        rom.reset();
        rom.mem_write(0x6000, 17);  // page 3
        expect(rom.mem_read(0x6000) == 17, "Write_Page3_Slot3_Updates");
        expect(rom.mem_read(0x2000) == 17, "Write_Page3_MirrorSlot1_TracksLiveValue");
    }

    // --- Slot 6 tracks slot 4; slot 7 tracks slot 5 (symmetric mirror). ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        rom.reset();
        rom.mem_write(0x8000, 9);   // page 4
        rom.mem_write(0xA000, 13);  // page 5
        expect(rom.mem_read(0x8000) == 9 && rom.mem_read(0xC000) == 9,
               "Write_Page4_Slot4AndMirrorSlot6_BothUpdate");
        expect(rom.mem_read(0xA000) == 13 && rom.mem_read(0xE000) == 13,
               "Write_Page5_Slot5AndMirrorSlot7_BothUpdate");
    }

    // --- Writes outside [0x6000, 0xC000) are ignored (including the
    //     "page at 4000 is fixed" address range 0x4000-0x5FFF itself). ---
    {
        CartridgeKonamiRom rom(make_marker_image(32));
        rom.reset();
        rom.mem_write(0x4000, 30);  // fixed page -- ignored
        rom.mem_write(0x0000, 30);  // below range -- ignored
        rom.mem_write(0xC000, 30);  // at/above range -- ignored (exclusive upper bound)
        rom.mem_write(0xE000, 30);  // above range -- ignored
        expect(rom.mem_read(0x4000) == 0, "Write_0x4000_Ignored_FixedPage");
        expect(rom.mem_read(0x0000) == 0, "Write_0x0000_Ignored_BelowRange");
        expect(rom.mem_read(0xC000) == 2, "Write_0xC000_Ignored_AtUpperBound");
        expect(rom.mem_read(0xE000) == 3, "Write_0xE000_Ignored_AboveRange");
    }

    // --- CRITICAL SUBTLETY (A-M19-6): an oversized (>256KB) Konami image is
    //     NOT capped -- every byte-value bank request 0..255 against a
    //     larger image is used UNMASKED when it satisfies block < nrBlocks
    //     directly (mask=31 never engages beyond the image's own size). ---
    {
        CartridgeKonamiRom rom(make_marker_image(40));  // 320 KB, > 256 KB
        rom.reset();
        rom.mem_write(0x6000, 35);  // 35 < 40(nrBlocks) -> direct, even though 35 > 31(mask)
        expect(rom.mem_read(0x6000) == 35, "OversizedImage_MaskNeverEngages_BankRequestBeyond31Works");
    }

    expect(CartridgeKonamiRom(make_marker_image(32)).mapper_type() == CartridgeMapperType::Konami,
           "MapperType_ReportsKonami");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeKonamiRom rom(make_marker_image(32));
            rom.reset();
            rom.mem_write(0x6000, 10);
            rom.mem_write(0x8000, 20);
            rom.mem_write(0xA000, 30);
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x1000) {
                reads.push_back(rom.mem_read(static_cast<std::uint16_t>(addr)));
            }
            return reads;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeKonamiRom_Unit cases passed\n";
    return 0;
}
