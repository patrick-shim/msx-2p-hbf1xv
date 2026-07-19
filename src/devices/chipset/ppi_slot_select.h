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

#pragma once

#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/slot_bus.h"

namespace sony_msx::devices::chipset {

// Minimal i8255 PPI port-A slot-select as an IoDevice on port #A8.
//
// Port A of the MSX PPI is the primary-slot select register (S1985 fact-sheet
// §3; openMSX 21.0: src/MSXPPI.cc). Writing #A8 latches the
// 2-bits-per-page primary selection and drives SlotBus; reading #A8 returns the
// latched byte. Only port A is modelled here. Ports #A9/#AA/#AB live in the
// full Ppi8255, which superseded this class in the machine wiring; this
// minimal implementation survives as a reference with its own unit test
// (and as Ppi8255's port-A sub-object).
class PpiSlotSelect final : public core::IoDevice {
public:
    explicit PpiSlotSelect(SlotBus& slot_bus);

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

private:
    SlotBus& slot_bus_;
    std::uint8_t port_a_ = 0;
};

}  // namespace sony_msx::devices::chipset
