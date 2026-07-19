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

#include <cstdint>

namespace sony_msx::core {

using BusAddress = std::uint16_t;
using BusData = std::uint8_t;

// The MSX base bus is 16-bit addressable; all accesses are normalized to 0x0000-0xFFFF.
constexpr BusAddress normalize_bus_address(const std::uint32_t address) {
    return static_cast<BusAddress>(address & 0xFFFFu);
}

class Bus {
public:
    virtual ~Bus() = default;

    // Memory path (0x0000-0xFFFF). Pure-virtual: every bus must model memory.
    virtual BusData read(BusAddress address) = 0;
    virtual void write(BusAddress address, BusData value) = 0;

    // I/O path (256-port MSX I/O space, keyed on port & 0xFF). Non-pure with
    // open-bus defaults so existing memory-only fake buses in the test
    // suites keep compiling unchanged. The default read returns 0xFF, the
    // documented floating/open-bus value for unmapped MSX I/O reads (S1985
    // fact-sheet §10: openMSX concluded true last-byte-on-bus behaviour is not
    // worth reproducing); the default write is ignored.
    virtual BusData io_read(BusAddress /*port*/) {
        return 0xFF;
    }
    virtual void io_write(BusAddress /*port*/, BusData /*value*/) {
    }
};

}  // namespace sony_msx::core
