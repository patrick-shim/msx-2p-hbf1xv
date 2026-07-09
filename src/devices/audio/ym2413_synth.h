#pragma once

#include <array>
#include <cstdint>

namespace sony_msx::devices::audio {

class Ym2413Opll;

// YM2413 (OPLL) FM waveform-synthesis engine (M31, backlog E1 -- the
// formulaically-derivable subset per docs/m31-planner-package.md §1.2, ratified
// by DEC-0035 from the M28 §2.3(a) finding). Pure, deterministic,
// self-contained: no clock attachment, advanced ONLY by explicit
// advance_cycles() calls (the SccWavetable precedent,
// src/devices/audio/scc_wavetable.h) with the owning Ym2413Opll register file
// passed in as the live register view (§2.6 register-file-as-truth; the M17
// decode accessors are genuinely REUSED -- no duplicated register decode).
//
// GROUNDING (every element -> its independent source, planner §2.1):
//   - log-sin/exp operator tables: COMPUTED AT STATIC CONSTRUCTION from
//     closed-form math (fact-sheet §4 structure: 256-entry 12-bit log-sin,
//     256-entry 10-bit exp, `expTable[logsin + 128*vol + 16*env]`
//     composition). Formulas re-derived from the fact-sheet's structural
//     description; NO table values transcribed from any reference source.
//   - phase generation: fact-sheet §8 (`dP = F-Num * 2^BLOCK * MUL(/2)` into
//     a 19-bit accumulator, top 10 bits index the sine, top 2 mirror);
//     19-bit width is A-M31-4 (a documented choice within §8's "~18-19-bit"
//     hedge, corroborated by fMSX's /2^19 frequency constant); the §3 MUL
//     table is carried in x2-integer form to represent the 0 -> 1/2 entry.
//   - EG decay/release: EXACT per fact-sheet §5 (128 levels over 48 dB,
//     0.375 dB/step; effective 6-bit rate = 4*R + Rks; rates 0-3 no change;
//     60-63 two levels/sample; eg_shift = 13 - rate/4, eg_select = rate & 3
//     with §5's four printed 8-entry select tables; ONE global counter shared
//     by all 18 operators -- deliberately preserving §5's audible
//     "first decay segment after key-on is typically shorter" consequence).
//     Overall timing anchored by §5's closed form
//     `cycles = (rate<60) ? (1<<(14-(rate/4)))*s[rate&3] : 63`,
//     s = {127,102,85,73} -- the unit-test oracle.
//   - feedback: average of the modulator's LAST TWO output samples
//     (fact-sheet §5, die-confirmed 2-deep delay) with FB[2:0] as an
//     exponential shift spanning modulation index 0, pi/16 .. 4pi (§3);
//     property: each FB step exactly DOUBLES the modulation index.
//   - KSR/Rks: Rks = (BLOCK*2 + F-Num[8]) when KSR=1, >>2 when KSR=0 --
//     A-M31-3, the only natural 0-15 construction satisfying §3's stated
//     ranges; semantics cross-checked against openMSX YM2413Okazaki.cc
//     updateRKS (freq >> (KSR ? 8 : 10) over a block<<9|fnum packing --
//     bit-identical to this construction; semantics only, no tables) and
//     against fMSX YM2413.c (which has NO operator-level KSR handling at all,
//     planner §2.8 -- no disagreement possible).
//   - rhythm mode: fact-sheet §6 slot commitment (BD = ch6 normal 2-op FM;
//     TOM = ch8-modulator 1-op; HH/SD on ch7 sharing its frequency; TOM/T-CY
//     sharing ch8's), $0E bit-keying, $36-$38 nibble volumes via the M17
//     accessors, and the DOUBLE-OUTPUT quirk as an exact x2 rhythm gain law
//     (§6 quirk 1 / §7 "output twice ... effectively +6 dB after LPF").
//   - native rate: one native sample tick per 72 master cycles = exactly
//     3.579545 MHz / 72 = 49716 Hz (fact-sheet §7).
//
// ============================================================================
// MANDATORY DISCLOSURE BLOCK -- DOCUMENTED APPROXIMATIONS (planner §2.4/§2.1;
// acceptance criterion 3; NONE of the following is claimed cycle-exact):
//
// 1. ATTACK ENVELOPE (THE §2.4 ATTACK POLICY -- the named E1 remainder, new
//    backlog row E3 item 1): the real chip's per-rate attack step data is,
//    inside this repository, available ONLY in the GPL pre-generated table
//    file `references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii`
//    (per YM2413NukeYKT.cc:106-114), which this project deliberately NEVER
//    OPENED in any form (license-sensitive-scope rule; the M30 SHA1.c
//    precedent). The fact-sheet (§5) says only "Attack rises exponentially",
//    that rates 0-3 never complete and 60-63 are ~instant ("Attack times are
//    separately tabulated" -- without reproducing them). This implementation
//    therefore ships a DISCLOSED EXPONENTIAL-APPROACH APPROXIMATION: at each
//    EG advance event granted by the SAME fact-sheet-grounded
//    global-counter/eg_shift/eg_select mechanism used for decay/release, an
//    attacking operator's attenuation moves toward 0 as
//    `att -= (att >> 2) + 1` per granted step (k = 2, a documented choice:
//    full-scale attack completes in ~14 granted events vs ~254 for the
//    equal-rate decay traversal, preserving "attack much faster than decay at
//    the same rate value" and the exponential shape; the +1 floor guarantees
//    termination), with effective rate 0-3 -> no change (infinite) and
//    60-63 -> instant, both per §5's own statements. The approximation's
//    per-rate durations are NOT hardware-exact and are never claimed to be.
// 2. EG RATE 52-59 ANOMALY (E3 item 2): real hardware deviates from the
//    naive 8-entry-select algorithm at rates 52-59 (fact-sheet §5/§9 names
//    the anomaly; the required 16-entry tables are not reproduced anywhere
//    independently). This implementation uses the naive mechanism's natural
//    generalization (shift saturates at 0; step values double for rates
//    56-59, which reproduces §5's own closed-form totals s={127,102,85,73}
//    at 1-sample event spacing) -- the measured-hardware deviation is NOT
//    modeled.
// 3. VIBRATO / PM TABLE (E3 item 3): the exact 8x8 PM table values and LFO
//    counter widths are not independently available. Implemented as the
//    documented derived law: an 8-step, zero-mean triangle F-Num offset
//    {0,+p/2,+p,+p/2,0,-p/2,-p,-p/2} with peak p = F-Num >> 7 (== +-fnum/128
//    ~= +-13.5 cents, from §5's own ~14-cent depth figure and "indexed by
//    top 3 bits of F-Num"), stepped every 1024 native samples
//    (49716/8192 = 6.069 Hz, within 0.5% of §5's measured ~6.1 Hz).
// 4. AM / TREMOLO RATE: §5's shape and depth are implemented exactly as a
//    0..13-step triangle of 0.375 dB EG steps (13 x 0.375 = 4.875 dB ~= the
//    measured 4.8 dB); the step counter (one step per 512 native samples ->
//    49716/13312 = 3.734 Hz, within 1% of the measured ~3.7 Hz) is a
//    documented counter choice, not a measured hardware divider.
// 5. SD / HH / T-CY SYNTHESIS (E3 item 4): §6 gives "1-bit noise + square,
//    phase-generator override" for SD and only "special synthesis" for
//    HH/T-CY -- no derivable formula exists in the fact-sheet. Disclosed
//    approximations: SD = +-magnitude signed by (ch7 phase top bit XOR
//    noise); HH = +-magnitude signed by the noise bit; T-CY = +-magnitude
//    signed by (ch8 phase top bit XOR ch7 phase top bit -- the §6
//    "optimal 3:1 ch7:ch8 ratio" square mix idea). BD and TOM are full
//    synthesis (2-op FM / 1-op sine) per §6's own structural description.
// 6. NOISE LFSR (E3 item 4): a standard maximal-length 23-bit Fibonacci LFSR
//    (taps x^23 + x^18 + 1, seed 1), clocked once per native sample -- a
//    documented standard choice; the real OPLL's exact LFSR width/taps are
//    not independently available.
// 7. KSL BASE CURVE (E3 item 5): §3 gives the dB/octave rates
//    (0 / 1.5 / 3.0 / 6.0). The per-note base curve is approximated as
//    attenuation = rate * max(0, block*8 + (F-Num >> 6) - 8) / 8 octaves
//    (pitch position in 1/8-octave steps from one octave above absolute
//    bottom); the real chip's exact breakpoint table is not reproduced.
// 8. KEY-ON PHASE RESET (A-M31-5, E3 item 6): key-on resets the operator
//    phase accumulator (and the channel's 2-deep feedback history) and
//    enters attack from the CURRENT attenuation. The fact-sheet does not
//    state phase-reset behaviour either way; the real chip's damp-then-attack
//    key-on sequence is NukeYKT-era knowledge without independent grounding
//    here. Deterministic either way.
// 9. DIGITAL SUMMING (E3 item 7): channels are summed digitally with an
//    exact x2 rhythm gain (a disclosed divergence from the adder-less
//    time-division-multiplexed 9-bit DAC, §7); per-code DAC nonlinearity
//    beyond §7's per-volume peak series (511 codes, missing -256, NMOS
//    crossover) is not modeled. Operator output width (+-2048 linear before
//    the per-channel >>3 presentation scale) is a documented width choice
//    (R-M31-2) -- the §7 measured per-volume peak series is the unit-test
//    oracle pinning the observable 3 dB/step law.
// ============================================================================
class Ym2413Synth {
public:
    // Fact-sheet §7: 3.579545 MHz / 72 = 49716 Hz native sample rate.
    static constexpr std::uint32_t kMasterCyclesPerNativeSample = 72;
    static constexpr int kChannelCount = 9;
    static constexpr int kSlotCount = 18;  // 2 operators per channel
    // 7-bit EG attenuation: 128 levels over 48 dB, 0.375 dB/step (§5).
    static constexpr std::int32_t kEgSilent = 127;

