#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvSlotMap_Unit  (M13-S3/S4)
//
// Verifies the M13 slot population + the authentic reset default, using the
// non-perturbing debug bus/IO seams (route through SlotBus/IoBus exactly like
// the CPU) so each (primary, sub, page) cell can be resolved directly:
//   - authentic reset: #A8 == 0 (updated from the M11 bring-up #A8 == 0xFF, R-1);
//   - BOTH primary slots 0 and 3 report expanded (A-6, corrects M11);
//   - each cell resolves to the expected device against the real loaded images:
//       BIOS 0-0 p0-1, SUB 3-1 p0, Kanji 3-1 p1-2, DISK 3-2 p1 (0xFF in p0/2/3),
//       FM-MUSIC 3-3 p1;
//   - the 64 KB mapper RAM at 3-0 read/writes hit DRAM via the current segments,
//     and a segment switch reroutes the physical cell;
//   - #FFFF sub-slot decode + 0xFF^reg readback is active in both slots 0 and 3;
//   - cold-boot DRAM content matches the A-5 alternating 00/FF pattern.
//
// The real bios directory is injected as an absolute path by CMake so the test
// is independent of the working directory.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_rom(const char* filename) {
    const std::filesystem::path path = std::filesystem::path(SONY_MSX_BIOS_DIR) / filename;
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

