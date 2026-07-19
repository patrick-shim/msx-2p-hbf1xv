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

// Suite: Machine_Hbf1xvRamDetectBoot_Integration (M42 QA, DEC-0061)
//
// THE end-to-end "software actually SEES the extra RAM" proof, matched byte-for-
// byte to the openMSX A/B (docs/m42-openmsx-ram-ab.md). Both engines:
//   1. instantiate a REAL HB-F1XV (this side loads the repo bios/*.rom), cold-
//      boot, and let the actual BIOS run on the real Z80 for a spell (this side:
//      kBootFrames NTSC frames);
//   2. THEN run the canonical mapper-detection routine mapper-aware software uses:
//      route page 2 (0x8000) through the #FE main-RAM segment register, write the
//      segment index into each of 32 candidate 16 KB segments, then read them all
//      back. The count of DISTINCT read-back values == the number of independently
//      addressable (software-visible) 16 KB segments.
//
// openMSX result (openmsx 19.1, real Sony HB-F1XV ROMs, /tmp/mapper_probe.tcl):
//   - internal 512 KB override -> READBACK 0..31, DISTINCT_COUNT=32
//   - stock 64 KB               -> READBACK 28,29,30,31 repeating, DISTINCT_COUNT=4
// This test asserts OUR engine reproduces the SAME distinct counts AND the SAME
// exact aliasing sequence, driven through the CPU-visible SlotBus/IoBus decode
// (the identical MapperIo::io_write + MemoryMapperRam fold the fetch-executed
// OUT/LD instructions use -- there is no separate mapper code path).

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

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

// NTSC frames of real BIOS execution before probing -- comfortably past the
// power-on RAM initialization. Kept modest so the deterministic test stays fast.
constexpr int kBootFrames = 180;

// Run the exact openMSX probe on OUR engine for a machine fitted to `size_kb`,
// after a real BIOS cold-boot + kBootFrames of live Z80 execution. Returns the
// 32-entry read-back vector; fills `distinct_count`.
std::array<std::uint8_t, 32> boot_and_probe(const int size_kb, int& distinct_count, const char* tag) {
    const std::size_t bytes = static_cast<std::size_t>(size_kb) * 1024u;

    Hbf1xvMachine machine(bytes);
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    // The BIOS must actually be present (no fabricated pass): a missing/short ROM
    // records a diagnostic. Empty => every required ROM loaded.
    expect(machine.rom_diagnostics().empty(), "RealBios_AllRomsPresent");

    // Live boot: run the real BIOS on the real Z80.
    machine.run_frames(static_cast<std::uint32_t>(kBootFrames));
    expect(machine.elapsed_cycles() > 0, "LiveBoot_AdvancedCycles");
    expect(machine.dram_size() == bytes, tag);

    // Establish deterministic mapper routing (all four CPU pages -> slot 3-0 RAM),
    // then probe page 2 exactly as the openMSX Tcl script does via {Main RAM regs}
    // index 2 (== port 0xFE) + CPU-visible memory at 0x8000.
    machine.map_flat_ram();

    // Sanity: page 2 is RAM (mirrors openMSX PAGE2_RAMCHECK=170).
    machine.debug_io_write(0xFE, 0x00);       // page 2 -> segment 0
    machine.debug_bus_write(0x8000, 0xAA);
    expect(machine.debug_bus_read(0x8000) == 0xAA, "Page2_IsRam_MarkerRoundTrip");

    // Write the segment index into each of 32 candidate segments via page 2.
    for (int s = 0; s < 32; ++s) {
        machine.debug_io_write(0xFE, static_cast<std::uint8_t>(s));
        machine.debug_bus_write(0x8000, static_cast<std::uint8_t>(s));
    }
    // Read them all back; count distinct values.
    std::array<std::uint8_t, 32> readback{};
    std::vector<std::uint8_t> distinct;
    for (int s = 0; s < 32; ++s) {
        machine.debug_io_write(0xFE, static_cast<std::uint8_t>(s));
        const std::uint8_t v = machine.debug_bus_read(0x8000);
        readback[static_cast<std::size_t>(s)] = v;
        bool seen = false;
        for (const std::uint8_t d : distinct) {
            if (d == v) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            distinct.push_back(v);
        }
    }
    distinct_count = static_cast<int>(distinct.size());
    return readback;
}

void print_readback(const char* label, const std::array<std::uint8_t, 32>& rb, const int distinct) {
    std::cerr << "[M42] " << label << " READBACK=";
    for (const std::uint8_t v : rb) {
        std::cerr << static_cast<int>(v) << " ";
    }
    std::cerr << " DISTINCT_COUNT=" << distinct << "\n";
}

}  // namespace

int main() {
    // --- 512 KB: all 32 segments software-visible (matches openMSX 512). ---
    {
        int distinct = 0;
        const auto rb = boot_and_probe(512, distinct, "Ram512_DramSize_AfterBoot");
        print_readback("internal-512", rb, distinct);
        expect(distinct == 32, "Ram512_LiveBoot_32DistinctSegments");
        bool identity = true;
        for (int s = 0; s < 32; ++s) {
            if (rb[static_cast<std::size_t>(s)] != static_cast<std::uint8_t>(s)) {
                identity = false;
            }
        }
        expect(identity, "Ram512_Readback_Is_0_To_31_MatchesOpenMsx");
    }

    // --- 64 KB stock: exactly 4 segments, the fold aliasing sequence
    //     28,29,30,31 repeating (byte-for-byte the openMSX stock-64 READBACK). ---
    {
        int distinct = 0;
        const auto rb = boot_and_probe(64, distinct, "Ram64_DramSize_AfterBoot");
        print_readback("stock-64", rb, distinct);
        expect(distinct == 4, "Ram64_LiveBoot_4DistinctSegments");
        bool matches_openmsx = true;
        for (int s = 0; s < 32; ++s) {
            const std::uint8_t want = static_cast<std::uint8_t>(28 + (s & 3));
            if (rb[static_cast<std::size_t>(s)] != want) {
                matches_openmsx = false;
            }
        }
        expect(matches_openmsx, "Ram64_Readback_28_29_30_31_Repeating_MatchesOpenMsx");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvRamDetectBoot_Integration cases passed\n";
    return 0;
}
