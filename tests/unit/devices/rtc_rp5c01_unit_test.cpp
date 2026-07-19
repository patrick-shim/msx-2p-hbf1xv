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

#include <cstdint>
#include <iostream>

#include "devices/chipset/system_control.h"
#include "devices/rtc/rp5c01.h"

// Suite: Devices_RtcRp5c01_Unit
//
// RP5C01 RTC: address latch mask, block select, #B5 upper-nibble float, Block-2
// valid-CMOS marker, unreadable regs 14/15, deterministic epoch advance, and the
// #F5 CLOCK-IC gate. Grounding: fact-sheet §5; MSXRTC.cc / RP5C01.cc.

namespace {

using sony_msx::devices::chipset::SystemControlF5;
using sony_msx::devices::rtc::Rp5c01;
using sony_msx::devices::rtc::RtcClockSource;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

// Deterministic settable clock source (emulated cycles).
class StubClock final : public RtcClockSource {
public:
    std::uint64_t cpu_cycles() const override { return cycles; }
    std::uint64_t cycles = 0;
};

// Select RTC block `block` by writing mode register 13.
void select_block(Rp5c01& rtc, std::uint8_t block) {
    rtc.io_write(0xB4, 13);                                   // latch mode reg
    rtc.io_write(0xB5, static_cast<std::uint8_t>(0x08 | (block & 0x3)));  // timer-en + block
}

}  // namespace

int main() {
    // --- Address latch mask 0x0F; #B4 read returns 0xFF. ---
    {
        Rp5c01 rtc;
        rtc.reset();
        rtc.io_write(0xB4, 0x1D);
        if (!expect_true(rtc.address_latch() == 0x0D, "AddressLatch_MaskedTo0F")) {
            return 1;
        }
        if (!expect_true(rtc.io_read(0xB4) == 0xFF, "B4Read_ReturnsFF")) {
            return 1;
        }
    }

    // --- Block-2 valid-CMOS marker reg0 = 0x0A; #B5 upper nibble floats to 1. ---
    {
        Rp5c01 rtc;
        rtc.reset();
        if (!expect_true(rtc.peek_register(2, 0) == 0x0A, "Block2Reg0_ValidCmosMarker")) {
            return 1;
        }
        select_block(rtc, 2);
        rtc.io_write(0xB4, 0);            // latch reg 0
        if (!expect_true(rtc.io_read(0xB5) == 0xFA, "B5Read_Block2Reg0_UpperNibbleFloats")) {
            return 1;
        }
    }

    // --- Registers 14 and 15 are unreadable (peek 0x0F -> read 0xFF). ---
    {
        Rp5c01 rtc;
        rtc.reset();
        rtc.io_write(0xB4, 14);
        if (!expect_true(rtc.io_read(0xB5) == 0xFF, "Reg14_Unreadable")) {
            return 1;
        }
        rtc.io_write(0xB4, 15);
        if (!expect_true(rtc.io_read(0xB5) == 0xFF, "Reg15_Unreadable")) {
            return 1;
        }
        // Mode register 13 is readable.
        rtc.io_write(0xB4, 13);
        if (!expect_true(rtc.io_read(0xB5) == 0xF8, "Reg13_Mode_ReadableTimerEnable")) {
            return 1;
        }
    }

    // --- Deterministic epoch advance off the emulated clock. ---
    {
        Rp5c01 rtc;
        StubClock clock;
        rtc.attach_clock_source(&clock);
        rtc.reset();
        // At epoch (0 cycles) block-0 seconds-units = 0.
        rtc.io_write(0xB4, 0);
        if (!expect_true(rtc.io_read(0xB5) == 0xF0, "Epoch_Seconds_Zero")) {
            return 1;
        }
        // Advance one emulated second (system clock cycles) -> seconds = 1.
        clock.cycles = Rp5c01::kSystemClockHz;
        rtc.io_write(0xB4, 0);
        if (!expect_true(rtc.io_read(0xB5) == 0xF1, "OneSecond_SecondsUnits_Is1")) {
            return 1;
        }
        // Advance to 75 seconds total -> 1 min 15 sec: sec-units=5, min-units=1.
        clock.cycles = 75ull * Rp5c01::kSystemClockHz;
        rtc.io_write(0xB4, 0);
        const bool sec5 = rtc.io_read(0xB5) == 0xF5;
        rtc.io_write(0xB4, 2);  // minutes units
        const bool min1 = rtc.io_read(0xB5) == 0xF1;
        if (!expect_true(sec5 && min1, "SeventyFiveSeconds_BcdRollover")) {
            return 1;
        }
    }

    // --- Two identical runs produce byte-identical RTC reads (determinism). ---
    {
        auto run = [] {
            Rp5c01 rtc;
            StubClock clock;
            rtc.attach_clock_source(&clock);
            rtc.reset();
            clock.cycles = 12345ull * Rp5c01::kSystemClockHz + 678;
            rtc.io_write(0xB4, 0);
            std::uint8_t a = rtc.io_read(0xB5);
            rtc.io_write(0xB4, 4);  // hours units
            std::uint8_t b = rtc.io_read(0xB5);
            return static_cast<int>(a) * 256 + b;
        };
        if (!expect_true(run() == run(), "Determinism_TwoRunsIdentical")) {
            return 1;
        }
    }

    // --- #F5 CLOCK-IC gate: disabling bit7 makes the RTC data path inert. ---
    {
        Rp5c01 rtc;
        SystemControlF5 f5;
        f5.reset();  // default 0x80 -> enabled
        rtc.attach_clock_gate(&f5);
        rtc.reset();
        select_block(rtc, 2);
        rtc.io_write(0xB4, 0);
        if (!expect_true(rtc.io_read(0xB5) == 0xFA, "F5Enabled_RtcReadsData")) {
            return 1;
        }
        f5.io_write(0xF5, 0x00);  // clear bit7 -> CLOCK-IC disabled
        if (!expect_true(!f5.clock_ic_enabled() && rtc.io_read(0xB5) == 0xFF,
                         "F5Disabled_RtcOpenBus")) {
            return 1;
        }
        // Writes are also gated (no state change while disabled).
        rtc.io_write(0xB4, 5);  // ignored
        f5.io_write(0xF5, 0x80);  // re-enable
        if (!expect_true(rtc.address_latch() == 0, "F5Disabled_WriteIgnored")) {
            return 1;
        }
    }

    return 0;
}
