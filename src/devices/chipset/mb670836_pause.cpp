#include "devices/chipset/mb670836_pause.h"

#include <algorithm>

namespace sony_msx::devices::chipset {

void Mb670836PauseController::reset() {
    engaged_ = false;
    speed_level_ = 0;
    speed_paused_ = false;
    frame_index_ = 0;
}

void Mb670836PauseController::press_pause_button() {
    engaged_ = !engaged_;
}

bool Mb670836PauseController::button_engaged() const {
    return engaged_;
}

void Mb670836PauseController::set_speed_level(const int level) {
    speed_level_ = std::clamp(level, 0, kMaxSpeedLevel);
}

int Mb670836PauseController::speed_level() const {
    return speed_level_;
}

void Mb670836PauseController::on_vsync() {
    // Evaluate the frame that is JUST STARTING (the pre-increment index),
    // then advance the counter for the next call. speed_level_==0 (the
    // deterministic post-reset() default) always yields false here, since
    // (f % kPeriodFrames) < 0 is never true -- a regression-safe idle
    // default (planner §2.4).
    const int position_in_period = static_cast<int>(frame_index_ % static_cast<std::uint64_t>(kPeriodFrames));
    speed_paused_ = position_in_period < speed_level_;
    ++frame_index_;
}

bool Mb670836PauseController::speed_controller_paused_this_frame() const {
    return speed_paused_;
}

bool Mb670836PauseController::cpu_should_pause() const {
    return engaged_ || speed_paused_;
}

}  // namespace sony_msx::devices::chipset
