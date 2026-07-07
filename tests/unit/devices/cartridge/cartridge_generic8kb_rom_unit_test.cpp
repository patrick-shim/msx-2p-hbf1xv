#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_generic8kb_rom.h"

// Suite: Devices_CartridgeGeneric8kbRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomGeneric8kB.cc:7-36 (behaviour
// reference only, never copied -- GPL isolation).

namespace {

using sony_msx::devices::cartridge::CartridgeGeneric8kbRom;
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
    // --- Load-time validation: positive multiple of 0x2000, no upper bound. ---
    expect(CartridgeGeneric8kbRom::is_valid_image_size(0x2000), "IsValid_OneBank_Accepted");
    expect(CartridgeGeneric8kbRom::is_valid_image_size(0x100000), "IsValid_512Banks_1MB_Accepted");
    expect(!CartridgeGeneric8kbRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeGeneric8kbRom::is_valid_image_size(0x1234), "IsValid_NonMultiple_Rejected");

    // --- reset(): slots 0,1 unmapped; slots 2-5 = banks 0-3; slots 6,7 unmapped. ---
    {
        CartridgeGeneric8kbRom rom(make_marker_image(8));
        expect(rom.mem_read(0x0000) == CartridgeRomWindow::kOpenBus, "Reset_Slot0_Unmapped");
        expect(rom.mem_read(0x2000) == CartridgeRomWindow::kOpenBus, "Reset_Slot1_Unmapped");
        expect(rom.mem_read(0x4000) == 0, "Reset_Slot2_Bank0");
        expect(rom.mem_read(0x6000) == 1, "Reset_Slot3_Bank1");
        expect(rom.mem_read(0x8000) == 2, "Reset_Slot4_Bank2");
        expect(rom.mem_read(0xA000) == 3, "Reset_Slot5_Bank3");
        expect(rom.mem_read(0xC000) == CartridgeRomWindow::kOpenBus, "Reset_Slot6_Unmapped");
        expect(rom.mem_read(0xE000) == CartridgeRomWindow::kOpenBus, "Reset_Slot7_Unmapped");
    }

    // --- mem_write: writable at ANY address; slot = addr >> 13; full byte = bank. ---
    {
        CartridgeGeneric8kbRom rom(make_marker_image(8));
        rom.mem_write(0x0000, 5);  // slot 0
        expect(rom.mem_read(0x0000) == 5, "Write_Slot0_ViaAddress0x0000");
        rom.mem_write(0xE000, 7);  // slot 7
        expect(rom.mem_read(0xE000) == 7, "Write_Slot7_ViaAddress0xE000");
        rom.mem_write(0x4000, 6);  // slot 2, overrides reset default
        expect(rom.mem_read(0x4000) == 6, "Write_Slot2_OverridesResetDefault");
    }

    // --- reset() after bank-switching restores the documented default. ---
    {
        CartridgeGeneric8kbRom rom(make_marker_image(8));
        rom.mem_write(0x4000, 7);
        expect(rom.mem_read(0x4000) == 7, "PreReset_BankSwitched");
        rom.reset();
        expect(rom.mem_read(0x4000) == 0, "Reset_RestoresDefault_Bank0AtSlot2");
    }

    expect(CartridgeGeneric8kbRom(make_marker_image(8)).mapper_type() == CartridgeMapperType::Generic8kB,
           "MapperType_ReportsGeneric8kB");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeGeneric8kbRom rom(make_marker_image(8));
            rom.mem_write(0x4000, 200);
            rom.mem_write(0x0000, 3);
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
    std::cout << "All Devices_CartridgeGeneric8kbRom_Unit cases passed\n";
    return 0;
}
