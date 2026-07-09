// Suite: Devices_Ym2413SynthOperator_Unit (M31-S2, backlog E1,
// docs/m31-planner-package.md §3-S2)
//
// Oracles (license-safe, acceptance criterion 1):
//   - PITCH: f = F-Num * 2^BLOCK * MUL_eff * 49716 / 2^19 (fact-sheet §8;
//     the period of a generated carrier-only tone is measured in native
//     samples and compared against 2^19 / dP). D-M31-1 note: fMSX
//     (YM2413.c:193-195) uses a 50000 Hz rounding of the 49716 Hz native
//     rate -- arbitrated to the fact-sheet/openMSX 3.579545 MHz / 72 value
//     (planner §2.8); fMSX independently corroborates the /2^19 scaling.
//   - VOLUME: the fact-sheet §7 andete-measured per-volume peak series
//     {255,180,127,90,63,45,31,22,15,11,7,5,3,2,1,1} -- an INDEPENDENT
//     hardware measurement used as a test oracle for the 3 dB/step law.
//   - FEEDBACK: §3's exponential ladder -- each FB step exactly doubles the
//     modulation index (property-testable without any golden table).
//   - HALF-SINE: §3 DC/DM -- the negative half is silence.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "devices/audio/ym2413_opll.h"
#include "devices/audio/ym2413_synth.h"

namespace {

using sony_msx::devices::audio::Ym2413Opll;
using sony_msx::devices::audio::Ym2413Synth;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

void write_reg(Ym2413Opll& opll, const std::uint8_t addr, const std::uint8_t value) {
    opll.write_address(addr);
    opll.write_data(value);
}

// Programs a carrier-only user-patch tone on channel 0: the modulator's
// AR = 0 keeps its EG at maximum attenuation forever (exact-zero output),
// the carrier attacks instantly (AR = 15) and holds at 0 dB (sustained
// EG-TYP, DR irrelevant, SL = 0).
void program_carrier_only(Ym2413Opll& opll, const std::uint16_t fnum, const std::uint8_t block,
                          const std::uint8_t mul_field, const std::uint8_t volume,
                          const bool half_sine) {
    opll.reset();
    write_reg(opll, 0x00, 0x00);  // mod: MUL=0, no flags
    write_reg(opll, 0x01, static_cast<std::uint8_t>(0x20 | (mul_field & 0x0F)));  // car: EG-TYP=1
    write_reg(opll, 0x02, 0x3F);  // mod TL = max (defence in depth; mod EG idle anyway)
    write_reg(opll, 0x03, half_sine ? 0x10 : 0x00);  // carrier DC bit4 (§3 layout)
    write_reg(opll, 0x04, 0x00);  // mod AR=0 (never attacks), DR=0
    write_reg(opll, 0x05, 0xF0);  // car AR=15 (instant), DR=0
    write_reg(opll, 0x06, 0x00);
    write_reg(opll, 0x07, 0x00);  // car SL=0, RR=0
    write_reg(opll, 0x10, static_cast<std::uint8_t>(fnum & 0xFF));
    write_reg(opll, 0x30, volume & 0x0F);  // instrument 0 (user), volume
    write_reg(opll, 0x20, static_cast<std::uint8_t>(0x10 | ((block & 7) << 1) | ((fnum >> 8) & 1)));
}

std::vector<std::int32_t> run_native_samples(Ym2413Opll& opll, const int count) {
    std::vector<std::int32_t> samples;
    samples.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        opll.advance_cycles(Ym2413Synth::kMasterCyclesPerNativeSample);
        samples.push_back(opll.fm_sample());
    }
    return samples;
}

// Counts native samples spanning `cycles` rising sign transitions
// (negative -> positive), starting from the first observed transition.
// Tracks the LAST NON-ZERO sign so that near-crossing samples quantized to
// exactly 0 by the log-domain floor do not hide a transition.
long measure_span(const std::vector<std::int32_t>& samples, const int cycles) {
    int seen = 0;
    long first = -1;
    int last_sign = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (samples[i] == 0) {
            continue;
        }
        const int sign = samples[i] > 0 ? 1 : -1;
        if (last_sign < 0 && sign > 0) {
            if (seen == 0) {
                first = static_cast<long>(i);
            }
            if (seen == cycles) {
                return static_cast<long>(i) - first;
            }
            ++seen;
        }
        last_sign = sign;
    }
    return -1;
}

}  // namespace

