#include <cstdint>
#include <iostream>

#include "core/device_contracts.h"
#include "devices/chipset/io_bus.h"

// Suite: Devices_ChipsetIoBus_Unit
//
// M11-S3: port dispatch (port & 0xFF), straight-alias mirrors #A8->#AC and
// #98->#9C, and open-bus for unmapped ports.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// A single-register I/O device: read returns the stored byte; write stores it.
class RegDevice final : public sony_msx::core::IoDevice {
public:
    sony_msx::core::BusData io_read(const sony_msx::core::BusAddress port) override {
        ++reads;
        last_read_port = port;
        return value;
    }
    void io_write(const sony_msx::core::BusAddress port, const sony_msx::core::BusData v) override {
        ++writes;
        last_write_port = port;
        value = v;
    }
    std::uint8_t value = 0x00;
    int reads = 0;
    int writes = 0;
    std::uint16_t last_read_port = 0;
    std::uint16_t last_write_port = 0;
};

}  // namespace

int main() {
    using sony_msx::devices::chipset::IoBus;

    // --- Basic dispatch on port & 0xFF. ---
    {
        IoBus bus;
        RegDevice dev;
        bus.attach(0xA8, &dev);
        bus.io_write(0xA8, 0x5A);
        if (!expect_true(dev.value == 0x5A && dev.writes == 1, "Attach_Write_ReachesDevice")) {
            return 1;
        }
        // High byte ignored for dispatch: port 0x5AA8 still maps to 0xA8.
        if (!expect_true(bus.io_read(0x5AA8) == 0x5A && dev.reads == 1,
                         "Dispatch_UsesLowByteOfPort")) {
            return 1;
        }
    }

    // --- PPI mirror: #AC aliases #A8 (engine-detection oracle read(#AC)==read(#A8)). ---
    {
        IoBus bus;
        RegDevice ppi;
        bus.attach(0xA8, &ppi);
        bus.register_mirror(0xA8, 0xAC);  // #AC -> #A8
        bus.io_write(0xA8, 0x33);
        if (!expect_true(bus.io_read(0xAC) == bus.io_read(0xA8),
                         "EngineDetection_MirrorAc_EqualsA8")) {
            return 1;
        }
        // A write to the mirror also lands on the base device.
        bus.io_write(0xAC, 0x77);
        if (!expect_true(ppi.value == 0x77, "MirrorWrite_LandsOnBaseDevice")) {
            return 1;
        }
    }

    // --- VDP mirror seam: #9C aliases #98 (proves the M13 VDP alias will work). ---
    {
        IoBus bus;
        RegDevice vdp_stub;
        bus.attach(0x98, &vdp_stub);
        bus.register_mirror(0x98, 0x9C);
        bus.io_write(0x9C, 0xC4);  // write via the mirror
        if (!expect_true(vdp_stub.value == 0xC4 && vdp_stub.last_write_port == 0x9C,
                         "VdpMirror_9CWrite_ReachesBase98Device")) {
            return 1;
        }
        if (!expect_true(bus.io_read(0x98) == 0xC4, "VdpMirror_ReadBase_SeesMirrorWrite")) {
            return 1;
        }
    }

    // --- Unmapped port: read 0xFF, write ignored (no crash). ---
    {
        IoBus bus;
        if (!expect_true(bus.io_read(0x40) == 0xFF, "UnmappedPort_Read_ReturnsOpenBus")) {
            return 1;
        }
        bus.io_write(0x40, 0x12);  // ignored
        if (!expect_true(bus.io_read(0x40) == 0xFF, "UnmappedPort_Write_Ignored")) {
            return 1;
        }
    }

    return 0;
}
