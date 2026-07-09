#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "frontend/psg_audio_pump.h"

// Suite: Frontend_PsgAudioPump_Unit (M26-S5, docs/m26-planner-package.md §2.6
// point 1; discharges risk R-M26-4).
//
// PsgYm2149::advance_cycles()/sample() are real, already-implemented,
// already-unit-tested (M15) sample-generation machinery, but had ZERO call
// sites anywhere in src/ before M26 (A-M26-4, confirmed by a repo-wide grep).
// This test proves the NEW WIRING (PsgAudioPump, the exact pattern
// Sdl3AudioPresenter's real SDL_AudioStream callback uses) is genuinely
// correct against a hand-computed square-wave oracle -- not merely that the
// underlying PSG functions work in isolation.

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::frontend::PsgAudioPump;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Programs channel A as a period=1 square wave at fixed volume 15 (resolved
// amplitude 2*15+1=31), channels B/C fully muted (tone+noise both disabled).
// R7's #A0-A2-style write forces MSX's own bit6=0/bit7=1 I/O-direction
// convention automatically (write_register, psg_ym2149.cpp:210-212) -- the
// raw 0x3E value below already has bit6=0, so the stored result is 0xBE.
void program_channel_a_square_wave(PsgYm2149& psg) {
    psg.write_address(0);
    psg.write_data(1);  // R0 (A fine) = 1
    psg.write_address(1);
    psg.write_data(0);  // R1 (A coarse) = 0 -> tone_period(0) == 1
    psg.write_address(8);
    psg.write_data(15);  // R8 (A volume) = fixed level 15 (bit4=0, not envelope)
    psg.write_address(7);
    psg.write_data(0x3E);  // R7: tone A on (bit0=0), tone B/C off, noise A/B/C off
}

}  // namespace

int main() {
    // --- Case 1 (M34-RE-DERIVED, docs/m34-planner-package.md §2.5 row 1;
    // dwell math authored BEFORE execution): cycles_per_sample ==
    // kCyclesPerGeneratorStep (16) -- exactly one generator step per sample.
    // With period=1 the tone toggles at EVERY step; under the §2.3.3
    // boundary convention the step completing at cycle t changes the level
    // AFTER cycle t, so level(t) = 31 iff floor((t-1)/16) is odd. Window k
    // covers cycles [16k+1, 16k+16] -- entirely inside block k:
    //   w0 = block0 (low)  -> 0        w1 = block1 (high) -> 31
    // Integrated sequence {0, 31, 0, 31, 0, 31} -- the EXACT flip of the
    // pre-M34 point-sample oracle {31, 0, ...} (the point sampler read the
    // level AFTER the window's closing toggle; the box average reads the
    // level DURING the window). ---
    {
        PsgYm2149 psg;
        psg.reset();
        program_channel_a_square_wave(psg);

        const PsgAudioPump pump(PsgYm2149::kCyclesPerGeneratorStep);
        const std::vector<PsgYm2149::StereoSample> samples = pump.pump_samples(psg, 6);

        expect(samples.size() == 6, "OneStepPerSample_CollectsRequestedSampleCount");
        const std::array<std::int32_t, 6> expected_left{0, 31, 0, 31, 0, 31};
        bool all_left_match = true;
        bool all_right_match = true;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (samples[i].left != expected_left[i]) {
                all_left_match = false;
            }
            // A=Center contributes to both channels (fact-sheet §2); B/C are
            // muted, so right must equal left exactly for this program.
            if (samples[i].right != expected_left[i]) {
                all_right_match = false;
            }
        }
        expect(all_left_match, "OneStepPerSample_LeftChannel_MatchesHandComputedDwellOracle");
        expect(all_right_match, "OneStepPerSample_RightChannel_MatchesHandComputedDwellOracle");
    }

    // --- Case 2 (M34-RE-DERIVED, §2.5 row 1; dwell math authored BEFORE
    // execution): cycles_per_sample == kCyclesPerGeneratorStep/2 (8) -- HALF
    // a generator step per pump call, proving cycle_residual_ genuinely
    // accumulates FRACTIONAL cycles across repeated pump_one_sample calls
    // (R-M26-4's wiring hazard, unchanged). Same level timeline as case 1
    // (level = 31 iff floor((t-1)/16) odd); windows of 8 cycles:
    //   w0 = cycles  1- 8 (low)  -> 0     w1 = cycles  9-16 (low)  -> 0
    //   w2 = cycles 17-24 (high) -> 31    w3 = cycles 25-32 (high) -> 31
    //   w4 = cycles 33-40 (low)  -> 0     w5 = cycles 41-48 (low)  -> 0
    // Every 8-cycle window sits entirely inside one 16-cycle level block, so
    // each sample is an exact constant -- {0, 0, 31, 31, 0, 0} (the pre-M34
    // point oracle was {0, 31, 31, 0, 0, 31}: it read the post-toggle level
    // at windows 1/3/5). ---
    {
        PsgYm2149 psg;
        psg.reset();
        program_channel_a_square_wave(psg);

        const PsgAudioPump pump(PsgYm2149::kCyclesPerGeneratorStep / 2);
        const std::vector<PsgYm2149::StereoSample> samples = pump.pump_samples(psg, 6);

        const std::array<std::int32_t, 6> expected_left{0, 0, 31, 31, 0, 0};
        bool all_match = true;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (samples[i].left != expected_left[i] || samples[i].right != expected_left[i]) {
                all_match = false;
            }
        }
        expect(all_match, "FractionalCyclesPerSample_ResidualAccumulatesCorrectly_MatchesHandComputedOracle");
    }

    // --- Case 3: an unattached/never-pumped PSG (post-reset, idle) is
    // silent -- a regression-safe baseline the wiring must never disturb by
    // itself (pump_samples with cycles_per_sample=0 issues zero real
    // generator advance, matching a genuinely idle channel). ---
    {
        PsgYm2149 psg;
        psg.reset();
        const PsgAudioPump pump(0);
        const std::vector<PsgYm2149::StereoSample> samples = pump.pump_samples(psg, 3);
        bool all_silent = true;
        for (const auto& s : samples) {
            if (s.left != 0 || s.right != 0) {
                all_silent = false;
            }
        }
        expect(all_silent, "IdlePostReset_ZeroCyclesPerSample_RemainsSilent");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_PsgAudioPump_Unit cases passed\n";
    return 0;
}
