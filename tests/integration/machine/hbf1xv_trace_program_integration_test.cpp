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

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "machine/cpu_trace_sink.h"
#include "machine/hbf1xv_machine.h"

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Fixed program: LD A,0x2A / LD B,0x03 / ADD A,B / HALT.
const std::array<std::uint8_t, 6> kProgram{
    0x3E, 0x2A,  // LD A,0x2A
    0x06, 0x03,  // LD B,0x03
    0x80,        // ADD A,B
    0x76,        // HALT
};

std::string run_traced_serialization() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    machine.set_cpu_trace_enabled(true);
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }
    return machine.cpu_trace().serialize();
}

std::size_t count_lines(const std::string& text) {
    std::size_t lines = 0;
    for (const char ch : text) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
}

}  // namespace

int main() {
    // Suite: Machine_Hbf1xvTraceProgram_Integration

    // --- Trace is off by default; stepping records nothing until enabled. ---
    sony_msx::machine::Hbf1xvMachine baseline;
    baseline.cold_boot();
    baseline.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    baseline.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    if (!expect_true(!baseline.cpu_trace_enabled() && baseline.cpu_trace().empty(),
                     "DefaultMachine_TraceDisabled_NoRecords")) {
        return 1;
    }
    baseline.step_cpu_instruction();
    if (!expect_true(baseline.cpu_trace().empty(),
                     "TraceDisabled_Step_StillNoRecords")) {
        return 1;
    }

    // --- Enabled trace collects one record per instruction to the checkpoint. ---
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    machine.set_cpu_trace_enabled(true);
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }

    if (!expect_true(machine.cpu_trace_enabled() && machine.cpu_trace().size() == 4,
                     "TraceEnabled_FourInstructions_FourRecords")) {
        return 1;
    }

    // The CPU trace publishes DATASHEET cumulative T-states (unchanged, 22); the
    // machine clock includes the S1985 +1-per-M1 wait, so it diverges: 4 fetched
    // instructions -> +4 -> elapsed 26 (M11, fact-sheet §8; A-4). This divergence
    // is intentional and is the whole point of surfacing the wait at the machine
    // layer while keeping the reusable Z80 datasheet timings intact.
    if (!expect_true(machine.cpu_trace().at(3).cumulative_tstates == 22 &&
                         machine.elapsed_cycles() == 26,
                     "TraceEnabled_CpuDatasheetTc22_MachineClockWithM1Wait26")) {
        return 1;
    }

    // --- Serialization format is fixed and byte-exact for the first record. ---
    const std::string expected_first_line =
        "SEQ=0000 PC=0000 OP=3E A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 "
        "AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 "
        "IX=0000 IY=0000 SP=FFFF I=00 R=00 IFF1=0 IFF2=0 IM=1 T=7 TC=7";
    const std::string actual_first_line =
        sony_msx::machine::CpuTraceSink::format_record(machine.cpu_trace().at(0));
    if (!expect_true(actual_first_line == expected_first_line,
                     "FormatRecord_FirstInstruction_MatchesGoldenLine")) {
        std::cerr << "  expected: " << expected_first_line << "\n";
        std::cerr << "  actual:   " << actual_first_line << "\n";
        return 1;
    }

    // --- Whole-trace serialization: four '\n'-terminated lines. ---
    const std::string serialized = machine.cpu_trace().serialize();
    if (!expect_true(count_lines(serialized) == 4 && !serialized.empty() &&
                         serialized.back() == '\n',
                     "Serialize_FourRecords_FourNewlineTerminatedLines")) {
        return 1;
    }

    // --- Deterministic oracle: two identical traced runs are byte-for-byte equal. ---
    const std::string run_a = run_traced_serialization();
    const std::string run_b = run_traced_serialization();
    if (!expect_true(run_a == run_b && run_a == serialized,
                     "TwoTracedRuns_SameProgram_ByteForByteIdentical")) {
        return 1;
    }

    return 0;
}
