#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_ascii8kb_rom.h"

// Suite: Devices_CartridgeAscii8kbRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomAscii8kB.cc (header comment
// lines 1-10 + code lines 18-52; behaviour reference only, never copied --
// GPL isolation).

namespace {

using sony_msx::devices::cartridge::CartridgeAscii8kbRom;
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
    // --- Load-time validation: positive multiple of 0x2000. ---
    expect(CartridgeAscii8kbRom::is_valid_image_size(0x2000), "IsValid_OneBank_Accepted");
    expect(!CartridgeAscii8kbRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeAscii8kbRom::is_valid_image_size(0x3000), "IsValid_NonMultiple_Rejected");

    // --- reset(): slots 0,1 unmapped; slots 2-5 ALL = image bank 0; slots 6,7
    //     unmapped (RomAscii8kB.cc:24-33). ---
    {
        CartridgeAscii8kbRom rom(make_marker_image(4));
        expect(rom.mem_read(0x0000) == CartridgeRomWindow::kOpenBus, "Reset_Slot0_Unmapped");
        expect(rom.mem_read(0x2000) == CartridgeRomWindow::kOpenBus, "Reset_Slot1_Unmapped");
        expect(rom.mem_read(0x4000) == 0, "Reset_Slot2_Bank0");
        expect(rom.mem_read(0x6000) == 0, "Reset_Slot3_Bank0");
        expect(rom.mem_read(0x8000) == 0, "Reset_Slot4_Bank0");
        expect(rom.mem_read(0xA000) == 0, "Reset_Slot5_Bank0");
        expect(rom.mem_read(0xC000) == CartridgeRomWindow::kOpenBus, "Reset_Slot6_Unmapped");
        expect(rom.mem_read(0xE000) == CartridgeRomWindow::kOpenBus, "Reset_Slot7_Unmapped");
    }

    // --- mem_write: only 0x6000 <= addr < 0x8000; region = ((addr>>11)&3)+2. ---
    {
        CartridgeAscii8kbRom rom(make_marker_image(4));
        rom.mem_write(0x6000, 1);  // region 2 -> slot 2 (0x4000-0x5FFF)
        expect(rom.mem_read(0x4000) == 1, "Write_0x6000_LandsRegion2_Slot4000");
        rom.mem_write(0x6800, 2);  // region 3 -> slot 3 (0x6000-0x7FFF)
        expect(rom.mem_read(0x6000) == 2, "Write_0x6800_LandsRegion3_Slot6000");
        rom.mem_write(0x7000, 3);  // region 4 -> slot 4 (0x8000-0x9FFF)
        expect(rom.mem_read(0x8000) == 3, "Write_0x7000_LandsRegion4_Slot8000");
        rom.mem_write(0x7800, 0);  // region 5 -> slot 5 (0xA000-0xBFFF)
        expect(rom.mem_read(0xA000) == 0, "Write_0x7800_LandsRegion5_SlotA000");
    }

    // --- Writes outside 0x6000-0x7FFF are ignored. ---
    {
        CartridgeAscii8kbRom rom(make_marker_image(4));
        rom.mem_write(0x0000, 3);
        rom.mem_write(0x4000, 3);
        rom.mem_write(0x8000, 3);
        rom.mem_write(0xC000, 3);
        expect(rom.mem_read(0x0000) == CartridgeRomWindow::kOpenBus, "Write_0x0000_Ignored");
        expect(rom.mem_read(0x4000) == 0, "Write_0x4000_Ignored_StillBank0");
        expect(rom.mem_read(0x8000) == 0, "Write_0x8000_Ignored_StillBank0");
        expect(rom.mem_read(0xC000) == CartridgeRomWindow::kOpenBus, "Write_0xC000_Ignored");
    }

    // --- reset() after bank-switching restores the documented default. ---
    {
        CartridgeAscii8kbRom rom(make_marker_image(4));
        rom.mem_write(0x6000, 3);
        expect(rom.mem_read(0x4000) == 3, "PreReset_BankSwitched");
        rom.reset();
        expect(rom.mem_read(0x4000) == 0, "Reset_RestoresDefault");
    }

    expect(CartridgeAscii8kbRom(make_marker_image(4)).mapper_type() == CartridgeMapperType::Ascii8kB,
           "MapperType_ReportsAscii8kB");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeAscii8kbRom rom(make_marker_image(4));
            rom.mem_write(0x6000, 2);
            rom.mem_write(0x7800, 1);
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x800) {
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
    std::cout << "All Devices_CartridgeAscii8kbRom_Unit cases passed\n";
    return 0;
}
