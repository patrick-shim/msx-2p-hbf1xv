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
#include <vector>

#include "peripherals/rensha_turbo.h"

// Suite: Peripherals_RenshaTurbo_Unit (M25-S2, backlog C8)
//
// Isolated unit tests for the Ren-Sha Turbo autofire signal generator, per
// docs/m25-planner-package.md §2.5/M25-S2. Zero peripheral wiring at this
// slice. The half_period_cycles() derivation (A-M25-6) is independently
// re-verified BY THE TEST ITSELF at concrete speed values, hand-computed
// from the header's documented formula:
//   ints(speed) = kDefaultMaxInts - (speed * (kDefaultMaxInts - kDefaultMinInts)) / 100
//   half_period_cycles = (kSystemClockHz * ints) / 6000
// (all integer arithmetic).

namespace {

using sony_msx::peripherals::RenshaTurbo;
using sony_msx::peripherals::RenshaTurboClockSource;

int g_failures = 0;

void expect_true(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// A simple, test-only, directly-settable clock source (mirrors the
// CassetteInterface unit test's own FakeClock precedent exactly).
class FakeClock final : public RenshaTurboClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycles_; }
    void set_cycles(const std::uint64_t c) { cycles_ = c; }

private:
    std::uint64_t cycles_ = 0;
};

// Hand-computed half_period_cycles() for a given speed, per the header's
// documented formula -- an INDEPENDENT re-derivation inside the test itself,
// not a call into the class under test.
std::uint64_t hand_computed_half_period(const int speed) {
    const std::uint64_t speed64 = static_cast<std::uint64_t>(speed);
    const std::uint64_t span = RenshaTurbo::kDefaultMaxInts - RenshaTurbo::kDefaultMinInts;  // 174
    const std::uint64_t ints = RenshaTurbo::kDefaultMaxInts - (speed64 * span) / 100;
    return (RenshaTurbo::kSystemClockHz * ints) / 6000;
}

}  // namespace

int main() {
    // --- reset()/unattached-clock idle default: signal() always false. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        expect_true(!rensha.signal(), "Reset_Unattached_SignalFalse");
        expect_true(rensha.speed() == 0, "Reset_Idle_SpeedZero");
        expect_true(rensha.keyboard_row8_or_mask() == 0x00, "Reset_Idle_KeyboardMaskZero");
        expect_true(rensha.joystick_trigger_a_or_mask() == 0x00, "Reset_Idle_JoystickMaskZero");

        FakeClock clock;
        clock.set_cycles(123456);
        rensha.attach_clock_source(&clock);
        expect_true(!rensha.signal(), "Reset_AttachedButSpeedZero_SignalFalse");
    }

    // --- speed()==0 -> always false regardless of cycles, even at high
    //     speed values set-then-reset-to-zero. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        rensha.set_speed(0);
        bool any_true = false;
        for (std::uint64_t c = 0; c < 200000; c += 4999) {
            clock.set_cycles(c);
            if (rensha.signal()) {
                any_true = true;
            }
        }
        expect_true(!any_true, "SpeedZero_AfterHavingBeenNonzero_AlwaysFalse");
    }

    // --- set_speed() clamping. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        rensha.set_speed(-10);
        expect_true(rensha.speed() == 0, "SetSpeed_Negative_ClampedToZero");
        rensha.set_speed(500);
        expect_true(rensha.speed() == 100, "SetSpeed_TooLarge_ClampedTo100");
        rensha.set_speed(50);
        expect_true(rensha.speed() == 50, "SetSpeed_InRange_StoredExactly");
    }

    // --- half_period_cycles() derivation independently re-verified at
    //     speed=100 (fastest, ints=47): the class's own signal() toggles
    //     EXACTLY at the hand-computed period boundary. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        const std::uint64_t period = hand_computed_half_period(100);
        expect_true(period > 0, "Speed100_HandComputedPeriod_Positive");

        clock.set_cycles(0);
        expect_true(!rensha.signal(), "Speed100_AtCycle0_SignalFalse");  // (0/period)&1 == 0
        clock.set_cycles(period);
        expect_true(rensha.signal(), "Speed100_AtOnePeriod_SignalTrue");  // (1)&1 == 1
        clock.set_cycles(period - 1);
        expect_true(!rensha.signal(), "Speed100_OneCycleBeforePeriod_SignalFalse");
        clock.set_cycles(2 * period);
        expect_true(!rensha.signal(), "Speed100_AtTwoPeriods_SignalFalse");  // (2)&1 == 0
        clock.set_cycles(3 * period);
        expect_true(rensha.signal(), "Speed100_AtThreePeriods_SignalTrue");  // (3)&1 == 1
    }

    // --- half_period_cycles() derivation independently re-verified at
    //     speed=1 (slowest, ints=221 - 174/100 = 220 via integer truncation)
    //     -- a SECOND, distinct concrete speed value. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(1);
        const std::uint64_t period = hand_computed_half_period(1);
        expect_true(period > 0, "Speed1_HandComputedPeriod_Positive");
        // speed=1 should be a much SLOWER (larger period) toggle than speed=100.
        expect_true(period > hand_computed_half_period(100),
                    "Speed1_Period_LargerThanSpeed100Period");

        clock.set_cycles(period);
        expect_true(rensha.signal(), "Speed1_AtOnePeriod_SignalTrue");
        clock.set_cycles(period - 1);
        expect_true(!rensha.signal(), "Speed1_OneCycleBeforePeriod_SignalFalse");
    }

    // --- half_period_cycles() derivation at a THIRD, mid-range speed value
    //     (speed=50). ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(50);
        const std::uint64_t period = hand_computed_half_period(50);
        expect_true(period > 0, "Speed50_HandComputedPeriod_Positive");
        clock.set_cycles(period);
        expect_true(rensha.signal(), "Speed50_AtOnePeriod_SignalTrue");
        clock.set_cycles(0);
        expect_true(!rensha.signal(), "Speed50_AtCycle0_SignalFalse");
    }

    // --- keyboard_row8_or_mask()/joystick_trigger_a_or_mask() return the
    //     correct bit values exactly when signal() is true, 0x00 otherwise. ---
    {
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        const std::uint64_t period = hand_computed_half_period(100);

        clock.set_cycles(0);
        expect_true(rensha.keyboard_row8_or_mask() == 0x00, "Masks_SignalFalse_KeyboardMaskZero");
        expect_true(rensha.joystick_trigger_a_or_mask() == 0x00, "Masks_SignalFalse_JoystickMaskZero");

        clock.set_cycles(period);
        expect_true(rensha.keyboard_row8_or_mask() == 0x01, "Masks_SignalTrue_KeyboardMaskBit0");
        expect_true(rensha.joystick_trigger_a_or_mask() == 0x10, "Masks_SignalTrue_JoystickMaskBit4");
    }

    // --- Two-run determinism: identical cycle sequence -> identical signal
    //     sequence. ---
    {
        auto run = [](std::uint64_t base_cycle) {
            RenshaTurbo rensha;
            rensha.reset();
            FakeClock clock;
            rensha.attach_clock_source(&clock);
            rensha.set_speed(73);
            std::vector<bool> observed;
            for (int i = 0; i < 20; ++i) {
                clock.set_cycles(base_cycle + static_cast<std::uint64_t>(i) * 1000);
                observed.push_back(rensha.signal());
            }
            return observed;
        };
        const std::vector<bool> a = run(9000);
        const std::vector<bool> b = run(9000);
        expect_true(a == b, "TwoRunDeterminism_IdenticalCycleSequence_IdenticalSignals");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Peripherals_RenshaTurbo_Unit cases passed\n";
    return 0;
}
