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

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "frontend/audio_pacer.h"
#include "frontend/machine_audio_mixer.h"
#include "frontend/psg_audio_pump.h"

namespace sony_msx::frontend {

// Real SDL3 audio output (docs/m26-planner-package.md §2.6, extended M29 + M31).
// History: M26 shipped PSG-only, YM2413 deliberately silent (backlog E1 -- an ungrounded
// DSP pipeline was disallowed per R-M26-5); M29 added 0..2 Konami SCC sources via the
// SDL3-independent MachineAudioMixer; M31 (backlog E1 CLOSED, DEC-0035 RC) added YM2413
// (OPLL) as a third mixed source once a formula-grounded synth existed
// (src/devices/audio/ym2413_synth.h, docs/m31-planner-package.md §2.2/§2.4). The DSP
// itself stays in src/devices/; this file is thin SDL_AudioStream plumbing over the
// headlessly-tested mixer.
//
// PSG generator-advance wiring lives HERE in the frontend, never in src/machine/ or
// src/devices/ core (§2.6 point 1's rejected core-level alternative): a headless build has
// no use for PSG generator state, and this keeps the M9/M12/M23 CPU-timing oracle untouched.
//
// Sample-rate-paced, independent of CPU/video frame cadence (audio samples occur far more
// often than once per video frame). Wraps the headlessly-tested (M26-S5) SDL3-independent
// PsgAudioPump in real SDL_AudioStream plumbing (SDL_OpenAudioDeviceStream,
// SDL_ResumeAudioStreamDevice, SDL_PutAudioStreamData -- per references/sdl3/include/SDL3/
// SDL_audio.h, R-M26-6).
class Sdl3AudioPresenter {
public:
    static constexpr int kSampleRateHz = 44100;
    // The real MSX NTSC system clock (also independently declared in wd2793.h,
    // disk_drive.h, rp5c01.h, rensha_turbo.h).
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    // Per-sample GENERATOR-ADVANCE cycle delta (kSystemClockHz / kSampleRateHz, integer
    // division). Disclosed simplification: no fractional-cycle dithering at the sample-rate
    // boundary, distinct from PsgAudioPump's own exact intra-generator-step residual
    // accumulation (planner §2.6 point 3).
    //
    // IMPORTANT (docs/audio-latency-investigation.md): this is ONLY the per-sample generator
    // step -- it no longer derives how many samples a frame produces. That count now comes
    // from AudioPacer's exact cumulative accounting (floor(elapsed_cycles * 44100 / 3579545)),
    // because the old per-frame floor(59736/81) = 737 overproduced by ~1 sample/frame and
    // accumulated unbounded host-queue latency. Consequence of the 81-vs-81.1688
    // simplification: the PSG generator runs at 81/81.1688 = 99.792% of machine time (a
    // constant -3.6 cent pitch offset), while the sample count tracks machine time exactly.
    static constexpr std::uint64_t kCyclesPerSample = kSystemClockHz / kSampleRateHz;
    // Backpressure policy (presentation policy, not chip behavior -- docs/audio-latency-
    // investigation.md). Bounded-queue + drop-excess shape grounded in openMSX's SDL sound
    // driver: a ~69.7 ms ring that drops excess samples when full
    // (references/openmsx-21.0/src/sound/SDLSoundDriver.cc:43,152-155 + Mixer.cc:21-23):
    //   - low-water base latency ~33 ms (~2 video frames) -- primed with silence at underrun
    //     boundaries only;
    //   - hard queue cap ~67 ms (~4 video frames) -- production above it is trimmed so
    //     latency re-converges after host stalls.
    static constexpr std::uint64_t kLowWaterSamples = kSampleRateHz * 33 / 1000;   // 1455
    static constexpr std::uint64_t kMaxQueuedSamples = kSampleRateHz * 67 / 1000;  // 2954
    // Interleaved S16 stereo: bytes per sample frame on the host stream.
    static constexpr std::uint64_t kBytesPerSampleFrame = 2 * sizeof(std::int16_t);
    // Fixed linear amplitude scale from the PSG's 0..62 combined-channel range
    // (StereoSample.left/right = level[0]+level[1 or 2], each 0..31) into signed 16-bit PCM
    // headroom (62*400=24800, within int16's +-32767 range). Disclosed simplification: no
    // dynamic-range/loudness modeling.
    static constexpr int kAmplitudeScale = 400;

    Sdl3AudioPresenter();
    ~Sdl3AudioPresenter();

    Sdl3AudioPresenter(const Sdl3AudioPresenter&) = delete;
    Sdl3AudioPresenter& operator=(const Sdl3AudioPresenter&) = delete;

    // Opens the real SDL_AudioStream on the default playback device (`nullptr` callback --
    // samples are pushed manually via `pump_and_push()`, SDL_audio.h's documented "push"
    // pattern) and starts it. Returns false (and records last_error()) on any SDL3 call
    // failure.
    bool init();
    void shutdown();

    // Pumps `sample_count` real PSG samples (via the wrapped PsgAudioPump) and pushes them
    // as interleaved signed-16-bit stereo PCM (SDL_PutAudioStreamData). Disclosed
    // simplification: nearest-neighbor sampling, no anti-aliasing/interpolation (mirrors the
    // M21 renderer's disclosed simplifications).
    //
    // RAW, UNPACED push primitive (no exact accounting, no backpressure) -- kept for
    // plumbing tests; the real per-frame production path is pump_and_push_paced().
    void pump_and_push(devices::audio::PsgYm2149& psg, std::size_t sample_count);

