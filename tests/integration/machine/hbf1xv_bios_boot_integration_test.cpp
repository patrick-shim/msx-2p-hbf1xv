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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvBiosBoot_Integration
//
// Proves the authentic reset (#A8 = 0) boots the CPU from the slot-0 BIOS ROM:
//   1. the bus-visible byte at 0x0000 equals the loaded BIOS image byte 0 (the
//      fetch source is slot-0 ROM, not open bus / RAM);
//   2. single-stepping a bounded K instructions, every opcode the CPU fetches
//      equals the slot-resolved BIOS image byte at that PC (golden self-derived
//      from the image itself — no guessed disassembly), while page 0/1 remain
//      routed to slot 0-0;
//   3. the run is deterministic: two cold_boot+run cycles produce the identical
//      PC/opcode sequence and reach the same PC.
//
// The absolute PC/trace values are cross-validated against openMSX by the
// separate parity-checkpoint tests; this test only asserts image-grounded,
// self-consistent, deterministic execution.
//
// The real bios directory is injected as an absolute path by CMake.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

constexpr int kCheckpointInstructions = 32;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

std::vector<std::uint8_t> read_bios() {
    const std::filesystem::path path = std::filesystem::path(SONY_MSX_BIOS_DIR) / "f1xvbios.rom";
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

struct Step {
    std::uint16_t pc;
    std::uint8_t opcode;
};

// Cold-boot and single-step, recording (pc, opcode) for each step. Also verifies
// the image-grounded fetch invariant while page 0/1 route to slot 0-0.
std::vector<Step> boot_and_trace(const std::vector<std::uint8_t>& bios, bool& invariant_ok) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    std::vector<Step> trace;
    invariant_ok = true;
    for (int i = 0; i < kCheckpointInstructions; ++i) {
        const std::uint16_t pc = machine.cpu().state().regs().pc;
        const std::uint8_t fetched = machine.debug_bus_read(pc);
        trace.push_back({pc, fetched});

        // While this page routes to primary slot 0 (pages 0-1 = BIOS), the fetched
        // byte must equal the BIOS image byte at PC.
        const int page = (pc >> 14) & 0x03;
        const int primary = (machine.debug_io_read(0xA8) >> (2 * page)) & 0x03;
        if (primary == 0 && page < 2) {
            if (pc >= bios.size() || fetched != bios[pc]) {
                invariant_ok = false;
            }
        }
        machine.step_cpu_instruction();
    }
    return trace;
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> bios = read_bios();
    if (!expect_true(bios.size() == 0x8000, "Bios_Present_Is32K")) {
        return 1;
    }

    // --- 1. Bus byte at 0x0000 is the BIOS image byte 0. ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.cold_boot();
        if (!expect_true(machine.rom_diagnostics().empty(), "Boot_NoMissingAssetDiagnostics")) {
            for (const auto& n : machine.rom_diagnostics()) {
                std::cerr << "  " << n << "\n";
            }
            return 1;
        }
        if (!expect_true(machine.debug_bus_read(0x0000) == bios[0],
                         "Reset_BusByteAt0000_IsBiosImageByte0")) {
            return 1;
        }
        // The reset vector is real ROM, not open bus (0xFF would be RST 38h).
        if (!expect_true(bios[0] != 0xFF, "Reset_BiosByte0_IsNotOpenBus")) {
            return 1;
        }
        if (!expect_true(machine.cpu().state().regs().pc == 0x0000, "Reset_ProgramCounter_IsZero")) {
            return 1;
        }
    }

    // --- 2. Bounded single-step: every fetched opcode matches the BIOS image. ---
    bool invariant_a = false;
    const std::vector<Step> trace_a = boot_and_trace(bios, invariant_a);
    if (!expect_true(invariant_a, "BootCheckpoint_EveryFetchedOpcode_MatchesBiosImage")) {
        return 1;
    }
    // The first fetched opcode is exactly BIOS[0] and the PC actually advanced
    // (the CPU executed real ROM, it did not stall at the reset vector).
    if (!expect_true(!trace_a.empty() && trace_a.front().pc == 0x0000 &&
                         trace_a.front().opcode == bios[0],
                     "BootCheckpoint_FirstFetch_IsBiosByte0")) {
        return 1;
    }
    if (!expect_true(trace_a.back().pc != 0x0000, "BootCheckpoint_PcAdvanced_FromResetVector")) {
        return 1;
    }

    // --- 3. Determinism: a second boot+run is byte-for-byte identical. ---
    bool invariant_b = false;
    const std::vector<Step> trace_b = boot_and_trace(bios, invariant_b);
    bool identical = invariant_b && trace_a.size() == trace_b.size();
    for (std::size_t i = 0; identical && i < trace_a.size(); ++i) {
        if (trace_a[i].pc != trace_b[i].pc || trace_a[i].opcode != trace_b[i].opcode) {
            identical = false;
        }
    }
    if (!expect_true(identical, "BootCheckpoint_TwoRuns_IdenticalPcOpcodeSequence")) {
        return 1;
    }

    std::cerr << "boot checkpoint: reached PC=0x" << std::hex << trace_a.back().pc
              << " after " << std::dec << kCheckpointInstructions << " instructions\n";
    return 0;
}