    enum class EgState : std::uint8_t { Idle, Attack, Decay, Sustain, Release };

    // Documented power-on state: all operators Idle at maximum attenuation,
    // phases/counters zero, LFSR seeded to 1, held sample 0 (§2.3 determinism
    // contract; the global EG counter is part of documented state).
    void reset();

    // Recomputes the 18 effective slot key signals from the register file and
    // processes key-on/key-off edges (planner §2.6: melody keys from $20-$28
    // bit4 -- suppressed for ch6-8 while rhythm mode is on, D-M31-3
    // arbitration: registers store what is written, drum key bits simply have
    // no effect while rhythm is off -- drum keys from $0E bits 0-4 gated by
    // $0E bit5). Called by Ym2413Opll on every accepted data write.
    void refresh_keys(const Ym2413Opll& regs);

    // Deterministic advance in 3.58 MHz MASTER cycles: one native FM sample
    // tick (all 18 operators) per 72 cycles; the latest native sample is held
    // (zero-order hold, §2.5 resampling policy). Never touches CPU T-state
    // accounting.
    void advance_cycles(std::uint64_t delta_cycles, const Ym2413Opll& regs);

    // The held latest native sample (ZOH). EXACTLY 0 whenever every operator
    // is idle (never keyed / fully released) -- the S5 zero-YM2413
    // byte-identity oracle depends on this exact-zero guarantee.
    [[nodiscard]] std::int32_t sample() const { return held_sample_; }

