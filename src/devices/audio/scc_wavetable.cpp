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

#include "devices/audio/scc_wavetable.h"

#include "devices/audio/dwell_rounding.h"

namespace sony_msx::devices::audio {

namespace {

// All-rotate deform configuration (bits7:6 == 01): the mode in which channel
// 4 rotates at channel 5's period (SCC fact-sheet §6 bit6 row / openMSX
// SCC.cc:257-262, behaviour reference only).
constexpr std::uint8_t kDeformRotateAll = 0x40;

}  // namespace

void SccWavetable::reset() {
    // enen-measured power-on state (SCC fact-sheet §7; A-M29-5 disclosure in
    // the header): openMSX fills the "random-ish mostly 0xFF" measured wave
    // content deterministically with 0xFF -- mirrored here.
    for (auto& w : wave_) {
        w.fill(static_cast<std::int8_t>(-1));  // 0xFF
    }
    org_period_.fill(0);
    period_.fill(0);  // < 9 => channels stopped (fact-sheet §4/§7)
    volume_.fill(15);
    pos_.fill(0);
    count_.fill(0);
    enable_ = 0;
    deform_ = 0;
    rotate_.fill(false);
    read_only_.fill(false);
    deform_cycles_ = 0;
    level_dwell_integral_ = 0;
    // Held outputs refreshed from the power-on wave/volume state.
    for (int ch = 0; ch < kChannels; ++ch) {
        out_[static_cast<std::size_t>(ch)] = adjust(ch);
    }
}

std::int32_t SccWavetable::adjust(const int channel) const {
    // De Schrijder (fact-sheet §5): signed product, low 4 bits dropped --
    // (SampleValue*Vol) AND #7FF0, div 16 == arithmetic shift right by 4
    // (C++20 guarantees arithmetic right shift for signed operands).
    const auto ch = static_cast<std::size_t>(channel);
    return (static_cast<std::int32_t>(wave_[ch][pos_[ch]]) * volume_[ch]) >> 4;
}

std::uint8_t SccWavetable::read_wave(const int channel, const std::uint8_t offset) const {
    const auto ch = static_cast<std::size_t>(channel);
    if (!rotate_[ch]) {
        return static_cast<std::uint8_t>(wave_[ch][offset & 0x1F]);
    }
    // Rotation-shifted readback (fact-sheet §6): shift advances once per
    // (period+1) MASTER cycles off the internal accumulator (A-M29-6; the
    // measured Pazos rate -- see the header's disclosed divergence from
    // openMSX's /32 tick granularity). In all-rotate mode, ch4 (index 3)
    // uses ch5's (index 4) period (plain-SCC quirk).
    const int period_channel =
        (channel == 3 && (deform_ & 0xC0) == kDeformRotateAll) ? 4 : channel;
    const std::uint64_t shift =
        deform_cycles_ / (static_cast<std::uint64_t>(period_[static_cast<std::size_t>(period_channel)]) + 1);
    return static_cast<std::uint8_t>(wave_[ch][(offset + shift) & 0x1F]);
}

std::uint8_t SccWavetable::peek(const std::uint8_t offset) const {
    switch (mode_) {
        case Mode::Real:
            if (offset < 0x80) {
                // 0x00-0x7F: read waveforms ch1..ch4 (fact-sheet §3).
                return read_wave(offset >> 5, offset);
            }
            // 0x80-0x9F freq/vol block: write-only, reads 0xFF.
            // 0xA0-0xDF: no function, 0xFF.
            // 0xE0-0xFF: deformation register, cannot be read back, 0xFF.
            return 0xFF;
    }
    return 0xFF;  // unreachable for a valid mode; keeps -Wreturn-type quiet
}

std::uint8_t SccWavetable::read(const std::uint8_t offset) {
    switch (mode_) {
        case Mode::Real:
            if (offset >= 0xE0) {
                // "Reading Mode Setting Register, is equivalent to write #FF
                // to it" (Pazos, fact-sheet §3/§6).
                set_deform(0xFF);
            }
            break;
    }
    return peek(offset);
}

void SccWavetable::write(const std::uint8_t offset, const std::uint8_t value) {
    switch (mode_) {
        case Mode::Real:
            if (offset < 0x80) {
                write_wave(offset >> 5, offset, value);
            } else if (offset < 0xA0) {
                // 0x80-0x9F: freq/vol/enable block, visible twice (x2 mirror,
                // fact-sheet §3) -- set_freq_vol masks to the 16-byte layout.
                set_freq_vol(offset, value);
            } else if (offset < 0xE0) {
                // 0xA0-0xDF: no function.
            } else {
                set_deform(value);
            }
            break;
    }
}

void SccWavetable::write_wave(const int channel, const std::uint8_t offset, const std::uint8_t value) {
    const auto ch = static_cast<std::size_t>(channel);
    // Rotation modes make the affected wave RAM read-only (fact-sheet §6);
    // the gate is the WRITTEN channel's flag (ch4's write gates the ch5 copy
    // too -- matching SCC.cc:361's single readOnly[channel] guard).
    if (read_only_[ch]) {
        return;
    }
    const std::uint8_t p = offset & 0x1F;
    const auto signed_value = static_cast<std::int8_t>(value);
    wave_[ch][p] = signed_value;
    if (channel == 3) {
        // Plain SCC: ch5 has no wave RAM of its own -- a ch4 write lands in
        // ch5's playback copy too (fact-sheet §3, double-grounded).
        wave_[4][p] = signed_value;
    }
    // NOTE (fact-sheet §4 latching): the held output is NOT refreshed here --
    // a write to the currently-playing byte reaches the output at the next
    // position step (or at a frequency write).
}

void SccWavetable::set_freq_vol(std::uint8_t address, const std::uint8_t value) {
    address &= 0x0F;  // the 16-byte block is visible twice (fact-sheet §3)
    if (address < 0x0A) {
        // 12-bit period assembly; the high write's bits 4-7 are ignored
        // (fact-sheet §3, double-grounded agreement row).
        const auto ch = static_cast<std::size_t>(address / 2);
        std::uint16_t per =
            (address & 1)
                ? static_cast<std::uint16_t>(((value & 0x0F) << 8) | (org_period_[ch] & 0xFF))
                : static_cast<std::uint16_t>((org_period_[ch] & 0xF00) | value);
        org_period_[ch] = per;
        // Deform bits 0/1 mask the EFFECTIVE period at frequency-write time
        // (bit1 wins when both set -- fact-sheet §6 "Additions").
        if (deform_ & 0x02) {
            per &= 0xFF;  // 8-bit mode
        } else if (deform_ & 0x01) {
            per >>= 8;  // 4-bit mode
        }
        period_[ch] = per;
        // NYYRIKKI restart (fact-sheet §4): the current byte restarts.
        count_[ch] = 0;
        if (deform_ & 0x20) {
            // Deform bit5: the whole waveform restarts too, also
            // resynchronising the rotation time base (fact-sheet §6 bit5).
            pos_[ch] = 0;
            deform_cycles_ = 0;
        }
        // The held output refreshes immediately.
        out_[ch] = adjust(static_cast<int>(ch));
    } else if (address < 0x0F) {
        // 4-bit volume; takes effect at the next position step (NYYRIKKI
        // volume-latch, fact-sheet §4) -- deliberately NO out_ refresh here.
        volume_[static_cast<std::size_t>(address - 0x0A)] = value & 0x0F;
    } else {
        // Channel enable, bits 0-4 (bits 5-7 stored but unused).
        enable_ = value;
    }
}

void SccWavetable::set_deform(const std::uint8_t value) {
    // Matches openMSX's no-op-on-same-value + rotation-time-base resync shape
    // (SCC.cc:417-424, behaviour reference only).
    if (value == deform_) {
        return;
    }
    deform_cycles_ = 0;
    apply_deform(value);
}

void SccWavetable::apply_deform(const std::uint8_t value) {
    deform_ = value;
    // Plain SCC: bit7 is honoured (rotate-ch4/5-only; it is the SCC-I that
    // ignores it -- fact-sheet §6 bit7 row; G5 territory, not built here).
    switch (value & 0xC0) {
        case 0x00:
            rotate_.fill(false);
            read_only_.fill(false);
            break;
        case 0x40:
            // Rotate ALL waveforms; all wave RAM read-only.
            rotate_.fill(true);
            read_only_.fill(true);
            break;
        case 0x80:
            // Rotate ch4(/5) only; that RAM read-only.
            for (int i = 0; i < 3; ++i) {
                rotate_[static_cast<std::size_t>(i)] = false;
                read_only_[static_cast<std::size_t>(i)] = false;
            }
            for (int i = 3; i < kChannels; ++i) {
                rotate_[static_cast<std::size_t>(i)] = true;
                read_only_[static_cast<std::size_t>(i)] = true;
            }
            break;
        case 0xC0:
            // bits6+7: only ch1-3 rotate; ALL wave RAM read-only. (Pazos's
            // data-bus noise anomaly in this configuration is documented,
            // NOT emulated -- planner §1.3.)
            for (int i = 0; i < 3; ++i) {
                rotate_[static_cast<std::size_t>(i)] = true;
                read_only_[static_cast<std::size_t>(i)] = true;
            }
            for (int i = 3; i < kChannels; ++i) {
                rotate_[static_cast<std::size_t>(i)] = false;
                read_only_[static_cast<std::size_t>(i)] = true;
            }
            break;
        default:
            break;  // unreachable: all four 2-bit patterns covered
    }
}

void SccWavetable::advance_cycles(const std::uint64_t delta_cycles) {
    deform_cycles_ += delta_cycles;  // rotation time base (A-M29-6)
    for (int ch = 0; ch < kChannels; ++ch) {
        const auto i = static_cast<std::size_t>(ch);
        // M34 (§2.3.2): enable gating of the dwell integral matches sample()
        // exactly -- a disabled channel contributes 0 while its phase keeps
        // running.
        const bool enabled = ((enable_ >> ch) & 1) != 0;
        if (period_[i] <= 8) {
            // NYYRIKKI: a period value under 9 stops the sample counter
            // entirely -- the current output byte is held (fact-sheet §4,
            // §9.2 arbitration; count deliberately NOT accumulated). Held
            // output constant across the whole delta => exact contribution
            // out × delta (M34 §2.3.2).
            if (enabled) {
                level_dwell_integral_ +=
                    static_cast<std::int64_t>(out_[i]) * static_cast<std::int64_t>(delta_cycles);
            }
            continue;
        }
        // M34 dwell walk at the channel's own (period+1)-master-cycle
        // position-step boundaries. Phase-state evolution (count_/pos_/out_)
        // is IDENTICAL to the pre-M34 bulk advance; only the Σ level×dwell
        // bookkeeping is new. Boundary convention (§2.3.3): the completing
        // cycle's dwell belongs to the PRE-step held output.
        const std::uint64_t step_cycles = static_cast<std::uint64_t>(period_[i]) + 1;
        std::uint64_t remaining = delta_cycles;
        while (remaining > 0) {
            const std::uint64_t to_boundary = step_cycles - count_[i];
            const std::uint64_t dwell = remaining < to_boundary ? remaining : to_boundary;
            if (enabled) {
                level_dwell_integral_ +=
                    static_cast<std::int64_t>(out_[i]) * static_cast<std::int64_t>(dwell);
            }
            count_[i] += dwell;
            remaining -= dwell;
            if (count_[i] == step_cycles) {
                count_[i] = 0;
                pos_[i] = static_cast<std::uint8_t>((pos_[i] + 1) % kWaveLength);
                // Held output refreshed at the position step -- this is where
                // pending volume/wave writes become audible (fact-sheet §4).
                // Refreshed for disabled channels too (phase keeps running;
                // sample()/the integral gate the contribution).
                out_[i] = adjust(ch);
            }
        }
    }
}

std::int32_t SccWavetable::take_integrated_sample(const std::uint64_t window_cycles) {
    const std::int64_t integral = level_dwell_integral_;
    level_dwell_integral_ = 0;
    if (window_cycles == 0) {
        // §2.3.5 zero-window guard: silence, no division.
        return 0;
    }
    return static_cast<std::int32_t>(
        round_div_half_away_from_zero(integral, static_cast<std::int64_t>(window_cycles)));
}

std::int32_t SccWavetable::sample() const {
    std::int32_t sum = 0;
    for (int ch = 0; ch < kChannels; ++ch) {
        if ((enable_ >> ch) & 1) {
            sum += out_[static_cast<std::size_t>(ch)];
        }
        // Enable bit clear <=> VolX = 0 <=> contributes 0 (De Schrijder,
        // fact-sheet §5).
    }
    return sum;
}

std::int32_t SccWavetable::amp_out() const {
    return kDcCentre + sample();
}

std::int8_t SccWavetable::wave(const int channel, const int index) const {
    return wave_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(index & 0x1F)];
}

std::uint16_t SccWavetable::period(const int channel) const {
    return period_[static_cast<std::size_t>(channel)];
}

std::uint16_t SccWavetable::org_period(const int channel) const {
    return org_period_[static_cast<std::size_t>(channel)];
}

std::uint8_t SccWavetable::volume(const int channel) const {
    return volume_[static_cast<std::size_t>(channel)];
}

std::uint8_t SccWavetable::position(const int channel) const {
    return pos_[static_cast<std::size_t>(channel)];
}

std::int32_t SccWavetable::held_output(const int channel) const {
    return out_[static_cast<std::size_t>(channel)];
}

}  // namespace sony_msx::devices::audio
