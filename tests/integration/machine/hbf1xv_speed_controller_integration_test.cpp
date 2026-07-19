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
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvSpeedController_Integration (M25-S4, backlog C8)
//
// Drives the same deterministic counter-increment loop as
// hbf1xv_pause_integration_test.cpp purely via step_cpu_instruction()
// while calling machine.pause_controller().on_vsync() DIRECTLY at simulated
// VBlank boundaries -- NEVER calling run_frame() in the same test (R-M25-5,
// mirrors the established A-M24-8 precedent of never mixing run_frame()'s
// atomic scheduler tick with step_cpu_instruction()'s per-instruction tick).
//
// Simulated-VBlank quantum (a documented TESTING SIMPLIFICATION, not a
// production change): each simulated frame is exactly kStepsPerWindow=40
// step_cpu_instruction() calls -- a whole multiple of the 4-instruction
// counter loop body (10 full iterations), chosen specifically so the
// expected counter growth is an EXACT, hand-computable integer with zero
// partial-instruction overshoot ambiguity, rather than the real, literal
// frame_cycles_per_frame()==59736 T-state quantum (which would not divide
// the loop body's 44 T-state iteration cost evenly and would require
// modeling mid-instruction overshoot arithmetic unrelated to what this test
// needs to prove: that the Speed Controller's on_vsync()-driven duty cycle
// genuinely gates real CPU execution at the machine level).

namespace {

using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect_true(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();

    // 0000: LD A,(0010)   3A 10 00   (14T: 13 datasheet + 1 M1 wait)
    // 0003: INC A         3C         ( 5T:  4 datasheet + 1 M1 wait)
    // 0004: LD (0010),A   32 10 00   (14T: 13 datasheet + 1 M1 wait)
    // 0007: JP 0000       C3 00 00   (11T: 10 datasheet + 1 M1 wait)
    // Full-iteration cost: 14+5+14+11 = 44 T-states, ALWAYS exactly 4
    // step_cpu_instruction() calls per counter increment.
    const std::array<std::uint8_t, 10> program{
        0x3A, 0x10, 0x00,  // LD A,(0x0010)
        0x3C,              // INC A
        0x32, 0x10, 0x00,  // LD (0x0010),A
        0xC3, 0x00, 0x00,  // JP 0x0000
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.load_memory(0x0010, std::array<std::uint8_t, 1>{0x00}.data(), 1);

    constexpr int kStepsPerWindow = 40;       // 10 full loop iterations
    constexpr int kIterationsPerWindow = 10;  // kStepsPerWindow / 4
    constexpr int kIterationCost = 44;        // T-states/iteration (see above)

    constexpr int kSpeedLevel = 3;  // paused 3 of every kPeriodFrames(=8)
    machine.pause_controller().set_speed_level(kSpeedLevel);

    // Hand-computed duty-cycle sequence for speed_level=3 over kPeriodFrames
    // (=8): frame f paused iff (f % 8) < 3 -> [T,T,T,F,F,F,F,F], identical to
    // the M25-S1 Mb670836PauseController unit test's own oracle.
    constexpr int kTotalWindows = 16;  // exactly 2 full periods
    const bool expected_paused[kTotalWindows] = {
        true, true, true, false, false, false, false, false,   // period 1
        true, true, true, false, false, false, false, false,   // period 2 (repeats)
    };

    std::uint32_t counter = 0;  // total observed counter growth
    bool paused_windows_never_progress = true;
    bool paused_windows_all_one_tstate = true;
    bool unpaused_windows_progress_exactly_ten = true;
    bool unpaused_windows_sum_exactly_440 = true;

    for (int w = 0; w < kTotalWindows; ++w) {
        // Mark the frame that is about to run (planner §2.3 point 4: called
        // directly by the test, mirroring run_frame()'s own additive
        // on_vsync() hook).
        machine.pause_controller().on_vsync();
        const bool is_paused = machine.pause_controller().speed_controller_paused_this_frame();
        if (is_paused != expected_paused[w]) {
            std::cerr << "window " << w << ": expected paused=" << expected_paused[w]
                       << " got=" << is_paused << "\n";
            ++g_failures;
        }

        const std::uint8_t counter_before = machine.read_memory(0x0010);
        std::uint32_t tstate_sum = 0;
        bool all_one_tstate = true;
        for (int s = 0; s < kStepsPerWindow; ++s) {
            const std::uint32_t t = machine.step_cpu_instruction();
            tstate_sum += t;
            if (t != 1) {
                all_one_tstate = false;
            }
        }
        const std::uint8_t counter_after = machine.read_memory(0x0010);
        const std::uint8_t delta = static_cast<std::uint8_t>(counter_after - counter_before);

        if (expected_paused[w]) {
            if (delta != 0) {
                paused_windows_never_progress = false;
            }
            if (!all_one_tstate) {
                paused_windows_all_one_tstate = false;
            }
        } else {
            if (delta != kIterationsPerWindow) {
                unpaused_windows_progress_exactly_ten = false;
            }
            if (tstate_sum != static_cast<std::uint32_t>(kIterationsPerWindow) * kIterationCost) {
                unpaused_windows_sum_exactly_440 = false;
            }
            counter += delta;
        }
    }

    expect_true(paused_windows_never_progress,
                "SpeedController_PausedWindows_CounterNeverAdvances_ExactZeroDelta");
    expect_true(paused_windows_all_one_tstate,
                "SpeedController_PausedWindows_EveryStepChargesExactlyOneTState");
    expect_true(unpaused_windows_progress_exactly_ten,
                "SpeedController_UnpausedWindows_CounterAdvancesByExactlyTenIterations");
    expect_true(unpaused_windows_sum_exactly_440,
                "SpeedController_UnpausedWindows_TStateSumMatchesHandComputedTotal");

    // Total growth across 2 full periods: 5 unpaused windows/period * 2
    // periods * 10 iterations/window = 100 -- a fully hand-computable,
    // deterministic oracle (Acceptance Criterion 4).
    expect_true(counter == 100, "SpeedController_TotalGrowthOverTwoFullPeriods_MatchesHandComputedOracle");
    expect_true(machine.read_memory(0x0010) == 100,
                "SpeedController_FinalCounterMemoryValue_MatchesHandComputedOracle");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvSpeedController_Integration cases passed\n";
    return 0;
}
