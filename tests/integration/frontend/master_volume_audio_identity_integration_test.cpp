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

// Suite: Frontend_MasterVolumeAudioIdentity_Integration (M52, DEC-0079,
// docs/m52-planner-package.md §5 item 5).
//
// THE HARD "master gain is provably post-mix + identity at full volume" ORACLE
// -- WITHOUT an audio device. Produce a real interleaved-stereo int16 PCM buffer
// from the DEC-0039 MachineAudioMixer (an arbitrary PSG + SCC input sequence),
// then:
//   * apply_master_gain(buf, 100) leaves it BYTE-FOR-BYTE equal to the un-gained
//     mix (the byte-identity guarantee that keeps the DEC-0039 balance untouched);
//   * apply_master_gain(buf, 0) zeroes it;
//   * apply_master_gain(buf, mid) attenuates deterministically (== per-sample
//     s*mid/100), and NEVER exceeds the un-gained magnitude (attenuation only).
//
// Deterministic oracle: the same PSG/SCC program yields the same mix every run,
// and the gain law is a pure integer function of that mix -- fully reproducible,
// no SDL3, no wall clock.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "frontend/machine_audio_mixer.h"
#include "frontend/master_volume.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
using sony_msx::frontend::apply_master_gain;
using sony_msx::frontend::apply_master_gain_sample;
using sony_msx::frontend::MachineAudioMixer;

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;  // the presenter's own integer step

// A non-trivial, varying PSG program (mirrors the mixer unit test's driver):
// channel A square wave, channel B constant-on -- a genuinely varying sequence.
void program_psg(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(25);  // A fine period
    psg.write_address(8);
    psg.write_data(12);  // A volume
    psg.write_address(9);
    psg.write_data(7);  // B volume
    psg.write_address(7);
    psg.write_data(0x3C);  // tones A+B on, C off
}

// A constant-output SCC on channel 1 so the mix carries a second source too.
void program_scc(SccWavetable& scc) {
    scc.reset();
    for (int i = 0; i < 32; ++i) {
        scc.write(static_cast<std::uint8_t>(i), 0x40);
    }
    scc.write(0x8A, 0x0F);
    scc.write(0x8F, 0x01);
    scc.write(0x80, 0x63);  // period 99
}

// Produce the un-gained interleaved-stereo mix for the fixed program.
std::vector<std::int16_t> produce_mix(std::size_t sample_count) {
    PsgYm2149 psg;
    program_psg(psg);
    SccWavetable scc;
    program_scc(scc);
    const MachineAudioMixer mixer(kCyclesPerSample);
    return mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{&scc, nullptr}, sample_count);
}

}  // namespace

int main() {
    constexpr std::size_t kSamples = 2000;

    const std::vector<std::int16_t> reference = produce_mix(kSamples);
    expect(reference.size() == kSamples * 2, "Mix_ProducedExpectedLength");
    // Sanity: the mix is genuinely non-trivial (not all-zero), else the identity
    // oracle would be vacuous.
    bool any_nonzero = false;
    for (const std::int16_t v : reference) {
        if (v != 0) {
            any_nonzero = true;
            break;
        }
    }
    expect(any_nonzero, "Mix_IsNonTrivial_OracleNotVacuous");

    // --- volume 100 -> BYTE-FOR-BYTE identical to the un-gained mix. ---
    {
        std::vector<std::int16_t> at100 = produce_mix(kSamples);
        apply_master_gain(at100, 100);
        expect(at100 == reference, "FullVolume100_ByteIdenticalToUnGainedMix_HardOracle");
    }

    // --- volume 0 -> all zero. ---
    {
        std::vector<std::int16_t> at0 = produce_mix(kSamples);
        apply_master_gain(at0, 0);
        expect(at0 == std::vector<std::int16_t>(reference.size(), 0), "Volume0_AllZero");
    }

    // --- a mid value (37%) attenuates deterministically (== per-sample gain) and
    //     never exceeds the un-gained magnitude (attenuation only). ---
    {
        std::vector<std::int16_t> at37 = produce_mix(kSamples);
        apply_master_gain(at37, 37);
        bool matches = at37.size() == reference.size();
        bool bounded = true;
        for (std::size_t i = 0; i < reference.size() && matches; ++i) {
            if (at37[i] != apply_master_gain_sample(reference[i], 37)) {
                matches = false;
            }
            if (std::abs(static_cast<int>(at37[i])) > std::abs(static_cast<int>(reference[i]))) {
                bounded = false;
            }
        }
        expect(matches, "Volume37_MatchesPerSampleGain_Deterministic");
        expect(bounded, "Volume37_NeverExceedsUnGainedMagnitude_AttenuationOnly");
    }

    // --- two identical gain runs are byte-identical (determinism). ---
    {
        auto run = [] {
            std::vector<std::int16_t> b = produce_mix(kSamples);
            apply_master_gain(b, 65);
            return b;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalGainedPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MasterVolumeAudioIdentity_Integration cases passed\n";
    return 0;
}
