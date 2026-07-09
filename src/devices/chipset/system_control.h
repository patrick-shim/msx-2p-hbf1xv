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
#include "devices/rtc/rp5c01.h"

namespace sony_msx::devices::chipset {

// System-control register on port #F5 (M15-S3).
//
// On the HB-F1XV the RTC (RP5C01) is gated by the system-control port #F5
// bit 7 — the CLOCK-IC enable (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §5). This models the
// #F5 latch and exposes the gate to the RTC via rtc::RtcClockGate.
//
// Polarity / reset value (grounded, A-M15-2 / R-4): bit 7 = 1 -> CLOCK-IC
// enabled (active-high enable). openMSX does not model an #F5 gate at all — its
// MSXRTC answers #B4/#B5 unconditionally (MSXRTC.cc:24-46). To preserve A/B
// boot parity with that always-on behaviour, the reset value enables the clock
// IC (bit 7 set); software that clears it disables the RTC data path. Sony's
// non-inverted system-flag polarity (fact-sheet §9) supports active-high here.
class SystemControlF5 final : public core::IoDevice, public rtc::RtcClockGate {
public:
    static constexpr std::uint8_t kClockEnableBit = 0x80;  // bit 7
    static constexpr std::uint8_t kResetValue = 0x80;      // CLOCK-IC enabled

    void reset();

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    [[nodiscard]] std::uint8_t value() const;

    // rtc::RtcClockGate
    [[nodiscard]] bool clock_ic_enabled() const override;

private:
    std::uint8_t value_ = kResetValue;
};

}  // namespace sony_msx::devices::chipset
