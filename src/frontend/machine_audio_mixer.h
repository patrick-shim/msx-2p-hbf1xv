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

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "devices/audio/click_dac.h"
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
// where scc_sum is the sum of every attached chip's mono AC contribution
// (SCC is mono: the same contribution lands on both channels).
//
// M34 (DEC-0043 Defect A, docs/m34-planner-package.md §2.3.6): psg_raw and
// each SCC contribution are the chips' EXACT box averages over the advanced
// window (PsgYm2149/SccWavetable::take_integrated_sample()), replacing the
// aliasing point samples; §2.3.4's fixed-point property keeps every
// constant/silent source byte-identical to before (the M29/M31 zero-source
// oracles survive in meaning and, for constant signals, in bytes). Sample
// COUNT accounting (AudioPacer, DEC-0033) is untouched -- this changes only
// how each sample's VALUE is produced.
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
// HARD REGRESSION ORACLE (M29 §2.4, re-grounded by M34 §2.5 row 3): with
// zero SCC sources attached, the output is byte-identical to the bare-pump
// presenter arithmetic (psg pump sample * 400 per channel, clamped) for ANY
// input sequence -- proven by a dedicated unit test, and structurally
// evident below (a null-only source array contributes an scc_sum of exactly
// 0 to every sample). Since M34, both sides of that oracle compute through
// the integrated-sample pump, so the meaning -- absent/silent sources
// contribute exactly 0 -- is unchanged.
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
//
// M37 Slice B (DEC-0055, R-D): a FOURTH+ source class -- the OPLL(s) of any
// inserted external Panasonic FM-PAC CARTRIDGE (a SECOND, independent YM2413
// per external cartridge bay; the real HB-F1XV's built-in MSX-MUSIC OPLL is
// the `fm` term above, and an inserted FM-PAC adds ANOTHER whole OPLL). These
// are mixed ADDITIVELY through an `FmSources` array (one slot per external
// bay, exactly the SccSources shape -- real hardware sums both bays' sound-
// out lines), reusing the SAME kFmAmplitudeScale and the SAME per-sample
// advance cadence as the built-in `fm` (no new scaling, no new DSP -- the
// planner's "wholly-additive follow-on", cartridge_fmpac_rom.h:61-62). The
// mixing law extends to:
//
//   pcm = clamp_int16(psg_raw * kPsgAmplitudeScale
//                     + scc_sum  * kSccAmplitudeScale
//                     + fm_sample     * kFmAmplitudeScale
//                     + fm_pac_sum    * kFmAmplitudeScale)
//
// where fm_pac_sum is the sum of every non-null FM-PAC OPLL's mono native
// sample (mono, like SCC/FM: the same term lands on both channels). Each
// non-null FM-PAC OPLL is advanced by the same cycles_per_sample as every
// other generator (generator time always tracks machine time).
//
// M37 HARD REGRESSION ORACLE (§ Deliverable): with NO FM-PAC inserted -- an
// all-null FmSources -- fm_pac_sum is exactly 0, so the output is byte-for-
// byte identical to the pre-M37 (v1.0.36) built-in-OPLL + PSG + SCC mix for
// ANY input sequence. The pre-M37 overloads (2/3/4-arg) delegate with an
// all-null FmSources and stay byte-behaviour identical (the M29/M31/M34
// oracles survive unchanged). CLAMP MATH: adding up to two more full-scale
// OPLLs only pushes the already-saturating worst case higher (27 aligned
// carriers max: 9 built-in + 9 + 9) -- the int16 clamp was already REQUIRED
// (R-M29-4/R-M31-4) and remains correct; realistic FM+FM-PAC music sits far
// below it.
class MachineAudioMixer {
public:
    // Must match Sdl3AudioPresenter::kAmplitudeScale (static_assert'ed in
    // sdl3_audio_presenter.cpp under the SDL3 build).
    static constexpr int kPsgAmplitudeScale = 400;
    static constexpr int kSccAmplitudeScale = 12;
    // M32 (Defect B of DEC-0039, D-2 ratified in RESP-M32-001; docs/
    // m32-planner-package.md §2.8): the honestly-derived FM presentation
    // scale, replacing M31's k=5 (which left the fully-functional YM2413
    // ~29 dB under the PSG -- coordinator-measured from the committed
    // debug/sounds/m31-fm-aleste-fmON/fmOFF.wav pair: FM peak 900 on the
    // +-32,767 scale vs PSG effects at 24,800).
    //
    // Reference ratio: openMSX's own machine definition balances this
    // exact machine's PSG:MSX-MUSIC at 21000:9000 = 7:3 (~-7.36 dB FM
    // under PSG) -- references/openmsx-21.0/share/machines/
    // Sony_HB-F1XV.xml:63 (<volume>21000</volume>) and :191
    // (<volume>9000</volume>).
    //
    // What the ratio applies to (source-verified): openMSX normalizes BOTH
    // chips to the same PER-CHANNEL unit before the XML volumes apply --
    // AY8910 per-channel volume table peaks at exactly 1.0 with device
    // amplification 1.0 (references/openmsx-21.0/src/sound/AY8910.cc:64-93,
    // :977-980); YM2413 per-slot linear peak is (1<<DB2LIN_AMP_BITS)-1 =
    // 255 with device amplification 1/256 (references/openmsx-21.0/src/
    // sound/YM2413Okazaki.cc:48, :154-165, :1051-1054) -> per-channel peak
    // ~= 1.0. So 21000:9000 is a PER-CHANNEL loudness ratio. (The
    // alternative worst-case-SUM normalization is rejected: it yields
    // k ~= 4.6 (melody) / 2.6 (rhythm) -- "no change or quieter",
    // reproducing exactly the flawed M31 arithmetic that caused the
    // defect. Worst-case sums belong in the CLAMP analysis below.)
    //
    // Our units: one full-volume PSG channel = 31 (psg_ym2149.h resolved
    // amplitude) x 400 = 12,400 PCM; one full-volume FM channel peaks at
    // +-256 (ym2413_synth.h: +-2048 operator width through the documented
    // >>3 presentation scale, anchored to the fact-sheet §7 measured
    // per-volume peak series "255, 180, 127, ..." --
    // references/fact-sheets/Yamaha YM2413 FM Chip.md:184; §7's "blend
    // ratio varies by machine" note at :186 is why an external reference
    // ratio is the right calibrator). Therefore:
    //
    //   kFmAmplitudeScale = round(400 * 31 * (9000/21000) / 256)
    //                     = round(12,400 * 0.42857 / 256)
    //                     = round(20.76) = 21
    //
    // Cross-check against DEC-0039's own full-scale form: the charter's
    // "24,800 * 9000/21000 ~= 10,600" -- 24,800 is TWO PSG channel-units
    // per stereo side, so the FM equivalent is two FM channel-units:
    // 2 * 256 * 21 = 10,752, within 1.2% (integer rounding of k).
    // Resulting single-channel ratio (256*21)/12,400 = 0.4335 vs reference
    // 0.42857 -> -7.26 dB vs -7.36 dB. FM rises 21/5 = 4.2x (+12.5 dB)
    // over v1.0.32.
    //
    // CLAMP MATH (redone honestly at k = 21): worst theoretical alignments
    // -- melody 9 * 256 * 21 = 48,384 (FM ALONE exceeds int16); rhythm
    // mode 6 * 256 + 5 * (256 * 2) = 4,096 raw (the fact-sheet §6 rhythm
    // x2 double-output law, Yamaha YM2413 FM Chip.md:179/:183) -> 4,096 *
    // 21 = 86,016; plus PSG 24,800 and two SCCs 14,400 = 125,216 >>
    // 32,767. The int16 clamp is therefore REQUIRED and is unit-tested at
    // a constructed worst case (machine_audio_mixer_fm_unit_test.cpp:
    // 9 aligned carriers + max PSG + two max SCCs -> exact clamp at
    // +32,767 on both stereo sides; FM-alone case clamps BOTH rails).
    // Realistic music sits far below (M31's measured raw FM peak 180 ->
    // ~3,780 at k = 21).
    static constexpr int kFmAmplitudeScale = 21;

