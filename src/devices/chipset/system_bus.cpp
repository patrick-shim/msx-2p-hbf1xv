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
