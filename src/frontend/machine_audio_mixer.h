#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "frontend/psg_audio_pump.h"

namespace sony_msx::frontend {

// Deterministic, SDL3-independent machine audio mixer (M29-S5, docs/m29-
// planner-package.md §2.4): PSG + 0..2 attached Konami SCC wavetable chips
// (one per external cartridge bay -- real hardware mixes both slots'
// sound-in lines) into interleaved signed-16-bit stereo PCM.
//
// Placement/testability: lives in src/frontend/ (presentation-layer sample
// production, src/CLAUDE.md boundary) but is intentionally SDL3-independent
// so the whole mixing law is headlessly ctest-provable -- the exact M26-S5
// PsgAudioPump precedent. Sdl3AudioPresenter delegates its per-sample
// production here; DEC-0033's AudioPacer exact-accounting is UNTOUCHED (the
// pacer decides HOW MANY samples to pump; this class only produces them).
//
// Per planned sample: advance the PSG by cycles_per_sample via the wrapped,
// already-tested PsgAudioPump (genuine reuse), advance each attached
// SccWavetable by the SAME cycles_per_sample (both generators always track
// machine time together -- including when the pacer later trims the PUSHED
// output), then produce one stereo pair:
//
//   pcm = clamp_int16(psg_raw * kPsgAmplitudeScale + scc_sum * kSccAmplitudeScale)
//
// where scc_sum is the sum of every attached chip's mono AC sample()
// (SCC is mono: the same contribution lands on both channels).
//
// Amplitude constants (documented presentation policy, the same disclosed-
// simplification class as M26's kAmplitudeScale itself):
//   - kPsgAmplitudeScale = 400: UNCHANGED from M26 (sdl3_audio_presenter.h);
//     PSG raw range 0..62 -> 0..24,800.
//   - kSccAmplitudeScale = 12: SCC mono AC ~ +-600 (SCC fact-sheet §5)
//     -> +-7,200 per chip. Worst case 62*400 + 7,200*2 = 39,200 > 32,767,
//     so the int16 clamp is REQUIRED with two SCCs (risk R-M29-4) -- it is
//     unit-tested at a constructed saturation input. One SCC stays inside
//     int16 (24,800 + 7,200 = 32,000).
//
// HARD REGRESSION ORACLE (§2.4): with zero SCC sources attached, the output
// is byte-identical to the pre-M29 presenter arithmetic (psg_raw * 400 per
// channel, clamped) for ANY input sequence -- proven by a dedicated unit
// test, and structurally evident below (a null-only source array contributes
// an scc_sum of exactly 0 to every sample).
class MachineAudioMixer {
public:
    // Must match Sdl3AudioPresenter::kAmplitudeScale (static_assert'ed in
    // sdl3_audio_presenter.cpp under the SDL3 build).
    static constexpr int kPsgAmplitudeScale = 400;
    static constexpr int kSccAmplitudeScale = 12;

    // 0..2 attached chips; nullptr entries are skipped (the "no SCC cart
    // loaded" regression null, Hbf1xvMachine::scc_chip()'s own contract).
    using SccSources = std::array<devices::audio::SccWavetable*, 2>;

    // cycles_per_sample: the 3.58 MHz system-cycle delta advanced per sample
    // (the caller computes it; kCyclesPerSample = 81 inherits the disclosed
    // -3.6 cent integer simplification documented in sdl3_audio_presenter.h
    // -- NOT "fixed" here, DEC-0033 accounting untouched).
    explicit MachineAudioMixer(std::uint64_t cycles_per_sample);

    // Produces sample_count interleaved stereo pairs (2 * sample_count
    // int16 values), advancing psg and every non-null scc by
    // cycles_per_sample per pair.
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, std::size_t sample_count) const;

    [[nodiscard]] const PsgAudioPump& pump() const { return pump_; }
    [[nodiscard]] std::uint64_t cycles_per_sample() const { return pump_.cycles_per_sample(); }

private:
    PsgAudioPump pump_;
};

}  // namespace sony_msx::frontend
