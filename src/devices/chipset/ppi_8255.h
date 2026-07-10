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

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/ppi_slot_select.h"
#include "devices/chipset/slot_bus.h"

namespace sony_msx::devices::chipset {

// Source of PPI port B keyboard-row columns (injected keyboard matrix). The
// concrete matrix lives in src/peripherals/ (fact-sheet §3; plan §4.3/§4.5).
class KeyboardRowSource {
public:
    virtual ~KeyboardRowSource() = default;
    // 8 column bits of the selected matrix row, INVERTED (0 = pressed); an idle
    // row reads 0xFF.
    [[nodiscard]] virtual std::uint8_t keyboard_row(int row) const = 0;
};

// Sink for port-C bit-7 (key-click / 1-bit DAC) EDGES (M39-A digitized-voice
// fix). The PPI fires on_click_edge() ONLY when bit 7 actually toggles (the
// openMSX `(prevBits ^ value) & 8` edge trigger, MSXPPI.cc:128-130), stamping
// the change with the current scheduler cycle. Nullable + default-unattached
// -> zero behaviour change (the click DAC is an ADDITIVE mixer source). The
// concrete sink is the machine's ClickDac adapter; Ppi8255 stays pure/testable
// and its #A8 slot-select path is byte-untouched.
class ClickEdgeSink {
public:
    virtual ~ClickEdgeSink() = default;
    // level: true = 0xFF (click asserted), false = 0x80 (idle).
    virtual void on_click_edge(std::uint64_t cycle, bool level) = 0;
};

// Deterministic emulated-cycle source for cycle-stamping the click edges
// above (X-pattern of the machine's RtcClock/FdcClock adapters). Returns the
// scheduler's total cycles READ-ONLY; never perturbs CPU T-state accounting.
class PpiCycleSource {
public:
    virtual ~PpiCycleSource() = default;
    [[nodiscard]] virtual std::uint64_t current_cycle() const = 0;
};

// Full i8255-compatible PPI on ports #A8-#AB (M15-S4, expands the M11
// PpiSlotSelect per change X1). Grounding: fact-sheet §3; openMSX
// references/openmsx-21.0/src/I8255.cc + MSXPPI.cc (behaviour reference, never
// copied — GPL isolation).
//
//   #A8 port A (r/w)  : primary slot select. PRESERVED byte-for-byte by reusing
//                       the M11-verified PpiSlotSelect (X1: #A8 is M11/M13-
//                       critical; it drives SlotBus unconditionally on write and
//                       reads back the latch, exactly as before).
//   #A9 port B (read) : keyboard matrix row (inverted) of the selected row.
//   #AA port C (r/w)  : bits0-3 keyboard row select, bit4 cassette motor, bit5
//                       cassette out, bit6 CAPS LED, bit7 key-click.
//   #AB control       : i8255 bit-set/reset mode (bit7=1 mode set; bit7=0 sets/
//                       resets one port-C bit).
//
// Dispatch is on port & 0x03. The S1985 mirror #AC-#AF -> #A8-#AB is wired in the
// machine IoBus (fact-sheet §10; engine detection IN(#AC)==IN(#A8)).
class Ppi8255 final : public core::IoDevice {
public:
    // i8255 control-register direction bits (I8255.cc:16-24).
    static constexpr std::uint8_t kSetMode = 0x80;
    static constexpr std::uint8_t kDirectionA = 0x10;
    static constexpr std::uint8_t kDirectionB = 0x02;
    static constexpr std::uint8_t kDirectionC0 = 0x01;
    static constexpr std::uint8_t kDirectionC1 = 0x08;
    static constexpr std::uint8_t kBitNr = 0x0E;
    static constexpr std::uint8_t kSetReset = 0x01;
    // Reset = all ports input (I8255.cc:40-41).
    static constexpr std::uint8_t kResetControl =
        kSetMode | kDirectionA | kDirectionB | kDirectionC0 | kDirectionC1;  // 0x9B

    Ppi8255(SlotBus& slot_bus, KeyboardRowSource& keyboard);

    void reset();

    // M39-A: attach the key-click 1-bit-DAC edge sink + its cycle source
    // (both nullable; default-unattached preserves pre-M39 behaviour exactly).
    // A bit-7 toggle fires the sink ONLY when BOTH are attached.
    void attach_click_sink(ClickEdgeSink* sink) { click_sink_ = sink; }
    void attach_cycle_source(PpiCycleSource* source) { cycle_source_ = source; }

    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // Introspection for deterministic tests.
    [[nodiscard]] std::uint8_t control() const;
    [[nodiscard]] std::uint8_t port_c_latch() const;
    [[nodiscard]] int selected_row() const;      // port C bits 0-3
    [[nodiscard]] bool cassette_motor_off() const;  // port C bit4 (1 = off)
    [[nodiscard]] bool caps_led_off() const;        // port C bit6 (1 = off)
    [[nodiscard]] bool key_click() const;           // port C bit7

private:
    [[nodiscard]] std::uint8_t read_port_b() const;
    [[nodiscard]] std::uint8_t read_port_c() const;
    void write_control(std::uint8_t value);
    // M39-A: emit a click edge if bit 7 of port C changed between prev_latch_c
    // and the current latch_c_, and both the sink and cycle source are wired.
    void emit_click_edge_if_toggled(std::uint8_t prev_latch_c);

    PpiSlotSelect port_a_;  // #A8 — reused verbatim (X1 preservation)
    KeyboardRowSource& keyboard_;
    std::uint8_t latch_b_ = 0;
    std::uint8_t latch_c_ = 0;
    std::uint8_t control_ = kResetControl;
    ClickEdgeSink* click_sink_ = nullptr;    // M39-A key-click 1-bit DAC edges
    PpiCycleSource* cycle_source_ = nullptr;  // M39-A edge cycle-stamp source
};

}  // namespace sony_msx::devices::chipset
