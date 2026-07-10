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

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::audio {

// Source of the PSG I/O ports — R14 (port A, read) and R15 (port B, write).
//
// On the HB-F1XV the joystick pins, keyboard-layout line and cassette input are
// wired to the S1985 but are READ THROUGH the PSG (fact-sheet
// references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §2, "Joystick
// ports"; openMSX references/openmsx-21.0/src/sound/AY8910.cc:542-560 delegates
// port A/B to an AY8910Periphery). The PSG owns R14/R15; the concrete joystick/
// keyboard-layout/cassette state is injected via this contract (X5 ownership:
// peripheral -> PSG, never PSG -> S1985Engine).
class PsgPortSource {
public:
    virtual ~PsgPortSource() = default;

    // R14 read: joystick directions/triggers (bits 0-5, 0=pressed) | keyboard
    // layout (bit 6) | cassette input (bit 7).
    [[nodiscard]] virtual std::uint8_t read_port_a() = 0;

    // R15 write: port select (bit 6), KANA LED (bit 7), STB/enable bits.
    virtual void write_port_b(std::uint8_t value) = 0;
};

// YM2149 PSG numeric/register-accurate sample model (M15-S1, backlog B1).
//
// Ports (fact-sheet §2; openMSX references/openmsx-21.0/src/sound/MSXPSG.cc):
//   #A0 write -> register-address latch (mirrored-register mask 0x0F)
//   #A1 write -> data to the selected register
//   #A2 read  -> data from the selected register
// Dispatch is on port & 0x03 (0=address, 1=data-write, 2=data-read).
//
// This is the DETERMINISTIC NUMERIC model only (DEC-0009 Q4): it produces the
// register file, tone/noise/envelope generator state and a numeric stereo mix.
// It carries NO audio device / output / presentation (deferred to the SDL3
// frontend milestone). Grounding for the generator/envelope semantics:
// references/openmsx-21.0/src/sound/AY8910.cc (behaviour reference; never copied
// — GPL isolation, guardrails).
//
// YM2149 specifics vs AY-3-8910 (fact-sheet §2, "YM2149 vs AY-3-8910"):
//   - 5-bit / 32-step envelope counter.
//   - Register readback returns values AS WRITTEN (the AY masks unused bits to 0;
//     the YM does not). Modelled here: read_register returns the raw store.
class PsgYm2149 final : public core::IoDevice {
public:
    static constexpr std::uint8_t kRegisterCount = 16;
    static constexpr std::uint32_t kCyclesPerGeneratorStep = 16;  // fc = clock/2; step = fc/8

    // R7 mixer/IO-direction bits (fact-sheet §2 "Critical R7 note").
    static constexpr std::uint8_t kPortADirection = 0x40;  // bit6: 1=output; MSX forces 0 (input)
    static constexpr std::uint8_t kPortBDirection = 0x80;  // bit7: 1=output; MSX forces 1 (output)

    void reset();

    // Inject the joystick/keyboard-layout/cassette source that backs R14/R15.
    void attach_port_source(PsgPortSource* source);

    // Bus-independent register access (used by the machine wiring and tests).
    void write_address(std::uint8_t reg);   // #A0
    void write_data(std::uint8_t value);     // #A1
    [[nodiscard]] std::uint8_t read_data();  // #A2
    [[nodiscard]] std::uint8_t selected_register() const;

    // Raw register store (readback view; YM2149 returns as-written).
    [[nodiscard]] std::uint8_t register_value(std::uint8_t reg) const;

    // core::IoDevice on #A0/#A1/#A2 (keyed on port & 0x03).
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // Deterministic generator advance, driven READ-ONLY off the machine clock
    // (X4: never perturbs CPU T-state accounting). delta_cpu_cycles is the number
    // of 3.58 MHz system cycles elapsed since the last advance.
    void advance_cycles(std::uint64_t delta_cpu_cycles);

    struct StereoSample {
        std::int32_t left;
        std::int32_t right;
    };
    // Numeric stereo mix (fact-sheet §2: A=Center, B=Left, C=Right). Each channel
    // contributes its resolved 5-bit amplitude when audible.
    //
    // POINT-SAMPLE API, byte-kept by M34 (docs/m34-planner-package.md §2.3):
    // returns the instantaneous level at the current generator state. The
    // PRODUCTION sample path uses take_integrated_sample() below instead --
    // point-sampling a 3.58 MHz-grain square at the ~44.2 kHz host rate
    // aliases >Nyquist content into the audible band (DEC-0043 Defect A).
    [[nodiscard]] StereoSample sample() const;

    // ------------------------------------------------------------------
    // M34 box-average integration API (DEC-0043 Defect A;
    // docs/m34-planner-package.md §2.3 contract).
    //
    // During advance_cycles(), the chip accumulates per-channel
    //   Σ level_ch(t) × dwell_cycles
    // into three uint64 integrals, where level_ch = channel_audible(ch) ?
    // resolved_amplitude(ch) : 0 is piecewise-constant between generator
    // steps. The dwell walk visits the TRUE chip-step boundaries: a head
    // partial step from cycle_residual_, whole kCyclesPerGeneratorStep(16)-
    // cycle steps, and a tail partial step.
    //
    // BOUNDARY CONVENTION (§2.3.3, load-bearing for every hand oracle): a
    // generator step completing at cycle t changes the level effective AFTER
    // cycle t -- the completing cycle's dwell belongs to the PRE-step level.
    // Consequence: with window_cycles == 16 and a period-1 tone from reset,
    // the integrated sequence is {0, 31, 0, 31, ...} where the point sampler
    // produced {31, 0, 31, 0, ...}.
    //
    // take_integrated_sample(W) returns the exact box average over the
    // accumulated window: left = round(intA+intB, W), right =
    // round(intA+intC, W) (stereo law unchanged, fact-sheet §2), where
    // round is the shared round-half-away-from-zero helper
    // (dwell_rounding.h, §2.3.4), then resets the integrals. W == 0 returns
    // {0, 0} (§2.3.5, the M26 pump idle case). PRECONDITION (§2.3.7): the
    // caller advances exactly window_cycles between takes -- PsgAudioPump
    // guarantees this by construction.
    //
    // FIXED-POINT PROPERTY: a constant summed level L over the whole window
    // integrates to exactly L (dwell_rounding.h) -- silent stays exactly
    // silent, constant stays exactly constant (the §2.5 oracle re-baseline
    // rests on this).
    //
    // WHAT THE BOX FILTER HONESTLY IS (§2.4, disclosed simplification): the
    // exact integer zero-order model of the analog output reconstruction;
    // its frequency response is |sin(pi f T)/(pi f T)| (T = W/3,579,545 s),
    // NOT a brickwall. Exact worst-case AC bounds of the box average for a
    // full-volume tone (levels 0/A, half-period H = 16P, P = max(1, p),
    // W = 81): |avg - A/2| <= A*B(P)/W with B(P) = H/2 when H <= W/2, else
    // B(P) = H - W/2:
    //
    //  p | f (Hz)  | alias@44,192Hz | sinc atten.     | B(P) | per-ch bound  | 2-ch/side bound
    //    |         |                |                 |cycles| (A=31,x400PCM)| + rounding
    //  0/1 111,861 | 20,715         | 0.125 (-18.1dB) | 7.5  | <=1,148       | <=2,500
    //  2 |  55,930 | 11,738         | 0.187 (-14.6dB) | 16   | <=2,449       | <=5,100
    //  3 |  37,287 |  6,905         | 0.178 (-15.0dB) | 7.5  | <=1,148       | <=2,500
    //  4 |  27,965 | 16,227         | 0.460 ( -6.7dB) | 23.5 | <=3,597       | <=7,400
    //  5 |  22,372 | 21,820         | 0.629           | 39.5 | <=6,046       | <=12,300
    // >=6| <=18,643| (passband)     | >=0.73 @18.6kHz | --   | --            | --
    //
    // Passband droop: -0.0004 dB @440 Hz, -0.007 dB @1 kHz, -0.18 dB @5 kHz,
    // -0.74 dB @10 kHz, -1.72 dB @15 kHz.
    //
    // HONEST STATEMENT (package §2.4, verbatim-in-spirit): the box filter
    // fully resolves the actual defect (the p=0/1 ~112 kHz silence idiom:
    // >=18 dB down AND the residual parked at ~20.7 kHz), but periods 2-4
    // receive only PARTIAL suppression (audible-band aliases at up to
    // ~19%/46% of channel amplitude per the table), and the audible band has
    // sinc rolloff. This is a disclosed simplification vs openMSX's true
    // band-limited resampling (references/openmsx-21.0/src/sound/
    // AY8910.cc:38-39,482 native-rate generation + ResampledSoundDevice.hh:
    // 23,29,46-48 / BlipBuffer.hh:1-28 band-limited resampling -- behaviour
    // reference only, never copied); genuine band-limited depth is the named
    // E-series backlog row (agent-protocol/state/deferred-backlog.md).
    // ------------------------------------------------------------------
    [[nodiscard]] StereoSample take_integrated_sample(std::uint64_t window_cycles);

    // Introspection for deterministic tests.
    [[nodiscard]] std::uint16_t tone_period(int channel) const;   // 12-bit
    [[nodiscard]] std::uint8_t noise_period() const;              // 5-bit
    [[nodiscard]] std::uint16_t envelope_period() const;          // 16-bit
    [[nodiscard]] std::uint8_t envelope_volume() const;           // 0..31 (step ^ attack)
    [[nodiscard]] std::uint8_t channel_amplitude(int channel) const;  // resolved 0..31
    [[nodiscard]] bool channel_audible(int channel) const;

    // --- M36 Phase 3 debug snapshot: additive read-only view of the RAW
    //     generator state (tone/noise/envelope counters, LFSR, sub-step
    //     residual, box-average integrals) for a restore-ready snapshot. ONE
    //     struct-returning accessor keeps the header additive-but-compact and
    //     leaves the private Tone/Noise/Envelope structs encapsulated
    //     (docs/m36-phase3-planner-package.md §2.4 item 5). Non-perturbing. ---
    struct GeneratorSnapshot {
        std::array<int, 3> tone_count{};
        std::array<int, 3> tone_output{};
        int noise_count = 0;
        std::uint32_t noise_lfsr = 0;
        int noise_output = 0;
        int envelope_count = 0;
        int envelope_step = 0;
        int envelope_attack = 0;
        bool envelope_hold = false;
        bool envelope_alternate = false;
        bool envelope_holding = false;
        std::uint64_t cycle_residual = 0;
        std::array<std::uint64_t, 3> level_dwell_integral{};
    };
    [[nodiscard]] GeneratorSnapshot generator_snapshot() const;

    // Test-only: single-step the envelope state machine (bypasses the clock
    // divider) to exercise the shape logic deterministically.
    void debug_step_envelope(int steps);

private:
    // Envelope generator — faithful to AY8910.cc Envelope (doSteps/setShape).
    struct Envelope {
        int period = 1;
        int count = 0;
        int step = 0x1F;
        int attack = 0;
        bool hold = false;
        bool alternate = false;
        bool holding = false;

        void reset();
        void set_period(int value);
        void set_shape(unsigned shape);
        [[nodiscard]] int volume() const { return step ^ attack; }
        void do_step();
        void advance(int generator_steps);
    };

    struct Tone {
        int period = 1;
        int count = 0;
        int output = 0;  // square-wave phase (0/1)
        void reset();
        void set_period(int value);
        void advance(int generator_steps);
    };

    struct Noise {
        int period = 1;
        int count = 0;
        std::uint32_t lfsr = 1;  // 17-bit LFSR
        int output = 0;
        void reset();
        void set_period(int value);
        void advance(int generator_steps);
    };

    void write_register(std::uint8_t reg, std::uint8_t value);
    [[nodiscard]] std::uint8_t read_register(std::uint8_t reg);
    [[nodiscard]] std::uint8_t resolved_amplitude(int channel) const;

    std::array<std::uint8_t, kRegisterCount> regs_{};
    std::uint8_t address_ = 0;
    std::array<Tone, 3> tone_{};
    Noise noise_{};
    Envelope envelope_{};
    std::uint64_t cycle_residual_ = 0;
    // M34: per-channel Σ level×dwell accumulators for take_integrated_sample()
    // (§2.3 contract above). Levels are 0..31, so the integral stays far from
    // uint64 overflow for any realistic window.
    std::array<std::uint64_t, 3> level_dwell_integral_{};
    PsgPortSource* port_source_ = nullptr;
};

}  // namespace sony_msx::devices::audio
