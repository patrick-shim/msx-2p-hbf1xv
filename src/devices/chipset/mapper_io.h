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

#pragma once

#include <array>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::chipset {

// Memory-mapper I/O registers on ports #FC-#FF (M11-S4).
//
// One write-only 8-bit segment register per 16 KB page (#FC page0 ... #FF page3;
// S1985 fact-sheet §4). Reading a mapper port is discouraged by the standard;
// on the S1985 it is not a true register — it returns base | (segment & mask)
// with base 0x80 and mask 0x1F, i.e. the measured `100xxxxx` pattern
// (fact-sheet §4; openMSX MSXMapperIO / MSXS1985.cc:31-34
// MapperReadBackBaseValue = 0x80, mask 0b0001'1111).
//
// M11 models storage + the readback pattern only; the segment does not yet
// select a physical RAM bank (that is M12).
class MapperIo final : public core::IoDevice {
public:
    static constexpr std::uint8_t kBasePort = 0xFC;
    static constexpr std::uint8_t kReadBackBase = 0x80;
    static constexpr std::uint8_t kReadBackMask = 0x1F;

    void set_readback(std::uint8_t base, std::uint8_t mask);

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    [[nodiscard]] std::uint8_t segment(int page) const;

private:
    std::array<std::uint8_t, 4> segments_{};
    std::uint8_t base_ = kReadBackBase;
    std::uint8_t mask_ = kReadBackMask;
};

}  // namespace sony_msx::devices::chipset
