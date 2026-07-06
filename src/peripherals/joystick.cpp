#include "peripherals/joystick.h"

namespace sony_msx::peripherals {

void JoystickPorts::reset() {
    ports_ = {};
    selected_ = 0;
    kana_off_ = false;
}

void JoystickPorts::set_port(const int index, const PortState& state) {
    if (index < 0 || index > 1) {
        return;
    }
    ports_[static_cast<std::size_t>(index)] = state;
}

const JoystickPorts::PortState& JoystickPorts::port(const int index) const {
    return ports_[static_cast<std::size_t>(index & 1)];
}

int JoystickPorts::selected_port() const {
    return selected_;
}

bool JoystickPorts::kana_led_off() const {
    return kana_off_;
}

std::uint8_t JoystickPorts::encode(const PortState& state) const {
    // Active-low: start all-released (1) and clear the pressed bits (0 = pressed).
    std::uint8_t bits = 0x3F;  // bits 0-5 (directions + triggers) all released
    if (state.up) {
        bits &= ~0x01;
    }
    if (state.down) {
        bits &= ~0x02;
    }
    if (state.left) {
        bits &= ~0x04;
    }
    if (state.right) {
        bits &= ~0x08;
    }
    if (state.trigger_a) {
        bits &= ~0x10;
    }
    if (state.trigger_b) {
        bits &= ~0x20;
    }
    // bit6 keyboard layout = JIS (1) on the HB-F1XV; bit7 cassette input idle high.
    bits = static_cast<std::uint8_t>(bits | kLayoutBit | kCassetteInputBit);
    return bits;
}

std::uint8_t JoystickPorts::read_port_a() {
    return encode(ports_[static_cast<std::size_t>(selected_)]);
}

void JoystickPorts::write_port_b(const std::uint8_t value) {
    // fact-sheet §2 "PSG R15 (write)": bit6 selects the joystick port feeding
    // R14 (1 = port 2), bit7 KANA LED (1 = off).
    selected_ = (value & 0x40) ? 1 : 0;
    kana_off_ = (value & 0x80) != 0;
}

}  // namespace sony_msx::peripherals
