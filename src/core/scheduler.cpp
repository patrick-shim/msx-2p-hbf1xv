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
