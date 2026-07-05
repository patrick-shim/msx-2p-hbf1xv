#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

namespace {

constexpr std::uint64_t kExpectedCyclesPerFrame = 228 * 262;

bool expect_equal(const std::uint64_t lhs, const std::uint64_t rhs, const char* case_name) {
    if (lhs == rhs) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << ", expected " << rhs << ", got " << lhs << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Machine_Hbf1xv_Integration
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();

    machine.run_frame();
    const std::uint64_t first = machine.elapsed_cycles();
    if (!expect_equal(first, kExpectedCyclesPerFrame, "RunFrame_OneStep_ReproducibleCycleCount")) {
        return 1;
    }

    machine.run_frame();
    const std::uint64_t second = machine.elapsed_cycles();
    if (!expect_equal(second, kExpectedCyclesPerFrame * 2, "RunFrame_TwoSteps_ReproducibleCycleCount")) {
        return 1;
    }

    machine.run_cycles(1234);
    const std::uint64_t third = machine.elapsed_cycles();
    if (!expect_equal(third, (kExpectedCyclesPerFrame * 2) + 1234,
                      "RunCycles_CustomStep_ReproducibleCycleCount")) {
        return 1;
    }

    const std::uint64_t frames_before_reset = machine.frame_count();
    if (!expect_equal(frames_before_reset, 2, "FrameCount_AfterTwoFrames_TracksRunFrameOnly")) {
        return 1;
    }

    machine.cold_boot();
    if (!expect_equal(machine.elapsed_cycles(), 0, "ColdBoot_AfterProgress_ResetsCycleCount")) {
        return 1;
    }

    if (!expect_equal(machine.frame_count(), 0, "ColdBoot_AfterProgress_ResetsFrameCount")) {
        return 1;
    }

    if (!expect_equal(machine.frame_cycles_per_frame(), kExpectedCyclesPerFrame,
                      "FrameCyclesPerFrame_Constant_ExpectedValue")) {
        return 1;
    }

    machine.run_frames(3);
    if (!expect_equal(machine.frame_count(), 3, "RunFrames_ThreeFrames_TracksFrameCount")) {
        return 1;
    }

    if (!expect_equal(machine.elapsed_cycles(), kExpectedCyclesPerFrame * 3,
                      "RunFrames_ThreeFrames_AccumulatesExpectedCycles")) {
        return 1;
    }

    const bool advanced = machine.run_until_cycle((kExpectedCyclesPerFrame * 3) + 77);
    if (!advanced || !expect_equal(machine.elapsed_cycles(), (kExpectedCyclesPerFrame * 3) + 77,
            "RunUntilCycle_ForwardTarget_AdvancesDeterministically")) {
        return 1;
    }

    const bool advanced_again = machine.run_until_cycle((kExpectedCyclesPerFrame * 3) + 77);
    if (advanced_again || !expect_equal(machine.elapsed_cycles(), (kExpectedCyclesPerFrame * 3) + 77,
            "RunUntilCycle_CurrentTarget_NoStateChange")) {
        return 1;
    }

    return 0;
}
