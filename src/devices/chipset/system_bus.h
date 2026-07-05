#pragma once

#include "core/bus.h"
#include "devices/chipset/io_bus.h"
#include "devices/chipset/slot_bus.h"

namespace sony_msx::devices::chipset {

// The composed CPU-facing bus (M11-S5): memory accesses route through the
// slot-decode fabric (SlotBus), I/O accesses through the port-dispatch fabric
// (IoBus). This is the concrete core::Bus the CPU talks to, replacing the flat
// DRAM Hbf1xvMachine::MachineBus.
//
// SystemBus does not own the fabrics — the machine owns SlotBus/IoBus and their
// devices and injects them here — so composition stays in machine/ and decode
// logic stays in the fabrics.
class SystemBus final : public core::Bus {
public:
    SystemBus(SlotBus& slot_bus, IoBus& io_bus);

    core::BusData read(core::BusAddress address) override;
    void write(core::BusAddress address, core::BusData value) override;
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

private:
    SlotBus& slot_bus_;
    IoBus& io_bus_;
};

}  // namespace sony_msx::devices::chipset