    // M39-A (digitized-voice fix): the MSX 1-bit key-click DAC on PPI port-C
    // bit 7 -- a FIFTH additive source, the ONLY sub-frame audio path on a bare
    // HB-F1XV (games bit-bang bit 7 as PWM/PDM to synthesize sampled speech;
    // Aleste 2 "(c)1989 COMPILE" voice, Laydock speech). See ClickDac for the
    // openMSX KeyClick->DACSound grounding + the AC-coupled box reconstruction.
    //
    // AMPLITUDE (same PER-CHANNEL-loudness method as kFmAmplitudeScale). The
    // machine XML balances this exact machine's PSG:keyclick at 21000:16000
    // (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:63
    // <volume>21000</volume>; MSXPPI/KeyClick registers its DACSound8U with the
    // default 16000 -- KeyClick.cc:5-8 / the machine's <PPI> has no explicit
    // <volume>, so the SoundDevice default 16000 applies). Our full-scale PSG
    // per-channel unit is 31 * kPsgAmplitudeScale = 12,400 PCM. The keyclick
    // 1-bit DAC swings only 0x80..0xFF (128 of the 256 8-bit codes), peak
    // 0x7F/0x80 = 127/128 of half-scale (DACSound8U.cc:17-20 value-0x80).
    // Therefore, for a full-scale AC click sample (|ClickDac output| == kUnit):
    //
    //   kClickAmplitudeScale = round(12,400 * (16000/21000) * (127/128))
    //                        = round(9,447.62 * 0.992188)
    //                        = round(9,373.8) = 9,374
    //
    // The ClickDac returns a signed fixed-point sample in [-kUnit, +kUnit]
    // (kUnit = full scale); the mixer normalizes by kUnit, so a full-scale
    // click contributes +-9,374 PCM per (mono) rail. A HELD level contributes
    // ~0 (the ClickDac's DC-blocker), and an idle (never-toggled) bit 7
    // contributes EXACTLY 0 -- the byte-identity oracle below.
    static constexpr int kClickAmplitudeScale = 9374;

