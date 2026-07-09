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

#include "peripherals/printer_port.h"

namespace sony_msx::peripherals {

void PrinterPort::reset() {
    // Set fields directly (NOT via set_strobe/write_data) so no falling edge
    // is observed and no spurious capture occurs (A-M18-6, class-doc note).
    strobe_ = true;
    data_ = 0;
    captured_bytes_.clear();
}

void PrinterPort::set_strobe(const bool strobe) {
    // Falling edge (previous HIGH, new LOW) captures the last-written data
    // byte (Printer.cc:59-66: `if (!strobe && prevStrobe) { write(toPrint); }`).
    if (!strobe && strobe_) {
        captured_bytes_.push_back(data_);
    }
    strobe_ = strobe;
}

void PrinterPort::write_data(const std::uint8_t value) {
    data_ = value;
}

core::BusData PrinterPort::io_read(const core::BusAddress port) {
    switch (port & 0x03) {
    case 0:
        // Status byte: unused_bits(0x00) | (busy ? 0b10 : 0). Always ready
        // (busy=0) in this milestone -- a disclosed divergence from openMSX's
        // *unplugged* default (A-M18-7, class-doc note).
        return 0x00;
    default:
        // case 1: open-bus (write-only). case 2/3: inert / unimplemented.
        return 0xFF;
    }
}

void PrinterPort::io_write(const core::BusAddress port, const core::BusData value) {
    switch (port & 0x03) {
    case 0:
        set_strobe((value & 0x01) != 0);
        break;
    case 1:
        write_data(value);
        break;
    default:
        // case 2: no-op. case 3: PDIR/bidirectional, not implemented (matches
        // openMSX's own scope limit, MSXPrinterPort.cc:57).
        break;
    }
}

}  // namespace sony_msx::peripherals
