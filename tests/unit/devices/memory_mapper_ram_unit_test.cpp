#include <cstdint>
#include <iostream>

#include "devices/chipset/mapper_io.h"
#include "devices/memory/memory_mapper_ram.h"
#include "machine/memory_region.h"

// Suite: Devices_MemoryMapperRam_Unit  (M13-S1)
//
// Deterministic isolation coverage for the 64 KB mapper-RAM device driven by a
// real M11 chipset::MapperIo:
//   - a segment write changes which physical 16 KB cell a page reads/writes;
//   - the linear config {0,1,2,3} gives a flat 64 KB view (page p -> phys p);
//   - the unpopulated-segment wrap folds seg 4 -> 0 and seg 0x25 -> 1
//     (matching openMSX MSXMemoryMapperBase.cc:72-83 for numSegments == 4);
//   - readback independence: the same write that folds physical to seg&3 still
//     reads back 0x80 | (seg & 0x1F) on the MapperIo (5-bit vs 2-bit masks).

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
    using sony_msx::devices::chipset::MapperIo;
    using sony_msx::devices::memory::MemoryMapperRam;
    using sony_msx::machine::MemoryRegion;

    MemoryRegion ram(64u * 1024u);
    MapperIo mapper;
    MemoryMapperRam device(ram, mapper);

    // --- physical_address folds exactly like openMSX for 4 segments. ---
    expect(MemoryMapperRam::physical_address(0, 0x0000) == 0x0000, "Phys_Seg0_Addr0");
    expect(MemoryMapperRam::physical_address(1, 0x4321) == 0x4000u + 0x0321u, "Phys_Seg1_KeepsLow14");
    expect(MemoryMapperRam::physical_address(3, 0xFFFF) == 0xC000u + 0x3FFFu, "Phys_Seg3_Top");
    expect(MemoryMapperRam::physical_address(4, 0x0000) == 0x0000, "Phys_Seg4_WrapsToSeg0");
    expect(MemoryMapperRam::physical_address(0x25, 0x0000) == 0x4000, "Phys_Seg0x25_WrapsToSeg1");

    // --- Linear flat 64 KB view with segments {0,1,2,3}. ---
    for (int page = 0; page < 4; ++page) {
        mapper.io_write(static_cast<std::uint16_t>(0xFC + page), static_cast<std::uint8_t>(page));
    }
    // A distinct byte in each page maps to a distinct physical cell.
    device.mem_write(0x0000, 0xA0);  // page0 seg0 -> phys 0x0000
    device.mem_write(0x4000, 0xB1);  // page1 seg1 -> phys 0x4000
    device.mem_write(0x8000, 0xC2);  // page2 seg2 -> phys 0x8000
    device.mem_write(0xC000, 0xD3);  // page3 seg3 -> phys 0xC000
    expect(ram.read(0x0000) == 0xA0 && ram.read(0x4000) == 0xB1 && ram.read(0x8000) == 0xC2 &&
               ram.read(0xC000) == 0xD3,
           "LinearConfig_EachPage_HitsDistinctSegment");
    expect(device.mem_read(0x0000) == 0xA0 && device.mem_read(0xC000) == 0xD3,
           "LinearConfig_ReadBack_Matches");

    // --- A segment write reroutes a page to a different physical cell. ---
    // Point page 0 at segment 3 (where page 3 wrote 0xD3 above).
    mapper.io_write(0xFC, 0x03);
    expect(device.mem_read(0x0000) == 0xD3, "SegWrite_Page0Seg3_ReadsSegment3Cell");
    // Write through page 0 now lands in segment 3, visible via page 3.
    device.mem_write(0x0001, 0x7E);
    expect(ram.read(0xC001) == 0x7E, "SegWrite_Page0Seg3_WriteHitsSegment3Cell");

    // --- Unpopulated-segment wrap through the live device: seg 0x25 -> seg 1. ---
    mapper.io_write(0xFC, 0x25);           // page 0 segment register = 0x25
    device.mem_write(0x0002, 0x9C);        // folds to seg (0x25 & 3) = 1 -> phys 0x4002
    expect(ram.read(0x4002) == 0x9C, "Wrap_Seg0x25_FoldsToSegment1");

    // --- Readback independence: MapperIo still yields 0x80 | (0x25 & 0x1F). ---
    expect(mapper.io_read(0xFC) == 0x85, "Readback_5bit_IndependentOf2bitPhysicalFold");
    expect(mapper.segment(0) == 0x25, "SegmentAccessor_ReturnsRawWrittenValue");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
