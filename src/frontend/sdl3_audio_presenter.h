#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "devices/audio/psg_ym2149.h"
#include "frontend/audio_pacer.h"
#include "frontend/psg_audio_pump.h"

namespace sony_msx::frontend {

// Real SDL3 audio output for the PSG (YM2149) ONLY (docs/m26-planner-
// package.md §2.6). YM2413/FM-PAC is honestly, deliberately left SILENT in
// the mix: it has real register-file/channel/rhythm-decode fidelity (M17,
// backlog B3) but ZERO waveform-synthesis capability (backlog E1, still
// OPEN) -- binding it to real audio output here would require inventing an
// ungrounded DSP pipeline (log-sin/exp operator tables, the 128-level EG
// global-counter rate mechanism, AM/VIB LFOs, etc. -- YM2413 fact-sheet
// §4/§5/§7/§9), which this project's culture explicitly disallows (R-M26-5,
// a hard, non-negotiable constraint, independently re-checked at every
// review pass: this file touches NO ym2413_opll.* file and adds NO
// waveform/DSP-shaped code for YM2413 whatsoever).
//
// PSG generator-advance wiring lives HERE, in the frontend, never in
// src/machine/ or src/devices/ core code (§2.6 point 1's considered-and-
// rejected core-level alternative) -- a headless (non-SDL3) build has no use
// for PSG generator state, and this keeps the M9/M12/M23 zero-tolerance
// CPU-timing-oracle path completely untouched.
//
// Sample-rate-paced (independent of the CPU/video frame cadence -- audio
// samples occur far more often than once per video frame). Wraps the
// already-unit-tested-headlessly (M26-S5) SDL3-independent PsgAudioPump in
// the real SDL_AudioStream plumbing (SDL_OpenAudioDeviceStream,
// SDL_ResumeAudioStreamDevice, SDL_PutAudioStreamData -- all read directly
// from references/sdl3/include/SDL3/SDL_audio.h this cycle, R-M26-6).
class Sdl3AudioPresenter {
public:
    static constexpr int kSampleRateHz = 44100;
    // The real MSX NTSC system clock (X-pattern precedent: wd2793.h/
    // disk_drive.h/rp5c01.h/rensha_turbo.h all independently declare this
    // same constant).
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    // Per-sample GENERATOR-ADVANCE cycle delta, computed once (kSystemClockHz
    // / kSampleRateHz -- integer division, a documented, disclosed
    // simplification: no fractional-cycle dithering across the sample-rate
    // boundary itself, distinct from PsgAudioPump's own genuinely-exact
    // intra-generator-step residual accumulation, planner §2.6 point 3).
    //
    // IMPORTANT (audio-latency fix, docs/audio-latency-investigation.md):
    // this constant is ONLY the per-sample generator step. It is NO LONGER
    // used to derive how many samples a frame produces -- that count comes
    // from AudioPacer's exact cumulative accounting (floor(elapsed_cycles *
    // 44100 / 3579545)), because the old per-frame floor(59736/81) = 737
    // overproduced by ~1 sample/frame and accumulated unbounded host-queue
    // latency. Consequence of the disclosed 81-vs-81.1688 simplification: the
    // PSG generator experiences 81/81.1688 = 99.792% of machine time (a
    // constant -3.6 cent pitch offset), while the SAMPLE COUNT tracks machine
    // time exactly.
    static constexpr std::uint64_t kCyclesPerSample = kSystemClockHz / kSampleRateHz;
    // Backpressure policy (frontend presentation policy, not chip behavior --
    // docs/audio-latency-investigation.md; bounded-queue + drop-excess shape
    // grounded in openMSX's own SDL sound driver, ~69.7 ms ring that drops
    // excess samples when full, references/openmsx-21.0/src/sound/
    // SDLSoundDriver.cc:43,152-155 + Mixer.cc:21-23):
    //   - low-water base latency ~33 ms (~2 video frames) -- primed with
    //     silence at underrun boundaries only;
    //   - hard queue cap ~67 ms (~4 video frames) -- production above it is
    //     trimmed so latency re-converges after host stalls.
    static constexpr std::uint64_t kLowWaterSamples = kSampleRateHz * 33 / 1000;   // 1455
    static constexpr std::uint64_t kMaxQueuedSamples = kSampleRateHz * 67 / 1000;  // 2954
    // Interleaved S16 stereo: bytes per sample frame on the host stream.
    static constexpr std::uint64_t kBytesPerSampleFrame = 2 * sizeof(std::int16_t);
    // A fixed linear amplitude scale from the PSG's 0..62 combined-channel
    // range (StereoSample.left/right = level[0]+level[1 or 2], each 0..31)
    // into signed 16-bit PCM headroom (62*400=24800, well inside
    // int16's +-32767 range) -- a documented, disclosed simplification (no
    // dynamic-range/loudness modeling), consistent with this project's
    // established "documented simplification, not a fabricated fact"
    // standard.
    static constexpr int kAmplitudeScale = 400;

    Sdl3AudioPresenter();
    ~Sdl3AudioPresenter();

    Sdl3AudioPresenter(const Sdl3AudioPresenter&) = delete;
    Sdl3AudioPresenter& operator=(const Sdl3AudioPresenter&) = delete;

    // Opens the real SDL_AudioStream on the default playback device (a
    // `nullptr` callback -- samples are pushed manually via
    // `pump_and_push()`, SDL_audio.h's own documented "push" usage pattern)
    // and starts it. Returns false (and records last_error()) on any SDL3
    // call failure.
    bool init();
    void shutdown();

    // Pumps `sample_count` real PSG samples (via the wrapped PsgAudioPump)
    // and pushes them to the stream as interleaved signed-16-bit stereo PCM
    // (SDL_PutAudioStreamData). A documented, disclosed simplification:
    // nearest-neighbor sampling, no anti-aliasing/interpolation (mirrors the
    // M21 renderer's own several disclosed simplifications).
    //
    // RAW, UNPACED push primitive (no exact accounting, no backpressure) --
    // kept for plumbing tests; the real per-frame production path is
    // pump_and_push_paced().
    void pump_and_push(devices::audio::PsgYm2149& psg, std::size_t sample_count);

    // The real per-frame production path (audio-latency fix, docs/audio-
    // latency-investigation.md): derives this batch's sample count from the
    // machine's CUMULATIVE elapsed cycles via AudioPacer's exact accounting,
    // reads the live host-queue depth (SDL_GetAudioStreamQueued) for
    // backpressure, pushes any underrun-boundary re-prime silence first, then
    // pumps the full batch (PSG generator time ALWAYS tracks machine time)
    // and pushes only what fits under the queue cap.
    void pump_and_push_paced(devices::audio::PsgYm2149& psg, std::uint64_t total_elapsed_cycles);

    [[nodiscard]] SDL_AudioStream* stream() const { return stream_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }
    [[nodiscard]] const PsgAudioPump& pump() const { return pump_; }
    [[nodiscard]] const AudioPacer& pacer() const { return pacer_; }

private:
    PsgAudioPump pump_{kCyclesPerSample};
    AudioPacer pacer_{kSampleRateHz, kSystemClockHz, kLowWaterSamples, kMaxQueuedSamples};
    SDL_AudioStream* stream_ = nullptr;
    std::string last_error_;
};

}  // namespace sony_msx::frontend
