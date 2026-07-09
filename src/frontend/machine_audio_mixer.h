#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
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
//
// M31 (backlog E1, docs/m31-planner-package.md §2.2): a THIRD source -- the
// YM2413 (OPLL) FM synthesis engine -- is added ADDITIVELY via a new
// overload taking a `Ym2413Opll* fm`. The mixing law extends the documented
// M29 pattern:
//
//   pcm = clamp_int16(psg_raw * kPsgAmplitudeScale
//                     + scc_sum * kSccAmplitudeScale
//                     + fm_sample * kFmAmplitudeScale)
//
// FM is mono (one summed OPLL output), so like SCC the same term lands on
// both channels. The FM device is advanced by the SAME cycles_per_sample as
// every other source (generator time always tracks machine time); with the
// standard 81-cycle step this is the planner's exact 9:8 decimation --
// 8 output samples x 81 cycles = 648 = 9 x 72-cycle native FM ticks, an
// exact repeating pattern (zero-order hold drops one native sample in nine;
// a disclosed simplification, §2.5 -- no band-limited resampling).
//
// M31 HARD REGRESSION ORACLE (§ S5): with `fm == nullptr` -- and separately
// with a real but never-keyed (silent) Ym2413Opll attached, whose
// fm_sample() is exactly 0 by the synth's idle-operator guarantee -- the
// output is byte-identical to the v1.0.31 3-arg arithmetic for ANY input
// sequence (the M29 zero-SCC oracle pattern). The 3-arg overload below
// delegates with fm = nullptr and stays byte-behaviour identical.
class MachineAudioMixer {
public:
    // Must match Sdl3AudioPresenter::kAmplitudeScale (static_assert'ed in
    // sdl3_audio_presenter.cpp under the SDL3 build).
    static constexpr int kPsgAmplitudeScale = 400;
    static constexpr int kSccAmplitudeScale = 12;
    // M31 documented presentation policy (same disclosed class as 400/12,
    // planner §2.2): FM per-channel peak ~ +-256 (the §7 measured-peaks
    // scale) -> +-1280 per loud melody channel; a realistic loud mix of ~6
    // melody channels + rhythm x2 can exceed int16 together with a loud PSG
    // (e.g. 9 aligned melody carriers 9*256*5 = 11,520 plus PSG max 24,800 =
    // 36,320 > 32,767), so the int16 clamp is REQUIRED and unit-tested at a
    // constructed saturation input (R-M31-4). §7's own "blend ratio varies
    // by machine" note covers this being a policy, not a measured constant.
    static constexpr int kFmAmplitudeScale = 5;

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
    // cycles_per_sample per pair. Byte-behaviour identical to pre-M31:
    // delegates below with fm = nullptr (M29's unit tests untouched).
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, std::size_t sample_count) const;

    // M31: the three-source overload. A nullptr fm contributes exactly 0 to
    // every sample (the hard regression oracle); a non-null fm is advanced
    // by cycles_per_sample per pair, exactly like the SCC sources.
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* fm,
        std::size_t sample_count) const;

    [[nodiscard]] const PsgAudioPump& pump() const { return pump_; }
    [[nodiscard]] std::uint64_t cycles_per_sample() const { return pump_.cycles_per_sample(); }

private:
    PsgAudioPump pump_;
};

}  // namespace sony_msx::frontend
