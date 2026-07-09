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

#include "devices/chipset/switched_io.h"

namespace sony_msx::devices::chipset {

namespace {
constexpr core::BusData kOpenBus = 0xFF;
}  // namespace

void SwitchedIoController::attach(core::SwitchedDevice* device) {
    if (device == nullptr) {
        return;
    }
    devices_[device->id()] = device;
}

void SwitchedIoController::reset() {
    selected_ = 0;
}

core::BusData SwitchedIoController::io_read(const core::BusAddress port) {
    core::SwitchedDevice* const device = devices_[selected_];
    if (device == nullptr) {
        return kOpenBus;
    }
    return device->switched_read(port);
}

void SwitchedIoController::io_write(const core::BusAddress port, const core::BusData value) {
    if ((port & 0x0F) == 0x00) {
        selected_ = value;
        return;
    }
    core::SwitchedDevice* const device = devices_[selected_];
    if (device == nullptr) {
        return;
    }
    device->switched_write(port, value);
}

std::uint8_t SwitchedIoController::selected() const {
    return selected_;
}

}  // namespace sony_msx::devices::chipset
