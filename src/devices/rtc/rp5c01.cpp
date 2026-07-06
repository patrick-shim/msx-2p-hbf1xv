#include "devices/rtc/rp5c01.h"

#include <array>

namespace sony_msx::devices::rtc {

namespace {

constexpr std::uint8_t kTimeBlock = 0;
constexpr std::uint8_t kAlarmBlock = 1;
constexpr std::uint8_t kModeBlockSelect = 0x3;
constexpr std::uint8_t kModeTimerEnable = 0x8;

constexpr std::uint8_t kTestSeconds = 0x1;
constexpr std::uint8_t kTestMinutes = 0x2;
constexpr std::uint8_t kTestDays = 0x4;
constexpr std::uint8_t kTestYears = 0x8;

constexpr std::uint8_t kResetAlarm = 0x1;
constexpr std::uint8_t kResetFraction = 0x2;

// 0-bits are ignored on write and read back 0 (RP5C01.cc:36-41).
constexpr std::array<std::array<std::uint8_t, 13>, 4> kMask = {{
    {0xf, 0x7, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0xf, 0x1, 0xf, 0xf},
    {0x0, 0x0, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0x0, 0x1, 0x3, 0x0},
    {0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
    {0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
}};

constexpr int days_in_month(int month, unsigned leap_year) {
    constexpr std::array<std::uint8_t, 12> table = {31, 28, 31, 30, 31, 30,
                                                    31, 31, 30, 31, 30, 31};
    month %= 12;
    return ((month == 1) && (leap_year == 0)) ? 29
                                              : table[static_cast<std::size_t>(month)];
}

}  // namespace

void Rp5c01::reset() {
    // Deterministic in-memory CMOS (no battery file): zero the register bank,
    // seed a fixed epoch, and stamp the Block-2 valid-CMOS marker so the BIOS
    // takes the configured (not defaults+clear) path (fact-sheet §5).
    regs_.fill(0);
    latch_ = 0;
    mode_reg_ = kModeTimerEnable;  // RP5C01.cc:62
    test_reg_ = 0;
    reset_reg_ = 0;
    last_rtc_ticks_ = 0;
    seed_epoch();
    // Block 2, register 0 must read 0x0A to signal valid CMOS.
    regs_[static_cast<std::size_t>(2 * kRegsPerBlock + 0)] = kValidCmosMarker;
    time_to_regs();
}

void Rp5c01::seed_epoch() {
    // Fixed deterministic epoch: 1988-01-01 00:00:00, Friday. 1988 is a leap
    // year (88 % 4 == 0). Chosen once; identical every run (A-M15-1).
    fraction_ = 0;
    seconds_ = 0;
    minutes_ = 0;
    hours_ = 0;
    day_week_ = 5;   // 0=Sunday .. 5=Friday
    days_ = 0;       // 0-based day-of-month (regs store +1)
    months_ = 0;     // 0-based month (regs store +1)
    years_ = 8;      // years since 1980
    leap_year_ = 0;  // 0 = leap year
}

void Rp5c01::attach_clock_source(RtcClockSource* source) {
    clock_source_ = source;
}

void Rp5c01::attach_clock_gate(RtcClockGate* gate) {
    clock_gate_ = gate;
}

std::uint8_t Rp5c01::address_latch() const {
    return latch_;
}

std::uint8_t Rp5c01::mode_register() const {
    return mode_reg_;
}

std::uint8_t Rp5c01::peek_register(const std::uint8_t block, const std::uint8_t reg) const {
    const std::size_t index = static_cast<std::size_t>(block % kBlocks) * kRegsPerBlock + (reg % kRegsPerBlock);
    return static_cast<std::uint8_t>(regs_[index] & kMask[block % kBlocks][reg % kRegsPerBlock]);
}

void Rp5c01::sync_time() {
    if (clock_source_ == nullptr) {
        return;
    }
    // RTC ticks elapsed since epoch = cpu_cycles * FREQ / systemClock. Computing
    // the absolute tick count and differencing keeps it integer-exact and
    // deterministic (no accumulated rounding drift).
    const std::uint64_t cpu_cycles = clock_source_->cpu_cycles();
    const std::uint64_t ticks = (cpu_cycles * kRtcFreq) / kSystemClockHz;
    if (ticks <= last_rtc_ticks_) {
        return;
    }
    unsigned elapsed = static_cast<unsigned>(ticks - last_rtc_ticks_);
    last_rtc_ticks_ = ticks;

    // openMSX RP5C01::updateTimeRegs (RP5C01.cc:202-255), EMUTIME path.
    fraction_ += (mode_reg_ & kModeTimerEnable) ? elapsed : 0;
    const unsigned carry_seconds = (test_reg_ & kTestSeconds) ? elapsed : fraction_ / kRtcFreq;
    seconds_ += carry_seconds;
    const unsigned carry_minutes = (test_reg_ & kTestMinutes) ? elapsed : seconds_ / 60;
    minutes_ += carry_minutes;
    hours_ += minutes_ / 60;
    const unsigned carry_days = (test_reg_ & kTestDays) ? elapsed : hours_ / 24;
    if (carry_days) {
        days_ += carry_days;
        day_week_ += carry_days;
        while (days_ >= static_cast<unsigned>(days_in_month(static_cast<int>(months_), leap_year_))) {
            days_ -= static_cast<unsigned>(days_in_month(static_cast<int>(months_), leap_year_));
            months_++;
        }
    }
    const unsigned carry_years = (test_reg_ & kTestYears) ? elapsed : months_ / 12;
    years_ += carry_years;
    leap_year_ += carry_years;

    fraction_ %= kRtcFreq;
    seconds_ %= 60;
    minutes_ %= 60;
    hours_ %= 24;
    day_week_ %= 7;
    months_ %= 12;
    years_ %= 100;
    leap_year_ %= 4;

    time_to_regs();
}

void Rp5c01::regs_to_time() {
    const auto r = [&](std::size_t i) { return regs_[kTimeBlock * kRegsPerBlock + i]; };
    seconds_ = r(0) + 10 * r(1);
    minutes_ = r(2) + 10 * r(3);
    hours_ = r(4) + 10 * r(5);
    day_week_ = r(6);
    days_ = r(7) + 10 * r(8) - 1;
    months_ = r(9) + 10 * r(10) - 1;
    years_ = r(11) + 10 * r(12);
    leap_year_ = regs_[kAlarmBlock * kRegsPerBlock + 11];

    if (!regs_[kAlarmBlock * kRegsPerBlock + 10]) {
        // 12-hour mode
        if (hours_ >= 20) {
            hours_ = (hours_ - 20) + 12;
        }
    }
}

void Rp5c01::time_to_regs() {
    unsigned hours = hours_;
    if (!regs_[kAlarmBlock * kRegsPerBlock + 10]) {
        if (hours_ >= 12) {
            hours = (hours_ - 12) + 20;
        }
    }
    auto w = [&](std::size_t i, unsigned v) {
        regs_[kTimeBlock * kRegsPerBlock + i] = static_cast<std::uint8_t>(v);
    };
    w(0, seconds_ % 10);
    w(1, seconds_ / 10);
    w(2, minutes_ % 10);
    w(3, minutes_ / 10);
    w(4, hours % 10);
    w(5, hours / 10);
    w(6, day_week_);
    w(7, (days_ + 1) % 10);
    w(8, (days_ + 1) / 10);
    w(9, (months_ + 1) % 10);
    w(10, (months_ + 1) / 10);
    w(11, years_ % 10);
    w(12, years_ / 10);
    regs_[kAlarmBlock * kRegsPerBlock + 11] = static_cast<std::uint8_t>(leap_year_);
}

std::uint8_t Rp5c01::peek_port(const std::uint8_t port) const {
    switch (port) {
    case kModeReg:
        return mode_reg_;
    case kTestReg:
    case kResetReg:
        return 0x0F;  // write-only (RP5C01.cc:92-94)
    default: {
        const unsigned block = mode_reg_ & kModeBlockSelect;
        const std::uint8_t value = regs_[block * kRegsPerBlock + port];
        return static_cast<std::uint8_t>(value & kMask[block][port]);
    }
    }
}

void Rp5c01::write_port(const std::uint8_t port, const std::uint8_t value) {
    switch (port) {
    case kModeReg:
        sync_time();
        mode_reg_ = value;
        break;
    case kTestReg:
        sync_time();
        test_reg_ = value;
        break;
    case kResetReg:
        reset_reg_ = value;
        if (value & kResetAlarm) {
            for (std::size_t i = 2; i < 9; ++i) {
                regs_[kAlarmBlock * kRegsPerBlock + i] = 0;
            }
        }
        if (value & kResetFraction) {
            fraction_ = 0;
        }
        break;
    default: {
        const unsigned block = mode_reg_ & kModeBlockSelect;
        const bool time_like = (block == kTimeBlock) || (block == kAlarmBlock);
        if (time_like) {
            sync_time();
        }
        regs_[block * kRegsPerBlock + port] =
            static_cast<std::uint8_t>(value & kMask[block][port]);
        if (time_like) {
            regs_to_time();
        }
        break;
    }
    }
}

core::BusData Rp5c01::io_read(const core::BusAddress port) {
    if (clock_gate_ != nullptr && !clock_gate_->clock_ic_enabled()) {
        return 0xFF;  // CLOCK-IC disabled via #F5 bit 7 (fact-sheet §5)
    }
    if ((port & 0x01) == 0) {
        return 0xFF;  // #B4 read is unstable/open (MSXRTC.cc:26; fact-sheet §10)
    }
    // #B5 data read: advance time for time/alarm blocks, then return data|0xF0.
    const unsigned block = mode_reg_ & kModeBlockSelect;
    if (latch_ != kModeReg && latch_ != kTestReg && latch_ != kResetReg &&
        (block == kTimeBlock || block == kAlarmBlock)) {
        sync_time();
    }
    return static_cast<core::BusData>(peek_port(latch_) | 0xF0);
}

void Rp5c01::io_write(const core::BusAddress port, const core::BusData value) {
    if (clock_gate_ != nullptr && !clock_gate_->clock_ic_enabled()) {
        return;  // CLOCK-IC disabled
    }
    switch (port & 0x01) {
    case 0:
        latch_ = static_cast<std::uint8_t>(value & 0x0F);
        break;
    case 1:
        write_port(latch_, static_cast<std::uint8_t>(value & 0x0F));
        break;
    default:
        break;
    }
}

}  // namespace sony_msx::devices::rtc
