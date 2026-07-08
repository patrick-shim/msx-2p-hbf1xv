#include "frontend/sdl3_audio_presenter.h"

#include <algorithm>
#include <vector>

namespace sony_msx::frontend {

Sdl3AudioPresenter::Sdl3AudioPresenter() = default;

Sdl3AudioPresenter::~Sdl3AudioPresenter() {
    shutdown();
}

bool Sdl3AudioPresenter::init() {
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kSampleRateHz};
    // A nullptr callback: this presenter manually pushes samples via
    // pump_and_push()/SDL_PutAudioStreamData (SDL_audio.h:2028-2031's own
    // documented "push" usage pattern) -- never a pull callback.
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_ == nullptr) {
        last_error_ = SDL_GetError();
        return false;
    }
    // The device begins paused (SDL_audio.h:2011-2014) -- must be explicitly
    // resumed for audio to actually flow.
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        last_error_ = SDL_GetError();
        return false;
    }
    return true;
}

void Sdl3AudioPresenter::shutdown() {
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);  // Also closes the bound device (SDL_audio.h:2033-2034).
        stream_ = nullptr;
    }
}

void Sdl3AudioPresenter::pump_and_push(devices::audio::PsgYm2149& psg, const std::size_t sample_count) {
    if (stream_ == nullptr || sample_count == 0) {
        return;
    }

    const std::vector<devices::audio::PsgYm2149::StereoSample> samples = pump_.pump_samples(psg, sample_count);

    std::vector<std::int16_t> pcm;
    pcm.reserve(samples.size() * 2);
    for (const auto& s : samples) {
        const std::int32_t left = std::clamp(s.left * kAmplitudeScale, -32768, 32767);
        const std::int32_t right = std::clamp(s.right * kAmplitudeScale, -32768, 32767);
        pcm.push_back(static_cast<std::int16_t>(left));
        pcm.push_back(static_cast<std::int16_t>(right));
    }

    if (!SDL_PutAudioStreamData(stream_, pcm.data(), static_cast<int>(pcm.size() * sizeof(std::int16_t)))) {
        last_error_ = SDL_GetError();
    }
}

}  // namespace sony_msx::frontend
