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

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::chipset {

// Reset-status register on I/O port #F4 (boot-logo investigation fix,
// DEC-0026-style targeted defect cycle).
//
// The HB-F1XV carries the MSX2+ #F4 "reset status" latch: the machine XML
// (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml,
// `<ResetStatusRegister id="Reset Status register"> <inverted>false</inverted>
// <io base="0xF4" num="1"/>`) declares the NON-inverted variant. The MSX2+
// BIOS reads #F4 at boot to distinguish cold power-up from a warm restart:
// bit 7 clear -> cold boot -> BIOS runs the animated MSX logo, then writes
// bit 7 set so the logo is skipped on subsequent warm restarts. (Empirically
// confirmed against openMSX Sony_HB-F1XV: #F4 reads 0x00 at power-on, the
// logo runs t=1.25s..4.0s, and #F4 reads 0x80 afterwards. bios/f1xvbios.rom
// has the IN A,(0F4h) / OUT (0F4h),A primitives at offsets 0x146A/0x146F.)
//
// Behaviour model (non-inverted variant), grounded at
// references/openmsx-21.0/src/MSXResetStatusRegister.cc:13-35 (read for
// understanding, never copied — GPL isolation): power-up sets status = 0x00;
// read returns the stored byte; write stores only bits 7 and 5 — bit 5 is
// preserved from the previous value while bit 7 (and bit 5 via the value
// mask) update, i.e. status = (status & 0x20) | (value & 0xA0).
//
// Real hardware keeps this latch across a RESET-button warm reset (its entire
// purpose); this machine model only exposes cold power-up
// (Hbf1xvMachine::cold_boot), which maps to power_on_reset() here.
class ResetStatusRegister final : public core::IoDevice {
public:
    static constexpr std::uint8_t kPowerOnStatus = 0x00;  // non-inverted variant

    // Cold power-up only. Deliberately NOT named reset(): a warm reset must
    // preserve the latch on real hardware.
    void power_on_reset();

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    [[nodiscard]] std::uint8_t status() const;

private:
    std::uint8_t status_ = kPowerOnStatus;
};

}  // namespace sony_msx::devices::chipset
