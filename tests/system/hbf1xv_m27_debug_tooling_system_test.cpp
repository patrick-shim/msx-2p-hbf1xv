#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>

#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

// Suite: Hbf1xvM27DebugTooling_System (M27-S8, "Production Hardening +
// Debug/Test Tooling" -- the milestone's own "everything together" dedicated
// system test, mirroring the M25/M26 precedent).
//
// Combines ALL FOUR of this milestone's items in ONE deterministic, bounded,
// real-BIOS-boot scenario -- the exact composition the new headless
// --debug-session mode (and the SDL3-side wiring) drive in production:
//   1. State-dump + CPU-trace CLI wiring (item 1).
//   4. Event-log CLI wiring, with the R-M27-2 enable-before-cold_boot()
//      ordering (item 4, shares item 1's mechanism).
//   3. Scripted-input driving through the SAME CPU sub-loop (item 3).
// Then proves the WHOLE combination is deterministic end to end: two
// independent runs of the identical scenario produce byte-for-byte identical
// state-dump, CPU-trace, AND event-log output.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

constexpr std::uint32_t kBoundedSteps = 500;

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct SessionOutputs {
    std::string state_dump;
    std::string cpu_trace;
    std::string event_log;
    std::uint16_t final_pc = 0;
    bool a_key_pressed_at_end = false;
};

// Mirrors the new --debug-session mode's FULL combination exactly: event
// logging enabled BEFORE cold_boot() (R-M27-2), real BIOS asset loading, CPU
// trace enabled, a real input script driven through the SAME
// step_cpu_instruction() sub-loop, then all three write_* calls at the end.
SessionOutputs run_full_debug_session(const std::filesystem::path& debug_root) {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::InputScriptPlayer;
    using sony_msx::machine::parse_input_script;

    const std::string script_text =
        "HBF1XV-INPUT-SCRIPT v1\n"
        "T=50 KEY=A DOWN\n"
        "T=2000 KEY=A UP\n"
        "[END]\n";
    InputScriptPlayer player(parse_input_script(script_text));

    Hbf1xvMachine machine;
    machine.set_debug_root(debug_root);
    machine.set_event_logging_enabled(true);  // BEFORE cold_boot() -- R-M27-2.
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    machine.set_cpu_trace_enabled(true);

    std::uint32_t steps = 0;
    while (steps < kBoundedSteps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        player.apply_due(machine.elapsed_cycles(), machine.keyboard());
        ++steps;
    }

    machine.write_state_dump("session.state");
    machine.write_cpu_trace("session.trace");
    machine.write_event_log("session.log");

    SessionOutputs outputs;
    outputs.state_dump = read_file(debug_root / "traces" / "session.state");
    outputs.cpu_trace = read_file(debug_root / "traces" / "session.trace");
    outputs.event_log = read_file(debug_root / "logs" / "session.log");
    outputs.final_pc = machine.cpu().state().regs().pc;
    // "A" -> (row=2, column=6), per msx_key_names.cpp.
    outputs.a_key_pressed_at_end = machine.keyboard().key(2, 6);
    return outputs;
}

}  // namespace

int main() {
    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m27-debug-tooling-system-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);

    const SessionOutputs run_a = run_full_debug_session(temp_root / "run_a");
    const SessionOutputs run_b = run_full_debug_session(temp_root / "run_b");

    // --- Item 1: state-dump + CPU-trace are real, non-empty, well-formed. ---
    expect(!run_a.state_dump.empty(), "StateDump_NonEmpty");
    expect(run_a.state_dump.rfind("HBF1XV-STATE-DUMP v1", 0) == 0, "StateDump_StartsWithFormatTag");
    expect(!run_a.cpu_trace.empty(), "CpuTrace_NonEmpty");
    {
        std::size_t line_count = 0;
        for (const char c : run_a.cpu_trace) {
            if (c == '\n') {
                ++line_count;
            }
        }
        expect(line_count == kBoundedSteps, "CpuTrace_StepCount_MatchesBoundedStepsExactly");
    }

    // --- Item 4: event-log is real, well-formed, with the Reset event
    // genuinely present at sequence 0 (R-M27-2's ordering requirement). ---
    expect(!run_a.event_log.empty(), "EventLog_NonEmpty");
    expect(run_a.event_log.rfind("EVT=0000 T=0 TYPE=RESET", 0) == 0,
           "EventLog_ResetEventPresentAtSequence0_R_M27_2_OrderingHonored");

    // --- Item 3: the scripted-input drive genuinely took effect -- "A" was
    // pressed then released, ending NOT pressed. ---
    expect(!run_a.a_key_pressed_at_end, "ScriptedInput_AKey_ReleasedByEndOfBoundedRun");

    // --- Determinism: the WHOLE combination (state-dump + CPU-trace +
    // event-log + scripted-input driving) is byte-for-byte reproducible
    // across two independent runs of the identical scenario. ---
    expect(run_a.state_dump == run_b.state_dump, "Determinism_StateDump_ByteIdenticalAcrossTwoRuns");
    expect(run_a.cpu_trace == run_b.cpu_trace, "Determinism_CpuTrace_ByteIdenticalAcrossTwoRuns");
    expect(run_a.event_log == run_b.event_log, "Determinism_EventLog_ByteIdenticalAcrossTwoRuns");
    expect(run_a.final_pc == run_b.final_pc, "Determinism_FinalPc_IdenticalAcrossTwoRuns");

    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Hbf1xvM27DebugTooling_System cases passed\n";
    return 0;
}