    // The real per-frame production path (docs/audio-latency-investigation.md): derives
    // this batch's sample count from the machine's cumulative elapsed cycles via
    // AudioPacer's exact accounting, reads the live host-queue depth
    // (SDL_GetAudioStreamQueued) for backpressure, pushes any underrun-boundary re-prime
    // silence first, then pumps the full batch (PSG generator time always tracks machine
    // time) and pushes only what fits under the queue cap.
    //
    // M29-S5 (docs/m29-planner-package.md §2.4): per-sample production delegates to the
    // SDL3-independent MachineAudioMixer, which mixes 0..2 attached Konami SCC chips into
    // the PSG stream. AudioPacer::plan()'s call and inputs are unchanged (SCC generator
    // time, like PSG generator time, always tracks machine time -- DEC-0033 untouched). The
    // 2-argument overload preserves the pre-M29 signature verbatim, forwarding zero SCC
    // sources whose mixed output is byte-identical to the pre-M29 arithmetic (the mixer's
    // hard regression oracle).
    //
    // M31 (backlog E1, docs/m31-planner-package.md §2.2): the fm overload adds the YM2413
    // (OPLL) as a third mixed source, advanced by the same per-sample cycle delta as every
    // other source (DEC-0033 untouched). Narrower overloads forward fm = nullptr,
    // byte-identical to v1.0.31 (the M31 hard oracle).
    //
    // M37 Slice B (DEC-0055): the fm_pacs overload adds 0..2 inserted external FM-PAC
    // cartridge OPLLs as additive sources beyond the built-in `fm`, each advanced by the
    // same per-sample cycle delta and mixed with the same scale (DEC-0033 untouched).
    // Narrower overloads forward an all-null FmSources, byte-identical to v1.0.36 (the M37
    // hard oracle).
    void pump_and_push_paced(devices::audio::PsgYm2149& psg, std::uint64_t total_elapsed_cycles);
    void pump_and_push_paced(devices::audio::PsgYm2149& psg, const MachineAudioMixer::SccSources& sccs,
                             std::uint64_t total_elapsed_cycles);
    void pump_and_push_paced(devices::audio::PsgYm2149& psg, const MachineAudioMixer::SccSources& sccs,
                             devices::audio::Ym2413Opll* fm, std::uint64_t total_elapsed_cycles);
    void pump_and_push_paced(devices::audio::PsgYm2149& psg, const MachineAudioMixer::SccSources& sccs,
                             devices::audio::Ym2413Opll* fm, const MachineAudioMixer::FmSources& fm_pacs,
                             std::uint64_t total_elapsed_cycles);

    // M39-A Fix B: push a buffer of ALREADY-PRODUCED interleaved stereo samples
    // (2*N int16), applying the SAME AudioPacer backpressure/silence-prime
    // policy as pump_and_push_paced but WITHOUT mixing -- the caller
    // (Sdl3App::run_one_frame) produced them sub-frame-accurately via
    // MachineAudioMixer::produce_synced_sample so the software-PCM voice
    // survives. The produced count matches the pacer's exact-accounting count
    // by construction (both floor(elapsed*44100/3579545)), so the trim/prime
    // decision applies unchanged. Empty buffer / no stream -> no-op.
    void push_produced_paced(const std::vector<std::int16_t>& produced,
                             std::uint64_t total_elapsed_cycles);

    // M52 (DEC-0079, docs/m52-planner-package.md §2.1): the POST-MIX master gain,
    // a pure presentation scalar applied to the already-mixed interleaved-stereo
    // int16 PCM immediately before SDL_PutAudioStreamData. It is SEPARATE from and
    // AFTER the DEC-0039 MachineAudioMixer balance (which stays byte-identical at
    // full volume) and is NOT a change to kAmplitudeScale. `volume_percent` in
    // [0,100] (clamped); 100 = unity (the default, byte-identical to pre-M52). At
    // 100 every push short-circuits to the ORIGINAL bytes (no copy, no scaling);
    // below 100 the scaled samples are pushed instead (frontend/master_volume.h).
    // SDL3-only: the headless build never instantiates this presenter, so the
    // knob cannot reach a headless/ctest audio fixture (the determinism guard).
    void set_master_volume(int volume_percent);
    [[nodiscard]] int master_volume() const { return master_volume_; }

    [[nodiscard]] SDL_AudioStream* stream() const { return stream_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }
    // Kept accessor shape (M26): the pump is now owned by the mixer.
    [[nodiscard]] const PsgAudioPump& pump() const { return mixer_.pump(); }
    [[nodiscard]] const MachineAudioMixer& mixer() const { return mixer_; }
    [[nodiscard]] const AudioPacer& pacer() const { return pacer_; }

private:
    MachineAudioMixer mixer_{kCyclesPerSample};
    AudioPacer pacer_{kSampleRateHz, kSystemClockHz, kLowWaterSamples, kMaxQueuedSamples};
    SDL_AudioStream* stream_ = nullptr;
    std::string last_error_;
    // M52 (DEC-0079): the post-mix master gain percent (100 = unity default,
    // byte-identical to pre-M52). Set from config + Alt+D/Alt+U hotkeys.
    int master_volume_ = 100;
    // Reusable scratch for the sub-100 scaled push on the LIVE path
    // (push_produced_paced), so the caller's produced buffer is never mutated
    // (it is reused across frames) and no per-frame allocation is incurred.
    std::vector<std::int16_t> scaled_buffer_;
};

}  // namespace sony_msx::frontend
