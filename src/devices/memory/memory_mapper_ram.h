#pragma once

#include <cstddef>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/mapper_io.h"
#include "machine/memory_region.h"

namespace sony_msx::devices::memory {

// 64 KB memory-mapper RAM device occupying slot 3-0, pages 0-3 (M13-S1).
//
// This is the CPU RAM backing for the HB-F1XV (Sony_HB-F1XV.xml:125-130,
// MemoryMapper size 64). It replaces the M11 inert flat `RamSlotBacking`.
//
// Segment ownership is SPLIT (single source of truth): the M11
// chipset::MapperIo remains the SOLE owner of the four #FC-#FF segment registers
// and of the S1985 `100xxxxx` (0x80 | seg & 0x1F) 5-bit readback. This device is
// a pure CONSUMER: for each CPU access it reads the live segment for that page
// from MapperIo and folds it onto the physical 64 KB store. No segment state is
// duplicated here, so a mapper write is observed on the very next CPU access.
//
// Physical address (behaviour reference — read only, never copied; GPL):
// references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:72-83
//   segmentOffset(page): segment = (segment < numSegments)
//                                    ? segment : segment & (numSegments - 1);
//                        return segment * 0x4000;
//   calcAddress(addr):   segmentOffset(addr / 0x4000) | (addr & 0x3FFF)
// For a 64 KB mapper numSegments == 4 (a power of two), so the unpopulated-
// segment wrap `segment & (numSegments - 1)` == `segment & 3` for every segment
// value (when segment < 4 the mask is a no-op). The readback (5-bit, chipset)
// and the physical fold (2-bit, here) authentically use different masks
// (Sony_HB-F1XV.xml:25 "includes 5 bits mapper-read-back").
class MemoryMapperRam final : public core::MemoryDevice {
public:
    // 64 KB / 16 KB = 4 populated segments.
    static constexpr int kSegments = 4;
    static constexpr std::size_t kSegmentBytes = 0x4000;

    MemoryMapperRam(machine::MemoryRegion& ram, const chipset::MapperIo& mapper_io);

    // Fold a (segment, address) pair onto the physical 64 KB store, matching
    // openMSX calcAddress for numSegments == 4.
    [[nodiscard]] static std::size_t physical_address(std::uint8_t segment, core::BusAddress address);

    // MemoryDevice contract.
    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

private:
    machine::MemoryRegion& ram_;
    const chipset::MapperIo& mapper_io_;
};

}  // namespace sony_msx::devices::memory
