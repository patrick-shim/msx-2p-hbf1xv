#pragma once

#include <cstdint>

namespace sony_msx::peripherals {

// Deterministic emulated-cycle clock source for Ren-Sha Turbo (M25, X-pattern
// of RtcClockSource/FdcClockSource/CassetteClockSource -- mirrors
// src/devices/rtc/rp5c01.h:14-18, src/devices/fdc/fdc_clock_source.h, and
// src/peripherals/cassette_interface.h:20-24 exactly). The autofire signal
// advances READ-ONLY off the machine cycle clock (Hbf1xvMachine::
// elapsed_cycles() == scheduler total cycles), never the host wall clock;
// CPU T-state accounting is never touched (protecting the M9/M12/M23
// zero-tolerance CPU-timing oracles -- this class is consulted PULL-STYLE
// ONLY, from KeyboardMatrix::keyboard_row()/JoystickPorts::read_port_a(),
// never wired into step_cpu_instruction()/run_cycles()/run_frame()).
class RenshaTurboClockSource {
public:
    virtual ~RenshaTurboClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

// Ren-Sha Turbo autofire (M25, backlog C8 sub-item) -- rapid button-press
// synthesis on the space bar and joystick trigger-A of both ports.
//
// Grounded concretely in real openMSX behavior (behavior reference, never
// copied -- GPL isolation) rather than guessed:
//   - references/openmsx-21.0/src/RenShaTurbo.{hh,cc}: a thin wrapper owning
//     one Autofire circuit.
//   - references/openmsx-21.0/src/Autofire.{hh,cc}: the actual signal
//     generator -- a DynamicClock running at a speed-derived frequency;
//     getSignal(time) returns whether the elapsed tick count is odd (a
//     square wave); speed 0 = disabled (special-cased, not "very slow").
//   - references/openmsx-21.0/src/MSXPPI.cc:90-93 (keyboard row 8 bit 0) and
//     references/openmsx-21.0/src/sound/MSXPSG.cc:90-93 (PSG R14 bit 4):
//     BOTH combine the autofire signal via bitwise-OR, applied AFTER the
//     normal (possibly-pressed) row/port read -- the autofire signal can
//     only ever force a bit from 0->1 (a periodic RELEASE), never force a 0
//     (a press). This is a critical correctness invariant this class's
//     consumers (KeyboardMatrix/JoystickPorts) must preserve exactly.
//   - references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:16-19: a real,
//     per-machine `<RenShaTurbo>` calibration block (min_ints=47,
//     max_ints=221), attributed to a real serial-numbered HB-F1XV unit
//     (line 14: "serialnumber Meits's machine: 225891"). Two small, real,
//     per-machine-measured scalar constants -- safe to cite under the
//     standing license-sensitivity directive (categorically different from
//     a large data table).
//
// The toggle-frequency FORMULA below is independently derived from the
// config data's own documented MEANING (references/openmsx-21.0/src/
// Autofire.hh:66-76: "Number of interrupts ... for 50 periods, measured in
// ntsc mode (which gives 60 interrupts per second)"), NOT transcribed from
// openMSX's own setClock() code shape
// (references/openmsx-21.0/src/Autofire.cc:79-87,
// `(2 * 50 * 60) / (max_ints - (speed * (max_ints - min_ints)) / 100)`):
// 50 on/off periods occur over `ints` VBlank interrupts, i.e. ints/60
// seconds, so freq_hz = 50 / (ints/60) = 3000/ints. A full square-wave cycle
// is 2 toggles, so half_period_cycles = kSystemClockHz / (2*freq_hz) =
// kSystemClockHz * ints / 6000, computed in integer uint64_t arithmetic (no
// floating point -- full determinism). `ints` itself is linearly
// interpolated between kDefaultMaxInts (speed=1, slowest) and
// kDefaultMinInts (speed=100, fastest); speed=0 is specially disabled, not
// "infinitely slow".
//
// Deliberately a CONCRETE class held by direct pointer (not an abstract
// interface like CassetteInputSource) -- unlike CassetteInterface, this
// class has no dependency of its own that would create coupling pressure on
// its consumers, so the extra interface layer buys nothing here.
class RenshaTurbo {
public:
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    // Sony_HB-F1XV.xml:17-18 -- real per-machine calibration (A-M25-5).
    static constexpr unsigned kDefaultMinInts = 47;   // fastest (speed=100)
    static constexpr unsigned kDefaultMaxInts = 221;  // slowest (speed=1)

    // Deterministic power-on idle default: speed=0 (disabled) -- signal() is
    // unconditionally false regardless of the attached clock source,
    // regression-safe.
    void reset();
    void attach_clock_source(RenshaTurboClockSource* source);

    // 0..100, 0 = disabled (mirrors Autofire.hh's own documented range).
    // Out-of-range input is clamped, never undefined.
    void set_speed(int speed);
    [[nodiscard]] int speed() const;

    // Negative-logic autofire pulse at the current cpu_cycles(). false when
    // disabled (speed()==0) or unattached (no clock source) -- both
    // regression-safe defaults.
    [[nodiscard]] bool signal() const;

    // Consumer-facing OR masks (openMSX MSXPPI.cc:90-93 / MSXPSG.cc:90-93):
    // 0x01 (keyboard row8 bit0 = SPACE) / 0x10 (PSG R14 bit4 = trigger A)
    // when signal() is true, else 0x00. Callers OR these into an
    // already-computed read value -- NEVER forces a 0 bit, only ever forces
    // a bit from 0->1 (a periodic RELEASE pulse, matching real hardware).
    [[nodiscard]] std::uint8_t keyboard_row8_or_mask() const;
    [[nodiscard]] std::uint8_t joystick_trigger_a_or_mask() const;

private:
    [[nodiscard]] std::uint64_t half_period_cycles() const;

    int speed_ = 0;
    RenshaTurboClockSource* clock_source_ = nullptr;
};

}  // namespace sony_msx::peripherals