    // --- Test introspection (side-effect free; SccWavetable precedent). ---
    [[nodiscard]] std::uint32_t global_counter() const { return counter_; }
    [[nodiscard]] std::uint8_t eg_level(int slot) const;
    [[nodiscard]] EgState eg_state(int slot) const;
    [[nodiscard]] std::uint32_t phase(int slot) const;  // 19-bit accumulator
    [[nodiscard]] std::uint32_t noise_lfsr() const { return lfsr_; }

    // --- Closed-form building blocks exposed for unit-test oracles. ---
    // logsin[i] = round(-log2(sin((i+0.5)*pi/512)) * 256), i in [0,256)
    // (fact-sheet §4 structure; 1 unit = 3/128 dB).
    [[nodiscard]] static std::uint16_t logsin_table(int index);
    // exp2neg[j] = round(2^(-j/256) * 2048), j in [0,256).
    [[nodiscard]] static std::uint16_t exp2neg_table(int index);
    // Linear operator magnitude for a total attenuation T in 3/128 dB units:
    // exp2neg[T & 255] >> (T >> 8)  (0 when shifted to nothing).
    [[nodiscard]] static std::int32_t operator_magnitude(std::uint32_t total_attenuation_units);
    // dP = (F-Num << BLOCK) * MULx2 / 2 (fact-sheet §8 + §3 MUL table).
    [[nodiscard]] static std::uint32_t phase_increment(std::uint16_t fnum, std::uint8_t block,
                                                        std::uint8_t mul_field);
    // Modulator self-feedback phase offset from the SUM of the last two
    // modulator outputs: (fb1 + fb2) >> (8 - FB), 0 when FB == 0 -- i.e. the
    // 2-sample AVERAGE shifted by (7 - FB); each FB step doubles the
    // modulation index (§3/§5).
    [[nodiscard]] static std::int32_t feedback_phase_offset(std::int32_t fb_sum, std::uint8_t fb);
    // Disclosure item 3's derived VIB law (step in [0,8)).
    [[nodiscard]] static std::int32_t vibrato_fnum_offset(int step, std::uint16_t fnum);
    // Disclosure item 7's KSL approximation, in 3/128 dB units.
    [[nodiscard]] static std::uint32_t ksl_attenuation_units(std::uint8_t ksl, std::uint16_t fnum,
                                                              std::uint8_t block);

private:
    struct SlotState {
        std::uint32_t phase = 0;  // 19-bit phase accumulator (A-M31-4)
        EgState state = EgState::Idle;
        std::int32_t level = kEgSilent;  // 0..127 EG attenuation
        bool key = false;                // effective (role-resolved) key signal
    };

    void tick(const Ym2413Opll& regs);
    void apply_key(int slot, bool key, const Ym2413Opll& regs);
    void key_on(int slot, const Ym2413Opll& regs);

    std::array<SlotState, kSlotCount> slots_{};
    // Per-channel modulator 2-deep output delay for feedback (§5).
    std::array<std::int32_t, kChannelCount> fb1_{};
    std::array<std::int32_t, kChannelCount> fb2_{};
    // ONE global counter shared by all 18 operators' EGs and both LFOs (§5).
    std::uint32_t counter_ = 0;
    // Disclosure item 6's 23-bit maximal-length noise LFSR.
    std::uint32_t lfsr_ = 1;
    std::uint64_t cycle_accum_ = 0;
    std::int32_t held_sample_ = 0;
};

}  // namespace sony_msx::devices::audio
