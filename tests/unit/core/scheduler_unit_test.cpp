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

#include <cstdint>
#include <iostream>

#include "core/scheduler.h"

namespace {

bool expect_equal(const std::uint64_t lhs, const std::uint64_t rhs, const char* case_name) {
    if (lhs == rhs) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << ", expected " << rhs << ", got " << lhs << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Core_Scheduler_Unit
    sony_msx::core::Scheduler scheduler;

    scheduler.reset();
    if (!expect_equal(scheduler.total_cycles(), 0, "Reset_InitialState_ZeroCycles")) {
        return 1;
    }

    scheduler.tick(42);
    scheduler.tick(58);
    if (!expect_equal(scheduler.total_cycles(), 100, "Tick_MultiStep_AccumulatesDeterministically")) {
        return 1;
    }

    scheduler.tick_many(3, 10);
    if (!expect_equal(scheduler.total_cycles(), 130, "TickMany_ThreeSteps_AccumulatesDeterministically")) {
        return 1;
    }

    scheduler.tick_many(0, 999);
    if (!expect_equal(scheduler.total_cycles(), 130, "TickMany_ZeroSteps_NoStateChange")) {
        return 1;
    }

    const bool moved_forward = scheduler.advance_to(200);
    if (!(moved_forward && expect_equal(scheduler.total_cycles(), 200,
            "AdvanceTo_ForwardTarget_UpdatesDeterministically"))) {
        return 1;
    }

    const bool moved_backward = scheduler.advance_to(150);
    if (moved_backward || !expect_equal(scheduler.total_cycles(), 200,
            "AdvanceTo_BackwardTarget_NoStateChange")) {
        return 1;
    }

    return 0;
}
