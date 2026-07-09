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

void JoystickPorts::attach_cassette_input_source(CassetteInputSource* source) {
    cassette_source_ = source;
}

void JoystickPorts::attach_rensha_turbo(const RenshaTurbo* source) {
    rensha_ = source;
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
    std::uint8_t bits = encode(ports_[static_cast<std::size_t>(selected_)]);
    // M18-S3 (A-M18-10): unattached (nullptr) leaves bit7 exactly as encode()
    // set it (unconditionally 1) -- byte-for-byte identical to the pre-M18
    // behavior (regression guard). Attached, bit7 reflects the live source.
    if (cassette_source_ != nullptr) {
        if (cassette_source_->cassette_input_high()) {
            bits = static_cast<std::uint8_t>(bits | kCassetteInputBit);
        } else {
            bits = static_cast<std::uint8_t>(bits & ~kCassetteInputBit);
        }
    }
    // Ren-Sha Turbo autofire (M25, A-M25-7): PSG R14 bit4 = trigger A,
    // applied to the SELECTED port's already-computed value (matches
    // openMSX's ports[selectedPort]-after-mux wiring). Unattached (the
    // default) leaves bits exactly as computed above -- byte-for-byte
    // identical to the pre-M25 behavior (regression guard, M25-S3). OR-only:
    // this can only ever force a 0 bit (pressed) to 1 (a periodic release),
    // never the reverse (R-M25-6).
    if (rensha_ != nullptr) {
        bits = static_cast<std::uint8_t>(bits | rensha_->joystick_trigger_a_or_mask());
    }
    return bits;
}

void JoystickPorts::write_port_b(const std::uint8_t value) {
    // fact-sheet §2 "PSG R15 (write)": bit6 selects the joystick port feeding
    // R14 (1 = port 2), bit7 KANA LED (1 = off).
    selected_ = (value & 0x40) ? 1 : 0;
    kana_off_ = (value & 0x80) != 0;
}

}  // namespace sony_msx::peripherals
