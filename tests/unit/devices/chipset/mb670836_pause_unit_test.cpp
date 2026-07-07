#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "devices/chipset/mb670836_pause.h"

// Suite: Devices_Mb670836PauseController_Unit (M25-S1, backlog C8)
//
// Isolated unit tests for the Sony MB670836 hardware-PAUSE / Speed-Controller
// gate, per docs/m25-planner-package.md §2.4/M25-S1. Zero machine wiring at
// this slice -- this class is tested entirely on its own, deterministic
// duty-cycle arithmetic hand-computed and asserted, not merely observed.

namespace {

using sony_msx::devices::chipset::Mb670836PauseController;

int g_failures = 0;

void expect_true(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- reset() idle defaults. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        expect_true(!pc.button_engaged(), "Reset_Idle_ButtonNotEngaged");
        expect_true(pc.speed_level() == 0, "Reset_Idle_SpeedLevelZero");
        expect_true(!pc.speed_controller_paused_this_frame(),
                    "Reset_Idle_SpeedControllerNotPausedBeforeAnyVsync");
        expect_true(!pc.cpu_should_pause(), "Reset_Idle_CpuShouldNotPause");
    }

    // --- press_pause_button() toggles button_engaged() true->false->true. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        expect_true(!pc.button_engaged(), "Toggle_InitiallyDisengaged");
        pc.press_pause_button();
        expect_true(pc.button_engaged(), "Toggle_FirstPress_Engaged");
        expect_true(pc.cpu_should_pause(), "Toggle_FirstPress_CpuShouldPause");
        pc.press_pause_button();
        expect_true(!pc.button_engaged(), "Toggle_SecondPress_Disengaged");
        expect_true(!pc.cpu_should_pause(), "Toggle_SecondPress_CpuShouldNotPause");
        pc.press_pause_button();
        expect_true(pc.button_engaged(), "Toggle_ThirdPress_EngagedAgain");
    }

    // --- set_speed_level() clamping/range. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        pc.set_speed_level(3);
        expect_true(pc.speed_level() == 3, "SetSpeedLevel_InRange_StoredExactly");
        pc.set_speed_level(-5);
        expect_true(pc.speed_level() == 0, "SetSpeedLevel_Negative_ClampedToZero");
        pc.set_speed_level(1000);
        expect_true(pc.speed_level() == Mb670836PauseController::kMaxSpeedLevel,
                    "SetSpeedLevel_TooLarge_ClampedToMax");
        pc.set_speed_level(Mb670836PauseController::kMaxSpeedLevel);
        expect_true(pc.speed_level() == Mb670836PauseController::kMaxSpeedLevel,
                    "SetSpeedLevel_ExactlyMax_Accepted");
        pc.set_speed_level(0);
        expect_true(pc.speed_level() == 0, "SetSpeedLevel_ExactlyZero_Accepted");
    }

    // --- on_vsync() + duty-cycle formula, speed_level=3 over a full
    //     kPeriodFrames(=8)-length window: frame f paused iff
    //     (f % 8) < 3 -> hand-computed sequence [T,T,T,F,F,F,F,F]. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        pc.set_speed_level(3);
        const bool expected[8] = {true, true, true, false, false, false, false, false};
        bool all_ok = true;
        for (int i = 0; i < 8; ++i) {
            pc.on_vsync();
            if (pc.speed_controller_paused_this_frame() != expected[i]) {
                all_ok = false;
            }
        }
        expect_true(all_ok, "OnVsync_SpeedLevel3_ExactExpectedSequenceOverFullPeriod");
    }

    // --- on_vsync() + duty-cycle formula, a SECOND distinct speed level
    //     (speed_level=6 of 8): frame f paused iff (f % 8) < 6 -> hand-
    //     computed sequence [T,T,T,T,T,T,F,F]. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        pc.set_speed_level(6);
        const bool expected[8] = {true, true, true, true, true, true, false, false};
        bool all_ok = true;
        for (int i = 0; i < 8; ++i) {
            pc.on_vsync();
            if (pc.speed_controller_paused_this_frame() != expected[i]) {
                all_ok = false;
            }
        }
        expect_true(all_ok, "OnVsync_SpeedLevel6_ExactExpectedSequenceOverFullPeriod");
    }

    // --- The duty-cycle pattern REPEATS identically across a second period
    //     (deterministic modular arithmetic, not a one-shot). ---
    {
        Mb670836PauseController pc;
        pc.reset();
        pc.set_speed_level(3);
        bool first_period[8];
        bool second_period[8];
        for (int i = 0; i < 8; ++i) {
            pc.on_vsync();
            first_period[i] = pc.speed_controller_paused_this_frame();
        }
        for (int i = 0; i < 8; ++i) {
            pc.on_vsync();
            second_period[i] = pc.speed_controller_paused_this_frame();
        }
        bool identical = true;
        for (int i = 0; i < 8; ++i) {
            if (first_period[i] != second_period[i]) {
                identical = false;
            }
        }
        expect_true(identical, "OnVsync_SpeedLevel3_PeriodRepeatsIdenticallyAcrossSecondWindow");
    }

    // --- speed_level=0 (post-reset default): never paused, any number of
    //     on_vsync() calls. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        bool any_paused = false;
        for (int i = 0; i < 16; ++i) {
            pc.on_vsync();
            if (pc.speed_controller_paused_this_frame()) {
                any_paused = true;
            }
        }
        expect_true(!any_paused, "OnVsync_SpeedLevelZero_NeverPausedOverManyFrames");
    }

    // --- speed_level=kMaxSpeedLevel (7 of 8): paused every frame except the
    //     last of each period -> hand-computed [T,T,T,T,T,T,T,F]. ---
    {
        Mb670836PauseController pc;
        pc.reset();
        pc.set_speed_level(Mb670836PauseController::kMaxSpeedLevel);
        const bool expected[8] = {true, true, true, true, true, true, true, false};
        bool all_ok = true;
        for (int i = 0; i < 8; ++i) {
            pc.on_vsync();
            if (pc.speed_controller_paused_this_frame() != expected[i]) {
                all_ok = false;
            }
        }
        expect_true(all_ok, "OnVsync_MaxSpeedLevel_ExactExpectedSequence");
    }

    // --- cpu_should_pause() truth table: all 4 combinations of
    //     (button-engaged, speed-controller-paused). ---
    {
        // neither
        Mb670836PauseController pc;
        pc.reset();
        expect_true(!pc.cpu_should_pause(), "TruthTable_Neither_False");

        // button only
        pc.press_pause_button();
        expect_true(pc.cpu_should_pause(), "TruthTable_ButtonOnly_True");
        pc.press_pause_button();  // release
        expect_true(!pc.cpu_should_pause(), "TruthTable_ButtonReleased_BackToFalse");

        // speed-controller only
        pc.set_speed_level(Mb670836PauseController::kMaxSpeedLevel);
        pc.on_vsync();  // frame 0: 0 < 7 -> paused
        expect_true(pc.speed_controller_paused_this_frame(), "TruthTable_Setup_SpeedControllerPaused");
        expect_true(!pc.button_engaged(), "TruthTable_Setup_ButtonStillDisengaged");
        expect_true(pc.cpu_should_pause(), "TruthTable_SpeedControllerOnly_True");

        // both
        pc.press_pause_button();
        expect_true(pc.button_engaged() && pc.speed_controller_paused_this_frame(),
                    "TruthTable_Setup_BothEngaged");
        expect_true(pc.cpu_should_pause(), "TruthTable_Both_True");

        // button released, speed-controller still paused this frame -> still true
        pc.press_pause_button();
        expect_true(!pc.button_engaged() && pc.speed_controller_paused_this_frame(),
                    "TruthTable_Setup_ButtonReleasedSpeedStillPaused");
        expect_true(pc.cpu_should_pause(), "TruthTable_SpeedControllerAloneAgain_True");
    }

    // --- Two-instance determinism: identical operation sequence -> identical
    //     observable state. ---
    {
        auto run = []() {
            Mb670836PauseController pc;
            pc.reset();
            pc.set_speed_level(4);
            pc.press_pause_button();
            bool observed[10];
            for (int i = 0; i < 10; ++i) {
                pc.on_vsync();
                observed[i] = pc.cpu_should_pause();
            }
            return std::pair{pc.button_engaged(), std::vector<bool>(observed, observed + 10)};
        };
        const auto a = run();
        const auto b = run();
        expect_true(a.first == b.first && a.second == b.second,
                    "TwoInstanceDeterminism_IdenticalSequence_IdenticalState");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Mb670836PauseController_Unit cases passed\n";
    return 0;
}
