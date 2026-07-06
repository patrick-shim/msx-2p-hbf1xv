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
}

}  // namespace sony_msx::devices::memory
