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

#include "core/scheduler.h"

namespace sony_msx::core {

void Scheduler::reset() {
    total_cycles_ = 0;
}

void Scheduler::tick(const std::uint64_t cycles) {
    total_cycles_ += cycles;
}

void Scheduler::tick_many(const std::uint32_t steps, const std::uint64_t cycles_per_step) {
    total_cycles_ += static_cast<std::uint64_t>(steps) * cycles_per_step;
}

bool Scheduler::advance_to(const std::uint64_t target_cycles) {
    if (target_cycles <= total_cycles_) {
        return false;
    }

    total_cycles_ = target_cycles;
    return true;
}

std::uint64_t Scheduler::total_cycles() const {
    return total_cycles_;
}

}  // namespace sony_msx::core
