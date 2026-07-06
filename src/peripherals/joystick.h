#pragma once

#include <array>
#include <cstdint>

#include "devices/audio/psg_ym2149.h"

namespace sony_msx::peripherals {

// Two MSX general-purpose (joystick) ports, read through PSG port A (R14) and
// selected through PSG port B (R15) — M15-S2, backlog C6.
//
// The joystick pins are wired to the S1985 but read via the PSG (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §2 "Joystick ports";
// X5: the peripheral connects to the PSG, not to the S1985 engine). This class
// implements devices::audio::PsgPortSource and is injected into the PSG.
//
//   R14 read : bit0 up, bit1 down, bit2 left, bit3 right, bit4 trigger A,
//              bit5 trigger B (0 = pressed); bit6 keyboard layout (1 = JIS on the
//              Japanese HB-F1XV); bit7 cassette input (idle high, inert in M15 —
//              tape transport is deferred, backlog C7).
//   R15 write: bit6 selects which port feeds R14 (0 = port 1, 1 = port 2);
//              bit7 KANA LED (1 = off). Other bits are pin-6/7/STB output enables
//              (inert here).
//
// Determinism: default = nothing pressed -> idle R14 read = 0xFF. Live input
// events are a frontend concern (backlog C9).
class JoystickPorts final : public devices::audio::PsgPortSource {
public:
    struct PortState {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool trigger_a = false;
        bool trigger_b = false;
    };

    // Keyboard layout line reported on R14 bit6 (JIS = 1 on the HB-F1XV).
    static constexpr std::uint8_t kLayoutBit = 0x40;
    // Cassette input line on R14 bit7 (idle high; inert in M15).
    static constexpr std::uint8_t kCassetteInputBit = 0x80;

    void reset();

    // Set the digital state of one port (index 0 or 1). Out-of-range is ignored.
    void set_port(int index, const PortState& state);
    [[nodiscard]] const PortState& port(int index) const;

    [[nodiscard]] int selected_port() const;
    [[nodiscard]] bool kana_led_off() const;

    // devices::audio::PsgPortSource
    [[nodiscard]] std::uint8_t read_port_a() override;
    void write_port_b(std::uint8_t value) override;

private:
    [[nodiscard]] std::uint8_t encode(const PortState& state) const;

    std::array<PortState, 2> ports_{};
    int selected_ = 0;
    bool kana_off_ = false;
};

}  // namespace sony_msx::peripherals
