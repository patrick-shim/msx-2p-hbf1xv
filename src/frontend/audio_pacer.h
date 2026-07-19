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

#include <cstdint>

namespace sony_msx::frontend {

// One frame's audio-production plan (see AudioPacer::plan()).
struct AudioPacingDecision {
    // How many real PSG samples to PUMP this call (generator advance). Always
    // the full exact-accounting delta -- the PSG generator's notion of time
    // never diverges from the machine's elapsed cycles, regardless of what
    // the host audio queue looks like.
    std::uint64_t samples_to_pump = 0;
    // How many of the pumped samples (the first samples_to_push, in order) to
    // push to the host stream. samples_to_push <= samples_to_pump; the
    // difference is dropped to keep the host queue depth bounded by
    // max_queued_samples.
    std::uint64_t samples_to_push = 0;
    // How many SILENCE samples to push before the real samples. Non-zero only
    // at an underrun boundary (queued_samples == 0 at plan time: session
    // start, or the host drained the queue during a stall) -- restores the
    // low-water base latency so audio plays contiguously instead of
    // chronically riding the empty-queue edge.
    std::uint64_t silence_samples_to_push = 0;
};

// Exact-accounting, backpressure-capped audio pacing policy (SDL-free, so
// the whole decision procedure is headlessly ctest-provable -- mirrors the
// PsgAudioPump SDL3-independence precedent).
//
// Root cause this class fixes: the earlier frontend pushed
// floor(59736/81) = 737 samples per frame on a wall-clock frame cadence whose
// 16.688 ms period was TRUNCATED to 16 ms, and SDL_PutAudioStreamData queues
// unboundedly -- measured +1,307 samples/s of unbounded host-queue growth
// (+29.7 ms of audio latency per second of play).
//
// Policy (frontend presentation policy, NOT chip behavior; bounded-queue +
// drop-excess shape grounded in openMSX's own SDL sound driver, which sizes
// its ring buffer at 3 fragments of the default 1024 samples ~= 69.7 ms @
// 44.1 kHz and drops excess samples when full --
// openMSX 21.0: src/sound/SDLSoundDriver.cc:43,152-155 and
// src/sound/Mixer.cc:21-23):
//
//   1. EXACT ACCOUNTING -- cumulative samples due are derived from cumulative
//      emulated cycles: total_due = floor(elapsed_cycles * rate / clock),
//      integer math, no per-frame rounding drift over any run length.
//   2. BACKPRESSURE CAP -- never let the host queue exceed max_queued_samples;
//      excess production is pumped (generator time still advances) but its
//      pushed output is trimmed, so latency re-converges after host stalls
//      instead of accumulating permanently.
//   3. LOW-WATER RE-PRIME -- only when the queue is found EMPTY (an underrun
//      boundary; the device already played silence), silence is pushed to
//      restore low_water_samples of base latency. Never triggered by
//      ordinary jitter dips, so it cannot inject artificial gaps into an
//      otherwise healthy stream.
//
// Determinism: samples_to_pump (the only output that touches emulated device
// state) is a pure function of total_elapsed_cycles -- host-queue state only
// ever affects what is PUSHED to the host, never the PSG generator.
class AudioPacer {
public:
    AudioPacer(std::uint64_t sample_rate_hz, std::uint64_t system_clock_hz,
               std::uint64_t low_water_samples, std::uint64_t max_queued_samples);

    // Exact floor(cycles * numerator / denominator) without 128-bit
    // intermediates: split into whole-denominator quotient and remainder so
    // nothing overflows for any realistic session length (remainder product is
    // bounded by denominator * numerator; the quotient term overflows only
    // after ~4e14 emulated seconds at MSX rates). Shared by sample accounting
    // (numerator = sample rate) and the frame-pacing nanosecond deadline
    // (numerator = 1e9) so both use one tested primitive.
    [[nodiscard]] static constexpr std::uint64_t scale_cycles(const std::uint64_t cycles,
                                                              const std::uint64_t numerator,
                                                              const std::uint64_t denominator) {
        const std::uint64_t whole = cycles / denominator;
        const std::uint64_t rem = cycles % denominator;
        return whole * numerator + rem * numerator / denominator;
    }

    // floor(cycles * sample_rate_hz / system_clock_hz) -- the exact cumulative
    // sample count due after `cycles` emulated system cycles.
    [[nodiscard]] std::uint64_t cycles_to_samples(std::uint64_t cycles) const {
        return scale_cycles(cycles, sample_rate_hz_, system_clock_hz_);
    }

    // Plans one production batch. total_elapsed_cycles is the machine's
    // CUMULATIVE elapsed cycle count (monotonic across the session);
    // queued_samples is the host stream's current queue depth in sample
    // frames. Stateful: tracks how many samples have already been produced so
    // repeated calls with the same cycle count pump nothing.
    [[nodiscard]] AudioPacingDecision plan(std::uint64_t total_elapsed_cycles, std::uint64_t queued_samples);

    // Rewind the CUMULATIVE production accounting to a fresh baseline
    // (samples_produced_/dropped_/silence back to 0), keeping the fixed
    // rate + water-mark config. plan()'s exact accounting is keyed to the machine's
    // CUMULATIVE elapsed cycles via a monotonic guard (total_due > samples_produced_
    // pumps nothing). When a runtime power-cycle (Sdl3App::reset_machine) restarts
    // elapsed_cycles at 0, a surviving (huge) samples_produced_ makes that guard
    // false for as long as it takes the machine to re-accumulate past it -> minutes
    // of permanent post-reset silence. The presenter calls this so post-reset
    // elapsed (0..) and the pacer baseline (0) realign. (DEC-0085)
    void reset() {
        samples_produced_ = 0;
        samples_dropped_ = 0;
        silence_samples_pushed_ = 0;
    }

    [[nodiscard]] std::uint64_t samples_produced() const { return samples_produced_; }
    [[nodiscard]] std::uint64_t samples_dropped() const { return samples_dropped_; }
    [[nodiscard]] std::uint64_t silence_samples_pushed() const { return silence_samples_pushed_; }
    [[nodiscard]] std::uint64_t low_water_samples() const { return low_water_samples_; }
    [[nodiscard]] std::uint64_t max_queued_samples() const { return max_queued_samples_; }

private:
    std::uint64_t sample_rate_hz_;
    std::uint64_t system_clock_hz_;
    std::uint64_t low_water_samples_;
    std::uint64_t max_queued_samples_;
    std::uint64_t samples_produced_ = 0;
    std::uint64_t samples_dropped_ = 0;
    std::uint64_t silence_samples_pushed_ = 0;
};

}  // namespace sony_msx::frontend
