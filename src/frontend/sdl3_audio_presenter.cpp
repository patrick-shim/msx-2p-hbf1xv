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

#include "frontend/sdl3_audio_presenter.h"

#include <algorithm>
#include <vector>

namespace sony_msx::frontend {

// The two constants are one presentation policy declared in two
// SDL3-independence-separated places (M29-S5); they must never drift apart.
static_assert(Sdl3AudioPresenter::kAmplitudeScale == MachineAudioMixer::kPsgAmplitudeScale,
              "presenter/mixer PSG amplitude scales must match (byte-identity oracle)");

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

    // Zero-SCC mix == the pre-M29 psg_raw * kAmplitudeScale arithmetic,
    // byte-for-byte (MachineAudioMixer's hard regression oracle, M29-S5).
    const std::vector<std::int16_t> pcm =
        mixer_.mix_interleaved_stereo(psg, MachineAudioMixer::SccSources{nullptr, nullptr}, sample_count);

    if (!SDL_PutAudioStreamData(stream_, pcm.data(), static_cast<int>(pcm.size() * sizeof(std::int16_t)))) {
        last_error_ = SDL_GetError();
    }
}

void Sdl3AudioPresenter::pump_and_push_paced(devices::audio::PsgYm2149& psg,
                                             const std::uint64_t total_elapsed_cycles) {
    // Pre-M29 signature preserved verbatim: zero SCC sources (byte-identical
    // output to v1.0.29 -- the mixer's regression oracle).
    pump_and_push_paced(psg, MachineAudioMixer::SccSources{nullptr, nullptr}, total_elapsed_cycles);
}

void Sdl3AudioPresenter::pump_and_push_paced(devices::audio::PsgYm2149& psg,
                                             const MachineAudioMixer::SccSources& sccs,
                                             const std::uint64_t total_elapsed_cycles) {
    // Pre-M31 signature preserved verbatim: no FM source (byte-identical
    // output to v1.0.31 -- the mixer's M31 hard regression oracle).
    pump_and_push_paced(psg, sccs, nullptr, total_elapsed_cycles);
}

void Sdl3AudioPresenter::pump_and_push_paced(devices::audio::PsgYm2149& psg,
                                             const MachineAudioMixer::SccSources& sccs,
                                             devices::audio::Ym2413Opll* const fm,
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

    // ALWAYS pump the full batch: EVERY generator's (PSG + attached SCCs' +
    // FM's) notion of time stays in lockstep with the machine's elapsed
    // cycles even when the pushed output is trimmed by backpressure
    // (M29-S5/M31-S5; the DEC-0033 invariant extended uniformly).
    const std::vector<std::int16_t> pcm = mixer_.mix_interleaved_stereo(
        psg, sccs, fm, static_cast<std::size_t>(decision.samples_to_pump));

    if (decision.samples_to_push == 0) {
        return;
    }

    // Push only the FIRST samples_to_push pairs (2 int16 values each) of the
    // fully-pumped batch -- exactly the pre-M29 trim behaviour.
    const auto push_bytes = static_cast<int>(static_cast<std::size_t>(decision.samples_to_push) * 2 *
                                             sizeof(std::int16_t));
    if (!SDL_PutAudioStreamData(stream_, pcm.data(), push_bytes)) {
        last_error_ = SDL_GetError();
    }
}

}  // namespace sony_msx::frontend
