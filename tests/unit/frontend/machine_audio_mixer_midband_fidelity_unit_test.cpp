// Suite: Frontend_MachineAudioMixerMidbandFidelity_Unit (M34-S3, DEC-0043
// Defect A, docs/m34-planner-package.md §2.6.2 / test matrix row 9)
//
// THE AUDIBLE-FIDELITY ORACLE: the box average must leave legitimate
// mid-band music essentially untouched. Frequency-domain justification
// (§2.4): |H(f)| = |sin(pi f T)/(pi f T)| with T = 81/3,579,545 s -> the
// sinc passband droop is -0.0004 dB @440 Hz and -0.007 dB @1 kHz --
// inaudible. The assertions below are exact integer TIME-domain properties
// (per the package: the sinc math is the comment, not the assert):
//
//   (a) peak == 31*400 = 12,400 EXACTLY (windows fully inside a half-period
//       integrate a constant -> fixed point) and min == 0 exactly;
//   (b) samples with intermediate values are confined to
//       transition-straddling windows: <= 2 per tone half-period + 1;
//   (c) block AC-RMS within +-1% of the ideal-square prediction, where the
//       prediction is computed IN-TEST from first principles (an ideal 0/31
//       square of half-period 16*p cycles pushed through an exact 81-cycle
//       box average with the documented §2.3.4 rounding -- pure arithmetic,
//       fully independent of the chip/mixer code). Hand-model figures
//       authored before execution: p=254 (440.1 Hz) predictor AC-RMS 6,158
//       (= -0.7% vs the transition-free A/2 = 6,200 -- within +-1% of A/2
//       too); p=112 (998.8 Hz) predictor AC-RMS 6,105 (= -1.5% vs A/2: at
//       999 Hz ~4.5% of windows straddle a transition, each contributing a
//       sub-full-swing value -- an exact property of ANY box average, not a
//       defect; the +-1% gate is therefore anchored on the exact predictor,
//       and the A/2 cross-check is asserted for the 440 Hz row where
//       straddling is rare).

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "frontend/machine_audio_mixer.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::frontend::MachineAudioMixer;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;
constexpr std::size_t kSamples = 8192;

// Single full-volume channel A square at period p; B/C muted, noise off.
void program_single_tone(PsgYm2149& psg, const int p) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(static_cast<std::uint8_t>(p & 0xFF));
    psg.write_address(1);
    psg.write_data(static_cast<std::uint8_t>((p >> 8) & 0x0F));
    psg.write_address(8);
    psg.write_data(15);
    psg.write_address(7);
    psg.write_data(0x3E);  // tone A only
}

// The independent ideal-square-through-ideal-box predictor (comment (c)
// above): level(t) = 31 iff floor((t-1)/(16p)) is odd (the chip's own reset
// phase: output starts low, first toggle completes at cycle 16p; §2.3.3
// boundary convention), window k covers cycles [81k+1, 81k+81], sample =
// round-half-away-from-zero(31*dwell, 81) * 400.
std::vector<std::int32_t> predict(const int p, const std::size_t n) {
    const std::int64_t half = static_cast<std::int64_t>(16) * p;
    std::vector<std::int32_t> out;
    out.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
        std::int64_t dwell = 0;
        for (std::int64_t t = static_cast<std::int64_t>(k) * 81 + 1;
             t <= static_cast<std::int64_t>(k + 1) * 81; ++t) {
            if (((t - 1) / half) % 2 == 1) {
                ++dwell;
            }
        }
        const std::int64_t sum = 31 * dwell;
        out.push_back(static_cast<std::int32_t>((2 * sum + 81) / 162 * 400));
    }
    return out;
}

double ac_rms(const std::vector<std::int32_t>& xs) {
    std::int64_t sum = 0;
    for (const std::int32_t x : xs) sum += x;
    const std::int64_t mean = sum / static_cast<std::int64_t>(xs.size());
    double acc = 0.0;
    for (const std::int32_t x : xs) {
        const double d = static_cast<double>(x - mean);
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(xs.size()));
}

struct ToneRun {
    std::vector<std::int32_t> left;
    std::vector<std::int16_t> pcm;
};

ToneRun run_tone(const int p) {
    PsgYm2149 psg;
    program_single_tone(psg, p);
    const MachineAudioMixer mixer(kCyclesPerSample);
    ToneRun r;
    r.pcm = mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{nullptr, nullptr},
                                         nullptr, kSamples);
    r.left.reserve(kSamples);
    for (std::size_t i = 0; i < r.pcm.size(); i += 2) {
        r.left.push_back(r.pcm[i]);
    }
    return r;
}

void check_tone(const int p, const char* label) {
    const ToneRun run = run_tone(p);

    // (a) exact fixed-point peak.
    std::int32_t max_v = -32768;
    std::int32_t min_v = 32767;
    for (const std::int32_t v : run.left) {
        if (v > max_v) max_v = v;
        if (v < min_v) min_v = v;
    }
    std::cout << label << ": max=" << max_v << " min=" << min_v;
    expect(max_v == 12400, "PeakExactly12400_FixedPointWindows");
    expect(min_v == 0, "MinExactly0_FixedPointWindows");

    // (b) transition confinement: intermediate values <= 2 per half-period + 1.
    const std::size_t half_periods = (kSamples * 81) / (16 * static_cast<std::size_t>(p));
    std::size_t intermediate = 0;
    for (const std::int32_t v : run.left) {
        if (v > 0 && v < 12400) {
            ++intermediate;
        }
    }
    std::cout << " intermediate=" << intermediate << " (bound " << (2 * half_periods + 1) << ")";
    expect(intermediate <= 2 * half_periods + 1,
           "IntermediateValues_ConfinedToTransitionWindows");

    // (c) AC-RMS within +-1% of the independent ideal-square prediction.
    const double measured = ac_rms(run.left);
    const double predicted = ac_rms(predict(p, kSamples));
    std::cout << " ac_rms=" << measured << " predicted=" << predicted << "\n";
    expect(measured >= predicted * 0.99 && measured <= predicted * 1.01,
           "BlockAcRms_Within1PercentOfIdealSquarePrediction");
}

}  // namespace

int main() {
    // --- p=254 (440.1 Hz): near-ideal passband. Also cross-checked against
    //     the transition-free A/2 = 6,200 (predictor 6,158 = -0.7%). ---
    check_tone(254, "midband p=254 (440 Hz)");
    {
        const double measured = ac_rms(run_tone(254).left);
        expect(measured >= 6200.0 * 0.99 && measured <= 6200.0 * 1.01,
               "P254_AcRms_Within1PercentOfPlainIdealSquareA2");
    }

    // --- p=112 (998.8 Hz): predictor-anchored (see header note (c)). ---
    check_tone(112, "midband p=112 (999 Hz)");

    // --- Two-run determinism (§2.6.3). ---
    {
        expect(run_tone(254).pcm == run_tone(254).pcm && run_tone(112).pcm == run_tone(112).pcm,
               "TwoRuns_ByteIdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerMidbandFidelity_Unit cases passed\n";
    return 0;
}
