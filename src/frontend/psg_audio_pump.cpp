#include "frontend/psg_audio_pump.h"

namespace sony_msx::frontend {

PsgAudioPump::PsgAudioPump(const std::uint64_t cycles_per_sample) : cycles_per_sample_(cycles_per_sample) {}

devices::audio::PsgYm2149::StereoSample PsgAudioPump::pump_one_sample(devices::audio::PsgYm2149& psg) const {
    psg.advance_cycles(cycles_per_sample_);
    return psg.sample();
}

std::vector<devices::audio::PsgYm2149::StereoSample> PsgAudioPump::pump_samples(
    devices::audio::PsgYm2149& psg, const std::size_t sample_count) const {
    std::vector<devices::audio::PsgYm2149::StereoSample> out;
    out.reserve(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        out.push_back(pump_one_sample(psg));
    }
    return out;
}

}  // namespace sony_msx::frontend
