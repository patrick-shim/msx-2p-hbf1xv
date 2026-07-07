#include <cstdint>
#include <iostream>

#include "devices/audio/psg_ym2149.h"
#include "peripherals/joystick.h"

// Suite: Peripherals_Joystick_Unit  (M15-S2, backlog C6)
//
// Two joystick ports read through PSG port A (R14) and selected through PSG port
// B (R15). Grounding: fact-sheet §2 "Joystick ports". Also exercises the port
// through a real PsgYm2149 to prove the R14/R15 wiring (X5: peripheral -> PSG).

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::peripherals::JoystickPorts;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

}  // namespace

int main() {
    // --- Idle read: nothing pressed -> all-high (0xFF). ---
    {
        JoystickPorts joy;
        joy.reset();
        if (!expect_true(joy.read_port_a() == 0xFF, "Idle_R14_AllHigh")) {
            return 1;
        }
    }

    // --- Direction/trigger bits are active-low (0 = pressed). ---
    {
        JoystickPorts joy;
        joy.reset();
        JoystickPorts::PortState s;
        s.up = true;         // bit0
        s.trigger_a = true;  // bit4
        joy.set_port(0, s);
        const std::uint8_t r = joy.read_port_a();
        // bits 0 and 4 cleared; bits 1-3,5 set; bit6 layout=1; bit7 cassette=1.
        if (!expect_true(r == static_cast<std::uint8_t>(0xFF & ~0x01 & ~0x10),
                         "Port0_UpAndTrigA_Pressed")) {
            std::cerr << "  got 0x" << std::hex << int(r) << "\n";
            return 1;
        }
    }

    // --- R15 bit6 selects which port feeds R14. ---
    {
        JoystickPorts joy;
        joy.reset();
        JoystickPorts::PortState p1;
        p1.left = true;  // bit2 on port 1
        JoystickPorts::PortState p2;
        p2.right = true;  // bit3 on port 2
        joy.set_port(0, p1);
        joy.set_port(1, p2);

        joy.write_port_b(0x00);  // select port 1
        if (!expect_true(joy.selected_port() == 0 &&
                             joy.read_port_a() == static_cast<std::uint8_t>(0xFF & ~0x04),
                         "R15Bit6Clear_SelectsPort1")) {
            return 1;
        }
        joy.write_port_b(0x40);  // select port 2
        if (!expect_true(joy.selected_port() == 1 &&
                             joy.read_port_a() == static_cast<std::uint8_t>(0xFF & ~0x08),
                         "R15Bit6Set_SelectsPort2")) {
            return 1;
        }
        // bit7 KANA LED tracked.
        joy.write_port_b(0x80);
        if (!expect_true(joy.kana_led_off(), "R15Bit7_KanaLed")) {
            return 1;
        }
    }

    // --- Through the PSG: OUT R15 then IN R14 reflects joystick state. ---
    {
        PsgYm2149 psg;
        psg.reset();
        JoystickPorts joy;
        joy.reset();
        psg.attach_port_source(&joy);

        JoystickPorts::PortState p2;
        p2.trigger_b = true;  // bit5 on port 2
        joy.set_port(1, p2);

        // Select port 2 via R15, then read R14.
        psg.write_address(15);
        psg.write_data(0x40);
        psg.write_address(14);
        const std::uint8_t r14 = psg.read_data();
        if (!expect_true(r14 == static_cast<std::uint8_t>(0xFF & ~0x20),
                         "PsgR14_ReflectsSelectedPort2Trigger")) {
            return 1;
        }
    }

    // --- M18-S3 (A-M18-10) cassette-input injection: REGRESSION GUARD. The
    // unattached path (cassette_source_ == nullptr, the default) must be
    // byte-for-byte IDENTICAL to the pre-M18 behavior -- bit7 unconditionally
    // 1, regardless of any joystick/direction state. ---
    {
        JoystickPorts joy;
        joy.reset();
        // Idle: bit7 still 1 exactly as before M18.
        if (!expect_true(joy.read_port_a() == 0xFF, "Unattached_Idle_Bit7StillOne")) {
            return 1;
        }
        // With other bits pressed, bit7 remains 1 (unaffected).
        JoystickPorts::PortState s;
        s.down = true;
        joy.set_port(0, s);
        const std::uint8_t r = joy.read_port_a();
        if (!expect_true((r & JoystickPorts::kCassetteInputBit) != 0,
                         "Unattached_WithOtherBitsPressed_Bit7StillOne")) {
            return 1;
        }
        if (!expect_true(r == static_cast<std::uint8_t>(0xFF & ~0x02),
                         "Unattached_WithOtherBitsPressed_RestUnchanged")) {
            return 1;
        }
    }

    // --- Attached cassette source: bit7 reflects the injected source's live
    // value (M18-S3, A-M18-10). ---
    {
        class FakeCassetteSource final : public sony_msx::peripherals::CassetteInputSource {
        public:
            bool high = true;
            [[nodiscard]] bool cassette_input_high() const override { return high; }
        };

        JoystickPorts joy;
        joy.reset();
        FakeCassetteSource source;
        joy.attach_cassette_input_source(&source);

        source.high = true;
        if (!expect_true((joy.read_port_a() & JoystickPorts::kCassetteInputBit) != 0,
                         "Attached_SourceHigh_Bit7Set")) {
            return 1;
        }
        source.high = false;
        if (!expect_true((joy.read_port_a() & JoystickPorts::kCassetteInputBit) == 0,
                         "Attached_SourceLow_Bit7Clear")) {
            return 1;
        }
        // Other bits are unaffected by the cassette-input override.
        JoystickPorts::PortState s;
        s.up = true;
        joy.set_port(0, s);
        source.high = false;
        const std::uint8_t r = joy.read_port_a();
        if (!expect_true(r == static_cast<std::uint8_t>(0xFF & ~0x01 & ~JoystickPorts::kCassetteInputBit),
                         "Attached_OtherBitsUnaffectedByCassetteOverride")) {
            std::cerr << "  got 0x" << std::hex << int(r) << "\n";
            return 1;
        }
        // Detaching (nullptr) restores the unconditional-1 default.
        joy.attach_cassette_input_source(nullptr);
        if (!expect_true((joy.read_port_a() & JoystickPorts::kCassetteInputBit) != 0,
                         "Detached_RevertsToUnconditionalOne")) {
            return 1;
        }
    }

    return 0;
}
