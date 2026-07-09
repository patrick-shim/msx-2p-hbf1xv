// Suite: Frontend_MachineAudioMixerUltrasonicSilence_Unit (M34-S3, DEC-0043
// Defect A, docs/m34-planner-package.md §2.6.1 / test matrix row 8)
//
// THE ULTRASONIC-SILENCE ORACLE: the Aleste-2 silence idiom (tone period 0
// on all three channels, tones enabled, volumes up) degenerates on a real
// YM2149 to a ~112 kHz square -- inaudible, analog-smoothed to its mean.
// The pre-M34 point sampler folded it to a LOUD ~20.7 kHz alias (measured
// peak |AC| 24,800 = full swing at every sampled beat; DEC-0043 evidence RMS
// up to 17,533). The M34 box average must hold every period p in {0..4} at
// or under the EXACT §2.4 dwell bounds -- honest bounds, not a false
// near-zero claim (p=2/4 keep PARTIAL audible-band aliases by design,
// disclosed; the E-series backlog row names the full band-limited fix).
//
// Bound derivation (§2.4, authored before execution): W=81 cycles,
// half-period H=16*max(1,p); per-channel worst-case dwell deviation
// B(P) = H/2 when H <= W/2 else H - W/2; per-side (two in-phase channels,
// single rounding) peak |AC| <= (2*31*B/81 + 0.5) * 400:
//   p=0/1: B=7.5  -> <= 2,500    p=2: B=16   -> <= 5,100
//   p=3:   B=7.5  -> <= 2,500    p=4: B=23.5 -> <= 7,400
// Exact hand-model prediction (ideal square through the exact box + the
// documented rounding, computed independently of the chip code): peak |AC|
// = 2,400 / 2,400 / 2,800 / 2,400 / 7,200 for p = 0/1/2/3/4 -- every value
// discriminates hard against point-sampling (24,800).
//
// Control arm: p=112 (~999 Hz, audible band) must NOT be suppressed --
// max == 31*2*400 = 24,800 exactly and min == 0 exactly (windows fully
// inside half-periods are fixed points).
//
// SCC arm (§2.6.1): a stopped channel (period <= 8) integrates to its exact
// held constant through the mixer; the fast-stepping chip-level dwell case
// lives in audio_scc_wavetable_unit_test.cpp (M34 block).

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "frontend/machine_audio_mixer.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
using sony_msx::frontend::MachineAudioMixer;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;
constexpr std::size_t kSamples = 4096;

// All three tones enabled (R7 bits0-2 = 0) at full fixed volume (R8/R9/R10
// = 15 -> resolved amplitude 31 each), noise disabled, same period p on all
// three channels -- the DEC-0043-measured Aleste register idiom generalized
// over p.
void program_all_tones(PsgYm2149& psg, const int p) {
    psg.reset();
    for (int ch = 0; ch < 3; ++ch) {
        psg.write_address(static_cast<std::uint8_t>(2 * ch));
        psg.write_data(static_cast<std::uint8_t>(p & 0xFF));
        psg.write_address(static_cast<std::uint8_t>(2 * ch + 1));
        psg.write_data(static_cast<std::uint8_t>((p >> 8) & 0x0F));
    }
    for (int ch = 0; ch < 3; ++ch) {
        psg.write_address(static_cast<std::uint8_t>(8 + ch));
        psg.write_data(15);
    }
    psg.write_address(7);
    psg.write_data(0x38);  // tones A/B/C on, noise off
}

struct SideStats {
    std::int64_t mean = 0;   // integer (floor) mean
    std::int64_t peak_ac = 0;
    std::int16_t max_v = -32768;
    std::int16_t min_v = 32767;
};

SideStats side_stats(const std::vector<std::int16_t>& pcm, const std::size_t offset) {
    SideStats s;
    std::int64_t sum = 0;
    for (std::size_t i = offset; i < pcm.size(); i += 2) {
        sum += pcm[i];
        if (pcm[i] > s.max_v) s.max_v = pcm[i];
        if (pcm[i] < s.min_v) s.min_v = pcm[i];
    }
    const auto n = static_cast<std::int64_t>(pcm.size() / 2);
    s.mean = sum / n;
    for (std::size_t i = offset; i < pcm.size(); i += 2) {
        const std::int64_t d = std::llabs(static_cast<std::int64_t>(pcm[i]) - s.mean);
        if (d > s.peak_ac) s.peak_ac = d;
    }
    return s;
}

std::vector<std::int16_t> mix_psg_alone(const int p) {
    PsgYm2149 psg;
    program_all_tones(psg, p);
    const MachineAudioMixer mixer(kCyclesPerSample);
    return mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{nullptr, nullptr},
                                        nullptr, kSamples);
}

}  // namespace

int main() {
    // --- 1. The §2.4 bound table, asserted verbatim for p = 0..4. ---
    {
        const int periods[5] = {0, 1, 2, 3, 4};
        const std::int64_t bounds[5] = {2500, 2500, 5100, 2500, 7400};
        for (int i = 0; i < 5; ++i) {
            const std::vector<std::int16_t> pcm = mix_psg_alone(periods[i]);
            const SideStats left = side_stats(pcm, 0);
            const SideStats right = side_stats(pcm, 1);
            std::cout << "ultrasonic p=" << periods[i] << ": left peak_ac=" << left.peak_ac
                      << " right peak_ac=" << right.peak_ac << " (bound " << bounds[i]
                      << "; point-sampler peaks at 24800)\n";
            if (left.peak_ac > bounds[i] || right.peak_ac > bounds[i]) {
                std::cerr << "  (period " << periods[i] << ")\n";
                expect(false, "UltrasonicFamily_PeakAc_WithinExactDwellBound");
            }
        }
    }

    // --- 2. Control arm p=112 (~999 Hz): legitimate audible content is NOT
    //     suppressed -- full swing exactly (fixed-point windows). ---
    {
        const std::vector<std::int16_t> pcm = mix_psg_alone(112);
        const SideStats left = side_stats(pcm, 0);
        const SideStats right = side_stats(pcm, 1);
        expect(left.max_v == 24800 && right.max_v == 24800,
               "ControlArm_P112_FullSwingMax24800Exactly_BothSides");
        expect(left.min_v == 0 && right.min_v == 0,
               "ControlArm_P112_FullSwingMin0Exactly_BothSides");
    }

    // --- 3. SCC arm: a stopped channel's held constant passes through the
    //     integral exactly (silent PSG + held -1 * kSccAmplitudeScale=12 ->
    //     every PCM value == -12 on both channels). ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent (all amplitudes 0 -> integrates to exactly 0)
        SccWavetable scc;
        scc.reset();            // period 0 -> stopped; held output -1
        scc.write(0x8F, 0x01);  // enable ch1
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> pcm = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{&scc, nullptr}, nullptr, 256);
        bool all_minus12 = pcm.size() == 512;
        for (const std::int16_t v : pcm) {
            if (v != -12) {
                all_minus12 = false;
            }
        }
        expect(all_minus12, "SccArm_StoppedChannelHeldConstant_ExactFixedPointThroughMixer");
    }

    // --- 4. Two-run determinism (§2.6.3). ---
    {
        expect(mix_psg_alone(1) == mix_psg_alone(1) && mix_psg_alone(4) == mix_psg_alone(4),
               "TwoRuns_ByteIdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerUltrasonicSilence_Unit cases passed\n";
    return 0;
}
