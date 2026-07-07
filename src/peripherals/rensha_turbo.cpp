#include "peripherals/rensha_turbo.h"

#include <algorithm>

namespace sony_msx::peripherals {

void RenshaTurbo::reset() {
    speed_ = 0;
    // clock_source_ is NOT reset (mirrors CassetteInterface's own reset()
    // precedent -- an attached clock/dependency source is wired once at
    // machine composition and persists across a cold boot).
}

void RenshaTurbo::attach_clock_source(RenshaTurboClockSource* source) {
    clock_source_ = source;
}

void RenshaTurbo::set_speed(const int speed) {
    speed_ = std::clamp(speed, 0, 100);
}

int RenshaTurbo::speed() const {
    return speed_;
}

std::uint64_t RenshaTurbo::half_period_cycles() const {
    // Linear interpolation of `ints` between kDefaultMaxInts (speed=1) and
    // kDefaultMinInts (speed=100) -- see the header comment for the full,
    // independently-derived frequency formula (A-M25-6).
    const std::uint64_t speed64 = static_cast<std::uint64_t>(speed_);
    const std::uint64_t span = kDefaultMaxInts - kDefaultMinInts;
    const std::uint64_t ints = kDefaultMaxInts - (speed64 * span) / 100;
    return (kSystemClockHz * ints) / 6000;
}

bool RenshaTurbo::signal() const {
    if (speed_ == 0 || clock_source_ == nullptr) {
        return false;
    }
    const std::uint64_t period = half_period_cycles();
    if (period == 0) {
        return false;  // defensive: never divide by zero
    }
    return ((clock_source_->cpu_cycles() / period) & 1u) == 1u;
}

std::uint8_t RenshaTurbo::keyboard_row8_or_mask() const {
    return signal() ? 0x01 : 0x00;
}

std::uint8_t RenshaTurbo::joystick_trigger_a_or_mask() const {
    return signal() ? 0x10 : 0x00;
}

}  // namespace sony_msx::peripherals
