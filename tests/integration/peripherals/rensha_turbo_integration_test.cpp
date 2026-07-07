#include <cstdint>
#include <iostream>

#include "devices/audio/psg_ym2149.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"
#include "peripherals/rensha_turbo.h"

// Suite: Peripherals_RenshaTurbo_Integration (M25-S3, backlog C8)
//
// Wires RenshaTurbo into KeyboardMatrix/JoystickPorts (the additive OR-mask
// attach points from M25-S3) and proves: (a) the default/unattached path is
// byte-for-byte unchanged from pre-M25 (regression guard); (b) with the
// underlying key/trigger NOT held, the autofire signal has ZERO observable
// effect at any sampled cycle across a full toggle period (R-M25-6); (c)
// with the key/trigger genuinely held, the read value alternates between
// "pressed" and "released" at the expected computed cadence.

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::peripherals::JoystickPorts;
using sony_msx::peripherals::KeyboardMatrix;
using sony_msx::peripherals::RenshaTurbo;
using sony_msx::peripherals::RenshaTurboClockSource;

int g_failures = 0;

void expect_true(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

class FakeClock final : public RenshaTurboClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycles_; }
    void set_cycles(const std::uint64_t c) { cycles_ = c; }

private:
    std::uint64_t cycles_ = 0;
};

}  // namespace

int main() {
    // --- (a) REGRESSION GUARD: attach_rensha_turbo(nullptr) is the default;
    //     keyboard_row(8)/read_port_a() must be byte-for-byte identical to
    //     the pre-M25 (M15/M18) behavior for representative scenarios. ---
    {
        KeyboardMatrix kb;
        kb.reset();
        expect_true(kb.keyboard_row(8) == 0xFF, "Regression_Keyboard_DefaultUnattached_Row8Idle");
        kb.set_key(8, 0, true);  // SPACE pressed
        expect_true(kb.keyboard_row(8) == static_cast<std::uint8_t>(0xFF & ~0x01),
                    "Regression_Keyboard_DefaultUnattached_Row8SpacePressed");
        // Other rows unaffected by the (unattached) RenSha seam.
        expect_true(kb.keyboard_row(3) == 0xFF, "Regression_Keyboard_DefaultUnattached_OtherRowsUnaffected");

        JoystickPorts joy;
        joy.reset();
        expect_true(joy.read_port_a() == 0xFF, "Regression_Joystick_DefaultUnattached_IdleAllHigh");
        JoystickPorts::PortState s;
        s.trigger_a = true;
        joy.set_port(0, s);
        expect_true(joy.read_port_a() == static_cast<std::uint8_t>(0xFF & ~0x10),
                    "Regression_Joystick_DefaultUnattached_TriggerAPressed");
    }

    // --- (a continued) Explicit attach(nullptr) is also byte-for-byte
    //     identical to never having called attach at all. ---
    {
        KeyboardMatrix kb;
        kb.reset();
        kb.attach_rensha_turbo(nullptr);
        expect_true(kb.keyboard_row(8) == 0xFF, "Regression_Keyboard_ExplicitNullptr_Row8Idle");

        JoystickPorts joy;
        joy.reset();
        joy.attach_rensha_turbo(nullptr);
        expect_true(joy.read_port_a() == 0xFF, "Regression_Joystick_ExplicitNullptr_IdleAllHigh");
    }

    // --- (b) With SPACE/trigger-A NOT held, a live, ENGAGED RenshaTurbo
    //     produces ZERO observable difference at ANY sampled cycle across a
    //     full toggle period (R-M25-6). ---
    {
        KeyboardMatrix kb;
        kb.reset();
        JoystickPorts joy;
        joy.reset();
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        kb.attach_rensha_turbo(&rensha);
        joy.attach_rensha_turbo(&rensha);

        bool keyboard_never_spuriously_pressed = true;
        bool joystick_never_spuriously_pressed = true;
        for (std::uint64_t c = 0; c < 400000; c += 4001) {
            clock.set_cycles(c);
            if (kb.keyboard_row(8) != 0xFF) {
                keyboard_never_spuriously_pressed = false;
            }
            if (joy.read_port_a() != 0xFF) {
                joystick_never_spuriously_pressed = false;
            }
        }
        expect_true(keyboard_never_spuriously_pressed,
                    "NotHeld_KeyboardRow8_NeverSpuriouslyPressed_ExhaustiveSweep");
        expect_true(joystick_never_spuriously_pressed,
                    "NotHeld_JoystickTriggerA_NeverSpuriouslyPressed_ExhaustiveSweep");
    }

    // --- (c) With SPACE genuinely held AND RenSha engaged, the keyboard row
    //     8 read alternates between "pressed" (bit0=0) and "released"
    //     (bit0=1) at the expected computed cadence. ---
    {
        KeyboardMatrix kb;
        kb.reset();
        kb.set_key(8, 0, true);  // SPACE held down
        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        kb.attach_rensha_turbo(&rensha);

        // half_period_cycles at speed=100 (kSystemClockHz*47)/6000, hand-
        // computed the same way as the M25-S2 unit test's own oracle.
        const std::uint64_t period = (RenshaTurbo::kSystemClockHz * 47ull) / 6000ull;

        clock.set_cycles(0);
        expect_true(kb.keyboard_row(8) == static_cast<std::uint8_t>(0xFF & ~0x01),
                    "Held_SpaceKey_AtCycle0_ReadsPressed");
        clock.set_cycles(period);
        expect_true(kb.keyboard_row(8) == 0xFF, "Held_SpaceKey_AtOnePeriod_ReadsReleasedByAutofire");
        clock.set_cycles(2 * period);
        expect_true(kb.keyboard_row(8) == static_cast<std::uint8_t>(0xFF & ~0x01),
                    "Held_SpaceKey_AtTwoPeriods_BackToPressed");

        bool saw_pressed = false;
        bool saw_released = false;
        for (std::uint64_t c = 0; c < 4 * period; c += period / 4) {
            clock.set_cycles(c);
            if (kb.keyboard_row(8) == 0xFF) {
                saw_released = true;
            } else {
                saw_pressed = true;
            }
        }
        expect_true(saw_pressed && saw_released,
                    "Held_SpaceKey_AlternatesBetweenPressedAndReleased_ExpectedCadence");
    }

    // --- (c continued) Same alternation proven through the PSG for the
    //     joystick trigger-A path (real PsgYm2149, mirrors the M15 joystick
    //     unit test's "through the PSG" precedent). ---
    {
        PsgYm2149 psg;
        psg.reset();
        JoystickPorts joy;
        joy.reset();
        psg.attach_port_source(&joy);
        JoystickPorts::PortState s;
        s.trigger_a = true;  // held down
        joy.set_port(0, s);

        RenshaTurbo rensha;
        rensha.reset();
        FakeClock clock;
        rensha.attach_clock_source(&clock);
        rensha.set_speed(100);
        joy.attach_rensha_turbo(&rensha);

        const std::uint64_t period = (RenshaTurbo::kSystemClockHz * 47ull) / 6000ull;

        auto read_r14 = [&]() -> std::uint8_t {
            psg.write_address(14);
            return psg.read_data();
        };

        clock.set_cycles(0);
        expect_true(read_r14() == static_cast<std::uint8_t>(0xFF & ~0x10),
                    "Held_TriggerA_ViaPsg_AtCycle0_ReadsPressed");
        clock.set_cycles(period);
        expect_true(read_r14() == 0xFF, "Held_TriggerA_ViaPsg_AtOnePeriod_ReadsReleasedByAutofire");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Peripherals_RenshaTurbo_Integration cases passed\n";
    return 0;
}
