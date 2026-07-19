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

#include "devices/chipset/ppi_8255.h"

namespace sony_msx::devices::chipset {

Ppi8255::Ppi8255(SlotBus& slot_bus, KeyboardRowSource& keyboard)
    : port_a_(slot_bus), keyboard_(keyboard) {
}

void Ppi8255::reset() {
    // Port A (slot select) is reset by the machine to a defined value via #A8
    // writes; here we reset the added B/C/control surface.
    latch_b_ = 0;
    latch_c_ = 0;
    control_ = kResetControl;
}

std::uint8_t Ppi8255::read_port_b() const {
    // Port B is normally input on MSX (keyboard). When programmed as output the
    // latch is returned (I8255.cc:130-144).
    if (control_ & kDirectionB) {
        return keyboard_.keyboard_row(selected_row());
    }
    return latch_b_;
}

std::uint8_t Ppi8255::read_port_c() const {
    // Compose the two 4-bit halves per their direction (I8255.cc:190-228). An
    // input half floats high; an output half returns the latch.
    const std::uint8_t hi = (control_ & kDirectionC1) ? 0xF0 : static_cast<std::uint8_t>(latch_c_ & 0xF0);
    const std::uint8_t lo = (control_ & kDirectionC0) ? 0x0F : static_cast<std::uint8_t>(latch_c_ & 0x0F);
    return static_cast<std::uint8_t>(hi | lo);
}

void Ppi8255::write_control(const std::uint8_t value) {
    if (value & kSetMode) {
        // Set the control mode (I8255.cc:317-329). MSX uses mode 0 only.
        control_ = value;
        return;
    }
    // Bit set/reset on a single port-C bit (I8255.cc:330-340). Bit 7
    // (key-click) can be driven this way as well as by a full #AA write, so
    // both paths must emit the click edge.
    const std::uint8_t prev_latch_c = latch_c_;
    const std::uint8_t bit = static_cast<std::uint8_t>((value & kBitNr) >> 1);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit);
    if (value & kSetReset) {
        latch_c_ = static_cast<std::uint8_t>(latch_c_ | mask);
    } else {
        latch_c_ = static_cast<std::uint8_t>(latch_c_ & ~mask);
    }
    emit_click_edge_if_toggled(prev_latch_c);
}

void Ppi8255::emit_click_edge_if_toggled(const std::uint8_t prev_latch_c) {
    // openMSX MSXPPI.cc:128-130: click fires on `(prevBits ^ value) & 8` of the
    // port-C high nibble -- i.e. a change of the full port-C bit 7. Edge-only,
    // stamped with the current scheduler cycle. No-op unless BOTH seams wired.
    if (click_sink_ == nullptr || cycle_source_ == nullptr) {
        return;
    }
    if (((prev_latch_c ^ latch_c_) & 0x80) == 0) {
        return;
    }
    click_sink_->on_click_edge(cycle_source_->current_cycle(), (latch_c_ & 0x80) != 0);
}

core::BusData Ppi8255::io_read(const core::BusAddress port) {
    switch (port & 0x03) {
    case 0:
        return port_a_.io_read(port);  // #A8 slot-select readback (preserved)
    case 1:
        return read_port_b();          // #A9 keyboard row
    case 2:
        return read_port_c();          // #AA port C
    case 3:
        return control_;               // #AB control readback
    default:
        return 0xFF;
    }
}

void Ppi8255::io_write(const core::BusAddress port, const core::BusData value) {
    switch (port & 0x03) {
    case 0:
        port_a_.io_write(port, value);  // #A8 -> drives SlotBus (preserved)
        break;
    case 1:
        latch_b_ = value;               // #A9 port B latch (inert on MSX)
        break;
    case 2: {
        const std::uint8_t prev_latch_c = latch_c_;
        latch_c_ = value;               // #AA port C (row select / LED / cassette)
        emit_click_edge_if_toggled(prev_latch_c);  // key-click 1-bit DAC edge
        break;
    }
    case 3:
        write_control(value);           // #AB control
        break;
    default:
        break;
    }
}

std::uint8_t Ppi8255::control() const {
    return control_;
}

std::uint8_t Ppi8255::port_c_latch() const {
    return latch_c_;
}

int Ppi8255::selected_row() const {
    return latch_c_ & 0x0F;
}

bool Ppi8255::cassette_motor_off() const {
    return (latch_c_ & 0x10) != 0;
}

bool Ppi8255::caps_led_off() const {
    return (latch_c_ & 0x40) != 0;
}

bool Ppi8255::key_click() const {
    return (latch_c_ & 0x80) != 0;
}

}  // namespace sony_msx::devices::chipset
