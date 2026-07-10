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

#include "frontend/machine_audio_mixer.h"

#include <algorithm>

namespace sony_msx::frontend {

MachineAudioMixer::MachineAudioMixer(const std::uint64_t cycles_per_sample) : pump_(cycles_per_sample) {}

std::vector<std::int16_t> MachineAudioMixer::mix_interleaved_stereo(
    devices::audio::PsgYm2149& psg, const SccSources& sccs, const std::size_t sample_count) const {
    // M31: byte-behaviour identical delegation (a nullptr fm contributes an
    // exact 0 term to every sample -- the hard regression oracle).
    return mix_interleaved_stereo(psg, sccs, nullptr, sample_count);
}

std::vector<std::int16_t> MachineAudioMixer::mix_interleaved_stereo(
    devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* const fm,
    const std::size_t sample_count) const {
    // M37: byte-behaviour identical delegation (an all-null FmSources
    // contributes an exact 0 fm_pac term to every sample -- the M37 hard
    // regression oracle; the built-in-OPLL + PSG + SCC mix is untouched).
    return mix_interleaved_stereo(psg, sccs, fm, FmSources{nullptr, nullptr}, sample_count);
}

std::vector<std::int16_t> MachineAudioMixer::mix_interleaved_stereo(
    devices::audio::PsgYm2149& psg, const SccSources& sccs, devices::audio::Ym2413Opll* const fm,
    const FmSources& fm_pacs, const std::size_t sample_count) const {
    std::vector<std::int16_t> pcm;
    pcm.reserve(sample_count * 2);
    for (std::size_t i = 0; i < sample_count; ++i) {
        // PSG: the genuine M26-S5 pump (advance + sample), unchanged.
        const devices::audio::PsgYm2149::StereoSample s = pump_.pump_one_sample(psg);

        // SCCs: advance by the SAME per-sample delta, then take the mono AC
        // box average over that window (M34, DEC-0043 Defect A --
        // SccWavetable::take_integrated_sample(); the §2.3.4 fixed-point
        // property keeps every constant-output SCC byte-identical to the
        // pre-M34 point sample). A null entry contributes exactly 0 -- the
        // zero-SCC path remains arithmetically identical to the pre-M29
        // presenter loop for ANY input (the hard regression oracle, M29
        // §2.4, re-grounded by M34: both sides of that oracle now compute
        // through the same integrated pump, and a silent/absent source
        // still contributes exactly 0).
        std::int32_t scc_sum = 0;
        for (devices::audio::SccWavetable* scc : sccs) {
            if (scc != nullptr) {
                scc->advance_cycles(pump_.cycles_per_sample());
                scc_sum += scc->take_integrated_sample(pump_.cycles_per_sample());
            }
        }

        // FM (M31): advance the OPLL by the SAME per-sample delta (one
        // native 72-cycle tick per 72 accumulated cycles -- the exact 9:8
        // decimation at the standard 81-cycle step, planner §2.5) and take
        // the zero-order-held native sample. nullptr (or a silent, never-
        // keyed device, whose fm_sample() is exactly 0) contributes exactly
        // 0 -- the M31 hard regression oracle.
        std::int32_t fm_term = 0;
        if (fm != nullptr) {
            fm->advance_cycles(pump_.cycles_per_sample());
            fm_term = fm->fm_sample() * kFmAmplitudeScale;
        }

        // FM-PAC (M37 Slice B): each inserted external FM-PAC cartridge owns
        // its OWN, second YM2413 OPLL. Advance every non-null one by the SAME
        // per-sample delta as the built-in `fm` (identical 9:8 decimation
        // cadence) and sum their mono native samples, then apply the SAME
        // kFmAmplitudeScale (reused, not a new scaling). An all-null fm_pacs
        // (no FM-PAC inserted) sums to exactly 0 -> byte-identical to the
        // pre-M37 mix for ANY input (the M37 hard regression oracle). FM-PAC
        // output is mono, so like SCC/FM the same term lands on both channels.
        std::int32_t fm_pac_sum = 0;
        for (devices::audio::Ym2413Opll* fm_pac : fm_pacs) {
            if (fm_pac != nullptr) {
                fm_pac->advance_cycles(pump_.cycles_per_sample());
                fm_pac_sum += fm_pac->fm_sample();
            }
        }
        const std::int32_t fm_pac_term = fm_pac_sum * kFmAmplitudeScale;

        const std::int32_t scc_term = scc_sum * kSccAmplitudeScale;
        // The clamp is REQUIRED (R-M29-4 / R-M31-4, extended M37): two SCCs +
        // a loud PSG exceed int16, and so can a loud built-in-FM + FM-PAC mix
        // (see header arithmetic). SCC and FM/FM-PAC are mono -- the same
        // terms land on both channels.
        const std::int32_t left = std::clamp(
            s.left * kPsgAmplitudeScale + scc_term + fm_term + fm_pac_term, -32768, 32767);
        const std::int32_t right = std::clamp(
            s.right * kPsgAmplitudeScale + scc_term + fm_term + fm_pac_term, -32768, 32767);
        pcm.push_back(static_cast<std::int16_t>(left));
        pcm.push_back(static_cast<std::int16_t>(right));
    }
    return pcm;
}

}  // namespace sony_msx::frontend
