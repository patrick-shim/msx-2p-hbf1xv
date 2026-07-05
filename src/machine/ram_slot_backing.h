#pragma once

#include "core/bus.h"
#include "core/device_contracts.h"
#include "machine/memory_region.h"

namespace sony_msx::machine {

// Machine-side adapter presenting the 64 KB main-RAM MemoryRegion as a
// core::MemoryDevice for attachment to the slot fabric at slot 3-0, pages 0-3
// (M11-S5; HB-F1XV layout, S1985 fact-sheet §9). The 64 KB device spans all
// four pages, so it indexes the full CPU address directly (see MemoryDevice
// contract). This keeps the chipset SlotBus ignorant of HB-F1XV specifics —
// composition glue lives in machine/ per src/CLAUDE.md.
class RamSlotBacking final : public core::MemoryDevice {
public:
    explicit RamSlotBacking(MemoryRegion& ram);

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

private:
    MemoryRegion& ram_;
};

}  // namespace sony_msx::machine
