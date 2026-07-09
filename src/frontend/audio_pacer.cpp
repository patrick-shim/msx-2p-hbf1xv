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

#include "frontend/audio_pacer.h"

#include <algorithm>

namespace sony_msx::frontend {

AudioPacer::AudioPacer(const std::uint64_t sample_rate_hz, const std::uint64_t system_clock_hz,
                       const std::uint64_t low_water_samples, const std::uint64_t max_queued_samples)
    : sample_rate_hz_(sample_rate_hz),
      system_clock_hz_(system_clock_hz),
      // Normalize: the low-water re-prime level can never exceed the cap.
      low_water_samples_(std::min(low_water_samples, max_queued_samples)),
      max_queued_samples_(max_queued_samples) {}

AudioPacingDecision AudioPacer::plan(const std::uint64_t total_elapsed_cycles, const std::uint64_t queued_samples) {
    AudioPacingDecision decision;

    // 1. Exact accounting: cumulative due count from cumulative cycles, no
    //    per-frame rounding drift. Monotonic guard: a repeated/stale cycle
    //    count pumps nothing (never rewinds samples_produced_).
    const std::uint64_t total_due = cycles_to_samples(total_elapsed_cycles);
    if (total_due > samples_produced_) {
        decision.samples_to_pump = total_due - samples_produced_;
        samples_produced_ = total_due;
    }

    // 2. Backpressure cap: push only into the remaining queue room; trim the
    //    rest (generator time still advanced via samples_to_pump).
    const std::uint64_t room = queued_samples >= max_queued_samples_ ? 0 : max_queued_samples_ - queued_samples;
    decision.samples_to_push = std::min(decision.samples_to_pump, room);
    samples_dropped_ += decision.samples_to_pump - decision.samples_to_push;

    // 3. Low-water re-prime, ONLY at an underrun boundary (empty queue at plan
    //    time). Bounded by the cap: silence + push never exceeds room.
    if (queued_samples == 0 && decision.samples_to_push < low_water_samples_) {
        decision.silence_samples_to_push =
            std::min(low_water_samples_ - decision.samples_to_push, room - decision.samples_to_push);
        silence_samples_pushed_ += decision.silence_samples_to_push;
    }

    return decision;
}

}  // namespace sony_msx::frontend
