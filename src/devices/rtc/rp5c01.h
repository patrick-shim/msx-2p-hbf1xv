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

namespace sony_msx::devices::rtc {

// Deterministic emulated-cycle clock source for the RTC (the RTC advances
// its time READ-ONLY off the machine clock, never the host wall clock). The
// machine supplies an adapter returning scheduler total cycles.
class RtcClockSource {
public:
    virtual ~RtcClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

// Gate for the RTC CLOCK-IC enable (system-control port #F5 bit 7, fact-sheet
// §5). When disabled the RTC data path is inert (open-bus). The machine backs
// this with the #F5 system-control register.
class RtcClockGate {
public:
    virtual ~RtcClockGate() = default;
    [[nodiscard]] virtual bool clock_ic_enabled() const = 0;
};

// Ricoh RP5C01(A)-compatible RTC as an IoDevice on #B4/#B5.
//
// Ports (fact-sheet §5; openMSX 21.0: src/MSXRTC.cc:24-46 +
// RP5C01.cc — behaviour reference, never copied, GPL isolation):
//   #B4 (port & 1 == 0) : register/address latch (value & 0x0F); read -> 0xFF.
//   #B5 (port & 1 == 1) : 4-bit data; read -> data | 0xF0 (upper nibble floats 1).
//
// Structure: 4 blocks x 13 four-bit registers, plus shared mode(13)/test(14)/
// reset(15). Mode register 13 selects the active block (bits 0-1). Registers 14
// and 15 are write-only (peek returns 0x0F). Block 0 = BCD time/date, Block 1 =
// alarm/12-24h/leap, Block 2 = system-init CMOS (reg 0 reads 0x0A = valid marker),
// Block 3 = title/prompt CMOS.
//
// Determinism (DEC-0009): seeded from a FIXED emulated epoch and
// advanced only from the RtcClockSource; NO host clock, NO file persistence.
class Rp5c01 final : public core::IoDevice {
public:
    static constexpr std::uint8_t kBlocks = 4;
    static constexpr std::uint8_t kRegsPerBlock = 13;
    static constexpr std::uint8_t kModeReg = 13;
    static constexpr std::uint8_t kTestReg = 14;
    static constexpr std::uint8_t kResetReg = 15;
    static constexpr std::uint8_t kValidCmosMarker = 0x0A;  // Block 2 reg 0

    // RTC fraction counter frequency (RP5C01.hh:46) and the HB-F1XV system clock.
    static constexpr std::uint32_t kRtcFreq = 16384;
    static constexpr std::uint32_t kSystemClockHz = 3579545;

    void reset();
    void attach_clock_source(RtcClockSource* source);
    void attach_clock_gate(RtcClockGate* gate);

    // core::IoDevice on #B4/#B5.
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // Introspection for deterministic tests (does not advance time).
    [[nodiscard]] std::uint8_t address_latch() const;
    [[nodiscard]] std::uint8_t peek_register(std::uint8_t block, std::uint8_t reg) const;
    [[nodiscard]] std::uint8_t mode_register() const;

    // --- Debug-snapshot seams: additive read-only introspection of the
    //     write-only test/reset registers + the decoded internal time counters
    //     + last-tick anchor, for a restore-ready snapshot. const returns of
    //     existing members, ZERO behavior change. These
    //     do NOT advance time (unlike sync_time()); they read the last-synced
    //     stored state, so the snapshot stays non-perturbing. ---
    [[nodiscard]] std::uint8_t test_register() const { return test_reg_; }
    [[nodiscard]] std::uint8_t reset_register() const { return reset_reg_; }
    [[nodiscard]] unsigned fraction() const { return fraction_; }
    [[nodiscard]] unsigned seconds() const { return seconds_; }
    [[nodiscard]] unsigned minutes() const { return minutes_; }
    [[nodiscard]] unsigned hours() const { return hours_; }
    [[nodiscard]] unsigned day_week() const { return day_week_; }
    [[nodiscard]] unsigned days() const { return days_; }
    [[nodiscard]] unsigned months() const { return months_; }
    [[nodiscard]] unsigned years() const { return years_; }
    [[nodiscard]] unsigned leap_year() const { return leap_year_; }
    [[nodiscard]] std::uint64_t last_rtc_ticks() const { return last_rtc_ticks_; }

private:
    void sync_time();  // advance decoded time from the clock source (read-only)
    [[nodiscard]] std::uint8_t peek_port(std::uint8_t port) const;
    void write_port(std::uint8_t port, std::uint8_t value);
    void regs_to_time();
    void time_to_regs();
    void seed_epoch();

    std::array<std::uint8_t, kBlocks * kRegsPerBlock> regs_{};
    std::uint8_t latch_ = 0;
    std::uint8_t mode_reg_ = 0;
    std::uint8_t test_reg_ = 0;
    std::uint8_t reset_reg_ = 0;

    // Decoded time (mirrors openMSX RP5C01 fields).
    unsigned fraction_ = 0;
    unsigned seconds_ = 0;
    unsigned minutes_ = 0;
    unsigned hours_ = 0;
    unsigned day_week_ = 0;
    unsigned days_ = 0;
    unsigned months_ = 0;
    unsigned years_ = 0;
    unsigned leap_year_ = 0;

    std::uint64_t last_rtc_ticks_ = 0;
    RtcClockSource* clock_source_ = nullptr;
    RtcClockGate* clock_gate_ = nullptr;
};

}  // namespace sony_msx::devices::rtc
