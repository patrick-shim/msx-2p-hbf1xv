#include "devices/chipset/system_control.h"

namespace sony_msx::devices::chipset {

void SystemControlF5::reset() {
    value_ = kResetValue;
}

core::BusData SystemControlF5::io_read(core::BusAddress /*port*/) {
    return value_;
}

void SystemControlF5::io_write(core::BusAddress /*port*/, const core::BusData value) {
    value_ = value;
}

std::uint8_t SystemControlF5::value() const {
    return value_;
}

bool SystemControlF5::clock_ic_enabled() const {
    return (value_ & kClockEnableBit) != 0;
}

}  // namespace sony_msx::devices::chipset
