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

#include "devices/chipset/ppi_slot_select.h"

namespace sony_msx::devices::chipset {

PpiSlotSelect::PpiSlotSelect(SlotBus& slot_bus) : slot_bus_(slot_bus) {
}

core::BusData PpiSlotSelect::io_read(core::BusAddress /*port*/) {
    // #A8 is readable and returns the latched primary-select byte (fact-sheet §3).
    return port_a_;
}

void PpiSlotSelect::io_write(core::BusAddress /*port*/, const core::BusData value) {
    port_a_ = value;
    slot_bus_.set_primary_select(value);
}

}  // namespace sony_msx::devices::chipset
