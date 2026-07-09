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

// Suite: Machine_Hbf1xvM15BootCheckpoint_Integration  (M15-S6, backlog C5 partial)
//
// With the M15 devices real (PSG #A0-A2, RTC #B4/B5, PPI/keyboard #A8-AB, VDP),
// the BIOS boot advances past the M13 first-device-read boundary (~PC 0x043C,
// docs/m13-parity-trace-diff.md). This test boots the real BIOS, single-steps a
// bounded number of instructions, and asserts:
//   1. no missing-asset diagnostics (real ROMs present);
//   2. the run is DETERMINISTIC — two cold_boot+run cycles produce a byte-
//      identical (PC,opcode) stream and reach the identical final PC;
//   3. boot ADVANCES strictly beyond the M13 boundary 0x043C (the max PC reached
//      during the checkpoint window exceeds it).
//
// The exact reached PC is DERIVED here (self-consistent + deterministic), not
// guessed; the value is emitted for the QA / openMSX A/B cross-check (S6).

#include <cstdint>
#include <iostream>
#include <vector>

#include "machine/hbf1xv_machine.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

constexpr int kCheckpointInstructions = 4096;
constexpr std::uint16_t kM13Boundary = 0x043C;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

struct Result {
    std::vector<std::uint32_t> stream;  // (pc<<8 | opcode) per step
    std::uint16_t final_pc = 0;
    std::uint16_t max_pc = 0;
};

Result boot_and_run() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    Result r;
    r.stream.reserve(kCheckpointInstructions);
    for (int i = 0; i < kCheckpointInstructions; ++i) {
        const std::uint16_t pc = machine.cpu().state().regs().pc;
        const std::uint8_t opcode = machine.debug_bus_read(pc);
        r.stream.push_back((static_cast<std::uint32_t>(pc) << 8) | opcode);
        if (pc > r.max_pc) {
            r.max_pc = pc;
        }
        machine.step_cpu_instruction();
    }
    r.final_pc = machine.cpu().state().regs().pc;
    return r;
}

}  // namespace

int main() {
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.cold_boot();
        expect(machine.rom_diagnostics().empty(), "Boot_NoMissingAssetDiagnostics");
        for (const auto& n : machine.rom_diagnostics()) {
            std::cerr << "  " << n << "\n";
        }
    }

    const Result a = boot_and_run();
    const Result b = boot_and_run();

    expect(a.stream == b.stream, "BootCheckpoint_TwoRuns_IdenticalStream");
    expect(a.final_pc == b.final_pc, "BootCheckpoint_TwoRuns_IdenticalFinalPc");
    expect(a.max_pc > kM13Boundary, "BootCheckpoint_AdvancedPastM13Boundary");

    std::cerr << "M15 boot checkpoint: final PC=0x" << std::hex << a.final_pc
              << " max PC=0x" << a.max_pc << std::dec << " over " << kCheckpointInstructions
              << " instructions\n";

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
