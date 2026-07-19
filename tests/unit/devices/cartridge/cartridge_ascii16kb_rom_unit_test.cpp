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

#include "devices/cartridge/cartridge_ascii16kb_rom.h"

// Suite: Devices_CartridgeAscii16kbRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomAscii16kB.cc (header comment
// + code lines 16-45; behaviour reference only, never copied -- GPL
// isolation). R-M19-2: the "both middle logical banks default to the SAME
// image bank at reset" quirk must NOT be silently "fixed" to a more intuitive
// sequential 0/1 pattern.

namespace {

using sony_msx::devices::cartridge::CartridgeAscii16kbRom;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeRomWindow;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> make_marker_image(const unsigned num_8k_banks) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_8k_banks) * CartridgeRomWindow::kBankSize);
    for (unsigned bank = 0; bank < num_8k_banks; ++bank) {
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[static_cast<std::size_t>(bank) * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- Load-time validation: positive multiple of 0x4000 (16 KB). ---
    expect(CartridgeAscii16kbRom::is_valid_image_size(0x4000), "IsValid_OneBank_Accepted");
    expect(!CartridgeAscii16kbRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeAscii16kbRom::is_valid_image_size(0x2000), "IsValid_8KBOnly_Rejected");

    // --- CRITICAL QUIRK (R-M19-2): reset() -> bank0 unmapped; bank1 = image
    //     bank0 (16KB, i.e. image 8kB banks 0/1); bank2 = image bank0 TOO
    //     (NOT bank1) -- both middle banks read IDENTICAL content; bank3
    //     unmapped (RomAscii16kB.cc:24-27). ---
    {
        CartridgeAscii16kbRom rom(make_marker_image(8));
        expect(rom.mem_read(0x0000) == CartridgeRomWindow::kOpenBus, "Reset_LogicalBank0_Unmapped");
        expect(rom.mem_read(0x4000) == 0, "Reset_LogicalBank1_LowHalf_ImageBank0");
        expect(rom.mem_read(0x6000) == 1, "Reset_LogicalBank1_HighHalf_ImageBank1");
        expect(rom.mem_read(0x8000) == 0, "Reset_LogicalBank2_LowHalf_ImageBank0_SAME_AS_Bank1");
        expect(rom.mem_read(0xA000) == 1, "Reset_LogicalBank2_HighHalf_ImageBank1_SAME_AS_Bank1");
        expect(rom.mem_read(0xC000) == CartridgeRomWindow::kOpenBus, "Reset_LogicalBank3_Unmapped");
        // Byte-for-byte identical content across the two middle logical banks.
        bool identical = true;
        for (std::uint32_t off = 0; off < 0x4000; off += 0x400) {
            if (rom.mem_read(static_cast<std::uint16_t>(0x4000 + off)) !=
                rom.mem_read(static_cast<std::uint16_t>(0x8000 + off))) {
                identical = false;
            }
        }
        expect(identical, "Reset_BothMiddleBanks_ByteForByteIdentical");
    }

    // --- mem_write: 0x6000 <= addr < 0x7800 AND (addr & 0x0800) == 0.
    //     region = ((addr>>12)&1)+1 -- 0x6xxx -> logical bank1, 0x7xxx ->
    //     logical bank2 (RomAscii16kB.cc:30-36). ---
    {
        CartridgeAscii16kbRom rom(make_marker_image(8));
        rom.mem_write(0x6000, 2);  // region1 (logical bank1) -> image banks 4,5
        expect(rom.mem_read(0x4000) == 4, "Write_0x6000_LogicalBank1_LowHalf_ImageBank4");
        expect(rom.mem_read(0x6000) == 5, "Write_0x6000_LogicalBank1_HighHalf_ImageBank5");
        rom.mem_write(0x7000, 3);  // region2 (logical bank2) -> image banks 6,7
        expect(rom.mem_read(0x8000) == 6, "Write_0x7000_LogicalBank2_LowHalf_ImageBank6");
        expect(rom.mem_read(0xA000) == 7, "Write_0x7000_LogicalBank2_HighHalf_ImageBank7");
    }

    // --- Excluded addresses (0x6800-0x6FFF, 0x7800-0x7FFF, and anything
    //     outside 0x6000-0x7800) are ignored. ---
    {
        CartridgeAscii16kbRom rom(make_marker_image(8));
        rom.mem_write(0x6800, 5);  // bit 0x0800 set -> excluded
        rom.mem_write(0x7800, 5);  // out of [0x6000,0x7800) -> excluded
        rom.mem_write(0x5000, 5);  // below range -> excluded
        rom.mem_write(0x8000, 5);  // above range -> excluded
        expect(rom.mem_read(0x4000) == 0, "Write_0x6800_Ignored_StillDefault");
        expect(rom.mem_read(0x8000) == 0, "Write_0x7800_Ignored_StillDefault");
    }

    // --- reset() after bank-switching restores the documented (quirky) default. ---
    {
        CartridgeAscii16kbRom rom(make_marker_image(8));
        rom.mem_write(0x6000, 3);
        expect(rom.mem_read(0x4000) == 6, "PreReset_BankSwitched");
        rom.reset();
        expect(rom.mem_read(0x4000) == 0 && rom.mem_read(0x8000) == 0, "Reset_RestoresQuirkyDefault");
    }

    expect(CartridgeAscii16kbRom(make_marker_image(4)).mapper_type() == CartridgeMapperType::Ascii16kB,
           "MapperType_ReportsAscii16kB");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeAscii16kbRom rom(make_marker_image(8));
            rom.mem_write(0x6000, 2);
            rom.mem_write(0x7000, 1);
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
    std::cout << "All Devices_CartridgeAscii16kbRom_Unit cases passed\n";
    return 0;
}
