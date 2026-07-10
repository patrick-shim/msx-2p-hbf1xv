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

#include "devices/audio/psg_ym2149.h"
#include "peripherals/rensha_turbo.h"

namespace sony_msx::peripherals {

// Source of the cassette input bit (CMI) feeding PSG R14 bit7 (M18-S3,
// backlog C7). Defined here in JoystickPorts's own header -- the consumer's
// header -- mirroring the existing devices::audio::PsgPortSource precedent
// (defined in psg_ym2149.h, the PSG's own header, for the same
// injected-source relationship). The concrete implementation lives in
// peripherals::CassetteInterface (src/peripherals/cassette_interface.h) and
// is injected via JoystickPorts::attach_cassette_input_source.
class CassetteInputSource {
public:
    virtual ~CassetteInputSource() = default;
    // true = idle-high (no signal / logic-1); false = logic-0.
    [[nodiscard]] virtual bool cassette_input_high() const = 0;
};

// Two MSX general-purpose (joystick) ports, read through PSG port A (R14) and
// selected through PSG port B (R15) — M15-S2, backlog C6.
//
// The joystick pins are wired to the S1985 but read via the PSG (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §2 "Joystick
// ports"; X5: the peripheral connects to the PSG, not the S1985 engine).
// This class implements devices::audio::PsgPortSource and is injected into
// the PSG.
//
//   R14 read : bit0 up, bit1 down, bit2 left, bit3 right, bit4 trigger A,
//              bit5 trigger B (0 = pressed); bit6 keyboard layout (1 = JIS
//              on the Japanese HB-F1XV); bit7 cassette input, via the
//              injectable CassetteInputSource (see
//              attach_cassette_input_source below). Unattached:
//              unconditionally idle high (1), a regression guard matching
//              pre-M18 behavior. Attached: reflects the source's live value.
//   R15 write: bit6 selects which port feeds R14 (0 = port 1, 1 = port 2);
//              bit7 KANA LED (1 = off). Other bits (pin-6/7/STB output
//              enables) are inert here.
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
    // Cassette input line on R14 bit7 (idle high by default; reflects the
    // live CassetteInputSource when attached, M18-S3).
    static constexpr std::uint8_t kCassetteInputBit = 0x80;

    void reset();

    // Set the digital state of one port (index 0 or 1). Out-of-range is ignored.
    void set_port(int index, const PortState& state);
    [[nodiscard]] const PortState& port(int index) const;

    [[nodiscard]] int selected_port() const;
    [[nodiscard]] bool kana_led_off() const;

    // Inject the cassette-input source backing R14 bit7 (M18-S3, A-M18-10).
    // nullptr (the default) reproduces the exact pre-M18 behavior (bit7
    // unconditionally 1) -- a regression guard, unit-tested explicitly.
    void attach_cassette_input_source(CassetteInputSource* source);

    // Inject the Ren-Sha Turbo autofire source backing R14 bit4/trigger-A
    // (M25, backlog C8, openMSX sound/MSXPSG.cc:90-93, A-M25-7). nullptr
    // (the default) reproduces the exact pre-M25 behavior byte-for-byte --
    // a hard regression guard, unit-tested explicitly.
    void attach_rensha_turbo(const RenshaTurbo* source);

    // devices::audio::PsgPortSource
    [[nodiscard]] std::uint8_t read_port_a() override;
    void write_port_b(std::uint8_t value) override;

private:
    [[nodiscard]] std::uint8_t encode(const PortState& state) const;

    std::array<PortState, 2> ports_{};
    int selected_ = 0;
    bool kana_off_ = false;
    CassetteInputSource* cassette_source_ = nullptr;
    const RenshaTurbo* rensha_ = nullptr;
};

}  // namespace sony_msx::peripherals
