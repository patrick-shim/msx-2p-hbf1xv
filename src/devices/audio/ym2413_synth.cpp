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

#include "devices/audio/ym2413_synth.h"

#include <algorithm>
#include <cmath>

#include "devices/audio/ym2413_opll.h"

namespace sony_msx::devices::audio {

namespace {

constexpr double kPi = 3.14159265358979323846;

// --- Operator tables, computed from closed-form math (fact-sheet §4
// structure). Never transcribed from any reference
// source -- identical numeric output from an independently-grounded closed
// form is expected and acceptable (DEC-0035).
// Unit tests recompute both tables independently (double-precision math +
// rounding) and check endpoint/monotonicity/symmetry properties.
//
// Attenuation unit: 1 exp-index unit = 3/128 dB (§4's own composition:
// 128 units = one 3 dB volume step, 16 units = one 0.375 dB EG step,
// 32 units = one 0.75 dB TL step, 256 units = 6.0206 dB = one factor of 2).
std::array<std::uint16_t, 256> build_logsin_table() {
    std::array<std::uint16_t, 256> table{};
    for (int i = 0; i < 256; ++i) {
        const double s = std::sin((static_cast<double>(i) + 0.5) * kPi / 512.0);
        table[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(std::lround(-std::log2(s) * 256.0));
    }
    return table;
}

std::array<std::uint16_t, 256> build_exp2neg_table() {
    std::array<std::uint16_t, 256> table{};
    for (int j = 0; j < 256; ++j) {
        table[static_cast<std::size_t>(j)] =
            static_cast<std::uint16_t>(std::lround(std::exp2(-static_cast<double>(j) / 256.0) * 2048.0));
    }
    return table;
}

const std::array<std::uint16_t, 256> kLogSin = build_logsin_table();
const std::array<std::uint16_t, 256> kExp2Neg = build_exp2neg_table();

// §3 MUL table in x2-integer form (0 -> 1/2, 1 -> 1, ..., A -> 10, B -> 10,
// C -> 12, D -> 12, E -> 15, F -> 15) -- a table printed in the fact sheet
// itself, a permitted literal.
constexpr std::array<std::uint32_t, 16> kMulX2 = {1,  2,  4,  6,  8,  10, 12, 14,
                                                  16, 18, 20, 20, 24, 24, 30, 30};

// §5's four printed 8-entry EG select tables (4/8, 5/8, 6/8, 7/8) -- a
// table printed in the fact sheet itself, a permitted literal.
constexpr std::uint8_t kEgSelect[4][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1},
};

// The role a slot plays this tick (melody, or its committed §6 rhythm role).
enum class SlotRole : std::uint8_t {
    MelodyMod,
    MelodyCar,
    BassMod,   // BD modulator (ch6): normal 2-op FM
    BassCar,   // BD carrier (ch6)
    HiHat,     // ch7 modulator: disclosed noise approximation
    Snare,     // ch7 carrier: disclosed noise+square approximation
    Tom,       // ch8 modulator: 1-op sine (full synthesis, §6)
    Cymbal,    // ch8 carrier: disclosed square-mix approximation
};

struct SlotConfig {
    SlotRole role = SlotRole::MelodyMod;
    Ym2413Opll::OperatorPatch op;
    std::uint16_t fnum = 0;
    std::uint8_t block = 0;
    // Total non-envelope, non-logsin attenuation in 3/128 dB units:
    // 32*TL (modulators) or 128*VOL (carriers / drum nibble volumes) + KSL.
    std::uint32_t base_attenuation = 0;
};

// Effective 6-bit rate: 4*R + Rks clamped to 63; R == 0 -> rate 0 (§5's
// "rates 0-3 = no change" -- Table III-7's own rows start at rate 4, i.e.
// R >= 1; a zero 4-bit rate register never advances its EG segment).
int effective_rate(const std::uint8_t rate4, const int rks) {
    if (rate4 == 0) {
        return 0;
    }
    return std::min(63, 4 * static_cast<int>(rate4) + rks);
}

// Rks = (BLOCK*2 + F-Num[8]) when KSR=1 (full 0-15), >> 2 when
// KSR=0 (0-3) -- §3's stated ranges; cross-checked semantics-only against
// YM2413Okazaki.cc updateRKS (freq >> (KSR ? 8 : 10)).
int rks_of(const Ym2413Opll::OperatorPatch& op, const std::uint16_t fnum, const std::uint8_t block) {
    const int full = static_cast<int>(block) * 2 + ((fnum >> 8) & 1);
    return op.ksr ? full : (full >> 2);
}

}  // namespace

std::uint16_t Ym2413Synth::logsin_table(const int index) {
    return kLogSin[static_cast<std::size_t>(index & 0xFF)];
}

std::uint16_t Ym2413Synth::exp2neg_table(const int index) {
    return kExp2Neg[static_cast<std::size_t>(index & 0xFF)];
}

std::int32_t Ym2413Synth::operator_magnitude(const std::uint32_t total_attenuation_units) {
    const std::uint32_t shift = total_attenuation_units >> 8;
    if (shift >= 31) {
        return 0;
    }
    return static_cast<std::int32_t>(kExp2Neg[total_attenuation_units & 0xFF]) >> shift;
}

std::uint32_t Ym2413Synth::phase_increment(const std::uint16_t fnum, const std::uint8_t block,
                                            const std::uint8_t mul_field) {
    // §8: dP = F-Num * 2^BLOCK * MUL (x2 form carries the MUL=0 -> 1/2 case).
    // Resulting pitch: f = dP * 49716 / 2^19 (native rate = 3.579545 MHz/72).
    // fMSX
    // (YM2413.c:193-195) uses `(3125*Frq*(1<<Oct))>>15` == a 50000 Hz
    // rounding of the native rate (~10 cents sharp) -- resolved in favour of
    // the fact-sheet/openMSX 49716 Hz; fMSX independently corroborates the
    // /2^19 accumulator scaling. (DEC-0030)
    return ((static_cast<std::uint32_t>(fnum) << block) * kMulX2[mul_field & 0x0F]) >> 1;
}

std::int32_t Ym2413Synth::feedback_phase_offset(const std::int32_t fb_sum, const std::uint8_t fb) {
    if ((fb & 0x07) == 0) {
        return 0;  // §3: FB=0 -> modulation index 0
    }
    // avg = fb_sum/2, shifted by (7 - FB): FB=7 -> +-full-scale (4pi at the
    // 1024-units-per-period sine index), FB=1 -> pi/16. Each step doubles.
    return fb_sum >> (8 - (fb & 0x07));
}

std::int32_t Ym2413Synth::vibrato_fnum_offset(const int step, const std::uint16_t fnum) {
    // Disclosure item 3: 8-step zero-mean triangle, peak +-(F-Num >> 7).
    const std::int32_t p = fnum >> 7;
    const std::int32_t h = p >> 1;
    switch (step & 7) {
        case 0: return 0;
        case 1: return h;
        case 2: return p;
        case 3: return h;
        case 4: return 0;
        case 5: return -h;
        case 6: return -p;
        default: return -h;
    }
}

std::uint32_t Ym2413Synth::ksl_attenuation_units(const std::uint8_t ksl, const std::uint16_t fnum,
                                                  const std::uint8_t block) {
    // §3: KSL[1:0] 00 -> 0, 01 -> 3.0, 10 -> 1.5, 11 -> 6.0 dB/oct, i.e.
    // 0 / 128 / 64 / 256 units per octave. Disclosure item 7's base-curve
    // approximation: pitch position in 1/8 octaves = block*8 + fnum[8:6],
    // referenced one octave above absolute bottom.
    static constexpr std::array<std::uint32_t, 4> kUnitsPerOctave = {0, 128, 64, 256};
    const std::uint32_t units = kUnitsPerOctave[ksl & 0x03];
    if (units == 0) {
        return 0;
    }
    const int x = static_cast<int>(block) * 8 + ((fnum >> 6) & 0x07);
    if (x <= 8) {
        return 0;
    }
    return (units * static_cast<std::uint32_t>(x - 8)) >> 3;
}

void Ym2413Synth::reset() {
    slots_.fill(SlotState{});
    fb1_.fill(0);
    fb2_.fill(0);
    counter_ = 0;
    lfsr_ = 1;
    cycle_accum_ = 0;
    held_sample_ = 0;
}

std::uint8_t Ym2413Synth::eg_level(const int slot) const {
    return static_cast<std::uint8_t>(slots_[static_cast<std::size_t>(slot)].level);
}

Ym2413Synth::EgState Ym2413Synth::eg_state(const int slot) const {
    return slots_[static_cast<std::size_t>(slot)].state;
}

std::uint32_t Ym2413Synth::phase(const int slot) const {
    return slots_[static_cast<std::size_t>(slot)].phase;
}

namespace {

// Resolves a slot's live configuration from the register file (the
// Ym2413Opll decode accessors as the register view -- instrument changes on
// a keyed channel take effect immediately, register-file-as-truth).
SlotConfig resolve_slot(const Ym2413Opll& regs, const int slot) {
    const int channel = slot / 2;
    const bool is_carrier = (slot & 1) != 0;
    const bool rhythm = regs.rhythm_enabled();

    SlotConfig cfg;
    cfg.fnum = regs.f_number(channel);
    cfg.block = regs.block(channel);

    if (rhythm && channel >= 6) {
        if (channel == 6) {
            const Ym2413Opll::Patch patch = Ym2413Opll::rhythm_bd_patch();
            cfg.role = is_carrier ? SlotRole::BassCar : SlotRole::BassMod;
            cfg.op = is_carrier ? patch.carrier : patch.modulator;
            cfg.base_attenuation = is_carrier
                                       ? 128u * regs.bd_volume()
                                       : 32u * cfg.op.total_level;
        } else if (channel == 7) {
            const Ym2413Opll::Patch patch = Ym2413Opll::rhythm_sd_hh_patch();
            cfg.role = is_carrier ? SlotRole::Snare : SlotRole::HiHat;
            cfg.op = is_carrier ? patch.carrier : patch.modulator;
            cfg.base_attenuation =
                128u * (is_carrier ? regs.sd_volume() : regs.hh_volume()) + 32u * cfg.op.total_level;
        } else {
            const Ym2413Opll::Patch patch = Ym2413Opll::rhythm_tom_cym_patch();
            cfg.role = is_carrier ? SlotRole::Cymbal : SlotRole::Tom;
            cfg.op = is_carrier ? patch.carrier : patch.modulator;
            cfg.base_attenuation =
                128u * (is_carrier ? regs.cym_volume() : regs.tom_volume()) + 32u * cfg.op.total_level;
        }
    } else {
        const Ym2413Opll::Patch patch = regs.patch(channel);
        cfg.role = is_carrier ? SlotRole::MelodyCar : SlotRole::MelodyMod;
        cfg.op = is_carrier ? patch.carrier : patch.modulator;
        cfg.base_attenuation =
            is_carrier ? 128u * regs.volume(channel) : 32u * cfg.op.total_level;
    }
    cfg.base_attenuation += Ym2413Synth::ksl_attenuation_units(cfg.op.ksl, cfg.fnum, cfg.block);
    return cfg;
}

}  // namespace

void Ym2413Synth::key_on(const int slot, const Ym2413Opll& regs) {
    SlotState& s = slots_[static_cast<std::size_t>(slot)];
    // Disclosure item 8: phase reset + attack from the current
    // attenuation; the channel feedback history clears with its modulator.
    s.phase = 0;
    if ((slot & 1) == 0) {
        fb1_[static_cast<std::size_t>(slot / 2)] = 0;
        fb2_[static_cast<std::size_t>(slot / 2)] = 0;
    }
    s.state = EgState::Attack;
    const SlotConfig cfg = resolve_slot(regs, slot);
    const int rate = effective_rate(cfg.op.attack_rate, rks_of(cfg.op, cfg.fnum, cfg.block));
    if (rate >= 60 || s.level == 0) {
        // §5: attack rates 60-63 are ~instant; an already-fully-risen
        // envelope has no attack segment left.
        s.level = 0;
        s.state = EgState::Decay;
    }
}

void Ym2413Synth::apply_key(const int slot, const bool key, const Ym2413Opll& regs) {
    SlotState& s = slots_[static_cast<std::size_t>(slot)];
    if (key && !s.key) {
        key_on(slot, regs);
    } else if (!key && s.key) {
        if (s.state != EgState::Idle) {
            s.state = EgState::Release;
        }
    }
    s.key = key;
}

void Ym2413Synth::refresh_keys(const Ym2413Opll& regs) {
    const bool rhythm = regs.rhythm_enabled();
    for (int channel = 0; channel < kChannelCount; ++channel) {
        bool mod_key;
        bool car_key;
        if (rhythm && channel >= 6) {
            // §6 slot commitment; $0E bits key the drums (effective only
            // while bit5 is set).
            if (channel == 6) {
                mod_key = car_key = regs.bd_key();
            } else if (channel == 7) {
                mod_key = regs.hh_key();
                car_key = regs.sd_key();
            } else {
                mod_key = regs.tom_key();
                car_key = regs.cym_key();
            }
        } else {
            // Melody: $20-$28 bit4 keys both operators. While rhythm mode is
            // on, ch6-8 melody keys are suppressed (the channels are
            // committed to percussion, §6 quirk 2).
            const bool melody_key = regs.key_on(channel) && !(rhythm && channel >= 6);
            mod_key = car_key = melody_key;
        }
        apply_key(channel * 2, mod_key, regs);
        apply_key(channel * 2 + 1, car_key, regs);
    }
}

void Ym2413Synth::advance_cycles(const std::uint64_t delta_cycles, const Ym2413Opll& regs) {
    cycle_accum_ += delta_cycles;
    while (cycle_accum_ >= kMasterCyclesPerNativeSample) {
        cycle_accum_ -= kMasterCyclesPerNativeSample;
        tick(regs);
    }
}

namespace {

// Per-tick evaluation context shared by the EG/output helpers below.
struct TickContext {
    std::uint32_t counter = 0;
    std::uint32_t am_units = 0;   // AM LFO attenuation contribution
    int vib_step = 0;             // VIB LFO step 0..7
    bool noise_bit = false;
};

// §5's global-counter rate mechanism, generalized exactly as the header's
// disclosure item 2 documents: shift = max(0, 13 - rate/4); step values
// double once for rates 56-59; rates 60-63 advance 2 levels every sample.
int eg_granted_steps(const int rate, const std::uint32_t counter) {
    if (rate < 4) {
        return 0;
    }
    if (rate >= 60) {
        return 2;
    }
    const int r4 = rate >> 2;
    const int sel = rate & 3;
    const int shift = (r4 <= 13) ? (13 - r4) : 0;
    const int mult = (r4 >= 14) ? (r4 - 13) : 0;
    if ((counter & ((1u << shift) - 1u)) != 0) {
        return 0;
    }
    return kEgSelect[sel][(counter >> shift) & 7] << mult;
}

}  // namespace

void Ym2413Synth::tick(const Ym2413Opll& regs) {
    TickContext ctx;
    ctx.counter = counter_;
    // AM LFO (disclosure item 4): 26-step triangle 0..13, one step per 512
    // native samples; contribution in 0.375 dB EG steps (16 units each).
    const std::uint32_t am_step = (counter_ / 512u) % 26u;
    const std::uint32_t am_tri = am_step < 13u ? am_step : 26u - am_step;
    ctx.am_units = 16u * am_tri;
    // VIB LFO (disclosure item 3): 8 steps, one per 1024 native samples.
    ctx.vib_step = static_cast<int>((counter_ / 1024u) % 8u);
    ctx.noise_bit = (lfsr_ & 1u) != 0;

    const bool rhythm = regs.rhythm_enabled();

    // Resolve all 18 slot configurations once for this tick (live register
    // view).
    std::array<SlotConfig, kSlotCount> cfgs;
    for (int slot = 0; slot < kSlotCount; ++slot) {
        cfgs[static_cast<std::size_t>(slot)] = resolve_slot(regs, slot);
    }

    // --- 1. EG advance for all 18 operators off the ONE global counter. ---
    for (int slot = 0; slot < kSlotCount; ++slot) {
        SlotState& s = slots_[static_cast<std::size_t>(slot)];
        if (s.state == EgState::Idle) {
            continue;
        }
        const SlotConfig& cfg = cfgs[static_cast<std::size_t>(slot)];
        const int rks = rks_of(cfg.op, cfg.fnum, cfg.block);
        std::uint8_t rate4 = 0;
        switch (s.state) {
            case EgState::Attack:
                rate4 = cfg.op.attack_rate;
                break;
            case EgState::Decay:
                rate4 = cfg.op.decay_rate;
                break;
            case EgState::Sustain:
                // EG-TYP=1 holds at SL while keyed; EG-TYP=0 (percussive)
                // keeps decaying at RR while keyed (§3).
                rate4 = cfg.op.sustained_eg ? 0 : cfg.op.release_rate;
                break;
            case EgState::Release:
                // §3: SUS=1 forces the release rate to 5 at key-off.
                rate4 = regs.sustain(slot / 2) ? 5 : cfg.op.release_rate;
                break;
            case EgState::Idle:
                break;
        }
        const int rate = effective_rate(rate4, rks);
        if (s.state == EgState::Attack) {
            if (rate >= 60) {
                s.level = 0;
                s.state = EgState::Decay;
            } else {
                int steps = eg_granted_steps(rate, ctx.counter);
                while (steps-- > 0 && s.level > 0) {
                    // THE ATTACK APPROXIMATION (header disclosure item
                    // 1): exponential approach, k = 2, +1 termination floor.
                    s.level -= (s.level >> 2) + 1;
                }
                if (s.level <= 0) {
                    s.level = 0;
                    s.state = EgState::Decay;
                }
            }
        } else {
            const int steps = eg_granted_steps(rate, ctx.counter);
            if (steps > 0) {
                s.level = std::min<std::int32_t>(s.level + steps, kEgSilent);
                if (s.state == EgState::Decay) {
                    // SL boundary: 3 dB/step from top = 8 EG levels per SL
                    // step (§3/§5).
                    const std::int32_t sl_boundary = 8 * static_cast<std::int32_t>(cfg.op.sustain_level);
                    if (s.level >= sl_boundary) {
                        s.state = EgState::Sustain;
                    }
                }
                if (s.level >= kEgSilent) {
                    s.level = kEgSilent;
                    s.state = EgState::Idle;
                }
            }
        }
    }

    // --- 2. Operator evaluation + channel summing. ---
    const auto eval_sine = [&](const int slot, const SlotConfig& cfg,
                               const std::int32_t pm_units) -> std::int32_t {
        const SlotState& s = slots_[static_cast<std::size_t>(slot)];
        if (s.state == EgState::Idle || s.level >= kEgSilent) {
            return 0;  // exact-zero guarantee for silent operators
        }
        const std::uint32_t idx =
            (static_cast<std::uint32_t>(static_cast<std::int32_t>(s.phase >> 9) + pm_units)) & 0x3FFu;
        const bool negative = (idx & 0x200u) != 0;
        if (negative && cfg.op.half_sine) {
            return 0;  // DC/DM half-rectified sine: negative half is silence
        }
        const std::uint32_t table_index = (idx & 0x100u) ? (255u - (idx & 0xFFu)) : (idx & 0xFFu);
        const std::uint32_t attenuation = kLogSin[table_index] +
                                          16u * static_cast<std::uint32_t>(s.level) +
                                          cfg.base_attenuation +
                                          (cfg.op.am ? ctx.am_units : 0u);
        const std::int32_t magnitude = operator_magnitude(attenuation);
        return negative ? -magnitude : magnitude;
    };

    // Noise-role magnitude (no logsin term): envelope + volume attenuation
    // only (header disclosure item 5).
    const auto noise_magnitude = [&](const int slot, const SlotConfig& cfg) -> std::int32_t {
        const SlotState& s = slots_[static_cast<std::size_t>(slot)];
        if (s.state == EgState::Idle || s.level >= kEgSilent) {
            return 0;
        }
        const std::uint32_t attenuation = 16u * static_cast<std::uint32_t>(s.level) +
                                          cfg.base_attenuation +
                                          (cfg.op.am ? ctx.am_units : 0u);
        return operator_magnitude(attenuation);
    };

    const auto phase_top_bit = [&](const int slot) -> bool {
        return (slots_[static_cast<std::size_t>(slot)].phase >> 18) & 1u;
    };

    std::int32_t acc = 0;
    const int melody_limit = rhythm ? 6 : kChannelCount;
    for (int channel = 0; channel < melody_limit; ++channel) {
        const int mod_slot = channel * 2;
        const int car_slot = channel * 2 + 1;
        const SlotConfig& mod_cfg = cfgs[static_cast<std::size_t>(mod_slot)];
        const SlotConfig& car_cfg = cfgs[static_cast<std::size_t>(car_slot)];
        const std::int32_t fb_pm = feedback_phase_offset(
            fb1_[static_cast<std::size_t>(channel)] + fb2_[static_cast<std::size_t>(channel)],
            mod_cfg.op.feedback);
        const std::int32_t mod_out = eval_sine(mod_slot, mod_cfg, fb_pm);
        fb2_[static_cast<std::size_t>(channel)] = fb1_[static_cast<std::size_t>(channel)];
        fb1_[static_cast<std::size_t>(channel)] = mod_out;
        const std::int32_t car_out = eval_sine(car_slot, car_cfg, mod_out);
        acc += car_out >> 3;  // per-channel presentation scale (peak ~ +-256)
    }

    if (rhythm) {
        // BD: normal 2-op FM on ch6 (§6). x2 double-output quirk applies to
        // every rhythm contribution (§6 quirk 1 / §7).
        {
            const SlotConfig& mod_cfg = cfgs[12];
            const SlotConfig& car_cfg = cfgs[13];
            const std::int32_t fb_pm =
                feedback_phase_offset(fb1_[6] + fb2_[6], mod_cfg.op.feedback);
            const std::int32_t mod_out = eval_sine(12, mod_cfg, fb_pm);
            fb2_[6] = fb1_[6];
            fb1_[6] = mod_out;
            const std::int32_t car_out = eval_sine(13, car_cfg, mod_out);
            acc += 2 * (car_out >> 3);
        }
        // HH (ch7 modulator): +-magnitude signed by the noise bit.
        {
            const std::int32_t mag = noise_magnitude(14, cfgs[14]);
            acc += 2 * ((ctx.noise_bit ? mag : -mag) >> 3);
        }
        // SD (ch7 carrier): 1-bit noise + square (ch7 phase top bit XOR
        // noise), §6's own naming, disclosed approximation.
        {
            const std::int32_t mag = noise_magnitude(15, cfgs[15]);
            acc += 2 * (((phase_top_bit(15) != ctx.noise_bit) ? mag : -mag) >> 3);
        }
        // TOM (ch8 modulator): essentially 1-op (§6) -- full sine synthesis.
        {
            const std::int32_t out = eval_sine(16, cfgs[16], 0);
            acc += 2 * (out >> 3);
        }
        // T-CY (ch8 carrier): square mix of the shared ch8/ch7 phase
        // generators (§6's 3:1 ratio note), disclosed approximation.
        {
            const std::int32_t mag = noise_magnitude(17, cfgs[17]);
            acc += 2 * (((phase_top_bit(17) != phase_top_bit(15)) ? mag : -mag) >> 3);
        }
    }

    held_sample_ = acc;

    // --- 3. Phase generators advance (all 18 slots free-run, §8). ---
    for (int slot = 0; slot < kSlotCount; ++slot) {
        SlotState& s = slots_[static_cast<std::size_t>(slot)];
        const SlotConfig& cfg = cfgs[static_cast<std::size_t>(slot)];
        std::int32_t fnum_eff = cfg.fnum;
        if (cfg.op.vib) {
            fnum_eff += vibrato_fnum_offset(ctx.vib_step, cfg.fnum);
        }
        const std::uint32_t inc = phase_increment(static_cast<std::uint16_t>(fnum_eff), cfg.block,
                                                  cfg.op.multiple);
        s.phase = (s.phase + inc) & 0x7FFFFu;
    }

    // --- 4. Noise LFSR clock (disclosure item 6) + the global counter. ---
    const std::uint32_t fb_bit = (lfsr_ ^ (lfsr_ >> 5)) & 1u;
    lfsr_ = (lfsr_ >> 1) | (fb_bit << 22);
    ++counter_;
}

}  // namespace sony_msx::devices::audio
