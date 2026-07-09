// Suite: Frontend_MachineAudioMixerFm_Unit (M31-S5, backlog E1,
// docs/m31-planner-package.md §3-S5)
//
// THE M31 HARD REGRESSION ORACLE lives here: with fm == nullptr AND
// separately with a real-but-silent (never-keyed) Ym2413Opll attached, the
// mixer's PCM output must be BYTE-IDENTICAL to the 3-arg zero-FM arithmetic
// for any input sequence -- reproduced below against independent twins
// driven by the raw PsgAudioPump + the literal conversion loop (the exact
// M29 zero-SCC oracle pattern). Plus: the documented three-source mixing
// law against a lock-step FM twin, the REQUIRED int16 clamp at a
// constructed saturation input (R-M31-4), the exact 9:8 native-tick
// decimation pattern (8 x 81 cycles = 648 = 9 x 72, planner §2.5), and
// two-run determinism.
//
// M34 RE-GROUNDING NOTE (2026-07-09, docs/m34-planner-package.md §2.5 row 4,
// disposition G): PsgAudioPump now produces box-average integrated samples
// (DEC-0043 Defect A), so the byte-identity arms recompute through the same
// integrated pump on both sides -- the MEANING survives intact: a nullptr
// or silent-attached FM contributes EXACTLY 0 to every sample. The FM
// native path (advance + fm_sample + k=21) is byte-untouched by M34; the
// clamp/decimation/determinism cases are constants + FM-native properties
// and hold UNMODIFIED (verified at M34, not assumed).

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
#include "devices/audio/ym2413_synth.h"
#include "frontend/machine_audio_mixer.h"
#include "frontend/psg_audio_pump.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
using sony_msx::devices::audio::Ym2413Opll;
using sony_msx::devices::audio::Ym2413Synth;
using sony_msx::frontend::MachineAudioMixer;
using sony_msx::frontend::PsgAudioPump;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;  // the presenter's own integer step

void write_reg(Ym2413Opll& opll, const std::uint8_t addr, const std::uint8_t value) {
    opll.write_address(addr);
    opll.write_data(value);
}

// The M29 unit test's own non-trivial PSG program (varying output).
void program_psg(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(25);
    psg.write_address(8);
    psg.write_data(12);
    psg.write_address(9);
    psg.write_data(7);
    psg.write_address(7);
    psg.write_data(0x3C);
}

// PSG at its constant maximum raw output (62 on both sides), as in the M29
// saturation case.
void program_psg_max(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(8);
    psg.write_data(15);
    psg.write_address(9);
    psg.write_data(15);
    psg.write_address(10);
    psg.write_data(15);
    psg.write_address(7);
    psg.write_data(0x3F);
}

// A held carrier-only FM tone on `channel` (user patch, instant attack,
// sustained at 0 dB): per-channel peak 2048 >> 3 = 256.
void program_fm_tone_channel(Ym2413Opll& opll, const int channel) {
    write_reg(opll, 0x00, 0x00);  // mod: AR=0 -> silent forever
    write_reg(opll, 0x01, 0x21);  // car: EG-TYP=1, MUL=1
    write_reg(opll, 0x02, 0x3F);
    write_reg(opll, 0x03, 0x00);
    write_reg(opll, 0x04, 0x00);
    write_reg(opll, 0x05, 0xF0);  // car AR=15 (instant)
    write_reg(opll, 0x06, 0x00);
    write_reg(opll, 0x07, 0x00);
    write_reg(opll, 0x10 + channel, 0x00);
    write_reg(opll, 0x30 + channel, 0x00);  // user patch, volume 0
    write_reg(opll, 0x20 + channel, 0x19);  // key-on, block 4, fnum 256
}

// The literal zero-FM presenter/mixer arithmetic, reproduced independently:
// PSG pump * 400 (+ SCC term) with the int16 clamp. M34 re-grounding (§2.5
// row 4): the pump is the integrated-sample pump on both sides -- this twin
// still proves the fm term is exactly 0 when fm is null/silent.
std::vector<std::int16_t> reference_pre_m31(PsgYm2149& psg, const std::size_t count) {
    const PsgAudioPump pump(kCyclesPerSample);
    std::vector<std::int16_t> reference;
    reference.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        const PsgYm2149::StereoSample s = pump.pump_one_sample(psg);
        reference.push_back(static_cast<std::int16_t>(std::clamp(s.left * 400, -32768, 32767)));
        reference.push_back(static_cast<std::int16_t>(std::clamp(s.right * 400, -32768, 32767)));
    }
    return reference;
}

}  // namespace

