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

namespace sony_msx::devices::memory {

namespace {
constexpr int page_of(const core::BusAddress address) {
    return (address >> 14) & 0x03;
}
}  // namespace

MemoryMapperRam::MemoryMapperRam(machine::MemoryRegion& ram, const chipset::MapperIo& mapper_io)
    : ram_(ram), mapper_io_(mapper_io) {
}

std::size_t MemoryMapperRam::physical_address(const std::uint8_t segment, const core::BusAddress address) {
    // numSegments == 4 (power of two): openMSX's `segment & (numSegments - 1)`
    // wrap reduces to `segment & 3` for every segment value
    // (MSXMemoryMapperBase.cc:72-83).
    const std::size_t folded = static_cast<std::size_t>(segment & (kSegments - 1));
    return folded * kSegmentBytes + (static_cast<std::size_t>(address) & 0x3FFF);
}

core::BusData MemoryMapperRam::mem_read(const core::BusAddress address) {
    const std::uint8_t segment = mapper_io_.segment(page_of(address));
    return ram_.read(physical_address(segment, address));
}

void MemoryMapperRam::mem_write(const core::BusAddress address, const core::BusData value) {
    const std::uint8_t segment = mapper_io_.segment(page_of(address));
    ram_.write(physical_address(segment, address), value);
    // DEC-0052 stream-light watchlog hook (default-off): notify with the
    // CPU-VISIBLE address, never the folded physical offset. No-op unless an
    // observer is installed (only while a stream capture is armed).
    if (write_observer_ != nullptr) {
        write_observer_->on_mem_write(address, value);
    }
}

}  // namespace sony_msx::devices::memory
