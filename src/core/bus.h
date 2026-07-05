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
    // open-bus defaults so existing memory-only fake buses in the M0-M10 test
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
