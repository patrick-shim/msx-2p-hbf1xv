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

#include "devices/chipset/system_bus.h"

namespace sony_msx::devices::chipset {

SystemBus::SystemBus(SlotBus& slot_bus, IoBus& io_bus) : slot_bus_(slot_bus), io_bus_(io_bus) {
}

core::BusData SystemBus::read(const core::BusAddress address) {
    return slot_bus_.read(address);
}

void SystemBus::write(const core::BusAddress address, const core::BusData value) {
    slot_bus_.write(address, value);
}

core::BusData SystemBus::io_read(const core::BusAddress port) {
    return io_bus_.io_read(port);
}

void SystemBus::io_write(const core::BusAddress port, const core::BusData value) {
    io_bus_.io_write(port, value);
}

}  // namespace sony_msx::devices::chipset
