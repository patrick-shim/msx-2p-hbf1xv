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

#include "devices/memory/memory_mapper_ram.h"

#include <cassert>

namespace sony_msx::devices::memory {

namespace {
constexpr int page_of(const core::BusAddress address) {
    return (address >> 14) & 0x03;
}
}  // namespace

MemoryMapperRam::MemoryMapperRam(machine::MemoryRegion& ram, const chipset::MapperIo& mapper_io)
    : ram_(ram),
      mapper_io_(mapper_io),
      num_segments_(static_cast<int>(ram.size() / kSegmentBytes)) {
    // Defensive invariant (DEC-0061): the fold `segment & (num_segments - 1)`
    // is a correct power-of-two modulo ONLY when num_segments is a power of two.
    // All four offered sizes satisfy this (64/128/256/512 KB -> 4/8/16/32); the
    // CLI rejects any other value, so this asserts the contract the caller must
    // uphold.
    assert(num_segments_ > 0 && (num_segments_ & (num_segments_ - 1)) == 0 &&
           "MemoryMapperRam requires a power-of-two segment count");
}

std::size_t MemoryMapperRam::physical_address(const std::uint8_t segment, const core::BusAddress address,
                                              const int num_segments) {
    // openMSX MSXMemoryMapperBase.cc:72-83 wraps the segment with
    // `segment & (numSegments - 1)` (a power-of-two modulo), then adds the 14-bit
    // in-segment offset. `& 3` at 64 KB (4 seg); the opt-in non-stock sizes use
    // `& 7/15/31` (128/256/512 KB). At 512 KB the fold mask (0x1F) coincides with
    // the S1985 5-bit read-back mask -- the exact internal ceiling.
    const std::size_t folded = static_cast<std::size_t>(segment & (num_segments - 1));
    return folded * kSegmentBytes + (static_cast<std::size_t>(address) & 0x3FFF);
}

core::BusData MemoryMapperRam::mem_read(const core::BusAddress address) {
    const std::uint8_t segment = mapper_io_.segment(page_of(address));
    return ram_.read(physical_address(segment, address, num_segments_));
}

void MemoryMapperRam::mem_write(const core::BusAddress address, const core::BusData value) {
    const std::uint8_t segment = mapper_io_.segment(page_of(address));
    ram_.write(physical_address(segment, address, num_segments_), value);
    // Stream-light watchlog hook (DEC-0052; default-off): notify with the
    // CPU-VISIBLE address, never the folded physical offset. No-op unless an
    // observer is installed (only while a stream capture is armed).
    if (write_observer_ != nullptr) {
        write_observer_->on_mem_write(address, value);
    }
}

}  // namespace sony_msx::devices::memory
