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

#include <cstdint>
#include <vector>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::peripherals {

// Centronics-style printer port I/O device on ports #90-#97.
//
// PORT-RANGE FINDING: only two functional registers exist (the classic
// "#90/#91" pair), but the machine XML claims the fuller eight-port
// window: openMSX 21.0: share/machines/Sony_HB-F1XV.xml:74-78,
// `<PrinterPort id="Printer Port"><io base="0x90" num="8" type="IO"/>
// <bidirectional>true</bidirectional>...`. Because `<bidirectional>true</
// bidirectional>` is present, `writePortMask = 0x03` (MSXPrinterPort.cc:18)
// and `readPortMask = writePortMask` (no `<status_readable_on_all_ports>`
// child, MSXPrinterPort.cc:19) -- the same two functional registers
// (strobe/status at mod-4 case 0, data at mod-4 case 1) are reachable at all
// eight physical ports (#90/#94, #91/#95, #92/#96, #93/#97) via mod-4
// aliasing. This class implements the #90-#97 claim, attached at all eight
// ports by the machine and dispatching internally on port & 0x03.
//
// Write protocol, byte-exact (openMSX 21.0:
// src/MSXPrinterPort.cc:46-62):
//   mod-4 case 0 (#90,#94) write : set_strobe(value & 1) -- bit0 = strobe. A
//     falling edge (previous strobe high, new strobe low) appends the
//     last-written data byte to captured_bytes() (Printer.cc:59-66,
//     `if (!strobe && prevStrobe) { write(toPrint); }` -- software convention
//     is to write data first, then pulse strobe low; fact-sheet §8, "BIOS
//     LPTOUT pulses STROBE low after writing data").
//   mod-4 case 0 read  : status byte -- always 0x00 (ready)
//     here (see the disclosed divergence note below).
//   mod-4 case 1 (#91,#95) write : data_ = value.
//   mod-4 case 1 read  : open-bus 0xFF (the XML declares no separate
//     `type="I"` entry for #91 -- write-only).
//   mod-4 case 2 (#92,#96) : no-op write / 0xFF read.
//   mod-4 case 3 (#93,#97) : PDIR/bidirectional register -- not implemented,
//     matching openMSX's own documented scope limit (MSXPrinterPort.cc:57,
//     "0x93 PDIR (BiDi) is not implemented" -- openMSX's own gap, not ours).
//
// DISCLOSED DIVERGENCE: openMSX's default *unplugged* printer port
// (DummyPrinterPortDevice::getStatus, DummyPrinterPortDevice.cc:5-8) reports
// busy=true (not ready) -- the state of nothing plugged in. This emulator has
// no pluggable-peripheral framework; this class IS the built-in port, so it
// reports ready (busy=0) by default, matching an openMSX run WITH a printer
// actually plugged in (PrinterCore::getStatus, Printer.cc:54-57, returns
// false=ready). The openMSX A/B parity probe deliberately
// never reads this bit -- this divergence is disclosed and intentional, not
// a bug.
//
// ARCHITECTURAL SIMPLIFICATION (disclosed, mirrors the Ppi8255/
// PpiSlotSelect folding precedent): openMSX splits the port protocol
// (MSXPrinterPort, the bus device) from the byte sink (a separately pluggable
// PrinterCore/ImagePrinterMSX via a Connector/Pluggable framework). This
// emulator has no pluggable-peripheral framework, so PrinterPort folds both
// the port protocol and the falling-edge byte-capture into one class.
//
// Purely combinational (no clock
// dependency). reset() sets strobe_=true (idle high), data_=0, and clears
// captured_bytes_ without triggering a capture: fields are set directly
// rather than through the edge-detecting setter, so no edge is observed
// (mirrors MSXPrinterPort::reset()'s own strobe transition false->true, a
// rising edge, which is not a capture trigger either).
//
// Debug/introspection convention: captured_bytes() exposes the raw vector
// for tests; a machine-level helper may serialize it deterministically to
// <debug_root>/printer/<filename> (a debug aid, not a rendered "printed
// page" -- rendering real printer output is out of scope here).
class PrinterPort final : public core::IoDevice {
public:
    void reset();

    // Bus-independent protocol access.
    void set_strobe(bool strobe);         // #90/#94 write bit0 -- edge-detects
    void write_data(std::uint8_t value);  // #91/#95 write

    [[nodiscard]] bool strobe() const { return strobe_; }
    [[nodiscard]] std::uint8_t data() const { return data_; }
    [[nodiscard]] const std::vector<std::uint8_t>& captured_bytes() const { return captured_bytes_; }

    // core::IoDevice on #90-#97 (dispatch on port & 0x03, mod-4 aliasing).
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

private:
    bool strobe_ = true;
    std::uint8_t data_ = 0;
    std::vector<std::uint8_t> captured_bytes_;
};

}  // namespace sony_msx::peripherals
