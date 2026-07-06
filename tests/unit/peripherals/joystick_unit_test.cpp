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

    return 0;
}
