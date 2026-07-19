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

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "devices/chipset/mapper_io.h"
#include "devices/memory/memory_mapper_ram.h"
#include "machine/memory_region.h"

// Suite: Devices_MemoryMapperRam_Unit  (M13-S1; M42/DEC-0061 RAM-size param)
//
// Deterministic isolation coverage for the mapper-RAM device driven by a
// real M11 chipset::MapperIo:
//   - a segment write changes which physical 16 KB cell a page reads/writes;
//   - the linear config {0,1,2,3} gives a flat 64 KB view (page p -> phys p);
//   - the unpopulated-segment wrap folds seg 4 -> 0 and seg 0x25 -> 1
//     (matching openMSX MSXMemoryMapperBase.cc:72-83 for numSegments == 4);
//   - readback independence: the same write that folds physical to seg&3 still
//     reads back 0x80 | (seg & 0x1F) on the MapperIo (5-bit vs 2-bit masks).
//
// M42 (DEC-0061): physical_address() is now parameterized by the fitted segment
// count (no built-in assumption of 4). The 64 KB (num_segments==4) assertions
// below are UNCHANGED IN VALUE -- they now pass the explicit `4` and prove the
// stock default fold is byte-identical. New parameterized fold tests
// (64/128/256/512 KB -> 4/8/16/32 segments) prove the RAM-detection-correctness
// invariant: distinct banks within the fitted range, high segments mirroring low
// for sub-512 sizes, and NO mirror at the 512 KB ceiling (all 32 distinct).

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

    // --- physical_address folds exactly like openMSX for 4 segments (stock
    // 64 KB; M42 passes the explicit fitted count, values UNCHANGED). ---
    expect(MemoryMapperRam::physical_address(0, 0x0000, 4) == 0x0000, "Phys_Seg0_Addr0");
    expect(MemoryMapperRam::physical_address(1, 0x4321, 4) == 0x4000u + 0x0321u, "Phys_Seg1_KeepsLow14");
    expect(MemoryMapperRam::physical_address(3, 0xFFFF, 4) == 0xC000u + 0x3FFFu, "Phys_Seg3_Top");
    expect(MemoryMapperRam::physical_address(4, 0x0000, 4) == 0x0000, "Phys_Seg4_WrapsToSeg0");
    expect(MemoryMapperRam::physical_address(0x25, 0x0000, 4) == 0x4000, "Phys_Seg0x25_WrapsToSeg1");

    // --- M42/DEC-0061: the device derives its segment count from the backing
    // region. A 64 KB region reports 4 segments (the stock default). ---
    expect(device.num_segments() == 4, "NumSegments_64KB_Is4");

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

    // ========================================================================
    // M42 (DEC-0061): parameterized fold / RAM-detection-correctness tests.
    //
    // For each offered size (64/128/256/512 KB -> 4/8/16/32 segments):
    //   (a) segments 0..num_segments-1 map to DISTINCT physical 16 KB banks
    //       (no aliasing within the fitted range -- a program can address every
    //       populated segment uniquely);
    //   (b) for the SUB-512 sizes, segment `num_segments` (the first past the
    //       fitted top) MIRRORS segment 0 (the fold wrap), so a RAM search that
    //       walks segments detects exactly the fitted size (high segments echo
    //       low ones, as on real hardware);
    //   (c) at 512 KB (32 segments) all 32 segments are DISTINCT with NO mirror
    //       -- the S1985 internal ceiling is fully populated.
    // The device also reports num_segments() == size/16 KB for each size.
    // ========================================================================
    {
        const int sizes_kb[] = {64, 128, 256, 512};
        for (const int size_kb : sizes_kb) {
            const int expected_segments = size_kb / 16;  // 16 KB per segment
            MemoryRegion sized_ram(static_cast<std::size_t>(size_kb) * 1024u);
            MapperIo sized_mapper;
            MemoryMapperRam sized_device(sized_ram, sized_mapper);
            expect(sized_device.num_segments() == expected_segments, "SizedDevice_NumSegments_Matches");

            // (a) distinct banks for every fitted segment (base of each 16 KB
            // segment -> a distinct physical multiple of 0x4000, strictly
            // increasing, so all unique).
            bool distinct = true;
            std::size_t prev_base = 0;
            for (int seg = 0; seg < expected_segments; ++seg) {
                const std::size_t base = MemoryMapperRam::physical_address(
                    static_cast<std::uint8_t>(seg), 0x0000, expected_segments);
                const std::size_t want = static_cast<std::size_t>(seg) * 0x4000u;
                if (base != want) {
                    distinct = false;
                }
                if (seg > 0 && base <= prev_base) {
                    distinct = false;
                }
                prev_base = base;
            }
            expect(distinct, "SizedDevice_FittedSegments_DistinctBanks");

            // (b)/(c) fold wrap at segment == expected_segments.
            const std::size_t wrap = MemoryMapperRam::physical_address(
                static_cast<std::uint8_t>(expected_segments & 0xFF), 0x1234, expected_segments);
            const std::size_t seg0 = MemoryMapperRam::physical_address(0, 0x1234, expected_segments);
            if (expected_segments < 32) {
                // Sub-512: segment `expected_segments` mirrors segment 0
                // (expected_segments & (expected_segments-1) == 0 for a power of
                // two, so the fold maps it back to bank 0).
                expect(wrap == seg0, "SizedDevice_SubCeiling_HighSegmentMirrorsLow");
            } else {
                // 512 KB: segment 32 wraps to segment 0 too (32 & 31 == 0), which
                // is the SAME as seg0 -- but crucially there is NO mirror WITHIN
                // the fitted 0..31 range (proven distinct in (a)). This asserts the
                // ceiling wraps exactly at 32, not before.
                expect(wrap == seg0, "SizedDevice_Ceiling512_WrapsAt32");
                // And segment 31 (the last fitted) is DISTINCT from segment 0 --
                // no early mirror at the ceiling.
                const std::size_t seg31 = MemoryMapperRam::physical_address(31, 0x1234, expected_segments);
                expect(seg31 != seg0 && seg31 == 31u * 0x4000u + 0x1234u,
                       "SizedDevice_Ceiling512_Segment31_DistinctNoEarlyMirror");
            }

            // Live device round-trip: write a distinct byte through every fitted
            // segment via page 0 and read it back at its physical base -- proves
            // the instance fold (mem_write/mem_read using num_segments_) matches
            // the static fold for this size.
            for (int seg = 0; seg < expected_segments; ++seg) {
                sized_mapper.io_write(0xFC, static_cast<std::uint8_t>(seg));  // page 0 -> segment `seg`
                sized_device.mem_write(0x0000, static_cast<std::uint8_t>(0x40 + seg));
            }
            bool roundtrip = true;
            for (int seg = 0; seg < expected_segments; ++seg) {
                if (sized_ram.read(static_cast<std::size_t>(seg) * 0x4000u) !=
                    static_cast<std::uint8_t>(0x40 + seg)) {
                    roundtrip = false;
                }
            }
            expect(roundtrip, "SizedDevice_LiveRoundTrip_EveryFittedSegmentDistinct");
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
