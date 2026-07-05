// Suite: Machine_Hbf1xvMemoryRegions_Unit
//
// Deterministic unit coverage for the M10-S2 INERT memory regions
// (DRAM 64 KB / VRAM 128 KB / FM-PAC SRAM 8 KB). Asserts exact sizes,
// deterministic zero-init at reset, boundary + interior read/write round-trips,
// region independence, and a load -> dump -> reload round-trip.

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

void expect_size(const std::size_t actual, const std::size_t expected, const char* case_name) {
    if (actual != expected) {
        std::cerr << "Case failed: " << case_name << ", expected " << expected << ", got " << actual
                  << "\n";
        ++g_failures;
    }
}

bool region_all_zero(const sony_msx::machine::MemoryRegion& region) {
    const std::vector<std::uint8_t> snapshot = region.dump();
    for (const std::uint8_t byte : snapshot) {
        if (byte != 0) {
            return false;
        }
    }
    return snapshot.size() == region.size();
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::MemoryRegion;

    Hbf1xvMachine machine;
    machine.cold_boot();

    // Exact spec-matched sizes.
    expect_size(machine.dram_size(), 64u * 1024u, "DramSize_Strict_Is64KiB");
    expect_size(machine.vram_size(), 128u * 1024u, "VramSize_Strict_Is128KiB");
    expect_size(machine.sram_size(), 8u * 1024u, "SramSize_FmPacAssumption_Is8KiB");
    expect_size(machine.dram().size(), 64u * 1024u, "DramRegion_Size_Matches");
    expect_size(machine.vram().size(), 128u * 1024u, "VramRegion_Size_Matches");
    expect_size(machine.sram().size(), 8u * 1024u, "SramRegion_Size_Matches");

    // Deterministic zero-init at cold boot.
    expect(region_all_zero(machine.dram()), "ColdBoot_Dram_ZeroInitialized");
    expect(region_all_zero(machine.vram()), "ColdBoot_Vram_ZeroInitialized");
    expect(region_all_zero(machine.sram()), "ColdBoot_Sram_ZeroInitialized");

    // Boundary + interior read/write round-trips per region.
    machine.vram().write(0, 0xA1);
    machine.vram().write(machine.vram_size() / 2, 0x5C);
    machine.vram().write(machine.vram_size() - 1, 0xFE);
    expect(machine.vram().read(0) == 0xA1, "Vram_WriteFirst_ReadsBack");
    expect(machine.vram().read(machine.vram_size() / 2) == 0x5C, "Vram_WriteInterior_ReadsBack");
    expect(machine.vram().read(machine.vram_size() - 1) == 0xFE, "Vram_WriteLast_ReadsBack");

    machine.sram().write(0, 0x11);
    machine.sram().write(machine.sram_size() - 1, 0x22);
    expect(machine.sram().read(0) == 0x11, "Sram_WriteFirst_ReadsBack");
    expect(machine.sram().read(machine.sram_size() - 1) == 0x22, "Sram_WriteLast_ReadsBack");

    machine.dram().write(0, 0x33);
    machine.dram().write(machine.dram_size() - 1, 0x44);
    expect(machine.dram().read(0) == 0x33, "Dram_WriteFirst_ReadsBack");
    expect(machine.dram().read(machine.dram_size() - 1) == 0x44, "Dram_WriteLast_ReadsBack");

    // Out-of-range accesses are deterministic and inert.
    machine.sram().write(machine.sram_size(), 0x99);
    expect(machine.sram().read(machine.sram_size()) == 0x00, "Sram_OutOfRange_ReadsZeroWriteIgnored");

    // Region independence: a write to one region must not alter another.
    expect(machine.dram().read(0) == 0x33, "RegionIndependence_DramUnaffectedByVramSram_First");
    expect(machine.vram().read(0) == 0xA1, "RegionIndependence_VramUnaffectedByDramSram_First");
    expect(machine.sram().read(0) == 0x11, "RegionIndependence_SramUnaffectedByDramVram_First");

    // Load -> dump -> reload round-trip on the VRAM region.
    std::vector<std::uint8_t> pattern(machine.vram_size());
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        pattern[index] = static_cast<std::uint8_t>((index * 7u + 3u) & 0xFFu);
    }
    machine.vram().load(0, pattern.data(), pattern.size());
    const std::vector<std::uint8_t> dumped = machine.vram().dump();
    expect(dumped == pattern, "Vram_LoadThenDump_MatchesPattern");

    MemoryRegion reloaded(machine.vram_size());
    reloaded.load(0, dumped.data(), dumped.size());
    expect(reloaded.dump() == pattern, "Vram_DumpReload_ByteIdentical");

    // Bulk load past end is clamped, never overflows.
    MemoryRegion small(4);
    const std::uint8_t five[5] = {1, 2, 3, 4, 5};
    small.load(2, five, 5);
    expect(small.read(2) == 1 && small.read(3) == 2, "Load_PastEnd_ClampsDeterministically");
    expect(small.size() == 4, "Load_PastEnd_SizeUnchanged");

    // Cold boot re-zeros every region deterministically.
    machine.cold_boot();
    expect(region_all_zero(machine.dram()), "ColdBootAgain_Dram_ReZeroed");
    expect(region_all_zero(machine.vram()), "ColdBootAgain_Vram_ReZeroed");
    expect(region_all_zero(machine.sram()), "ColdBootAgain_Sram_ReZeroed");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
