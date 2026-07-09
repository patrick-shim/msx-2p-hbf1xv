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

#include "machine/memory_region.h"

#include <algorithm>

namespace sony_msx::machine {

MemoryRegion::MemoryRegion(const std::size_t size) : bytes_(size, 0) {
}

std::size_t MemoryRegion::size() const {
    return bytes_.size();
}

void MemoryRegion::clear() {
    std::fill(bytes_.begin(), bytes_.end(), static_cast<std::uint8_t>(0));
}

std::uint8_t MemoryRegion::read(const std::size_t offset) const {
    if (offset >= bytes_.size()) {
        return 0;
    }
    return bytes_[offset];
}

void MemoryRegion::write(const std::size_t offset, const std::uint8_t value) {
    if (offset >= bytes_.size()) {
        return;
    }
    bytes_[offset] = value;
}

void MemoryRegion::load(const std::size_t offset, const std::uint8_t* bytes, const std::size_t count) {
    if (bytes == nullptr || count == 0 || offset >= bytes_.size()) {
        return;
    }

    const std::size_t available = bytes_.size() - offset;
    const std::size_t writable = std::min(count, available);
    for (std::size_t index = 0; index < writable; ++index) {
        bytes_[offset + index] = bytes[index];
    }
}

std::vector<std::uint8_t> MemoryRegion::dump() const {
    return bytes_;
}

const std::uint8_t* MemoryRegion::data() const {
    return bytes_.data();
}

}  // namespace sony_msx::machine
