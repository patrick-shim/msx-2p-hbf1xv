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

#include "devices/audio/psg_ym2149.h"

#include <algorithm>

#include "devices/audio/dwell_rounding.h"

namespace sony_msx::devices::audio {

namespace {
// Register indices (AY8910.cc:44-59).
constexpr std::uint8_t R_AFINE = 0;
constexpr std::uint8_t R_ACOARSE = 1;
constexpr std::uint8_t R_NOISEPER = 6;
constexpr std::uint8_t R_ENABLE = 7;
constexpr std::uint8_t R_AVOL = 8;
constexpr std::uint8_t R_EFINE = 11;
constexpr std::uint8_t R_ECOARSE = 12;
constexpr std::uint8_t R_ESHAPE = 13;
constexpr std::uint8_t R_PORTA = 14;
constexpr std::uint8_t R_PORTB = 15;
}  // namespace

// --- Envelope (AY8910.cc:364-445) ------------------------------------------

void PsgYm2149::Envelope::reset() {
    period = 1;
    count = 0;
    step = 0x1F;
    attack = 0;
    hold = false;
    alternate = false;
    holding = false;
}

void PsgYm2149::Envelope::set_period(int value) {
    // "twice as fast as AY8910" (AY8910.cc:369-374): period = max(1, 2*value).
    period = std::max(1, 2 * value);
    if (count >= period) {
        count = period - 1;
    }
}

void PsgYm2149::Envelope::set_shape(unsigned shape) {
    // AY8910.cc:382-412.
    attack = (shape & 0x04) ? 0x1F : 0x00;
    if ((shape & 0x08) == 0) {
        hold = true;
        alternate = attack != 0;
    } else {
        hold = (shape & 0x01) != 0;
        alternate = (shape & 0x02) != 0;
    }
    count = 0;
    step = 0x1F;
    holding = false;
}

void PsgYm2149::Envelope::do_step() {
    // AY8910.cc:419-445 doSteps(1).
    if (holding) {
        return;
    }
    step -= 1;
    if (step < 0) {
        if (hold) {
            if (alternate) {
                attack ^= 0x1F;
            }
            holding = true;
            step = 0;
        } else {
            if (alternate && (step & 0x10)) {
                attack ^= 0x1F;
            }
            step &= 0x1F;
        }
    }
}

void PsgYm2149::Envelope::advance(int generator_steps) {
    for (int i = 0; i < generator_steps; ++i) {
        if (holding) {
            return;
        }
        if (++count >= period) {
            count = 0;
            do_step();
        }
    }
}

// --- Tone -------------------------------------------------------------------

void PsgYm2149::Tone::reset() {
    period = 1;
    count = 0;
    output = 0;
}

void PsgYm2149::Tone::set_period(int value) {
    period = std::max(1, value);
    if (count >= period) {
        count = 0;
    }
}

void PsgYm2149::Tone::advance(int generator_steps) {
    for (int i = 0; i < generator_steps; ++i) {
        if (++count >= period) {
            count = 0;
            output ^= 1;
        }
    }
}

// --- Noise ------------------------------------------------------------------

void PsgYm2149::Noise::reset() {
    period = 1;
    count = 0;
    lfsr = 1;
    output = 0;
}

void PsgYm2149::Noise::set_period(int value) {
    // Noise runs at half tone frequency (AY8910.cc:628): 2 * max(1, value).
    period = 2 * std::max(1, value);
    if (count >= period) {
        count = 0;
    }
}

void PsgYm2149::Noise::advance(int generator_steps) {
    for (int i = 0; i < generator_steps; ++i) {
        if (++count >= period) {
            count = 0;
            // 17-bit LFSR, taps at bits 0 and 3 (standard AY/YM noise).
            const std::uint32_t feedback = ((lfsr ^ (lfsr >> 3)) & 1u);
            lfsr = (lfsr >> 1) | (feedback << 16);
            output = static_cast<int>(lfsr & 1u);
        }
    }
}

// --- PsgYm2149 --------------------------------------------------------------

void PsgYm2149::reset() {
    regs_.fill(0);
    address_ = 0;
    cycle_residual_ = 0;
    level_dwell_integral_.fill(0);
    for (auto& t : tone_) {
        t.reset();
    }
    noise_.reset();
    envelope_.reset();
    // Mirror openMSX AY8910::reset (AY8910.cc:525-535): write 0 to all 16 regs.
    for (std::uint8_t reg = 0; reg < kRegisterCount; ++reg) {
        write_register(reg, 0);
    }
}

void PsgYm2149::attach_port_source(PsgPortSource* source) {
    port_source_ = source;
}

void PsgYm2149::write_address(const std::uint8_t reg) {
    // Mirrored registers on the S1985-integrated PSG: mask 0x0F (A-M15-3;
    // MSXPSG.cc mirrored_registers default true).
    address_ = reg & 0x0F;
}

void PsgYm2149::write_data(const std::uint8_t value) {
    write_register(address_, value);
}

std::uint8_t PsgYm2149::read_data() {
    return read_register(address_);
}

std::uint8_t PsgYm2149::selected_register() const {
    return address_;
}

std::uint8_t PsgYm2149::register_value(const std::uint8_t reg) const {
    return regs_[reg & 0x0F];
}

core::BusData PsgYm2149::io_read(const core::BusAddress port) {
    // #A2 (port & 0x03 == 2) is the data-read port; others float (open-bus).
    if ((port & 0x03) == 2) {
        return read_data();
    }
    return 0xFF;
}

void PsgYm2149::io_write(const core::BusAddress port, const core::BusData value) {
    switch (port & 0x03) {
    case 0:
        write_address(value);
        break;
    case 1:
        write_data(value);
        break;
    default:
        break;
    }
}

void PsgYm2149::write_register(const std::uint8_t reg, std::uint8_t value) {
    const std::uint8_t index = reg & 0x0F;

    if (index == R_ENABLE) {
        // MSX forces port A input (bit6=0) and port B output (bit7=1)
        // (fact-sheet §2 "Critical R7 note"; AY8910.cc:598-603 ignorePortDirections).
        value = static_cast<std::uint8_t>((value & ~kPortADirection) | kPortBDirection);
    }

    // YM2149 stores all bits verbatim (readback as written). AY8910.cc:606-608.
    regs_[index] = value;

    switch (index) {
    case R_AFINE:
    case R_ACOARSE:
    case 2:
    case 3:
    case 4:
    case 5: {
        const int channel = index / 2;
        const int fine = regs_[static_cast<std::size_t>(channel * 2)];
        const int coarse = regs_[static_cast<std::size_t>(channel * 2 + 1)] & 0x0F;
        tone_[static_cast<std::size_t>(channel)].set_period(fine + 256 * coarse);
        break;
    }
    case R_NOISEPER:
        noise_.set_period(value & 0x1F);
        break;
    case R_EFINE:
    case R_ECOARSE:
        envelope_.set_period(regs_[R_EFINE] + 256 * regs_[R_ECOARSE]);
        break;
    case R_ESHAPE:
        envelope_.set_shape(value & 0x0F);
        break;
    case R_PORTB:
        // Port B is an output on MSX: forward R15 (port select / KANA) to the
        // injected source (AY8910.cc:671-674).
        if (port_source_ != nullptr) {
            port_source_->write_port_b(value);
        }
        break;
    default:
        break;
    }
}

std::uint8_t PsgYm2149::read_register(const std::uint8_t reg) {
    const std::uint8_t index = reg & 0x0F;
    if (index == R_PORTA) {
        // Port A is input on MSX (bit6 forced 0): return the live source
        // (AY8910.cc:542-546). No source attached -> idle high (all-released).
        if (!(regs_[R_ENABLE] & kPortADirection)) {
            regs_[R_PORTA] = port_source_ != nullptr ? port_source_->read_port_a() : 0xFF;
        }
    }
    // YM2149 returns the raw store (no AY unused-bit masking) — fact-sheet §2.
    return regs_[index];
}

std::uint8_t PsgYm2149::resolved_amplitude(const int channel) const {
    const std::uint8_t amp = regs_[static_cast<std::size_t>(R_AVOL + channel)];
    if (amp & 0x10) {
        // Follow envelope: 5-bit volume (step ^ attack), AY8910.cc:377-379.
        return static_cast<std::uint8_t>(envelope_.volume() & 0x1F);
    }
    // Fixed 4-bit level mapped to the 5-bit scale: 2*level+1 (AY8910.cc volumeTab
    // uses YM2149EnvelopeTab[2*i+1]); level 0 -> 0.
    const int level = amp & 0x0F;
    return level == 0 ? 0 : static_cast<std::uint8_t>(2 * level + 1);
}

bool PsgYm2149::channel_audible(const int channel) const {
    const std::uint8_t enable = regs_[R_ENABLE];
    const bool tone_on = (enable & (1u << channel)) == 0;      // bit=0 -> tone enabled
    const bool noise_on = (enable & (1u << (channel + 3))) == 0;  // bit=0 -> noise enabled
    const bool tone_level = tone_[static_cast<std::size_t>(channel)].output != 0;
    const bool noise_level = noise_.output != 0;
    // Audible when (tone disabled OR tone high) AND (noise disabled OR noise high).
    return (!tone_on || tone_level) && (!noise_on || noise_level);
}

std::uint8_t PsgYm2149::channel_amplitude(const int channel) const {
    return resolved_amplitude(channel);
}

void PsgYm2149::advance_cycles(const std::uint64_t delta_cpu_cycles) {
    // M34 dwell walk (docs/m34-planner-package.md §2.3.1): the level of each
    // channel is piecewise-constant between generator steps, so the exact
    // box integral is a walk over the true step boundaries -- head partial
    // step from cycle_residual_, whole 16-cycle steps, tail partial step.
    // Generator-state evolution is IDENTICAL to the pre-M34 bulk advance
    // (same residual arithmetic, same per-step Tone/Noise/Envelope
    // semantics); only the Σ level×dwell bookkeeping is new. Boundary
    // convention (§2.3.3): the step completing at cycle t changes the level
    // effective AFTER cycle t -- the completing cycle's dwell is accumulated
    // at the PRE-step level.
    std::uint64_t remaining = delta_cpu_cycles;
    while (remaining > 0) {
        const std::uint64_t to_boundary = kCyclesPerGeneratorStep - cycle_residual_;
        const std::uint64_t dwell = remaining < to_boundary ? remaining : to_boundary;
        for (int ch = 0; ch < 3; ++ch) {
            if (channel_audible(ch)) {
                level_dwell_integral_[static_cast<std::size_t>(ch)] +=
                    static_cast<std::uint64_t>(resolved_amplitude(ch)) * dwell;
            }
        }
        cycle_residual_ += dwell;
        remaining -= dwell;
        if (cycle_residual_ == kCyclesPerGeneratorStep) {
            cycle_residual_ = 0;
            for (auto& t : tone_) {
                t.advance(1);
            }
            noise_.advance(1);
            envelope_.advance(1);
        }
    }
}

PsgYm2149::StereoSample PsgYm2149::take_integrated_sample(const std::uint64_t window_cycles) {
    const std::uint64_t int_a = level_dwell_integral_[0];
    const std::uint64_t int_b = level_dwell_integral_[1];
    const std::uint64_t int_c = level_dwell_integral_[2];
    level_dwell_integral_.fill(0);
    if (window_cycles == 0) {
        // §2.3.5 zero-window guard (the M26 pump idle case): silence, no
        // division.
        return {0, 0};
    }
    // Stereo law unchanged (fact-sheet §2: A=Center, B=Left, C=Right);
    // rounding per the shared §2.3.4 helper.
    const auto window = static_cast<std::int64_t>(window_cycles);
    const auto left = static_cast<std::int32_t>(
        round_div_half_away_from_zero(static_cast<std::int64_t>(int_a + int_b), window));
    const auto right = static_cast<std::int32_t>(
        round_div_half_away_from_zero(static_cast<std::int64_t>(int_a + int_c), window));
    return {left, right};
}

PsgYm2149::StereoSample PsgYm2149::sample() const {
    // Per-channel contribution (resolved amplitude when audible).
    std::array<std::int32_t, 3> level{};
    for (int ch = 0; ch < 3; ++ch) {
        level[static_cast<std::size_t>(ch)] =
            channel_audible(ch) ? static_cast<std::int32_t>(resolved_amplitude(ch)) : 0;
    }
    // fact-sheet §2: A=Center (both), B=Left, C=Right.
    const std::int32_t left = level[0] + level[1];
    const std::int32_t right = level[0] + level[2];
    return {left, right};
}

std::uint16_t PsgYm2149::tone_period(const int channel) const {
    const int fine = regs_[static_cast<std::size_t>(channel * 2)];
    const int coarse = regs_[static_cast<std::size_t>(channel * 2 + 1)] & 0x0F;
    return static_cast<std::uint16_t>(fine + 256 * coarse);
}

std::uint8_t PsgYm2149::noise_period() const {
    return static_cast<std::uint8_t>(regs_[R_NOISEPER] & 0x1F);
}

std::uint16_t PsgYm2149::envelope_period() const {
    return static_cast<std::uint16_t>(regs_[R_EFINE] + 256 * regs_[R_ECOARSE]);
}

std::uint8_t PsgYm2149::envelope_volume() const {
    return static_cast<std::uint8_t>(envelope_.volume() & 0x1F);
}

void PsgYm2149::debug_step_envelope(const int steps) {
    for (int i = 0; i < steps; ++i) {
        envelope_.do_step();
    }
}

}  // namespace sony_msx::devices::audio
