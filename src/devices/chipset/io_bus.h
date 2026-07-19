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

// 256-port MSX I/O dispatch fabric with S1985 straight-alias mirrors.
//
// A device registers on one or more of the 256 ports (keyed on port & 0xFF).
// io_read/io_write dispatch to the registered device, or return open-bus 0xFF /
// ignore when unmapped (S1985 fact-sheet §10).
//
// register_mirror models the S1985's incomplete address decoding, which mirrors
// VDP ports #98-#9B onto #9C-#9F (fact-sheet §7) and PPI ports #A8-#AB onto
// #AC-#AF (fact-sheet §3, §10): a mirror port routes to whatever device is
// registered on its base port, so the MSX-ENGINE detection routine's
// read(#AC)==read(#A8) holds.
class IoBus final : public core::Bus {
public:
    // Register a device on a base port (port & 0xFF). Nullptr detaches.
    void attach(std::uint8_t port, core::IoDevice* device);

    // Make `mirror` alias `base`: accesses to `mirror` route to base's device.
    void register_mirror(std::uint8_t base, std::uint8_t mirror);

    // core::Bus I/O path. The memory path (read/write) is unused here and left
    // as open-bus defaults; SystemBus owns the composed memory+I/O bus.
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // core::Bus requires the memory path; IoBus is I/O-only, so these are inert.
    core::BusData read(core::BusAddress address) override;
    void write(core::BusAddress address, core::BusData value) override;

private:
    [[nodiscard]] core::IoDevice* resolve(std::uint8_t port) const;

    // Base device table plus a mirror map: mirror_target_[p] == p means "no
    // mirror" (dispatch on p itself); otherwise it is the base port to route to.
    std::array<core::IoDevice*, 256> devices_{};
    std::array<std::uint8_t, 256> mirror_target_{};
    bool mirror_initialized_ = false;

    void ensure_mirror_init();
};

}  // namespace sony_msx::devices::chipset
