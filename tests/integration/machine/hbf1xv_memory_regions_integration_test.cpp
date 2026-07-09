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

// Suite: Machine_Hbf1xvMemoryRegions_Integration
//
// Cross-boundary deterministic coverage for the M10-S2 INERT memory regions.
// Verifies that the CPU-visible DRAM aliases (load_memory/read_memory) and the
// bus stay coherent with the DRAM MemoryRegion, that the inert VRAM/SRAM
// regions survive a load -> dump -> reload round-trip through the machine, and
// that running the CPU never perturbs the inert VRAM/SRAM regions.

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

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    Hbf1xvMachine machine;
    machine.cold_boot();
    // Authentic reset boots slot-0 BIOS (#A8=0). This test loads + runs a program
    // from RAM, so page the flat 64 KB mapper RAM in explicitly (M13-S4).
    machine.map_flat_ram();

    // CPU-visible DRAM alias coherence: load_memory writes the DRAM region and
    // read_memory + the region accessor observe the same bytes.
    const std::uint8_t program[] = {0x3E, 0x2A, 0x06, 0x03, 0x80, 0x76};  // LD A,0x2A / LD B,3 / ADD A,B / HALT
    machine.load_memory(0x0000, program, sizeof(program));
    bool dram_alias_ok = true;
    for (std::size_t index = 0; index < sizeof(program); ++index) {
        if (machine.read_memory(static_cast<std::uint16_t>(index)) != program[index]) {
            dram_alias_ok = false;
        }
        if (machine.dram().read(index) != program[index]) {
            dram_alias_ok = false;
        }
    }
    expect(dram_alias_ok, "LoadMemory_DramRegionAndReadMemory_Coherent");

    // Snapshot inert regions before running the CPU.
    machine.sram().write(0x0010, 0xAB);
    machine.vdp().vram().write(0x4000, 0xCD);
    const std::vector<std::uint8_t> sram_before = machine.sram().dump();
    const std::vector<std::uint8_t> vram_before = machine.vdp().vram().dump();

    // Run the loaded program to HALT; inert regions must be untouched.
    for (int steps = 0; steps < 8 && !machine.cpu().state().halted(); ++steps) {
        machine.step_cpu_instruction();
    }
    expect(machine.cpu().state().halted(), "RunProgram_ReachesHalt");
    expect(machine.sram().dump() == sram_before, "RunProgram_SramRegion_Unperturbed");
    expect(machine.vdp().vram().dump() == vram_before, "RunProgram_VramRegion_Unperturbed");

    // Load -> dump -> reload round-trip across the machine boundary (VRAM).
    std::vector<std::uint8_t> vram_pattern(machine.vdp().vram().size());
    for (std::size_t index = 0; index < vram_pattern.size(); ++index) {
        vram_pattern[index] = static_cast<std::uint8_t>((index ^ 0x5Au) & 0xFFu);
    }
    machine.vdp().vram().load(0, vram_pattern.data(), vram_pattern.size());
    const std::vector<std::uint8_t> vram_dump = machine.vdp().vram().dump();
    expect(vram_dump == vram_pattern, "Vram_LoadDumpThroughMachine_Matches");

    machine.vdp().vram().clear();
    machine.vdp().vram().load(0, vram_dump.data(), vram_dump.size());
    expect(machine.vdp().vram().dump() == vram_pattern, "Vram_ReloadDump_ByteIdentical");

    // Determinism: two independent machines with identical inputs dump
    // byte-identical region contents.
    Hbf1xvMachine machine_a;
    Hbf1xvMachine machine_b;
    machine_a.cold_boot();
    machine_b.cold_boot();
    const std::uint8_t sram_bytes[] = {0x01, 0x23, 0x45, 0x67, 0x89};
    machine_a.sram().load(0x100, sram_bytes, sizeof(sram_bytes));
    machine_b.sram().load(0x100, sram_bytes, sizeof(sram_bytes));
    expect(machine_a.sram().dump() == machine_b.sram().dump(),
           "TwoMachines_SameSramLoad_ByteIdenticalDump");
    expect(machine_a.dram().dump() == machine_b.dram().dump(),
           "TwoMachines_ColdBootDram_ByteIdenticalDump");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
