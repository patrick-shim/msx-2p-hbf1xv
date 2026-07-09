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
