#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvM25Pause_Integration (M25-S4, backlog C8)
//
// Machine-level wiring test for the Sony MB670836 hardware-PAUSE gate
// (docs/m25-planner-package.md §2.3/§2.4): (a) REGRESSION GUARD -- with the
// pause controller in its default (post-cold_boot()) state,
// step_cpu_instruction()'s observable behavior is byte-for-byte identical
// to the pre-M25 CPU-timing oracle (the exact program from
// tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp, read but
// NOT modified -- that file remains one of the named zero-tolerance
// oracles); (b) a deterministic counter-increment loop proves PC/every
// register/R/memory stay byte-for-byte FROZEN across multiple paused
// step_cpu_instruction() calls, with elapsed_cycles() still advancing (1
// T-state/call); (c) releasing the gate resumes execution from the exact
// frozen PC and the counter continues incrementing correctly.

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
    // --- (a) REGRESSION GUARD: default (idle) pause controller state after
    //     cold_boot() reproduces hbf1xv_cpu_step_integration_test.cpp's own
    //     T-state/register/PC/elapsed_cycles oracle byte-for-byte. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        expect_true(!machine.pause_controller().cpu_should_pause(),
                    "Regression_ColdBoot_PauseControllerDefaultIdle");

        const std::array<std::uint8_t, 4> program{
            0x3E, 0x2A,  // LD A,0x2A
            0x3C,        // INC A
            0x76,        // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

        const std::uint32_t t0 = machine.step_cpu_instruction();
        expect_true(t0 == 8 && machine.cpu().state().regs().a() == 0x2A,
                    "Regression_LdAImmediate_SameAsPreM25Oracle");
        const std::uint32_t t1 = machine.step_cpu_instruction();
        expect_true(t1 == 5 && machine.cpu().state().regs().a() == 0x2B,
                    "Regression_IncA_SameAsPreM25Oracle");
        const std::uint32_t t2 = machine.step_cpu_instruction();
        expect_true(t2 == 5 && machine.cpu().state().halted(),
                    "Regression_Halt_SameAsPreM25Oracle");
        const std::uint16_t pc_before_idle = machine.cpu().state().regs().pc;
        const std::uint32_t t3 = machine.step_cpu_instruction();
        expect_true(t3 == 5 && machine.cpu().state().regs().pc == pc_before_idle,
                    "Regression_HaltedIdle_SameAsPreM25Oracle");
        expect_true(machine.elapsed_cycles() == 23,
                    "Regression_ElapsedCycles_SameAsPreM25Oracle");
    }

    // --- (b)/(c): a deterministic counter-increment loop, paused/resumed
    //     via the MB670836 hardware-PAUSE button. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        // 0000: LD A,(0010)   3A 10 00
        // 0003: INC A         3C
        // 0004: LD (0010),A   32 10 00
        // 0007: JP 0000       C3 00 00
        // 0010: DB 0          (the counter byte)
        const std::array<std::uint8_t, 10> program{
            0x3A, 0x10, 0x00,  // LD A,(0x0010)
            0x3C,              // INC A
            0x32, 0x10, 0x00,  // LD (0x0010),A
            0xC3, 0x00, 0x00,  // JP 0x0000
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.load_memory(0x0010, std::array<std::uint8_t, 1>{0x00}.data(), 1);

        // Step forward N instructions (exactly one full loop iteration: LD
        // A,(nn) / INC A / LD (nn),A / JP nn -- N=4 lands back at PC=0).
        constexpr int kN = 4;
        for (int i = 0; i < kN; ++i) {
            machine.step_cpu_instruction();
        }
        expect_true(machine.cpu().state().regs().pc == 0x0000,
                    "Setup_AfterOneFullLoop_PcBackAtOrigin");
        expect_true(machine.read_memory(0x0010) == 1, "Setup_AfterOneFullLoop_CounterIncrementedOnce");

        // Engage hardware PAUSE.
        machine.pause_controller().press_pause_button();
        expect_true(machine.pause_controller().button_engaged(), "Engage_ButtonEngagedTrue");
        expect_true(machine.pause_controller().cpu_should_pause(), "Engage_CpuShouldPauseTrue");

        const std::uint16_t pc_frozen = machine.cpu().state().regs().pc;
        const auto regs_frozen = machine.cpu().state().regs();
        const std::uint8_t counter_frozen = machine.read_memory(0x0010);
        const std::uint64_t cycles_before_pause = machine.elapsed_cycles();

        // Step forward M MORE times while paused: PC/every register/R/memory
        // must be byte-for-byte IDENTICAL across every one of these calls,
        // and elapsed_cycles() must advance by EXACTLY M (1 T-state/call).
        constexpr int kM = 20;
        bool frozen_ok = true;
        for (int i = 0; i < kM; ++i) {
            const std::uint32_t t = machine.step_cpu_instruction();
            if (t != 1) {
                frozen_ok = false;
            }
            const auto& regs_now = machine.cpu().state().regs();
            if (regs_now.pc != pc_frozen || regs_now.af != regs_frozen.af ||
                regs_now.bc != regs_frozen.bc || regs_now.de != regs_frozen.de ||
                regs_now.hl != regs_frozen.hl || regs_now.ix != regs_frozen.ix ||
                regs_now.iy != regs_frozen.iy || regs_now.sp != regs_frozen.sp ||
                regs_now.i != regs_frozen.i || regs_now.r != regs_frozen.r ||
                regs_now.af_shadow != regs_frozen.af_shadow || regs_now.bc_shadow != regs_frozen.bc_shadow ||
                regs_now.de_shadow != regs_frozen.de_shadow || regs_now.hl_shadow != regs_frozen.hl_shadow) {
                frozen_ok = false;
            }
            if (machine.read_memory(0x0010) != counter_frozen) {
                frozen_ok = false;
            }
        }
        expect_true(frozen_ok, "Paused_MSteps_PcRegistersRAndMemoryByteForByteFrozen");
        expect_true(machine.elapsed_cycles() == cycles_before_pause + kM,
                    "Paused_MSteps_ElapsedCyclesAdvancesByExactlyMOneTStatePerCall");

        // Release the PAUSE button; confirm execution resumes from the exact
        // frozen PC and the counter continues incrementing correctly.
        machine.pause_controller().press_pause_button();
        expect_true(!machine.pause_controller().button_engaged(), "Release_ButtonDisengaged");
        expect_true(!machine.pause_controller().cpu_should_pause(), "Release_CpuShouldPauseFalse");
        expect_true(machine.cpu().state().regs().pc == pc_frozen,
                    "Release_PcStillAtFrozenValueImmediatelyAfterRelease");

        // Run one more full loop iteration (5 real steps) and confirm the
        // counter increments again, correctly, from the resumed state.
        for (int i = 0; i < kN; ++i) {
            machine.step_cpu_instruction();
        }
        expect_true(machine.cpu().state().regs().pc == 0x0000,
                    "Resume_AfterOneMoreFullLoop_PcBackAtOrigin");
        expect_true(machine.read_memory(0x0010) == counter_frozen + 1,
                    "Resume_AfterOneMoreFullLoop_CounterIncrementedAgainCorrectly");
    }

    // --- Acceptance Criterion 3: PAUSE cannot be bypassed via any CPU-
    //     visible API. A CPU program alone (any instruction sequence,
    //     including port I/O) cannot self-release the gate -- only
    //     Mb670836PauseController::press_pause_button() can. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        // A program that touches I/O ports and does arithmetic -- if ANY
        // CPU-visible mechanism could clear the gate, this would do it.
        const std::array<std::uint8_t, 8> program{
            0x3E, 0xFF,  // LD A,0xFF
            0xD3, 0xA8,  // OUT (0xA8),A   (a real, wired I/O port)
            0xDB, 0xA9,  // IN A,(0xA9)    (a real, wired I/O port)
            0x3C,        // INC A
            0xC3,        // (JP target unused this test)
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

        machine.pause_controller().press_pause_button();
        expect_true(machine.pause_controller().cpu_should_pause(), "Bypass_Setup_Engaged");

        for (int i = 0; i < 10; ++i) {
            machine.step_cpu_instruction();
        }
        expect_true(machine.pause_controller().cpu_should_pause(),
                    "Bypass_TenStepsOfIoAndArithmeticProgram_GateStillEngaged");
        expect_true(machine.cpu().state().regs().pc == 0x0000,
                    "Bypass_TenSteps_PcNeverAdvancedPastOrigin");

        // Only press_pause_button() releases it.
        machine.pause_controller().press_pause_button();
        expect_true(!machine.pause_controller().cpu_should_pause(),
                    "Bypass_OnlyPressPauseButtonReleases_GateNowDisengaged");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM25Pause_Integration cases passed\n";
    return 0;
}
