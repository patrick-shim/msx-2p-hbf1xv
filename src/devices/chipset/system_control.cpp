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
