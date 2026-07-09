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

// Centronics-style printer port I/O device on ports #90-#97 (M18-S2, part of
// backlog C7).
//
// PORT-RANGE FINDING (A-M18-5, docs/m18-planner-package.md §2.7): the backlog
// text says "#90/#91", but the machine XML claims the fuller EIGHT-port
// window directly: references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:
// 74-78, `<PrinterPort id="Printer Port"><io base="0x90" num="8" type="IO"/>
// <bidirectional>true</bidirectional>...`. Because `<bidirectional>true</
// bidirectional>` is present, `writePortMask = 0x03` (MSXPrinterPort.cc:18)
// and `readPortMask = writePortMask` (no `<status_readable_on_all_ports>`
// child, MSXPrinterPort.cc:19) -- i.e. the SAME two functional registers
// (strobe/status at mod-4 case 0, data at mod-4 case 1) are reachable at
// EIGHT physical ports (#90/#94, #91/#95, #92/#96, #93/#97) via mod-4
// aliasing. This class implements the accurate #90-#97 claim (attached at
// all eight ports by the machine, dispatching internally on port & 0x03).
//
// Write protocol, byte-exact (A-M18-6, references/openmsx-21.0/src/
// MSXPrinterPort.cc:46-62):
//   mod-4 case 0 (#90,#94) write : set_strobe(value & 1) -- bit0 = strobe. A
//     FALLING edge (previous strobe HIGH, new strobe LOW) appends the
//     LAST-WRITTEN data byte to captured_bytes() (Printer.cc:59-66,
//     `if (!strobe && prevStrobe) { write(toPrint); }` -- software convention
//     is to write data first, then pulse strobe low; fact-sheet §8, "BIOS
//     LPTOUT pulses STROBE low after writing data").
//   mod-4 case 0 read  : status byte -- ALWAYS 0x00 (ready) in this
//     milestone (see the disclosed divergence note below).
//   mod-4 case 1 (#91,#95) write : data_ = value.
//   mod-4 case 1 read  : open-bus 0xFF (the XML declares no separate
//     `type="I"` entry for #91 -- write-only).
//   mod-4 case 2 (#92,#96) : no-op write / 0xFF read.
//   mod-4 case 3 (#93,#97) : PDIR/bidirectional register -- explicitly NOT
//     implemented, matching openMSX's OWN documented scope limit
//     (MSXPrinterPort.cc:57, "0x93 PDIR (BiDi) is not implemented" -- this is
//     openMSX's own gap, not ours).
//
// DISCLOSED DIVERGENCE (A-M18-7): openMSX's *default unplugged* printer port
// (DummyPrinterPortDevice::getStatus, DummyPrinterPortDevice.cc:5-8) reports
// busy=true (not ready) -- the state of NOTHING plugged in. This emulator has
// no separate pluggable/unplugged state (no pluggable-peripheral framework
// exists anywhere in src/); this class IS the built-in port, so it reports
// ready (busy=0) by default, matching the behavior of an openMSX run WITH a
// printer actually plugged in (PrinterCore::getStatus, Printer.cc:54-57,
// returns false=ready). The A/B probe (docs/m18-parity-trace-diff.md) is
// deliberately designed to never read this bit -- this divergence is
// disclosed and intentional, not a bug.
//
// ARCHITECTURAL SIMPLIFICATION (disclosed, mirrors the M15 Ppi8255/
// PpiSlotSelect folding precedent): openMSX splits the port protocol
// (MSXPrinterPort, the bus device) from the byte sink (a separately pluggable
// PrinterCore/ImagePrinterMSX via a Connector/Pluggable framework). This
// emulator has no pluggable-peripheral framework, so PrinterPort folds BOTH
// the port protocol AND the falling-edge byte-capture into one class.
//
// Purely combinational (no clock dependency; A-M18-4's reasoning applies
// identically). reset() sets strobe_=true (idle high), data_=0, and clears
// captured_bytes_ WITHOUT triggering a capture: the fields are set directly
// rather than through the edge-detecting setter, so no edge is observed (this
// mirrors MSXPrinterPort::reset()'s own strobe transition false->true, a
// RISING edge, which is not a capture trigger either).
//
// Debug/introspection convention: captured_bytes() exposes the raw vector
// for tests; a machine-level helper may serialize it deterministically to
// <debug_root>/printer/<filename> (this is a debug aid, NOT a rendered
// "printed page" -- that depth is backlog F2).
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
