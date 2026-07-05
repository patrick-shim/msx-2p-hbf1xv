#pragma once

#include <array>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::chipset {

// Expanded / switched-I/O controller on ports #40-#4F (M11-S4).
//
// Models the MSX device-switch mechanism (openMSX
// references/openmsx-21.0/src/MSXDeviceSwitch.cc — behaviour reference, not
// copied): writing #40 (port & 0x0F == 0) selects an 8-bit device ID; the
// selected SwitchedDevice then answers #40-#4F, dispatched on port & 0x0F.
//
//   - write, (port & 0x0F) == 0 -> selected = value.
//   - write, otherwise          -> selected device's switched_write, or ignore.
//   - read                      -> selected device's switched_read, or 0xFF.
//
// The S1985 backup RAM registers here under ID 0xFE (fact-sheet §6, §10). This
// is the seam through which the MSX-ENGINE detection routine distinguishes an
// S1985 from an S3527.
class SwitchedIoController final : public core::IoDevice {
public:
    // Register a switched device under its id(). Nullptr id slots are open-bus.
    void attach(core::SwitchedDevice* device);

    void reset();

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    [[nodiscard]] std::uint8_t selected() const;

private:
    std::array<core::SwitchedDevice*, 256> devices_{};
    std::uint8_t selected_ = 0;
};

}  // namespace sony_msx::devices::chipset
