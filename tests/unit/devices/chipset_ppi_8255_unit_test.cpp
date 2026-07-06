#include <cstdint>
#include <iostream>

#include "devices/chipset/ppi_8255.h"
#include "devices/chipset/slot_bus.h"
#include "peripherals/keyboard_matrix.h"

// Suite: Devices_ChipsetPpi8255_Unit  (M15-S4, backlog C6; change X1)
//
// Full i8255 PPI. The FIRST block is the M11 PpiSlotSelect #A8 contract reused
// VERBATIM as a locked regression guard (X1: #A8 must stay byte-for-byte). The
// remaining blocks cover the new port B/C/control surface + keyboard rows.
// Grounding: fact-sheet §3; I8255.cc / MSXPPI.cc.

namespace {

using sony_msx::devices::chipset::Ppi8255;
using sony_msx::devices::chipset::SlotBus;
using sony_msx::peripherals::KeyboardMatrix;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

}  // namespace

int main() {
    SlotBus slot_bus;
    KeyboardMatrix kb;
    kb.reset();
    Ppi8255 ppi(slot_bus, kb);
    ppi.reset();

    // ===== LOCKED GUARD: #A8 slot-select, byte-for-byte from the M11 test. =====
    ppi.io_write(0xA8, 0b11100100);
    if (!expect_true(slot_bus.primary_select() == 0b11100100,
                     "WriteA8_DrivesSlotBusPrimarySelect")) {
        return 1;
    }
    if (!expect_true(ppi.io_read(0xA8) == 0b11100100, "ReadA8_ReturnsLatchedByte")) {
        return 1;
    }
    ppi.io_write(0xA8, 0x00);
    if (!expect_true(ppi.io_read(0xA8) == 0x00 && slot_bus.primary_select() == 0x00,
                     "SecondWrite_UpdatesReadbackAndSlotBus")) {
        return 1;
    }
    // ===== end locked guard =====

    // --- Idle port B (#A9) keyboard read: inverted, all released -> 0xFF. ---
    if (!expect_true(ppi.io_read(0xA9) == 0xFF, "PortB_Idle_ReadsInverted0xFF")) {
        return 1;
    }

    // Program the MSX PPI mode: A output, B input, C output (control 0x82).
    ppi.io_write(0xAB, 0x82);
    if (!expect_true(ppi.control() == 0x82, "Control_ModeSet_Stored")) {
        return 1;
    }

    // --- Port C row select drives which matrix row port B returns. ---
    kb.set_key(2, 0, true);   // row 2, column 0 -> row 2 reads 0xFE
    kb.set_key(7, 3, true);   // row 7, column 3 -> row 7 reads 0xF7
    ppi.io_write(0xAA, 0x02);  // select row 2
    if (!expect_true(ppi.selected_row() == 2, "PortC_RowSelect_Bits0to3")) {
        return 1;
    }
    if (!expect_true(ppi.io_read(0xA9) == 0xFE, "PortB_Row2_ReturnsInvertedColumns")) {
        return 1;
    }
    ppi.io_write(0xAA, 0x07);  // select row 7
    if (!expect_true(ppi.io_read(0xA9) == 0xF7, "PortB_Row7_ReturnsInvertedColumns")) {
        return 1;
    }

    // --- Port C readback (output) returns the latch. ---
    ppi.io_write(0xAA, 0x35);
    if (!expect_true(ppi.io_read(0xAA) == 0x35, "PortC_Output_ReadsLatch")) {
        return 1;
    }

    // --- Control bit-set/reset toggles a single port-C bit (CAPS LED = bit6). ---
    ppi.io_write(0xAB, 0x0D);  // bit index 6 (0x0C), set
    if (!expect_true(ppi.caps_led_off(), "Control_SetBit6_CapsLed")) {
        return 1;
    }
    ppi.io_write(0xAB, 0x0C);  // bit index 6, reset
    if (!expect_true(!ppi.caps_led_off(), "Control_ResetBit6_CapsLed")) {
        return 1;
    }

    // --- Port C bit4 = cassette motor, bit7 = key-click (deterministic bits). ---
    ppi.io_write(0xAA, 0x90);  // bit4 (motor off) + bit7 (click)
    if (!expect_true(ppi.cassette_motor_off() && ppi.key_click(),
                     "PortC_MotorAndClickBits")) {
        return 1;
    }

    // --- Control register readback. ---
    ppi.io_write(0xAB, 0x82);
    if (!expect_true(ppi.io_read(0xAB) == 0x82, "Control_Readback")) {
        return 1;
    }

    return 0;
}