int main() {
    // --- 1. THE HARD ORACLE, part (a): fm == nullptr => byte-identical to
    //     the v1.0.31 arithmetic AND to the existing 3-arg overload. ---
    {
        PsgYm2149 psg_a;
        PsgYm2149 psg_b;
        PsgYm2149 psg_c;
        program_psg(psg_a);
        program_psg(psg_b);
        program_psg(psg_c);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> with_null_fm = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr, 2000);
        const std::vector<std::int16_t> three_arg = mixer.mix_interleaved_stereo(
            psg_b, MachineAudioMixer::SccSources{nullptr, nullptr}, 2000);
        const std::vector<std::int16_t> reference = reference_pre_m31(psg_c, 2000);

        expect(with_null_fm == reference, "NullFm_ByteIdenticalToPreM31Arithmetic_HardOracle");
        expect(three_arg == reference, "ThreeArgOverload_StillByteIdentical_PreM31");
    }

    // --- 2. THE HARD ORACLE, part (b): a REAL Ym2413Opll attached but never
    //     keyed contributes exactly 0 -- byte-identical output; the device's
    //     generator time still advances (9:8 native ticks). ---
    {
        PsgYm2149 psg_a;
        PsgYm2149 psg_c;
        program_psg(psg_a);
        program_psg(psg_c);
        Ym2413Opll silent_fm;
        silent_fm.reset();
        // Non-key register traffic must not break silence.
        write_reg(silent_fm, 0x30, 0x53);
        write_reg(silent_fm, 0x10, 0xAC);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> with_silent_fm = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{nullptr, nullptr}, &silent_fm, 2000);
        expect(with_silent_fm == reference_pre_m31(psg_c, 2000),
               "SilentAttachedFm_ByteIdenticalToPreM31Arithmetic_HardOracle");
        // 2000 samples x 81 cycles = 162,000 cycles -> 2250 native ticks.
        expect(silent_fm.synth().global_counter() == 2250,
               "SilentAttachedFm_GeneratorTimeTracksMachineTime");
    }

    // --- 3. The documented three-source law against a lock-step FM twin:
    //     silent PSG => pcm == clamp(fm_sample * kFmAmplitudeScale). ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent
        Ym2413Opll fm;
        Ym2413Opll twin;
        fm.reset();
        twin.reset();
        program_fm_tone_channel(fm, 0);
        program_fm_tone_channel(twin, 0);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{nullptr, nullptr}, &fm, 600);
        bool law_holds = mixed.size() == 1200;
        bool fm_audible = false;
        for (std::size_t i = 0; i < 600 && law_holds; ++i) {
            twin.advance_cycles(kCyclesPerSample);
            const std::int32_t expected = std::clamp(
                twin.fm_sample() * MachineAudioMixer::kFmAmplitudeScale, -32768, 32767);
            if (mixed[i * 2] != expected || mixed[i * 2 + 1] != expected) {
                law_holds = false;
            }
            if (expected != 0) {
                fm_audible = true;
            }
        }
        expect(law_holds, "MixingLaw_FmTimesScale_MonoOnBothChannels_LockStepTwin");
        expect(fm_audible, "MixingLaw_KeyedFm_ActuallyNonSilent");
    }

    // --- 4. Constructed SATURATION worst case (M32-S4, R-M31-4 updated per
    //     docs/m32-planner-package.md §2.8: the clamp is REQUIRED at k=21):
    //     9 phase-aligned full-scale FM carriers (9 * 256 * 21 = 48,384 --
    //     FM ALONE exceeds int16) + max PSG (62*400 = 24,800) + two max
    //     SCCs (2 * 600 * 12 = 14,400) => up to 87,584 raw => exact clamp
    //     to +32,767 must appear on BOTH stereo sides, and no value can
    //     exceed int16. ---
    {
        PsgYm2149 psg;
        program_psg_max(psg);
        Ym2413Opll fm;
        fm.reset();
        for (int channel = 0; channel < 9; ++channel) {
            program_fm_tone_channel(fm, channel);
        }
        // Two SCC chips at a loud constant-ish output: full-amplitude
        // square-ish waveform on channel 1 (the M29 saturation idiom).
        SccWavetable scc_a;
        SccWavetable scc_b;
        for (SccWavetable* scc : {&scc_a, &scc_b}) {
            scc->reset();
            for (int i = 0; i < 32; ++i) {
                scc->write(static_cast<std::uint8_t>(i), 0x7F);
            }
            scc->write(0x8A, 0x0F);  // channel 1 volume 15
            scc->write(0x8F, 0x01);  // channel 1 enable
            scc->write(0x80, 0xFF);  // channel 1 period (slow -> long flat tops)
            scc->write(0x81, 0x00);
        }
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{&scc_a, &scc_b}, &fm, 300);
        std::int16_t peak_left = -32768;
        std::int16_t peak_right = -32768;
        for (std::size_t i = 0; i + 1 < mixed.size(); i += 2) {
            peak_left = std::max(peak_left, mixed[i]);
            peak_right = std::max(peak_right, mixed[i + 1]);
        }
        expect(peak_left == 32767 && peak_right == 32767,
               "Saturation_WorstCaseFmPsgTwoSccs_ExactClampAt32767_BothSides");
    }

    // --- 4b. FM ALONE exceeds int16 at k=21 on BOTH rails (9 * 256 * 21 =
    //     +-48,384): silent PSG, no SCCs => the sine's positive AND
    //     negative extremes both clamp exactly. ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent
        Ym2413Opll fm;
        fm.reset();
        for (int channel = 0; channel < 9; ++channel) {
            program_fm_tone_channel(fm, channel);
        }
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{nullptr, nullptr}, &fm, 600);
        std::int16_t max_v = -32768;
        std::int16_t min_v = 32767;
        for (const std::int16_t v : mixed) {
            max_v = std::max(max_v, v);
            min_v = std::min(min_v, v);
        }
        expect(max_v == 32767, "Saturation_FmAlone_PositiveRail_ClampedTo32767");
        expect(min_v == -32768, "Saturation_FmAlone_NegativeRail_ClampedToMinus32768");
    }

    // --- 5. The exact 9:8 decimation pattern (planner §2.5): 8 output
    //     samples x 81 cycles = 648 = 9 x 72 -> exactly nine native FM
    //     ticks per eight output samples, an exact repeating pattern. ---
    {
        Ym2413Opll fm;
        fm.reset();
        // floor(81k/72) for k = 1..8: 1,2,3,4,5,6,7,9 -- the 8th output
        // sample spans TWO native ticks (ZOH drops one native sample in 9).
        const std::uint32_t expected_pattern[8] = {1, 2, 3, 4, 5, 6, 7, 9};
        bool pattern_holds = true;
        for (int repeat = 0; repeat < 3; ++repeat) {
            for (int k = 0; k < 8; ++k) {
                fm.advance_cycles(kCyclesPerSample);
                const std::uint32_t expected =
                    static_cast<std::uint32_t>(repeat) * 9 + expected_pattern[k];
                if (fm.synth().global_counter() != expected) {
                    pattern_holds = false;
                }
            }
        }
        expect(pattern_holds, "Decimation_648CycleRepeat_NineNativeTicksPerEightSamples");
    }

    // --- 6. Two-run determinism with all three sources active. ---
    {
        const auto run = [] {
            PsgYm2149 psg;
            program_psg(psg);
            SccWavetable scc;
            scc.reset();
            for (int i = 0; i < 32; ++i) {
                scc.write(static_cast<std::uint8_t>(i), 0x40);
            }
            scc.write(0x8A, 0x0F);
            scc.write(0x8F, 0x01);
            scc.write(0x80, 0x63);
            Ym2413Opll fm;
            fm.reset();
            program_fm_tone_channel(fm, 2);
            const MachineAudioMixer mixer(kCyclesPerSample);
            return mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{&scc, nullptr},
                                                 &fm, 1500);
        };
        expect(run() == run(), "ThreeSources_TwoRuns_ByteIdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerFm_Unit cases passed\n";
    return 0;
}
