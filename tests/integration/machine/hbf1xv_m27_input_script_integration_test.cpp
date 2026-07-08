#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

// Suite: Machine_Hbf1xvM27InputScript_Integration (M27-S7, "Production
// Hardening + Debug/Test Tooling" item 3, docs/m27-planner-package.md §2.4).
//
// Drives a REAL Hbf1xvMachine through a bounded, real CPU-execution loop
// (the SAME step_cpu_instruction() sub-loop the new headless --debug-session
// mode and the SDL3 Sdl3App::run_one_frame() both already run) with a real,
// text-format input script, asserting machine.keyboard().key(row,column)
// transitions at the EXACT expected T-state boundaries -- a deterministic,
// hand-computed oracle. Uses map_flat_ram() + a flat NOP program (mirrors
// run_parity_trace()'s own established flat-RAM convention) rather than a
// real BIOS boot: every NOP retires in EXACTLY 5 T-states (4 datasheet +
// 1 M1-wait, the S1985 per-opcode-fetch charge, hbf1xv_machine.cpp:462-468),
// so elapsed_cycles() after step i (0-indexed) is EXACTLY (i+1)*5 -- a fully
// hand-computable schedule, independent of real BIOS timing.

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::InputScriptPlayer;
    using sony_msx::machine::parse_input_script;

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();

    // A flat program of NOPs (0x00) is simply the RAM's own cold-boot
    // alternating 00/FF power-on pattern truncated at the first 0xFF byte --
    // load an EXPLICIT run of NOPs to avoid depending on that pattern.
    std::array<std::uint8_t, 64> nops{};
    nops.fill(0x00);
    machine.load_memory(0x0000, nops.data(), static_cast<std::uint32_t>(nops.size()));
    machine.cpu().state().regs().pc = 0x0000;

    const std::string script_text =
        "HBF1XV-INPUT-SCRIPT v1\n"
        "T=15 KEY=A DOWN\n"
        "T=30 KEY=A UP\n"
        "T=30 KEY=SPACE DOWN\n"
        "T=50 KEY=SPACE UP\n"
        "[END]\n";
    const std::vector<sony_msx::machine::InputScriptEvent> events = parse_input_script(script_text);
    expect(events.size() == 4, "Script_ParsesFourEvents");

    InputScriptPlayer player(events);

    // "A" -> (row=2, column=6); "SPACE" -> (row=8, column=0), per
    // msx_key_names.cpp (re-expressed from sdl3_input_mapper.cpp's own
    // table, R-M27-4).
    constexpr int kARow = 2;
    constexpr int kAColumn = 6;
    constexpr int kSpaceRow = 8;
    constexpr int kSpaceColumn = 0;

    bool checked_step3 = false;
    bool checked_step6 = false;
    bool checked_step10 = false;

    constexpr std::uint32_t kMaxSteps = 12;
    for (std::uint32_t step = 1; step <= kMaxSteps; ++step) {
        machine.step_cpu_instruction();
        player.apply_due(machine.elapsed_cycles(), machine.keyboard());

        if (step == 3) {
            // elapsed_cycles() == 15: T=15 KEY=A DOWN is due exactly here.
            expect(machine.elapsed_cycles() == 15, "Step3_ElapsedCycles_HandComputedOracle_Is15");
            expect(machine.keyboard().key(kARow, kAColumn), "Step3_AIsPressed_AtExactTStateBoundary");
            expect(!machine.keyboard().key(kSpaceRow, kSpaceColumn), "Step3_SpaceNotYetPressed");
            checked_step3 = true;
        }
        if (step == 6) {
            // elapsed_cycles() == 30: T=30 KEY=A UP and T=30 KEY=SPACE DOWN
            // are BOTH due exactly here, applied in file order.
            expect(machine.elapsed_cycles() == 30, "Step6_ElapsedCycles_HandComputedOracle_Is30");
            expect(!machine.keyboard().key(kARow, kAColumn), "Step6_AIsReleased_AtExactTStateBoundary");
            expect(machine.keyboard().key(kSpaceRow, kSpaceColumn), "Step6_SpaceIsPressed_AtExactTStateBoundary");
            checked_step6 = true;
        }
        if (step == 10) {
            // elapsed_cycles() == 50: T=50 KEY=SPACE UP is due exactly here.
            expect(machine.elapsed_cycles() == 50, "Step10_ElapsedCycles_HandComputedOracle_Is50");
            expect(!machine.keyboard().key(kSpaceRow, kSpaceColumn), "Step10_SpaceIsReleased_AtExactTStateBoundary");
            checked_step10 = true;
        }
    }

    expect(checked_step3 && checked_step6 && checked_step10, "AllThreeCheckpoints_WereReached");
    expect(player.cursor() == player.event_count(), "Player_AllEventsApplied_ByEndOfBoundedRun");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM27InputScript_Integration cases passed\n";
    return 0;
}
