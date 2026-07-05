#include "machine/ram_slot_backing.h"

namespace sony_msx::machine {

RamSlotBacking::RamSlotBacking(MemoryRegion& ram) : ram_(ram) {
}

core::BusData RamSlotBacking::mem_read(const core::BusAddress address) {
    return ram_.read(address);
}

void RamSlotBacking::mem_write(const core::BusAddress address, const core::BusData value) {
    ram_.write(address, value);
}

}  // namespace sony_msx::machine