int main() {
    // --- 1. PITCH ORACLE across representative (F-Num, BLOCK, MUL) triples,
    //     including MUL = 0 (the x1/2 entry). 32 measured periods must match
    //     32 * 2^19 / dP within crossing quantization (+-1 sample). ---
    {
        struct PitchCase {
            std::uint16_t fnum;
            std::uint8_t block;
            std::uint8_t mul;
            const char* name;
        };
        const PitchCase cases[] = {
            {256, 4, 1, "Pitch_Fnum256_Block4_Mul1_ExactPeriod128"},
            {256, 4, 0, "Pitch_MulField0_HalfMultiple_ExactPeriod256"},
            {343, 3, 5, "Pitch_Fnum343_Block3_Mul5_NonIntegerPeriod"},
            {511, 2, 2, "Pitch_Fnum511_Block2_Mul2"},
            {172, 5, 3, "Pitch_Fnum172_Block5_Mul3"},
        };
        for (const PitchCase& c : cases) {
            Ym2413Opll opll;
            program_carrier_only(opll, c.fnum, c.block, c.mul, 0, false);
            const std::uint32_t inc = Ym2413Synth::phase_increment(c.fnum, c.block, c.mul);
            const double expected_span = 32.0 * 524288.0 / static_cast<double>(inc);
            const std::vector<std::int32_t> samples =
                run_native_samples(opll, static_cast<int>(expected_span * 1.2) + 64);
            const long span = measure_span(samples, 32);
            expect(span > 0 && std::abs(static_cast<double>(span) - expected_span) <= 1.5, c.name);
        }
    }

    // --- 2. VOLUME LAW vs the §7 measured-peaks oracle. Per-channel output
    //     scale: carrier magnitude (max 2048) >> 3 -> peak ~ 256 at vol 0,
    //     compared against the measured series normalized to that scale. ---
    {
        // Fact-sheet §7 (andete hardware measurement) -- TEST ORACLE ONLY,
        // a fact-sheet-printed series (permitted literal, criterion 2).
        const int kMeasuredPeaks[16] = {255, 180, 127, 90, 63, 45, 31, 22,
                                        15,  11,  7,   5,  3,  2,  1,  1};
        std::int32_t peaks[16] = {};
        for (int vol = 0; vol < 16; ++vol) {
            Ym2413Opll opll;
            program_carrier_only(opll, 256, 4, 1, static_cast<std::uint8_t>(vol), false);
            const std::vector<std::int32_t> samples = run_native_samples(opll, 300);
            std::int32_t peak = 0;
            for (const std::int32_t s : samples) {
                peak = std::max(peak, std::abs(s));
            }
            peaks[vol] = peak;
        }
        // (a) absolute match to the normalized measured series within
        //     quantization tolerance (+-2 output LSBs).
        bool series_match = true;
        const double scale = static_cast<double>(peaks[0]) / 255.0;
        for (int vol = 0; vol < 16; ++vol) {
            const double expected = kMeasuredPeaks[vol] * scale;
            if (std::abs(static_cast<double>(peaks[vol]) - expected) > 2.0) {
                series_match = false;
            }
        }
        expect(series_match, "VolumeLaw_PeaksMatchMeasuredSeries_WithinQuantization");
        // (b) ratio law: each step ~ x1/sqrt(2) while quantization is small.
        bool ratios_ok = true;
        for (int vol = 0; vol < 7; ++vol) {
            const double ratio = static_cast<double>(peaks[vol]) / static_cast<double>(peaks[vol + 1]);
            if (ratio < 1.33 || ratio > 1.52) {
                ratios_ok = false;
            }
        }
        expect(ratios_ok, "VolumeLaw_3dBPerStep_RatioApproxSqrt2");
        // (c) monotonic non-increasing across the full range.
        bool monotonic = true;
        for (int vol = 1; vol < 16; ++vol) {
            if (peaks[vol] > peaks[vol - 1]) {
                monotonic = false;
            }
        }
        expect(monotonic, "VolumeLaw_MonotonicAttenuation");
    }

    // --- 3. FEEDBACK: each FB step exactly DOUBLES the phase offset (§3's
    //     exponential ladder), FB=0 contributes zero; end-to-end, FB
    //     changes the produced waveform deterministically. ---
    {
        bool doubles = true;
        for (std::uint8_t fb = 1; fb < 7; ++fb) {
            // Sums chosen as multiples of 256 so no truncation interferes.
            for (const std::int32_t sum : {256, 1024, -512, -2048}) {
                if (Ym2413Synth::feedback_phase_offset(sum, static_cast<std::uint8_t>(fb + 1)) !=
                    2 * Ym2413Synth::feedback_phase_offset(sum, fb)) {
                    doubles = false;
                }
            }
        }
        expect(doubles, "Feedback_EachStepDoublesModulationIndex");
        expect(Ym2413Synth::feedback_phase_offset(4096, 0) == 0, "Feedback_Fb0_ZeroIndex");
        // FB=7 with a full-scale 2-sample sum (+-2048 each) reaches the
        // +-2048-index-unit = 4pi ladder top; FB=1 reaches pi/16 (= 32).
        expect(Ym2413Synth::feedback_phase_offset(4096, 7) == 2048, "Feedback_Fb7_4PiLadderTop");
        expect(Ym2413Synth::feedback_phase_offset(4096, 1) == 32, "Feedback_Fb1_PiOver16");

        // End-to-end: modulator+carrier both audible; FB=0 vs FB=7 streams
        // genuinely differ; identical settings reproduce byte-identically.
        const auto run_with_fb = [](const std::uint8_t fb) {
            Ym2413Opll opll;
            opll.reset();
            write_reg(opll, 0x00, 0x21);  // mod: EG-TYP=1, MUL=1
            write_reg(opll, 0x01, 0x21);  // car: EG-TYP=1, MUL=1
            write_reg(opll, 0x02, 0x00);  // mod TL=0 (full modulation)
            write_reg(opll, 0x03, fb);    // FB[2:0]
            write_reg(opll, 0x04, 0xF0);  // mod AR=15
            write_reg(opll, 0x05, 0xF0);  // car AR=15
            write_reg(opll, 0x06, 0x00);
            write_reg(opll, 0x07, 0x00);
            write_reg(opll, 0x10, 0x00);
            write_reg(opll, 0x30, 0x00);
            write_reg(opll, 0x20, 0x19);  // key-on, block 4, fnum8=1 -> fnum 256
            std::vector<std::int32_t> out;
            for (int i = 0; i < 512; ++i) {
                opll.advance_cycles(Ym2413Synth::kMasterCyclesPerNativeSample);
                out.push_back(opll.fm_sample());
            }
            return out;
        };
        const auto fb0 = run_with_fb(0);
        const auto fb7 = run_with_fb(7);
        expect(fb0 != fb7, "Feedback_EndToEnd_Fb0VsFb7_DifferentWaveforms");
        expect(fb7 == run_with_fb(7), "Feedback_EndToEnd_TwoRuns_ByteIdentical");
    }

    // --- 4. HALF-SINE RECTIFICATION (§3 DC): with an exact 128-sample
    //     period, the negative half-cycle is exactly the silence floor and
    //     the positive half is untouched. ---
    {
        Ym2413Opll full;
        Ym2413Opll half;
        program_carrier_only(full, 256, 4, 1, 0, false);
        program_carrier_only(half, 256, 4, 1, 0, true);
        const std::vector<std::int32_t> full_s = run_native_samples(full, 384);
        const std::vector<std::int32_t> half_s = run_native_samples(half, 384);
        bool rectified = true;
        for (std::size_t i = 0; i < full_s.size(); ++i) {
            if (full_s[i] < 0) {
                if (half_s[i] != 0) {
                    rectified = false;  // negative half must be silence
                }
            } else {
                if (half_s[i] != full_s[i]) {
                    rectified = false;  // positive half must be identical
                }
            }
        }
        expect(rectified, "HalfSine_NegativeHalfSilenced_PositiveHalfIdentical");
        bool has_negative = false;
        for (const std::int32_t s : full_s) {
            if (s < 0) {
                has_negative = true;
            }
        }
        expect(has_negative, "HalfSine_ControlFullSine_HasNegativeHalf");
    }

    // --- 5. Silent device guarantee: a never-keyed device produces exactly
    //     0 forever (the S5 zero-YM2413 oracle's foundation). ---
    {
        Ym2413Opll opll;
        opll.reset();
        write_reg(opll, 0x30, 0x30);  // instrument 3, volume 0 -- but NO key
        write_reg(opll, 0x10, 0x80);
        bool all_zero = true;
        for (int i = 0; i < 2000; ++i) {
            opll.advance_cycles(Ym2413Synth::kMasterCyclesPerNativeSample);
            if (opll.fm_sample() != 0) {
                all_zero = false;
            }
        }
        expect(all_zero, "NeverKeyed_OutputExactlyZero_Always");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Ym2413SynthOperator_Unit cases passed\n";
    return 0;
}
