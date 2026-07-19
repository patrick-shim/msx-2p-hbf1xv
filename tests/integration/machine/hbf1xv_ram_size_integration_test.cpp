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

// Suite: Machine_Hbf1xvRamSize_Integration (M42, DEC-0061)
//
// Deterministic, asset-free coverage for the opt-in `--ram <64|128|256|512>`
// main-RAM sizing across the FULL machine bus fabric (mapper_io #FC segment
// registers -> MemoryMapperRam fold -> dram_). Proves:
//   1. DEFAULT (no argument) and explicit 64 KB are byte-identical: dram_size()
//      == 64 KB and the mapper exposes exactly 4 segments (stock spec).
//   2. Each configured size allocates the fitted DRAM (dram_size() == KB*1024)
//      and a genuine high-offset round-trip proves the larger allocation exists.
//   3. RAM-DETECTION CORRECTNESS through the bus: within the fitted segment
//      range every segment is a DISTINCT physical bank (no aliasing), and the
//      first segment PAST the fitted top mirrors segment 0 (the fold wrap the
//      real BIOS RAM search relies on to size RAM). At 512 KB all 32 segments
//      are distinct and the wrap first occurs at segment 32 (the S1985 ceiling).
//   4. A cold-boot + multi-frame headless run at 512 KB does not crash and keeps
//      dram_size() == 512 KB (the smoke).

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

using sony_msx::machine::Hbf1xvMachine;

// Drive the machine-level RAM-detection proof for a machine fitted to `size_kb`.
// Pages slot 3-0 (the mapper RAM) into all pages (map_flat_ram), then walks the
// #FC page-0 segment register to write/read every physical segment through the
// real decode fabric.
void check_size(const int size_kb, const char* tag) {
    const std::size_t bytes = static_cast<std::size_t>(size_kb) * 1024u;
    const int num_segments = size_kb / 16;  // 16 KB per mapper segment

    Hbf1xvMachine machine(bytes);
    machine.cold_boot();

    // (2) fitted allocation.
    expect(machine.dram_size() == bytes, tag);

    // High-offset round-trip: write near the TOP of the fitted region and read
    // it back -- impossible unless the region is genuinely that large (a 64 KB
    // region would drop the write past its end).
    const std::size_t high = bytes - 3u;
    machine.dram().write(high, 0x5A);
    expect(machine.dram().read(high) == 0x5A, "HighOffset_RoundTrip_ProvesAllocation");

    // Page the mapper RAM into all four CPU pages (flat view), then use page 0's
    // #FC segment register to reach every physical segment.
    machine.map_flat_ram();

    // (3a) write a distinct marker to each fitted segment via page 0.
    for (int seg = 0; seg < num_segments; ++seg) {
        machine.debug_io_write(0xFC, static_cast<std::uint8_t>(seg));  // page 0 -> segment `seg`
        machine.debug_bus_write(0x0000, static_cast<std::uint8_t>(0x10 + seg));
    }
    // Read each back: all markers survive => no aliasing within the fitted range
    // (every populated segment is uniquely addressable).
    bool all_distinct = true;
    for (int seg = 0; seg < num_segments; ++seg) {
        machine.debug_io_write(0xFC, static_cast<std::uint8_t>(seg));
        if (machine.debug_bus_read(0x0000) != static_cast<std::uint8_t>(0x10 + seg)) {
            all_distinct = false;
        }
    }
    expect(all_distinct, "FittedSegments_AllDistinct_NoAliasing");

    // (3b) first segment PAST the fitted top mirrors segment 0 (fold wrap). This
    // is precisely how a BIOS RAM search detects the size: writing to segment
    // `num_segments` corrupts segment 0, so the search stops there.
    machine.debug_io_write(0xFC, 0x00);
    machine.debug_bus_write(0x0000, 0xA0);                                    // segment 0 = 0xA0
    machine.debug_io_write(0xFC, static_cast<std::uint8_t>(num_segments));    // page 0 -> segment num_segments
    machine.debug_bus_write(0x0000, 0xB0);                                    // writes the MIRROR of segment 0
    machine.debug_io_write(0xFC, 0x00);
    expect(machine.debug_bus_read(0x0000) == 0xB0, "FirstPastTop_MirrorsSegment0_FoldWrap");
}

}  // namespace

int main() {
    // (1) Default construction == explicit 64 KB == stock spec (byte-identical
    // path): 4 segments, 64 KB.
    {
        Hbf1xvMachine def;  // default argument
        def.cold_boot();
        expect(def.dram_size() == 64u * 1024u, "DefaultCtor_Is64KB_Stock");
        Hbf1xvMachine explicit64(64u * 1024u);
        explicit64.cold_boot();
        expect(explicit64.dram_size() == 64u * 1024u, "Explicit64_Is64KB");
    }

    // (2)+(3) every offered size across the full bus fabric.
    check_size(64, "Ram64_DramSize");
    check_size(128, "Ram128_DramSize");
    check_size(256, "Ram256_DramSize");
    check_size(512, "Ram512_DramSize");

    // (3c) 512 KB ceiling detail: all 32 segments distinct + wrap first at 32.
    {
        Hbf1xvMachine machine(512u * 1024u);
        machine.cold_boot();
        machine.map_flat_ram();
        // Segment 31 (last fitted) must be DISTINCT from segment 0 (no early
        // mirror below the ceiling).
        machine.debug_io_write(0xFC, 0x00);
        machine.debug_bus_write(0x0000, 0xC1);   // segment 0
        machine.debug_io_write(0xFC, 31);
        machine.debug_bus_write(0x0000, 0xC2);   // segment 31 (distinct bank)
        machine.debug_io_write(0xFC, 0x00);
        expect(machine.debug_bus_read(0x0000) == 0xC1, "Ceiling512_Segment31_NoEarlyMirror");
    }

    // (4) --ram 512 cold-boot + multi-frame headless smoke: no crash, size holds.
    {
        Hbf1xvMachine machine(512u * 1024u);
        machine.cold_boot();
        machine.run_frames(3);
        expect(machine.dram_size() == 512u * 1024u, "Ram512_Smoke_MultiFrame_NoCrash_SizeHolds");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvRamSize_Integration cases passed\n";
    return 0;
}
