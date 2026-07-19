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

// Suite: System_Hbf1xvM25SpeedPauseRensha_System
//
// Milestone-required "dedicated system integration test" (docs/
// m25-planner-package.md M25-S5): combines all three new HB-F1XV-specific
// mechanisms -- hardware PAUSE (MB670836), the Speed Controller's VBlank-
// synced duty cycle, and Ren-Sha Turbo autofire -- against ONE real,
// stepped CPU program on ONE machine instance, in a single deterministic
// scenario (tests/CLAUDE.md's system-test tier: machine-level, real CPU
// program, real bus/device wiring).
//
// Phase 1: hardware PAUSE engage/release mid-program, driving a
//   deterministic counter-increment loop -- proves the machine-level gate
//   genuinely freezes and resumes real CPU execution.
// Phase 2: Speed Controller duty-cycle slow-motion across several simulated
//   VBlank windows (pause_controller().on_vsync() called directly, per
//   R-M25-5 -- run_frame() is never called in this test).
// Phase 3: Ren-Sha Turbo observed toggling the SPACE-key row and a joystick
//   trigger over many real CPU-driven cycles, both while held (alternation
//   observed) and, as a negative-control assertion, while NOT held (zero
//   observable effect at any sampled cycle, R-M25-6).

#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"
#include "peripherals/joystick.h"

namespace {

using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// The same 10-byte deterministic counter-increment loop used by the M25-S4
// integration tests: LD A,(0010) / INC A / LD (0010),A / JP 0000.
constexpr std::array<std::uint8_t, 10> kCounterLoop{
    0x3A, 0x10, 0x00, 0x3C, 0x32, 0x10, 0x00, 0xC3, 0x00, 0x00,
};

// A tight infinite self-jump loop (JP 0x0000, 11T/iteration -- 10 datasheet
// + 1 M1 wait) used purely to drive elapsed_cycles() forward deterministically
// for the Ren-Sha Turbo cycle-sampling phase.
constexpr std::array<std::uint8_t, 3> kSpinLoop{0xC3, 0x00, 0x00};

}  // namespace