    // 0..2 attached chips; nullptr entries are skipped (the "no SCC cart
    // loaded" regression null, Hbf1xvMachine::scc_chip()'s own contract).
    using SccSources = std::array<devices::audio::SccWavetable*, 2>;

    // M37 Slice B: 0..2 inserted FM-PAC cartridge OPLLs (one per external
    // cartridge bay); nullptr entries are skipped (the "no FM-PAC cart in this
    // bay" regression null, Hbf1xvMachine::fmpac()'s own contract). Distinct
    // from the built-in MSX-MUSIC OPLL passed as `fm` -- these are the SECOND,
    // independent YM2413(s) the inserted cartridge(s) own.
    using FmSources = std::array<devices::audio::Ym2413Opll*, 2>;

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
    // Byte-behaviour identical to pre-M37: delegates below with an all-null
    // FmSources (no inserted FM-PAC -> fm_pac_sum == 0).
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* fm,
        std::size_t sample_count) const;

    // M37 Slice B: the FM-PAC overload. Adds 0..2 inserted FM-PAC cartridge
    // OPLLs as ADDITIVE sources, mixed with the SAME kFmAmplitudeScale and the
    // SAME per-sample advance cadence as the built-in `fm`. An all-null
    // fm_pacs contributes exactly 0 to every sample (the M37 hard regression
    // oracle -- byte-identical to the 4-arg overload); each non-null entry is
    // advanced by cycles_per_sample per pair, exactly like `fm` and the SCCs.
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* fm,
        const FmSources& fm_pacs, std::size_t sample_count) const;

    // M39-A: the FIVE-source overload adding the 1-bit key-click DAC. A nullptr
    // click contributes exactly 0 to every sample (the pre-M39 4-source overload
    // delegates here with click = nullptr and stays byte-behaviour identical);
    // an attached-but-idle ClickDac (no bit-7 toggles) ALSO contributes exactly
    // 0 (idle byte-identity). A non-null click is advanced by cycles_per_sample
    // per pair, exactly like every other generator (DEC-0033 lockstep).
    [[nodiscard]] std::vector<std::int16_t> mix_interleaved_stereo(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* fm,
        const FmSources& fm_pacs, devices::audio::ClickDac* click, std::size_t sample_count) const;

    // M39-A Fix B (the digitized-voice fix): produce ONE mixed stereo sample on
    // the INTERLEAVED production path, using the SAME mixing law as the batch
    // overloads but driven by MACHINE cycles instead of the fixed 81-cycle step.
    // The caller (SDL3 run_one_frame / the headless --audio-sync dump) weaves
    // this into its CPU-step loop so the PSG's sync-before-change writes
    // (psg.sync_to_cycle at each register write) land at their true sub-frame
    // position: `sample_boundary_cycle` is this sample's absolute machine-cycle
    // boundary and `window_cycles` = boundary - previous-boundary (the exact-
    // accounting ~81.16-cycle window, so the sample COUNT tracks machine time).
    //   - PSG: sync_to_cycle(boundary) finalizes the sub-frame-accurate integral,
    //     then take_integrated_sample(window). (The voice now survives.)
    //   - SCC/FM/FM-PAC/click: advance_cycles(window) + take, exactly as the
    //     batch path but over the true machine-cycle window (the click DAC's
    //     absolute-cycle edge timeline stays perfectly aligned -- no drift).
    // Byte-identity of the batch oracles is untouched (this is a separate path).
    [[nodiscard]] std::array<std::int16_t, 2> produce_synced_sample(
        devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* fm,
        const FmSources& fm_pacs, devices::audio::ClickDac* click,
        std::uint64_t sample_boundary_cycle, std::uint64_t window_cycles) const;

    [[nodiscard]] const PsgAudioPump& pump() const { return pump_; }
    [[nodiscard]] std::uint64_t cycles_per_sample() const { return pump_.cycles_per_sample(); }

private:
    PsgAudioPump pump_;
};

}  // namespace sony_msx::frontend
