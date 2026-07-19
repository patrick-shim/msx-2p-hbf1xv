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

#include "devices/memory/rom_device.h"

// Suite: Devices_MemoryRomDevice_Unit  (M13-S1)
//
// Deterministic isolation coverage for the read-only ROM window device:
//   - reads inside [base, base+size) return the image bytes at address - base;
//   - writes are ignored (still reads the original byte);
//   - addresses outside the window return open-bus 0xFF;
//   - the DISK-style page-1-only window (base 0x4000 size 0x4000) returns 0xFF in
//     pages 0/2/3 by construction (Sony_HB-F1XV.xml:174-175 rom_visibility).

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    using sony_msx::devices::memory::RomDevice;

    // --- BIOS-style window: base 0x0000, size 0x8000 (pages 0-1). ---
    {
        std::vector<std::uint8_t> image(0x8000);
        for (std::size_t i = 0; i < image.size(); ++i) {
            image[i] = static_cast<std::uint8_t>((i * 31u + 7u) & 0xFFu);
        }
        RomDevice rom(0x0000, 0x8000, image);

        expect(rom.mem_read(0x0000) == image[0], "Bios_ReadBase_ReturnsImageByte0");
        expect(rom.mem_read(0x1234) == image[0x1234], "Bios_ReadInterior_ReturnsImageByte");
        expect(rom.mem_read(0x7FFF) == image[0x7FFF], "Bios_ReadLast_ReturnsImageByte");

        // Writes are ignored (ROM).
        const std::uint8_t before = rom.mem_read(0x0100);
        rom.mem_write(0x0100, static_cast<std::uint8_t>(before ^ 0xFF));
        expect(rom.mem_read(0x0100) == before, "Bios_WriteIgnored_ReadsOriginal");

        // Outside the window -> open bus 0xFF.
        expect(rom.mem_read(0x8000) == 0xFF, "Bios_ReadPastWindow_OpenBus");
        expect(rom.mem_read(0xC000) == 0xFF, "Bios_ReadPage3_OpenBus");
    }

    // --- Kanji-driver-style window: base 0x4000, size 0x8000 (pages 1-2). ---
    {
        std::vector<std::uint8_t> image(0x8000);
        for (std::size_t i = 0; i < image.size(); ++i) {
            image[i] = static_cast<std::uint8_t>(i & 0xFFu);
        }
        RomDevice rom(0x4000, 0x8000, image);

        expect(rom.mem_read(0x3FFF) == 0xFF, "Kanji_BelowWindow_OpenBus");
        expect(rom.mem_read(0x4000) == image[0], "Kanji_ReadBase_ReturnsImageByte0");
        expect(rom.mem_read(0xBFFF) == image[0x7FFF], "Kanji_ReadLast_ReturnsImageByte");
        expect(rom.mem_read(0xC000) == 0xFF, "Kanji_AboveWindow_OpenBus");
    }

    // --- DISK-style page-1-only window: base 0x4000, size 0x4000. ---
    {
        std::vector<std::uint8_t> image(0x4000, 0xAB);
        image[0] = 0x11;
        image[0x3FFF] = 0x22;
        RomDevice rom(0x4000, 0x4000, image);

        // Page 1 (#4000-#7FFF): visible.
        expect(rom.mem_read(0x4000) == 0x11, "Disk_Page1Base_Visible");
        expect(rom.mem_read(0x7FFF) == 0x22, "Disk_Page1Last_Visible");
        // Pages 0/2/3: open bus (no mirroring).
        expect(rom.mem_read(0x0000) == 0xFF, "Disk_Page0_OpenBus");
        expect(rom.mem_read(0x8000) == 0xFF, "Disk_Page2_OpenBus");
        expect(rom.mem_read(0xC000) == 0xFF, "Disk_Page3_OpenBus");
    }

    // --- Default (image-less) construction is a 0xFF-filled window (missing
    //     asset determinism, A-7 is enforced machine-side but the device default
    //     must be open-bus). ---
    {
        RomDevice rom(0x0000, 0x4000);
        expect(rom.mem_read(0x0000) == 0xFF && rom.mem_read(0x2000) == 0xFF,
               "DefaultImage_AllOpenBus");
        // set_image re-loads and normalizes to the window size.
        std::vector<std::uint8_t> shorter(4, 0x55);
        rom.set_image(shorter);
        expect(rom.mem_read(0x0000) == 0x55 && rom.mem_read(0x0003) == 0x55,
               "SetImage_Prefix_Applied");
        expect(rom.mem_read(0x0004) == 0xFF, "SetImage_ShortImage_PaddedOpenBus");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
