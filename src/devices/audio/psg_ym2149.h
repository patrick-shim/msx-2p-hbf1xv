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
    [[nodiscard]] StereoSample sample() const;

    // Introspection for deterministic tests.
    [[nodiscard]] std::uint16_t tone_period(int channel) const;   // 12-bit
    [[nodiscard]] std::uint8_t noise_period() const;              // 5-bit
    [[nodiscard]] std::uint16_t envelope_period() const;          // 16-bit
    [[nodiscard]] std::uint8_t envelope_volume() const;           // 0..31 (step ^ attack)
    [[nodiscard]] std::uint8_t channel_amplitude(int channel) const;  // resolved 0..31
    [[nodiscard]] bool channel_audible(int channel) const;

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
    PsgPortSource* port_source_ = nullptr;
};

}  // namespace sony_msx::devices::audio
