#include "devices/chipset/reset_status_register.h"

namespace sony_msx::devices::chipset {

void ResetStatusRegister::power_on_reset() {
    status_ = kPowerOnStatus;
}

core::BusData ResetStatusRegister::io_read(core::BusAddress /*port*/) {
    return status_;
}

void ResetStatusRegister::io_write(core::BusAddress /*port*/, const core::BusData value) {
    // Non-inverted variant write mask
    // (references/openmsx-21.0/src/MSXResetStatusRegister.cc:28-35,
    // independently re-expressed): bit 5 of the stored value is preserved,
    // bits 7/5 of the written value are captured, all other bits ignored.
    status_ = static_cast<std::uint8_t>((status_ & 0x20) | (value & 0xA0));
}

std::uint8_t ResetStatusRegister::status() const {
    return status_;
}

}  // namespace sony_msx::devices::chipset