int main() {
    // ================= Phase 1: hardware PAUSE engage/release =================
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_memory(0x0000, kCounterLoop.data(), static_cast<std::uint32_t>(kCounterLoop.size()));
        machine.load_memory(0x0010, std::array<std::uint8_t, 1>{0x00}.data(), 1);

        // Run one full loop iteration (4 steps): counter -> 1, PC back at 0.
        for (int i = 0; i < 4; ++i) {
            machine.step_cpu_instruction();
        }
        expect(machine.read_memory(0x0010) == 1, "Phase1_OneFullIteration_CounterEqualsOne");
        expect(machine.cpu().state().regs().pc == 0x0000, "Phase1_OneFullIteration_PcBackAtOrigin");

        machine.pause_controller().press_pause_button();
        expect(machine.pause_controller().cpu_should_pause(), "Phase1_PressPauseButton_GateEngaged");

        const std::uint16_t pc_frozen = machine.cpu().state().regs().pc;
        for (int i = 0; i < 50; ++i) {
            const std::uint32_t t = machine.step_cpu_instruction();
            if (t != 1) {
                ++g_failures;
                std::cerr << "Case failed: Phase1_PausedStep_NotOneTState (i=" << i << ")\n";
            }
        }
        expect(machine.cpu().state().regs().pc == pc_frozen, "Phase1_AfterFiftyPausedSteps_PcStillFrozen");
        expect(machine.read_memory(0x0010) == 1, "Phase1_AfterFiftyPausedSteps_CounterStillFrozen");

        machine.pause_controller().press_pause_button();
        expect(!machine.pause_controller().cpu_should_pause(), "Phase1_ReleasePauseButton_GateDisengaged");

        for (int i = 0; i < 4; ++i) {
            machine.step_cpu_instruction();
        }
        expect(machine.read_memory(0x0010) == 2, "Phase1_ResumedOneMoreIteration_CounterEqualsTwo");
    }

    // ================= Phase 2: Speed Controller duty-cycle slow-motion =======
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_memory(0x0000, kCounterLoop.data(), static_cast<std::uint32_t>(kCounterLoop.size()));
        machine.load_memory(0x0010, std::array<std::uint8_t, 1>{0x00}.data(), 1);

        constexpr int kSpeedLevel = 2;  // paused 2 of every 8 simulated VBlanks
        machine.pause_controller().set_speed_level(kSpeedLevel);

        constexpr int kStepsPerWindow = 8;  // 2 full loop iterations
        constexpr int kTotalWindows = 8;    // one full period
        std::uint32_t total_growth = 0;
        for (int w = 0; w < kTotalWindows; ++w) {
            machine.pause_controller().on_vsync();
            const bool expected_paused = w < kSpeedLevel;  // (w % 8) < 2
            expect(machine.pause_controller().speed_controller_paused_this_frame() == expected_paused,
                   "Phase2_DutyCycleWindow_MatchesHandComputedPausedState");

            const std::uint8_t before = machine.read_memory(0x0010);
            for (int s = 0; s < kStepsPerWindow; ++s) {
                machine.step_cpu_instruction();
            }
            const std::uint8_t after = machine.read_memory(0x0010);
            const std::uint8_t delta = static_cast<std::uint8_t>(after - before);
            if (expected_paused) {
                if (delta != 0) {
                    ++g_failures;
                    std::cerr << "Case failed: Phase2_PausedWindow_ExpectedZeroDelta (w=" << w << ")\n";
                }
            } else {
                if (delta != 2) {  // 8 steps / 4 steps-per-iteration = 2 iterations
                    ++g_failures;
                    std::cerr << "Case failed: Phase2_UnpausedWindow_ExpectedTwoIterations (w=" << w << ")\n";
                }
                total_growth += delta;
            }
        }
        // 6 unpaused windows * 2 iterations/window = 12 (hand-computed oracle).
        expect(total_growth == 12, "Phase2_TotalGrowthOverOnePeriod_MatchesHandComputedOracle");
        expect(machine.read_memory(0x0010) == 12, "Phase2_FinalCounterValue_MatchesHandComputedOracle");
    }

    // ================= Phase 3: Ren-Sha Turbo (held + negative control) =======
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_memory(0x0000, kSpinLoop.data(), static_cast<std::uint32_t>(kSpinLoop.size()));

        machine.rensha_turbo().set_speed(100);
        expect(machine.rensha_turbo().speed() == 100, "Phase3_SetSpeed_StoredExactly");

        // --- Negative control FIRST: SPACE/trigger-A NOT held, RenSha
        //     engaged -- ZERO observable effect at ANY sampled cycle
        //     (R-M25-6), sampled across many real CPU-driven cycles. ---
        bool keyboard_never_spurious = true;
        bool joystick_never_spurious = true;
        for (int i = 0; i < 3000; ++i) {
            machine.step_cpu_instruction();  // 11T/step (JP $), spin loop
            if (i % 37 == 0) {
                if (machine.keyboard().keyboard_row(8) != 0xFF) {
                    keyboard_never_spurious = false;
                }
                if (machine.joystick().read_port_a() != 0xFF) {
                    joystick_never_spurious = false;
                }
            }
        }
        expect(keyboard_never_spurious,
               "Phase3_NotHeld_KeyboardRow8_NeverSpuriouslyPressed_RealCpuDrivenCycles");
        expect(joystick_never_spurious,
               "Phase3_NotHeld_JoystickTriggerA_NeverSpuriouslyPressed_RealCpuDrivenCycles");

        // --- SPACE key + joystick trigger-A held: over the same real-CPU-
        //     driven cycle range, the read alternates between pressed and
        //     released (both observed). ---
        machine.keyboard().set_key(8, 0, true);
        sony_msx::peripherals::JoystickPorts::PortState state;
        state.trigger_a = true;
        machine.joystick().set_port(0, state);

        bool saw_keyboard_pressed = false;
        bool saw_keyboard_released = false;
        bool saw_joystick_pressed = false;
        bool saw_joystick_released = false;
        for (int i = 0; i < 12000; ++i) {
            machine.step_cpu_instruction();
            if (i % 37 == 0) {
                const std::uint8_t row8 = machine.keyboard().keyboard_row(8);
                if ((row8 & 0x01) == 0) {
                    saw_keyboard_pressed = true;
                } else {
                    saw_keyboard_released = true;
                }
                const std::uint8_t port_a = machine.joystick().read_port_a();
                if ((port_a & 0x10) == 0) {
                    saw_joystick_pressed = true;
                } else {
                    saw_joystick_released = true;
                }
            }
        }
        expect(saw_keyboard_pressed && saw_keyboard_released,
               "Phase3_Held_KeyboardRow8_AlternatesBetweenPressedAndReleased");
        expect(saw_joystick_pressed && saw_joystick_released,
               "Phase3_Held_JoystickTriggerA_AlternatesBetweenPressedAndReleased");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All System_Hbf1xvM25SpeedPauseRensha_System cases passed\n";
    return 0;
}