std::uint8_t a5_byte(const std::size_t index) {
    const std::size_t p = index & 0x1FFu;
    if (p < 256) {
        return (p & 1u) ? 0xFF : 0x00;
    }
    return (p & 1u) ? 0x00 : 0xFF;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    // Real assets must be present for this test's device-content cross-checks.
    if (!machine.rom_diagnostics().empty()) {
        std::cerr << "Case failed: RomAssets_Present_NoDiagnostics\n";
        for (const auto& note : machine.rom_diagnostics()) {
            std::cerr << "  " << note << "\n";
        }
        return 1;
    }

    // --- Authentic reset default: #A8 == 0 (M13-S4, discharges M11 R-1). ---
    expect(machine.debug_io_read(0xA8) == 0x00, "ResetPrimarySelect_A8_IsZero_Authentic");

    // --- Both primary slots 0 and 3 are expanded (A-6). ---
    expect(machine.slot_expanded(0) && machine.slot_expanded(3), "BothSlots0And3_Expanded");

    // --- Cold-boot DRAM content is the A-5 alternating 00/FF pattern. ---
    {
        bool pattern_ok = true;
        for (std::uint32_t addr = 0; addr <= 0x0FFF; ++addr) {  // sample the first 4 KB
            if (machine.read_memory(static_cast<std::uint16_t>(addr)) != a5_byte(addr)) {
                pattern_ok = false;
                break;
            }
        }
        expect(pattern_ok, "ColdBootDram_MatchesA5Pattern");
    }

    const std::vector<std::uint8_t> bios = read_rom("f1xvbios.rom");
    const std::vector<std::uint8_t> sub = read_rom("f1xvext.rom");
    const std::vector<std::uint8_t> kanji = read_rom("f1xvkdr.rom");
    const std::vector<std::uint8_t> disk = read_rom("f1xvdisk.rom");
    const std::vector<std::uint8_t> fmmusic = read_rom("f1xvmus.rom");

    // --- BIOS at slot 0-0, pages 0-1 (#A8 all slot 0, sub 0). ---
    machine.debug_io_write(0xA8, 0x00);       // every page -> primary slot 0
    machine.debug_bus_write(0xFFFF, 0x00);    // slot-0 sub-slot reg -> sub 0
    expect(machine.debug_bus_read(0x0000) == bios[0x0000] &&
               machine.debug_bus_read(0x1234) == bios[0x1234],
           "Bios_Slot00_Page0_ReturnsImageBytes");
    expect(machine.debug_bus_read(0x4000) == bios[0x4000] &&
               machine.debug_bus_read(0x7FFF) == bios[0x7FFF],
           "Bios_Slot00_Page1_ReturnsImageBytes");
    // #FFFF readback active in expanded slot 0: 0xFF ^ 0 = 0xFF.
    expect(machine.debug_bus_read(0xFFFF) == 0xFF, "Ffff_ExpandedSlot0_Readback_IsFF");

    // --- SUB at slot 3-1 page 0. Route page0 & page3 to slot 3; set page0 sub 1. ---
    machine.debug_io_write(0xA8, 0xC3);       // page0=11(slot3), page3=11(slot3)
    machine.debug_bus_write(0xFFFF, 0x01);    // slot-3 sub reg: page0 sub 1, page3 sub 0
    expect(machine.debug_bus_read(0x0000) == sub[0x0000] &&
               machine.debug_bus_read(0x3FFF) == sub[0x3FFF],
           "Sub_Slot31_Page0_ReturnsImageBytes");

    // --- Kanji driver at slot 3-1 pages 1-2 (page1 & page2 sub 1). ---
    machine.debug_io_write(0xA8, 0xFF);       // all pages slot 3
    machine.debug_bus_write(0xFFFF, 0x14);    // page1 sub1 (bits3-2=01), page2 sub1 (bits5-4=01)
    expect(machine.debug_bus_read(0x4000) == kanji[0x0000] &&
               machine.debug_bus_read(0x8000) == kanji[0x4000] &&
               machine.debug_bus_read(0xBFFF) == kanji[0x7FFF],
           "Kanji_Slot31_Pages1And2_ReturnsImageBytes");

    // --- DISK at slot 3-2 page 1; page-1-only visibility. ---
    machine.debug_io_write(0xA8, 0xFF);
    // subreg 0x0A = 0b0000_1010 -> page0 sub2 (bits1-0=10), page1 sub2 (bits3-2=10).
    machine.debug_bus_write(0xFFFF, 0x0A);
    expect(machine.debug_bus_read(0x4000) == disk[0x0000] &&
               machine.debug_bus_read(0x7FFF) == disk[0x3FFF],
           "Disk_Slot32_Page1_ReturnsImageBytes");
    // DISK ROM is page-1-only: page 0 of slot 3-2 is unattached -> open bus.
    expect(machine.debug_bus_read(0x0000) == 0xFF, "Disk_Slot32_Page0_OpenBus");

    // --- FM-MUSIC at slot 3-3 page 1 (page1 sub 3). ---
    machine.debug_io_write(0xA8, 0xFF);
    machine.debug_bus_write(0xFFFF, 0x0C);    // page1 sub3 (bits3-2=11)
    expect(machine.debug_bus_read(0x4000) == fmmusic[0x0000] &&
               machine.debug_bus_read(0x7FFF) == fmmusic[0x3FFF],
           "FmMusic_Slot33_Page1_ReturnsImageBytes");

    // --- RAM mapper at slot 3-0: read/write hits DRAM via current segments. ---
    machine.map_flat_ram();  // all pages slot 3-0, linear segments {0,1,2,3}
    machine.debug_bus_write(0x2345, 0x5A);
    expect(machine.debug_bus_read(0x2345) == 0x5A && machine.read_memory(0x2345) == 0x5A,
           "Ram_Slot30_FlatConfig_WriteReadThroughDram");
    // A #FFFF write in expanded slot 3 (page3 sub) does NOT hit RAM; it is the
    // sub-slot register readback 0xFF ^ reg.
    machine.debug_bus_write(0xFFFF, 0x00);
    expect(machine.debug_bus_read(0xFFFF) == 0xFF, "Ffff_ExpandedSlot3_Readback_IsFF");

    // --- Segment switch reroutes the physical RAM cell (mapper consume). ---
    // Point page 0 at segment 3 (physical 0xC000 window). A write via page 0 now
    // lands in the segment-3 cell, observable through page 3 (also segment 3).
    machine.debug_io_write(0xFC, 0x03);       // page0 mapper segment = 3
    machine.debug_bus_write(0x0010, 0x77);    // page0 seg3 -> physical 0xC010
    expect(machine.read_memory(0xC010) == 0x77, "SegmentSwitch_Page0Seg3_HitsSegment3Cell");
    // Readback independence: writing 0x25 reads back 0x85 on #FC (5-bit) while the
    // physical fold uses seg & 3 (A-3).
    machine.debug_io_write(0xFC, 0x25);
    expect(machine.debug_io_read(0xFC) == 0x85, "MapperReadback_5bit_IndependentOfPhysicalFold");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
