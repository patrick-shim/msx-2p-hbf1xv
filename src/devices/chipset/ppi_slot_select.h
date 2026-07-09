#pragma once

#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/slot_bus.h"

namespace sony_msx::devices::chipset {

// Minimal i8255 PPI port-A slot-select as an IoDevice on port #A8 (M11-S2).
//
// Port A of the MSX PPI is the primary-slot select register (S1985 fact-sheet
// §3; openMSX references/openmsx-21.0/src/MSXPPI.cc). Writing #A8 latches the
// 2-bits-per-page primary selection and drives SlotBus; reading #A8 returns the
// latched byte. Only port A is modelled here. Ports #A9/#AA/#AB live in the
// full Ppi8255 (M15), which superseded this class in the machine wiring; this
// minimal implementation survives as the M11 reference with its own unit test.
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
