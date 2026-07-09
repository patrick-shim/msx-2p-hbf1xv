#include "frontend/machine_audio_mixer.h"

#include <algorithm>

namespace sony_msx::frontend {

MachineAudioMixer::MachineAudioMixer(const std::uint64_t cycles_per_sample) : pump_(cycles_per_sample) {}

std::vector<std::int16_t> MachineAudioMixer::mix_interleaved_stereo(
    devices::audio::PsgYm2149& psg, const SccSources& sccs, const std::size_t sample_count) const {
    std::vector<std::int16_t> pcm;
    pcm.reserve(sample_count * 2);
    for (std::size_t i = 0; i < sample_count; ++i) {
        // PSG: the genuine M26-S5 pump (advance + sample), unchanged.
        const devices::audio::PsgYm2149::StereoSample s = pump_.pump_one_sample(psg);

        // SCCs: advance by the SAME per-sample delta, then take the mono AC
        // sum (SCC fact-sheet §5). A null entry contributes exactly 0 --
        // the zero-SCC path is therefore arithmetically identical to the
        // pre-M29 presenter loop (the hard regression oracle, §2.4).
        std::int32_t scc_sum = 0;
        for (devices::audio::SccWavetable* scc : sccs) {
            if (scc != nullptr) {
                scc->advance_cycles(pump_.cycles_per_sample());
                scc_sum += scc->sample();
            }
        }

        const std::int32_t scc_term = scc_sum * kSccAmplitudeScale;
        // The clamp is REQUIRED (R-M29-4): two SCCs + a loud PSG exceed
        // int16 (see header arithmetic). SCC is mono -- same term on both.
        const std::int32_t left =
            std::clamp(s.left * kPsgAmplitudeScale + scc_term, -32768, 32767);
        const std::int32_t right =
            std::clamp(s.right * kPsgAmplitudeScale + scc_term, -32768, 32767);
        pcm.push_back(static_cast<std::int16_t>(left));
        pcm.push_back(static_cast<std::int16_t>(right));
    }
    return pcm;
}

}  // namespace sony_msx::frontend
