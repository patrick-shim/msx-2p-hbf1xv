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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "frontend/machine_audio_mixer.h"
#include "frontend/psg_audio_pump.h"

// Suite: Frontend_MachineAudioMixer_Unit (M29-S5, docs/m29-planner-package.md
// §2.4; backlog G1)
//
// THE HARD REGRESSION ORACLE lives here: with zero SCC sources attached, the
// mixer's PCM output must be BYTE-IDENTICAL to the bare-pump presenter
// arithmetic (psg pump sample * 400 per channel, clamped to int16) for any
// input sequence -- reproduced below against an independent PSG twin driven
// by the raw PsgAudioPump + the literal conversion loop. Plus the documented
// clamp formula with SCC sources (hand-computed cases incl. a constructed
// saturation input, risk R-M29-4) and two-run determinism.
//
// M34 RE-GROUNDING NOTE (2026-07-09, docs/m34-planner-package.md §2.5 row 3,
// disposition G): PsgAudioPump::pump_one_sample() now produces box-average
// integrated samples (DEC-0043 Defect A), so BOTH sides of case 1 compute
// through the same integrated pump -- the oracle's MEANING is unchanged and
// still load-bearing: an absent/null SCC source contributes EXACTLY 0 to
// every sample for ANY input sequence. Cases 2-5 are constant-signal fixed
// points of the §2.3.4 rounding law (constant level L integrates to exactly
// L), so their hand-computed values (720 / 32,767 / 31,940) hold UNCHANGED
// -- verified at M34, not assumed; any motion in them is an integration
// bug, never a rebaseline.

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
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

// A non-trivial PSG program: channel A square wave (period 25), channel B
// constant-on at a different volume -- gives a genuinely varying sequence.
void program_psg(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(25);  // A fine period
    psg.write_address(8);
    psg.write_data(12);  // A volume (fixed)
    psg.write_address(9);
    psg.write_data(7);  // B volume (fixed)
    psg.write_address(7);
    psg.write_data(0x3C);  // tones A+B on, C off, all noise off
}

// PSG at its constant MAXIMUM raw output (left = A+B = 62, right = A+C =
// 62): all tone/noise disabled in R7 (output held high => audible) with
// fixed volume 15 on A, B AND C (resolved 5-bit amplitude 2*15+1 = 31 each;
// A=Center feeds both sides, B=Left, C=Right).
void program_psg_max(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(8);
    psg.write_data(15);
    psg.write_address(9);
    psg.write_data(15);
    psg.write_address(10);
    psg.write_data(15);
    psg.write_address(7);
    psg.write_data(0x3F);  // all tone+noise disabled -> constant audible
}

// An SCC held at a constant output: wave value `wave_byte` everywhere on
// ch1, volume 15, ch1 enabled, period 99 -- held output (wave*15)>>4.
void program_scc_constant(SccWavetable& scc, const std::uint8_t wave_byte) {
    scc.reset();
    for (int i = 0; i < 32; ++i) {
        scc.write(static_cast<std::uint8_t>(i), wave_byte);
    }
    scc.write(0x8A, 0x0F);
    scc.write(0x8F, 0x01);
    scc.write(0x80, 0x63);  // period 99; refreshes the held output
}

// All five channels held at +127 with volume 15: per-channel (127*15)>>4 =
// 119, five channels => sample() == 595 (the documented +max AC extreme).
void program_scc_max(SccWavetable& scc) {
    scc.reset();
    for (int i = 0; i < 32; ++i) {
        scc.write(static_cast<std::uint8_t>(i), 0x7F);
        scc.write(static_cast<std::uint8_t>(0x20 + i), 0x7F);
        scc.write(static_cast<std::uint8_t>(0x40 + i), 0x7F);
        scc.write(static_cast<std::uint8_t>(0x60 + i), 0x7F);  // ch4 (+ch5 copy)
    }
    for (std::uint8_t v = 0x8A; v <= 0x8E; ++v) {
        scc.write(v, 0x0F);
    }
    scc.write(0x8F, 0x1F);
    for (std::uint8_t f = 0x80; f <= 0x89; ++f) {
        scc.write(f, 0x00);  // period 0 (stopped) -- held outputs refreshed
    }
}

}  // namespace

