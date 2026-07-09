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

#include "devices/chipset/slot_bus.h"

namespace sony_msx::devices::chipset {

namespace {
constexpr core::BusData kOpenBus = 0xFF;

int page_of(const core::BusAddress address) {
    return (address >> 14) & 0x03;
}
}  // namespace

void SlotBus::reset() {
    primary_select_ = 0;
    sub_slot_register_.fill(0);
}

void SlotBus::attach(const int primary, const int sub, const int page, core::MemoryDevice* device) {
    if (primary < 0 || primary >= kSlots || sub < 0 || sub >= kSlots || page < 0 || page >= kPages) {
        return;
    }
    devices_[primary][sub][page] = device;
}

void SlotBus::set_expanded(const int primary, const bool expanded) {
    if (primary < 0 || primary >= kSlots) {
        return;
    }
    expanded_[primary] = expanded;
}

bool SlotBus::is_expanded(const int primary) const {
    if (primary < 0 || primary >= kSlots) {
        return false;
    }
    return expanded_[primary];
}

void SlotBus::set_primary_select(const std::uint8_t value) {
    primary_select_ = value;
}

std::uint8_t SlotBus::primary_select() const {
    return primary_select_;
}

int SlotBus::primary_for_page(const int page) const {
    return (primary_select_ >> (2 * page)) & 0x03;
}

int SlotBus::sub_for_page(const int primary, const int page) const {
    if (!expanded_[primary]) {
        return 0;
    }
    return (sub_slot_register_[primary] >> (2 * page)) & 0x03;
}

std::uint8_t SlotBus::sub_slot_register(const int primary) const {
    if (primary < 0 || primary >= kSlots) {
        return 0;
    }
    return sub_slot_register_[primary];
}

void SlotBus::write_ffff(const core::BusData value) {
    // #FFFF sets the sub-slot register of the primary slot currently mapped to
    // page 3 (openMSX MSXCPUInterface.cc:757 setSubSlot(primarySlotState[3], value)).
    const int primary = primary_for_page(3);
    sub_slot_register_[primary] = value;
}

core::BusData SlotBus::read_ffff() const {
    // 0xFF ^ subSlotRegister (MSXCPUInterface.cc:209-210,769-770).
    const int primary = primary_for_page(3);
    return static_cast<core::BusData>(0xFF ^ sub_slot_register_[primary]);
}

core::BusData SlotBus::read(const core::BusAddress address) {
    const int page = page_of(address);
    const int primary = primary_for_page(page);

    // Secondary-slot register readback only when the page-3 primary is expanded.
    if (address == 0xFFFF && expanded_[primary]) {
        return read_ffff();
    }

    const int sub = sub_for_page(primary, page);
    core::MemoryDevice* const device = devices_[primary][sub][page];
    if (device == nullptr) {
        return kOpenBus;
    }
    return device->mem_read(address);
}

void SlotBus::write(const core::BusAddress address, const core::BusData value) {
    const int page = page_of(address);
    const int primary = primary_for_page(page);

    if (address == 0xFFFF && expanded_[primary]) {
        write_ffff(value);
        return;
    }

    const int sub = sub_for_page(primary, page);
    core::MemoryDevice* const device = devices_[primary][sub][page];
    if (device == nullptr) {
        return;  // unmapped write ignored
    }
    device->mem_write(address, value);
}

}  // namespace sony_msx::devices::chipset
