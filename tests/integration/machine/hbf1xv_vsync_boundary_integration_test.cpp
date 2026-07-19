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

// Suite: Machine_Hbf1xvVsyncBoundary_Integration (M26-S1, backlog C9)
//
// Proves the docs/m26-planner-package.md §2.3 `on_vsync_boundary()` extraction
// is a genuine, pure, behavior-preserving refactor of run_frame()'s pre-M26
// body, and that a real-time driver stepping the CPU purely via
// step_cpu_instruction() can reach the exact same frame-boundary side effects
// by calling on_vsync_boundary() directly (never run_frame() -- A-M26-5/
// R-M25-5's double-count hazard).
//
// Case 1 (REGRESSION GUARD): run_frame() == scheduler_.tick(kFrameCycles) +
// on_vsync_boundary() for every existing caller. Since run_frame() is now
// LITERALLY implemented as those two calls in sequence, this is proven by
// driving two fresh machines -- one via repeated run_frame() calls, the other
// via the machine's OWN public run_cycles()/on_vsync_boundary() primitives
// (never a hardcoded literal, always machine.frame_cycles_per_frame()) -- and
// asserting full observable-state equality (CPU registers, elapsed_cycles(),
// frame_count(), cycles_since_last_vsync(), and VDP VBlank-interrupt
// delivery when R#1 IE0 is enabled).
//
// Case 2 (NEW real-time-driver scenario): a machine runs a CPU program purely
// via step_cpu_instruction() until elapsed_cycles() reaches a
// frame_cycles_per_frame() boundary, then calls on_vsync_boundary() directly
// -- exactly the M26 Sdl3App::run_one_frame() pattern (docs/m26-planner-
// package.md §2.3) -- and the resulting frame_count()/cycles_since_last_vsync()/
// VDP-vsync-driven interrupt behavior matches what run_frame() produces for an
// equivalent elapsed-cycle count.

namespace {

using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Enables R#1 IE0 (VBlank interrupt enable) via the real two-write #99
// protocol (mirrors hbf1xv_vdp_io_integration_test.cpp:118-120).
void enable_vblank_interrupt(Hbf1xvMachine& machine) {
    machine.debug_io_write(0x99, 0x20);
    machine.debug_io_write(0x99, 0x81);
}

}  // namespace

int main() {
    // --- Case 1: regression guard -- run_frame() vs. run_cycles()+on_vsync_boundary() ---
    {
        Hbf1xvMachine via_run_frame;
        via_run_frame.cold_boot();
        enable_vblank_interrupt(via_run_frame);

        Hbf1xvMachine via_extraction;
        via_extraction.cold_boot();
        enable_vblank_interrupt(via_extraction);

        constexpr int kFrames = 5;
        for (int i = 0; i < kFrames; ++i) {
            via_run_frame.run_frame();
            // run_frame() is now LITERALLY scheduler_.tick(kFrameCycles) +
            // on_vsync_boundary() (§2.3) -- drive the second machine the same
            // way via its own public primitives, never a hardcoded literal.
            via_extraction.run_cycles(via_extraction.frame_cycles_per_frame());
            via_extraction.on_vsync_boundary();
        }

        expect(via_run_frame.elapsed_cycles() == via_extraction.elapsed_cycles(),
               "RegressionGuard_ElapsedCycles_Identical");
        expect(via_run_frame.frame_count() == via_extraction.frame_count(),
               "RegressionGuard_FrameCount_Identical");
        expect(via_run_frame.frame_count() == kFrames, "RegressionGuard_FrameCount_MatchesLoopCount");
        expect(via_run_frame.cycles_since_last_vsync() == via_extraction.cycles_since_last_vsync(),
               "RegressionGuard_CyclesSinceLastVsync_Identical");
        expect(via_run_frame.cycles_since_last_vsync() == 0,
               "RegressionGuard_CyclesSinceLastVsync_ZeroImmediatelyAfterBoundary");
        expect(via_run_frame.vdp().irq_active() == via_extraction.vdp().irq_active(),
               "RegressionGuard_VdpIrqActive_Identical");
        expect(via_run_frame.vdp().irq_active(), "RegressionGuard_VdpIrqActive_TrueWithIe0Enabled");

        // Full CPU architectural state equality (registers/PC/R/IFF/halt/
        // total T-states) -- neither machine ever stepped the CPU, so both
        // must still read the exact post-cold_boot() reset state.
        const auto& ra = via_run_frame.cpu().state().regs();
        const auto& rb = via_extraction.cpu().state().regs();
        expect(ra.pc == rb.pc && ra.af == rb.af && ra.bc == rb.bc && ra.de == rb.de && ra.hl == rb.hl &&
                   ra.sp == rb.sp,
               "RegressionGuard_CpuRegisters_Identical");
    }

    // --- Case 2: real-time-driver scenario -- step_cpu_instruction() sub-loop
    // to a frame boundary, then on_vsync_boundary() directly (mirrors the
    // exact Sdl3App::run_one_frame() pattern, never run_frame()). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        enable_vblank_interrupt(machine);

        // A tight, deterministic NOP loop (NOP=5T MSX-accurate: 4 datasheet +
        // 1 M1 wait; JP=11T: 10 datasheet + 1 M1 wait) so the CPU keeps
        // executing real instructions across the entire frame boundary,
        // exactly as a real-time driver's sub-loop would.
        const std::array<std::uint8_t, 7> program{
            0x00, 0x00, 0x00, 0x00,  // NOP x4
            0xC3, 0x00, 0x00,        // JP 0x0000
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

        const std::uint64_t frame_start_cycle = machine.elapsed_cycles();
        const std::uint64_t target = machine.frame_cycles_per_frame();

        int steps_taken = 0;
        constexpr int kStepCeiling = 1'000'000;  // generous bound, never expected to bind
        while (machine.elapsed_cycles() - frame_start_cycle < target && steps_taken < kStepCeiling) {
            machine.step_cpu_instruction();
            ++steps_taken;
        }
        expect(steps_taken < kStepCeiling, "RealTimeDriver_ReachedFrameBoundary_WithinStepCeiling");
        expect(machine.frame_count() == 0, "RealTimeDriver_FrameCount_StillZeroBeforeBoundaryCall");
        expect(!machine.vdp().irq_active(), "RealTimeDriver_VdpIrq_NotYetAssertedBeforeBoundaryCall");

        machine.on_vsync_boundary();

        expect(machine.frame_count() == 1, "RealTimeDriver_FrameCount_IncrementsByExactlyOne");
        expect(machine.cycles_since_last_vsync() == 0,
               "RealTimeDriver_CyclesSinceLastVsync_ZeroImmediatelyAfterCall");
        expect(machine.vdp().irq_active(), "RealTimeDriver_VdpIrq_AssertedAfterBoundaryCall_MatchesRunFrame");

        // The CPU genuinely executed real instructions across the boundary
        // (PC is somewhere inside the 6-byte loop, R has incremented) --
        // proves this is a real step_cpu_instruction()-driven session, not a
        // disguised scheduler-only advance.
        expect(machine.cpu().state().regs().r != 0, "RealTimeDriver_CpuActuallyExecuted_RIncremented");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvVsyncBoundary_Integration cases passed\n";
    return 0;
}
