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

void Sdl3AudioPresenter::pump_and_push_paced(devices::audio::PsgYm2149& psg,
                                             const std::uint64_t total_elapsed_cycles) {
    if (stream_ == nullptr) {
        return;
    }

    // Live host-queue depth (bytes of queued-but-unconsumed input data,
    // SDL_audio.h's SDL_GetAudioStreamQueued) in sample frames. Backpressure
    // is a function of THIS queue state only -- it never touches emulated
    // device state (determinism note in audio_pacer.h).
    const int queued_bytes = SDL_GetAudioStreamQueued(stream_);
    const std::uint64_t queued_samples =
        queued_bytes > 0 ? static_cast<std::uint64_t>(queued_bytes) / kBytesPerSampleFrame : 0;

    const AudioPacingDecision decision = pacer_.plan(total_elapsed_cycles, queued_samples);

    // Underrun-boundary re-prime silence goes FIRST (it stands in for already-
    // lost host time; real samples must land after it).
    if (decision.silence_samples_to_push > 0) {
        const std::vector<std::int16_t> silence(
            static_cast<std::size_t>(decision.silence_samples_to_push) * 2, std::int16_t{0});
        if (!SDL_PutAudioStreamData(stream_, silence.data(),
                                    static_cast<int>(silence.size() * sizeof(std::int16_t)))) {
            last_error_ = SDL_GetError();
        }
    }

    if (decision.samples_to_pump == 0) {
        return;
    }

    // ALWAYS pump the full batch: the PSG generator's notion of time stays in
    // lockstep with the machine's elapsed cycles even when the pushed output
    // is trimmed by backpressure.
    const std::vector<devices::audio::PsgYm2149::StereoSample> samples =
        pump_.pump_samples(psg, static_cast<std::size_t>(decision.samples_to_pump));

    if (decision.samples_to_push == 0) {
        return;
    }

    std::vector<std::int16_t> pcm;
    pcm.reserve(static_cast<std::size_t>(decision.samples_to_push) * 2);
    for (std::size_t i = 0; i < static_cast<std::size_t>(decision.samples_to_push); ++i) {
        const auto& s = samples[i];
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
