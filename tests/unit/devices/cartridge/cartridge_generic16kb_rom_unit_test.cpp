#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_generic16kb_rom.h"

// Suite: Devices_CartridgeGeneric16kbRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomGeneric16kB.cc:6-24
// (behaviour reference only, never copied -- GPL isolation). Uses the shared
// 8 KB-granularity CartridgeRomWindow via logical-16KB-bank window-slot
// pairs (planner §2.2).

namespace {

using sony_msx::devices::cartridge::CartridgeGeneric16kbRom;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeRomWindow;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// A synthetic image with `num_8k_banks` 8 KB banks (bank N's every byte == N)
// -- read back as 16 KB logical banks (low half = 2n, high half = 2n+1).
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
    expect(CartridgeGeneric16kbRom::is_valid_image_size(0x4000), "IsValid_OneBank_Accepted");
    expect(CartridgeGeneric16kbRom::is_valid_image_size(0x40000), "IsValid_16Banks_1MB_Accepted");
    expect(!CartridgeGeneric16kbRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeGeneric16kbRom::is_valid_image_size(0x2000), "IsValid_8KBOnly_Rejected");
    expect(!CartridgeGeneric16kbRom::is_valid_image_size(0x6000), "IsValid_NonMultipleOf16KB_Rejected");

    // --- reset(): bank0 unmapped; bank1 = image bank0 (16KB); bank2 = image
    //     bank1 (16KB); bank3 unmapped (RomGeneric16kB.cc:12-18). 8 8kB banks
    //     -> 4 logical 16kB banks, image banks {0,1}=logical0, {2,3}=logical1. ---
    {
        CartridgeGeneric16kbRom rom(make_marker_image(8));
        expect(rom.mem_read(0x0000) == CartridgeRomWindow::kOpenBus, "Reset_LogicalBank0_Unmapped");
        expect(rom.mem_read(0x4000) == 0, "Reset_LogicalBank1_LowHalf_ImageBank0");
        expect(rom.mem_read(0x6000) == 1, "Reset_LogicalBank1_HighHalf_ImageBank1");
        expect(rom.mem_read(0x8000) == 2, "Reset_LogicalBank2_LowHalf_ImageBank2");
        expect(rom.mem_read(0xA000) == 3, "Reset_LogicalBank2_HighHalf_ImageBank3");
        expect(rom.mem_read(0xC000) == CartridgeRomWindow::kOpenBus, "Reset_LogicalBank3_Unmapped");
    }

    // --- mem_write: bank = addr >> 14 (0-3); full byte = requested 16KB bank
    //     index -> image banks 2*val / 2*val+1. ---
    {
        CartridgeGeneric16kbRom rom(make_marker_image(8));
        rom.mem_write(0x0000, 3);  // logical bank 0 -> image banks 6,7
        expect(rom.mem_read(0x0000) == 6, "Write_LogicalBank0_LowHalf_ImageBank6");
        expect(rom.mem_read(0x2000) == 7, "Write_LogicalBank0_HighHalf_ImageBank7");
        rom.mem_write(0xC000, 0);  // logical bank 3 -> image banks 0,1
        expect(rom.mem_read(0xC000) == 0, "Write_LogicalBank3_LowHalf_ImageBank0");
        expect(rom.mem_read(0xE000) == 1, "Write_LogicalBank3_HighHalf_ImageBank1");
    }

    // --- reset() after bank-switching restores the documented default. ---
    {
        CartridgeGeneric16kbRom rom(make_marker_image(8));
        rom.mem_write(0x4000, 3);
        expect(rom.mem_read(0x4000) == 6, "PreReset_BankSwitched");
        rom.reset();
        expect(rom.mem_read(0x4000) == 0, "Reset_RestoresDefault");
    }

    expect(CartridgeGeneric16kbRom(make_marker_image(4)).mapper_type() == CartridgeMapperType::Generic16kB,
           "MapperType_ReportsGeneric16kB");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeGeneric16kbRom rom(make_marker_image(8));
            rom.mem_write(0x8000, 3);
            rom.mem_write(0x0000, 1);
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
    std::cout << "All Devices_CartridgeGeneric16kbRom_Unit cases passed\n";
    return 0;
}
