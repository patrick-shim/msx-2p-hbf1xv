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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

// Runs the known program with event logging enabled and returns the machine's
// serialized event log after four steps (Reset + 4 InstructionRetire + Halt).
std::string run_event_log() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_event_logging_enabled(true);
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }
    return machine.debug_event_log().serialize();
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}  // namespace

int main() {
    // Suite: Machine_Hbf1xvDebugEventLog_Integration

    // --- Logging is off by default; no events are recorded. ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
        machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
        machine.step_cpu_instruction();
        if (!expect_true(!machine.event_logging_enabled() && machine.debug_event_log().empty(),
                         "LoggingOff_Step_NoEvents")) {
            return 1;
        }
    }

    // --- Enabled logging captures Reset + one InstructionRetire per step + a
    //     Halt event on the step that enters the halted state. ---
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_event_logging_enabled(true);
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }

    // Reset(1) + InstructionRetire(4) + Halt(1) = 6.
    if (!expect_true(machine.debug_event_log().size() == 6,
                     "EnabledLogging_KnownProgram_SixEvents")) {
        std::cerr << "  actual size: " << machine.debug_event_log().size() << "\n";
        return 1;
    }

    // --- Event stream is exact against a committed golden oracle. ---
    // Event-log T-stamps and per-instruction T= are MSX-accurate machine time =
    // datasheet + S1985 +1-per-M1 wait (M11, fact-sheet §8). Each of LD A,n /
    // LD B,n / ADD A,B / HALT fetches one opcode (1 M1) so each gains +1:
    // 7->8, 7->8, 4->5, 4->5; cumulative stamps 8, 16, 21, 26.
    const std::string expected_log =
        "EVT=0000 T=0 TYPE=RESET cold_boot\n"
        "EVT=0001 T=8 TYPE=INSTR PC=0000 OP=3E T=8\n"
        "EVT=0002 T=16 TYPE=INSTR PC=0002 OP=06 T=8\n"
        "EVT=0003 T=21 TYPE=INSTR PC=0004 OP=80 T=5\n"
        "EVT=0004 T=26 TYPE=INSTR PC=0005 OP=76 T=5\n"
        "EVT=0005 T=26 TYPE=HALT PC=0005\n";
    const std::string actual_log = machine.debug_event_log().serialize();
    if (!expect_true(actual_log == expected_log, "EventLog_KnownProgram_MatchesGolden")) {
        std::cerr << "  --- expected ---\n" << expected_log;
        std::cerr << "  --- actual ---\n" << actual_log;
        return 1;
    }

    // --- Reproducibility: two identical runs produce byte-identical logs. ---
    const std::string run_a = run_event_log();
    const std::string run_b = run_event_log();
    if (!expect_true(run_a == run_b && run_a == expected_log,
                     "TwoRuns_SameProgram_ByteIdenticalLog")) {
        return 1;
    }

    // --- write_state_dump records a Dump event and writes both artifacts under
    //     a hermetic debug root; the on-disk event log matches serialize(). ---
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "hbf1xv_s3_eventlog_integration";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        sony_msx::machine::Hbf1xvMachine m;
        m.set_debug_root(root);
        m.set_event_logging_enabled(true);
        m.cold_boot();
        m.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
        m.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
        for (int i = 0; i < 4; ++i) {
            m.step_cpu_instruction();
        }
        const bool wrote_dump = m.write_state_dump("state.dump");

        // Dump event appended after the six execution events.
        const bool dump_event_ok =
            m.debug_event_log().size() == 7 &&
            m.debug_event_log().at(6).type == sony_msx::machine::DebugEventType::Dump &&
            m.debug_event_log().at(6).detail == "FILE=state.dump";

        const bool wrote_log = m.write_event_log("events.log");
        const std::string on_disk_log = read_file(root / "logs" / "events.log");
        const bool dump_file_exists = std::filesystem::exists(root / "traces" / "state.dump");

        std::filesystem::remove_all(root, ec);

        if (!expect_true(wrote_dump && wrote_log && dump_event_ok && dump_file_exists &&
                             on_disk_log == m.debug_event_log().serialize(),
                         "WriteStateDump_LogsDumpEvent_AndFlushesLogToDisk")) {
            return 1;
        }
    }

    // --- cold_boot re-arms a fresh, deterministic event stream. ---
    {
        sony_msx::machine::Hbf1xvMachine m;
        m.set_event_logging_enabled(true);
        m.cold_boot();
        m.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
        m.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
        m.step_cpu_instruction();
        const std::size_t after_first = m.debug_event_log().size();
        m.cold_boot();  // clears + re-records Reset.
        const bool rearmed = after_first == 2 && m.debug_event_log().size() == 1 &&
                             m.debug_event_log().at(0).type == sony_msx::machine::DebugEventType::Reset;
        if (!expect_true(rearmed, "ColdBoot_ReArmsFreshEventStream")) {
            return 1;
        }
    }

    return 0;
}
