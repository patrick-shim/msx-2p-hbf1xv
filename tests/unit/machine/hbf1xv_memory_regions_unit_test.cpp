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

// Suite: Machine_Hbf1xvMemoryRegions_Unit
//
// Deterministic unit coverage for the machine memory stores. DRAM (64 KB)
// remains an inert machine MemoryRegion. VRAM (128 KB) was MIGRATED to the
// V9958 VDP device, so its coverage now runs through machine.vdp().vram()
// (a devices::video::VdpVram, which exposes the same size/clear/read/write/
// load/dump/data surface). Asserts exact sizes, deterministic zero-init at
// reset, boundary + interior read/write round-trips, region independence, and
// a load -> dump -> reload round-trip.
//
// The speculative internal 8 KB `sram_` region was REMOVED -- the bare
// HB-F1XV's built-in MSX-MUSIC is SRAM-less; battery SRAM is a peripheral
// (external FM-PAC cartridge, see the FM-PAC integration test; DEC-0050). So
// this suite no longer asserts any machine-level SRAM region, and additionally
// asserts that a bare machine exposes NO FM-PAC cartridge (fmpac() == nullptr).

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/video/vdp_vram.h"
#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void expect_size(const std::size_t actual, const std::size_t expected, const char* case_name) {
    if (actual != expected) {
        std::cerr << "Case failed: " << case_name << ", expected " << expected << ", got " << actual
                  << "\n";
        ++g_failures;
    }
}

// Templated so it accepts both the machine's MemoryRegion (DRAM) and the
// VDP's VdpVram, which share the dump()/size() surface.
template <class Region>
bool region_all_zero(const Region& region) {
    const std::vector<std::uint8_t> snapshot = region.dump();
    for (const std::uint8_t byte : snapshot) {
        if (byte != 0) {
            return false;
        }
    }
    return snapshot.size() == region.size();
}

// Main RAM powers on with the alternating 00/FF initialContent the machine
// XML declares (openMSX 21.0: Sony_HB-F1XV.xml:129) — the 512-byte pattern
// (00,FF x128 then FF,00 x128) repeated across 64 KB, matching openMSX
// Ram::clear. It is NOT all-zero.
std::uint8_t a5_byte(const std::size_t index) {
    const std::size_t p = index & 0x1FFu;
    if (p < 256) {
        return (p & 1u) ? 0xFF : 0x00;
    }
    return (p & 1u) ? 0x00 : 0xFF;
}

bool region_matches_a5_pattern(const sony_msx::machine::MemoryRegion& region) {
    const std::vector<std::uint8_t> snapshot = region.dump();
    if (snapshot.size() != region.size()) {
        return false;
    }
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        if (snapshot[i] != a5_byte(i)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::MemoryRegion;

    Hbf1xvMachine machine;
    machine.cold_boot();

    // Exact spec-matched sizes.
    expect_size(machine.dram_size(), 64u * 1024u, "DramSize_Strict_Is64KiB");
    expect_size(machine.vdp().vram().size(), 128u * 1024u, "VramSize_Strict_Is128KiB");
    expect_size(machine.dram().size(), 64u * 1024u, "DramRegion_Size_Matches");
    expect_size(machine.vdp().vram().size(), 128u * 1024u, "VramRegion_Size_Matches");

    // A bare HB-F1XV has NO internal SRAM and NO FM-PAC peripheral
    // cartridge inserted -> fmpac() reports nullptr for both bays (DEC-0050).
    expect(machine.fmpac(1) == nullptr, "BareMachine_NoFmPacCartridge_Slot1");
    expect(machine.fmpac(2) == nullptr, "BareMachine_NoFmPacCartridge_Slot2");

    // Deterministic power-on content at cold boot. DRAM carries the XML
    // alternating 00/FF pattern; VRAM remains zero-initialized.
    expect(region_matches_a5_pattern(machine.dram()), "ColdBoot_Dram_A5AlternatingPattern");
    expect(region_all_zero(machine.vdp().vram()), "ColdBoot_Vram_ZeroInitialized");

    // Boundary + interior read/write round-trips per region.
    machine.vdp().vram().write(0, 0xA1);
    machine.vdp().vram().write(machine.vdp().vram().size() / 2, 0x5C);
    machine.vdp().vram().write(machine.vdp().vram().size() - 1, 0xFE);
    expect(machine.vdp().vram().read(0) == 0xA1, "Vram_WriteFirst_ReadsBack");
    expect(machine.vdp().vram().read(machine.vdp().vram().size() / 2) == 0x5C, "Vram_WriteInterior_ReadsBack");
    expect(machine.vdp().vram().read(machine.vdp().vram().size() - 1) == 0xFE, "Vram_WriteLast_ReadsBack");

    machine.dram().write(0, 0x33);
    machine.dram().write(machine.dram_size() - 1, 0x44);
    expect(machine.dram().read(0) == 0x33, "Dram_WriteFirst_ReadsBack");
    expect(machine.dram().read(machine.dram_size() - 1) == 0x44, "Dram_WriteLast_ReadsBack");

    // Region independence: a write to one region must not alter another.
    expect(machine.dram().read(0) == 0x33, "RegionIndependence_DramUnaffectedByVram_First");
    expect(machine.vdp().vram().read(0) == 0xA1, "RegionIndependence_VramUnaffectedByDram_First");

    // Load -> dump -> reload round-trip on the VRAM region.
    std::vector<std::uint8_t> pattern(machine.vdp().vram().size());
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        pattern[index] = static_cast<std::uint8_t>((index * 7u + 3u) & 0xFFu);
    }
    machine.vdp().vram().load(0, pattern.data(), pattern.size());
    const std::vector<std::uint8_t> dumped = machine.vdp().vram().dump();
    expect(dumped == pattern, "Vram_LoadThenDump_MatchesPattern");

    MemoryRegion reloaded(machine.vdp().vram().size());
    reloaded.load(0, dumped.data(), dumped.size());
    expect(reloaded.dump() == pattern, "Vram_DumpReload_ByteIdentical");

    // Bulk load past end is clamped, never overflows.
    MemoryRegion small(4);
    const std::uint8_t five[5] = {1, 2, 3, 4, 5};
    small.load(2, five, 5);
    expect(small.read(2) == 1 && small.read(3) == 2, "Load_PastEnd_ClampsDeterministically");
    expect(small.size() == 4, "Load_PastEnd_SizeUnchanged");

    // Cold boot re-initializes every region deterministically (DRAM -> the
    // alternating 00/FF pattern, VRAM -> zero).
    machine.cold_boot();
    expect(region_matches_a5_pattern(machine.dram()), "ColdBootAgain_Dram_ReInitializedToA5Pattern");
    expect(region_all_zero(machine.vdp().vram()), "ColdBootAgain_Vram_ReZeroed");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
