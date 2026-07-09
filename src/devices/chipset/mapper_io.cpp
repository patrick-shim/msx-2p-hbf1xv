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

#include "devices/chipset/mapper_io.h"

namespace sony_msx::devices::chipset {

void MapperIo::set_readback(const std::uint8_t base, const std::uint8_t mask) {
    base_ = base;
    mask_ = mask;
}

core::BusData MapperIo::io_read(const core::BusAddress port) {
    const int page = static_cast<int>((port & 0xFF) - kBasePort) & 0x03;
    return static_cast<core::BusData>(base_ | (segments_[static_cast<std::size_t>(page)] & mask_));
}

void MapperIo::io_write(const core::BusAddress port, const core::BusData value) {
    const int page = static_cast<int>((port & 0xFF) - kBasePort) & 0x03;
    segments_[static_cast<std::size_t>(page)] = value;
}

std::uint8_t MapperIo::segment(const int page) const {
    return segments_[static_cast<std::size_t>(page & 0x03)];
}

}  // namespace sony_msx::devices::chipset
