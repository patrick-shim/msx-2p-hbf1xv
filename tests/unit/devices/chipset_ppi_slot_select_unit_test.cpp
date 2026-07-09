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

#include <cstdint>
#include <iostream>

#include "devices/chipset/ppi_slot_select.h"
#include "devices/chipset/slot_bus.h"

// Suite: Devices_ChipsetPpiSlotSelect_Unit
//
// M11-S2: PPI port A (#A8) R/W drives SlotBus primary selection and reads back.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    using sony_msx::devices::chipset::PpiSlotSelect;
    using sony_msx::devices::chipset::SlotBus;

    SlotBus slot_bus;
    PpiSlotSelect ppi(slot_bus);

    // Write #A8 -> latches and drives SlotBus.
    ppi.io_write(0xA8, 0b11100100);
    if (!expect_true(slot_bus.primary_select() == 0b11100100,
                     "WriteA8_DrivesSlotBusPrimarySelect")) {
        return 1;
    }
    // Read #A8 -> returns latched byte.
    if (!expect_true(ppi.io_read(0xA8) == 0b11100100, "ReadA8_ReturnsLatchedByte")) {
        return 1;
    }

    // A second write updates both the readback and the SlotBus.
    ppi.io_write(0xA8, 0x00);
    if (!expect_true(ppi.io_read(0xA8) == 0x00 && slot_bus.primary_select() == 0x00,
                     "SecondWrite_UpdatesReadbackAndSlotBus")) {
        return 1;
    }

    return 0;
}