int main() {
    // --- 1. THE HARD REGRESSION ORACLE: zero SCC sources => byte-identical
    //     to the pre-M29 presenter arithmetic over a long, varying run. ---
    {
        PsgYm2149 psg_mixer;
        PsgYm2149 psg_reference;
        program_psg(psg_mixer);
        program_psg(psg_reference);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg_mixer, MachineAudioMixer::SccSources{nullptr, nullptr}, 2000);

        // The literal pre-M29 presenter loop shape (sdl3_audio_presenter.cpp
        // @ v1.0.29): pump_samples + s.{left,right} * 400, clamped. M34
        // re-grounding (§2.5 row 3): the pump inside is now the integrated-
        // sample pump on both sides -- the oracle still proves the mixer's
        // zero-SCC path adds exactly 0.
        const PsgAudioPump pump(kCyclesPerSample);
        const std::vector<PsgYm2149::StereoSample> samples = pump.pump_samples(psg_reference, 2000);
        std::vector<std::int16_t> reference;
        reference.reserve(samples.size() * 2);
        for (const auto& s : samples) {
            reference.push_back(static_cast<std::int16_t>(std::clamp(s.left * 400, -32768, 32767)));
            reference.push_back(static_cast<std::int16_t>(std::clamp(s.right * 400, -32768, 32767)));
        }

        expect(mixed.size() == reference.size() && mixed.size() == 4000,
               "ZeroScc_OutputLengthMatches");
        expect(mixed == reference, "ZeroScc_ByteIdenticalToPreM29PresenterArithmetic_HardOracle");
    }

    // --- 2. Documented clamp formula with one SCC source, hand-computed:
    //     silent PSG (post-reset) + SCC held at (64*15)>>4 = 60 =>
    //     every pcm value = 0*400 + 60*12 = 720 on both channels. ---
    {
        PsgYm2149 psg;
        psg.reset();  // amplitudes 0 -> psg raw 0 forever
        SccWavetable scc;
        program_scc_constant(scc, 0x40);  // +64 -> held 60

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed =
            mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{&scc, nullptr}, 100);
        bool all_720 = mixed.size() == 200;
        for (const std::int16_t v : mixed) {
            if (v != 720) {
                all_720 = false;
            }
        }
        expect(all_720, "OneScc_SilentPsg_HandComputed720_MonoOnBothChannels");
        // The SCC generator genuinely advanced machine time: 100 samples *
        // 81 cycles / (99+1) cycles-per-step = 81 position steps.
        expect(scc.position(0) == 81 % 32, "OneScc_GeneratorAdvancedBySampleCount");
    }

    // --- 3. Constructed SATURATION case (R-M29-4: the clamp is REQUIRED):
    //     PSG at max 62 (62*400 = 24,800) + TWO SCCs at max AC 595 each
    //     (2*595*12 = 14,280) => 39,080 > 32,767 => clamped to 32,767. ---
    {
        PsgYm2149 psg;
        program_psg_max(psg);
        SccWavetable scc_a;
        SccWavetable scc_b;
        program_scc_max(scc_a);
        program_scc_max(scc_b);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed =
            mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{&scc_a, &scc_b}, 10);
        bool all_clamped = mixed.size() == 20;
        for (const std::int16_t v : mixed) {
            if (v != 32767) {
                all_clamped = false;
            }
        }
        expect(all_clamped, "TwoSccsPlusMaxPsg_39080_ClampedTo32767");

        // Control: ONE max SCC stays inside int16 (24,800 + 7,140 = 31,940
        // -- the header's 32,000 bound uses the +-600 round figure; the
        // exact +max is 595*12 = 7,140).
        PsgYm2149 psg2;
        program_psg_max(psg2);
        SccWavetable scc_c;
        program_scc_max(scc_c);
        const std::vector<std::int16_t> one =
            mixer.mix_interleaved_stereo(psg2, MachineAudioMixer::SccSources{&scc_c, nullptr}, 4);
        bool all_31940 = one.size() == 8;
        for (const std::int16_t v : one) {
            if (v != 31940) {
                all_31940 = false;
            }
        }
        expect(all_31940, "OneSccPlusMaxPsg_31940_InsideInt16_NoClamp");
    }

    // --- 4. Second-slot-only source mixes identically (the SccSources
    //     array is positionally agnostic in the sum). ---
    {
        PsgYm2149 psg;
        psg.reset();
        SccWavetable scc;
        program_scc_constant(scc, 0x40);
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed =
            mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{nullptr, &scc}, 8);
        bool all_720 = mixed.size() == 16;
        for (const std::int16_t v : mixed) {
            if (v != 720) {
                all_720 = false;
            }
        }
        expect(all_720, "SlotTwoOnlySource_SameMixContribution");
    }

    // --- 5. Two-run determinism (identical machines, identical sequences). ---
    {
        auto run = [] {
            PsgYm2149 psg;
            program_psg(psg);
            SccWavetable scc;
            program_scc_constant(scc, 0x23);
            scc.write(0x88, 0x2B);  // ch5 period too (running but disabled)
            const MachineAudioMixer mixer(kCyclesPerSample);
            return mixer.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{&scc, nullptr}, 1500);
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalRuns_IdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixer_Unit cases passed\n";
    return 0;
}
