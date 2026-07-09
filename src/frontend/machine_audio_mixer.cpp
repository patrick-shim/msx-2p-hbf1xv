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

        const std::int32_t scc_term = scc_sum * kSccAmplitudeScale;
        // The clamp is REQUIRED (R-M29-4 / R-M31-4): two SCCs + a loud PSG
        // exceed int16, and so can a loud FM mix (see header arithmetic).
        // SCC and FM are mono -- the same terms land on both channels.
        const std::int32_t left =
            std::clamp(s.left * kPsgAmplitudeScale + scc_term + fm_term, -32768, 32767);
        const std::int32_t right =
            std::clamp(s.right * kPsgAmplitudeScale + scc_term + fm_term, -32768, 32767);
        pcm.push_back(static_cast<std::int16_t>(left));
        pcm.push_back(static_cast<std::int16_t>(right));
    }
    return pcm;
}

}  // namespace sony_msx::frontend
