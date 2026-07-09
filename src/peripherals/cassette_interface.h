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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "devices/chipset/ppi_8255.h"
#include "peripherals/joystick.h"

namespace sony_msx::peripherals {

// Deterministic emulated-cycle clock source for the cassette interface
// (M18-S3, X4 pattern -- mirrors devices::rtc::RtcClockSource,
// src/devices/rtc/rp5c01.h:14-18, and devices::fdc::FdcClockSource,
// src/devices/fdc/fdc_clock_source.h, exactly). The synthetic-tape input
// model advances READ-ONLY off the machine cycle clock, never the host wall
// clock; CPU T-state accounting is never touched (A-M18-12, protecting the
// M9/M12 timing oracles). Consulted PULL-STYLE ONLY -- never wired into
// step_cpu_instruction()/run_cycles()/run_frame() (A-M18-12).
class CassetteClockSource {
public:
    virtual ~CassetteClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

// Cassette interface peripheral (M18-S3, part of backlog C7).
//
// NOT a core::IoDevice (A-M18-8): the machine XML's `<CassettePort/>` is an
// EMPTY element -- no dedicated CPU-visible I/O port exists for the cassette
// on the HB-F1XV (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:21;
// confirmed the only device entry in this XML with no `<io>` child). Motor/
// output are read through the existing PPI port C (fact-sheet §3/§8:
// "controlled via PPI port C bits 4-5"); input is read through the existing
// PSG R14 bit 7 (fact-sheet §8: "read via PSG R14 bit 7"). This class owns NO
// slot/bus-composition knowledge of its own; the machine wires it entirely.
//
// Write path (motor/output) -- pure, on-demand derivation, NO owned state
// (A-M18-9): motor_on() = !ppi_.cassette_motor_off(); output_level() =
// (ppi_.port_c_latch() & 0x20) != 0 (CMO/cassette-write-data pin, S1985
// fact-sheet §3 Port C table). Both read the ALREADY-PUBLIC
// Ppi8255::cassette_motor_off()/port_c_latch() accessors
// (src/devices/chipset/ppi_8255.h:64,62) with ZERO edit to Ppi8255 -- mirrors
// the M13 "MemoryMapperRam is a pure CONSUMER" precedent. Two identical
// instruction streams trivially produce identical values at every read (there
// is nothing to "advance" or desynchronize).
//
// Read path (cassette input, CMI, feeding PSG R14 bit 7 via the
// CassetteInputSource contract this class implements, defined in
// peripherals::JoystickPorts's own header -- joystick.h -- A-M18-10) --
// cycle-driven, NOT wall-clock (A-M18-11): with NO tape loaded,
// cassette_input_high() ALWAYS returns true (idle-high) -- the exact
// externally-observed default of vanilla openMSX's own DummyCassetteDevice
// (DummyCassetteDevice.cc:10-13 returns 32767, converted to `true` by
// CassettePort.cc:103's `return sample >= 0;`), so this milestone changes
// NOTHING observable for anyone not loading a synthetic tape (a regression-
// safe default). With a tape loaded via load_synthetic_tape(bits,
// cycles_per_bit): `start_cycle_` is recorded at load time, and every
// subsequent read computes `index = (clock_source_->cpu_cycles() -
// start_cycle_) / cycles_per_bit`, returning `bits[index]` (clamped to `true`
// -- idle-high -- past the end, deterministically).
//
// Explicitly OUT OF SCOPE (new backlog row F1): real .CAS/.WAV/.TSX tape-file
// decode/encode, realistic FSK/UDF bit modulation and timing, save-to-host-
// file -- this is a deterministic, host-independent, in-memory synthetic
// bitstream contract only (mirrors the M16 synthetic-disk-image precedent).
class CassetteInterface final : public CassetteInputSource {
public:
    explicit CassetteInterface(const devices::chipset::Ppi8255& ppi);

    // Ejects any loaded tape (reverts to the idle-high default) and clears
    // the sample log -- deterministic, no host-file dependency.
    void reset();

    void attach_clock_source(CassetteClockSource* source);

    // Write path -- pure, on-demand derivation from the PPI (A-M18-9).
    [[nodiscard]] bool motor_on() const;
    [[nodiscard]] bool output_level() const;

    // Read path -- deterministic synthetic tape (A-M18-11).
    void load_synthetic_tape(std::vector<bool> bits, std::uint64_t cycles_per_bit);
    void eject_tape();
    [[nodiscard]] bool tape_loaded() const;

    // CassetteInputSource (peripherals::JoystickPorts's contract, A-M18-10).
    [[nodiscard]] bool cassette_input_high() const override;

    // Output/transition capture for tests -- an explicit, caller-driven
    // sampling utility, NOT a production hook (A-M18-12/§2.4 point 3): there
    // is no always-on per-cycle recorder in Hbf1xvMachine's stepping loop.
    struct Sample {
        std::uint64_t at_cycle;
        bool motor_on;
        bool output_level;
    };
    void sample_output(std::uint64_t at_cycle);
    [[nodiscard]] const std::vector<Sample>& samples() const { return samples_; }
    void clear_samples();

private:
    const devices::chipset::Ppi8255& ppi_;
    CassetteClockSource* clock_source_ = nullptr;

    bool tape_loaded_ = false;
    std::vector<bool> tape_bits_;
    std::uint64_t cycles_per_bit_ = 1;
    std::uint64_t start_cycle_ = 0;

    std::vector<Sample> samples_;
};

}  // namespace sony_msx::peripherals
