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

#include "devices/cartridge/cartridge_mirrored_rom.h"

// Suite: Devices_CartridgeMirroredRom_Unit (M19-S2, backlog B7)
//
// Grounds references/openmsx-21.0/src/memory/RomPlain.cc (MIRRORED case,
// behaviour reference only, never copied -- GPL isolation): load-time size
// validation (A-M19-7, RomPlain.cc:39-43), full mirror placement with no
// bank-switch registers (A-M19-8).

namespace {

using sony_msx::devices::cartridge::CartridgeMirroredRom;
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
    // --- Load-time validation (A-M19-7): positive multiple of 0x2000, <= 0x10000. ---
    expect(CartridgeMirroredRom::is_valid_image_size(0x2000), "IsValid_OneBank_8KB_Accepted");
    expect(CartridgeMirroredRom::is_valid_image_size(0x4000), "IsValid_TwoBanks_16KB_Accepted");
    expect(CartridgeMirroredRom::is_valid_image_size(0x10000), "IsValid_EightBanks_64KB_Accepted");
    expect(!CartridgeMirroredRom::is_valid_image_size(0), "IsValid_Zero_Rejected");
    expect(!CartridgeMirroredRom::is_valid_image_size(0x1000), "IsValid_NonMultipleOf8KB_Rejected");
    expect(!CartridgeMirroredRom::is_valid_image_size(0x2001), "IsValid_OneByteOver_Rejected");
    expect(!CartridgeMirroredRom::is_valid_image_size(0x12000), "IsValid_LargerThan64KB_Rejected");

    // --- A-M19-8 worked example: 16 KB image (nrBlocks=2) mirrors 4x across
    //     the 64 KB window: window-slots {0,2,4,6} -> bank0, {1,3,5,7} -> bank1. ---
    {
        CartridgeMirroredRom rom(make_marker_image(2));
        bool ok = true;
        for (int s = 0; s < 8; ++s) {
            const std::uint16_t addr = static_cast<std::uint16_t>(s * 0x2000);
            const std::uint8_t expected_bank = static_cast<std::uint8_t>(s % 2);
            if (rom.mem_read(addr) != expected_bank) {
                ok = false;
            }
        }
        expect(ok, "SixteenKbImage_MirrorsFourTimes_AcrossSixtyFourKB");
    }

    // --- Bank 0 at 0x0000 (no placement ambiguity, A-M19-8). ---
    {
        CartridgeMirroredRom rom(make_marker_image(4));
        expect(rom.mem_read(0x0000) == 0, "Bank0_AtAddressZero");
        expect(rom.mem_read(0x2000) == 1, "Bank1_AtAddress0x2000");
        expect(rom.mem_read(0x4000) == 2, "Bank2_AtAddress0x4000");
        expect(rom.mem_read(0x6000) == 3, "Bank3_AtAddress0x6000");
    }

    // --- Read-only: writes never change the mapping. ---
    {
        CartridgeMirroredRom rom(make_marker_image(2));
        const std::uint8_t before = rom.mem_read(0x0000);
        rom.mem_write(0x0000, 0xAA);
        rom.mem_write(0x6000, 0x55);
        expect(rom.mem_read(0x0000) == before, "MemWrite_NoOp_MappingUnchanged");
    }

    // --- reset() is idempotent (fixed placement, no runtime state to lose). ---
    {
        CartridgeMirroredRom rom(make_marker_image(2));
        const std::uint8_t before = rom.mem_read(0x2000);
        rom.reset();
        expect(rom.mem_read(0x2000) == before, "Reset_Idempotent_MappingUnchanged");
    }

    expect(sony_msx::devices::cartridge::CartridgeMirroredRom(make_marker_image(1)).mapper_type() ==
               sony_msx::devices::cartridge::CartridgeMapperType::Mirrored,
           "MapperType_ReportsMirrored");

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeMirroredRom rom(make_marker_image(3));
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
    std::cout << "All Devices_CartridgeMirroredRom_Unit cases passed\n";
    return 0;
}
