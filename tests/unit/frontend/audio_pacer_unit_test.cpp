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

#include <cstdint>
#include <iostream>

#include "frontend/audio_pacer.h"

// Suite: Frontend_AudioPacer_Unit (audio-latency fix, docs/audio-latency-
// investigation.md).
//
// The M26 frontend derived per-frame audio production from a per-frame
// rounded count (floor(59736/81) = 737 samples/frame vs the exact 735.948)
// on a ms-truncated wall-clock cadence, with NO backpressure on
// SDL_PutAudioStreamData's unbounded queue -- measured +1,307 samples/s of
// monotonic host-queue growth (+29.7 ms of audio latency per second of
// play). AudioPacer is the SDL-free exact-accounting + bounded-queue policy
// that replaces it. This test proves, headlessly and deterministically:
//   1. cycles_to_samples is the exact floor(cycles * rate / clock) with no
//      drift over a simulated multi-hour session (independent incremental
//      numerator oracle);
//   2. per-frame pump deltas are only ever 735 or 736 (never the old 737);
//   3. the backpressure cap and underrun-boundary low-water re-prime behave
//      exactly as documented, and generator time (samples_produced) NEVER
//      diverges from emulated time regardless of host-queue state;
//   4. the same scale_cycles primitive gives the exact nanosecond frame
//      deadline run_interactive() paces with (16,688,154 ns/frame, not the
//      old truncated 16 ms).

namespace {

using sony_msx::frontend::AudioPacer;
using sony_msx::frontend::AudioPacingDecision;

constexpr std::uint64_t kRate = 44100;
constexpr std::uint64_t kClock = 3579545;
constexpr std::uint64_t kFrameCycles = 228 * 262;  // 59736
constexpr std::uint64_t kLowWater = kRate * 33 / 1000;   // 1455 (~33 ms)
constexpr std::uint64_t kCap = kRate * 67 / 1000;        // 2954 (~67 ms)

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Case 1: ExactAccounting_SimulatedFourHourSession_NoDriftVsIncrementalOracle ---
    // Independent oracle: accumulate the numerator incrementally
    // (num += kFrameCycles * kRate per frame, then due = num / kClock) --
    // overflow-safe for this horizon (862k frames * 2.63e9 ~= 2.3e15 << 2^64)
    // and mathematically identical to floor(total_cycles * rate / clock).
    {
        AudioPacer pacer(kRate, kClock, kLowWater, kCap);
        constexpr std::uint64_t kFrames = 862'000;  // ~4 emulated hours at 59.92 Hz
        std::uint64_t cycles = 0;
        std::uint64_t numerator = 0;
        std::uint64_t produced = 0;
        std::uint64_t min_delta = ~0ull;
        std::uint64_t max_delta = 0;
        bool all_match = true;
        for (std::uint64_t f = 0; f < kFrames; ++f) {
            cycles += kFrameCycles;
            numerator += kFrameCycles * kRate;
            const std::uint64_t oracle_due = numerator / kClock;
            const AudioPacingDecision d = pacer.plan(cycles, /*queued_samples=*/0);
            produced += d.samples_to_pump;
            if (produced != oracle_due || pacer.cycles_to_samples(cycles) != oracle_due) {
                all_match = false;
            }
            min_delta = d.samples_to_pump < min_delta ? d.samples_to_pump : min_delta;
            max_delta = d.samples_to_pump > max_delta ? d.samples_to_pump : max_delta;
        }
        expect(all_match, "ExactAccounting_SimulatedFourHourSession_NoDriftVsIncrementalOracle");
        expect(pacer.samples_produced() == produced && produced == numerator / kClock,
               "ExactAccounting_SamplesProducedCounter_MatchesOracleExactly");
        // --- Case 2: per-frame deltas are only ever 735/736, never the old 737. ---
        expect(min_delta == 735 && max_delta == 736,
               "PerFramePumpDelta_AlwaysExact735Or736_NeverLegacy737");
    }

    // --- Case 3: scale_cycles spot values (sample and nanosecond scaling). ---
    {
        expect(AudioPacer::scale_cycles(kClock, kRate, kClock) == kRate,
               "ScaleCycles_OneEmulatedSecond_Exactly44100Samples");
        expect(AudioPacer::scale_cycles(kFrameCycles, kRate, kClock) == 735,
               "ScaleCycles_OneFrame_735Samples_FloorOf735p948");
        // 59736 * 1e9 / 3579545 = 16,688,154.500... -> floor 16,688,154 ns
        // (the run_interactive() deadline step; the old code truncated this
        // to a 16 ms SDL_Delay target, +4.3% speed error).
        expect(AudioPacer::scale_cycles(kFrameCycles, 1'000'000'000ull, kClock) == 16'688'154,
               "ScaleCycles_FramePeriodNs_Exactly16688154_NotTruncated16ms");
        // Overflow safety of the split arithmetic: one emulated YEAR of cycles
        // scaled to ns (naive cycles*1e9 would overflow 2^64 ~600x over).
        constexpr std::uint64_t kYearCycles = kClock * 86400ull * 365ull;
        expect(AudioPacer::scale_cycles(kYearCycles, 1'000'000'000ull, kClock) ==
                   86400ull * 365ull * 1'000'000'000ull,
               "ScaleCycles_OneEmulatedYearToNs_NoOverflow_ExactResult");
    }

    // --- Case 4: Backpressure cap semantics. ---
    {
        AudioPacer pacer(kRate, kClock, kLowWater, kCap);
        // Empty queue, one frame due: everything fits (735 < cap), plus
        // low-water re-prime silence topping the push up to exactly kLowWater.
        AudioPacingDecision d = pacer.plan(kFrameCycles, /*queued_samples=*/0);
        expect(d.samples_to_pump == 735 && d.samples_to_push == 735,
               "Plan_EmptyQueueOneFrame_PushesFullPump");
        expect(d.silence_samples_to_push == kLowWater - 735,
               "Plan_EmptyQueueOneFrame_RePrimesSilenceToExactlyLowWater");

        // Queue at the cap: pump still advances generator time in full, push
        // is fully trimmed, drop is accounted.
        d = pacer.plan(2 * kFrameCycles, /*queued_samples=*/kCap);
        expect(d.samples_to_pump == 736 && d.samples_to_push == 0 && d.silence_samples_to_push == 0,
               "Plan_QueueAtCap_PumpsFullDelta_PushesNothing");
        expect(pacer.samples_dropped() == 736, "Plan_QueueAtCap_DropAccountedExactly");

        // Queue just under the cap: push exactly the remaining room, never
        // exceeding the cap; no silence (queue not empty).
        d = pacer.plan(3 * kFrameCycles, /*queued_samples=*/kCap - 100);
        expect(d.samples_to_pump == 736 && d.samples_to_push == 100 && d.silence_samples_to_push == 0,
               "Plan_QueueJustUnderCap_PushesExactRoomOnly");

        // Generator time NEVER diverged from emulated time across all of the
        // above, regardless of queue state.
        expect(pacer.samples_produced() == pacer.cycles_to_samples(3 * kFrameCycles),
               "Plan_AnyQueueState_GeneratorTimeTracksEmulatedTimeExactly");
    }

    // --- Case 5: Monotonic guard -- a repeated cycle count pumps nothing. ---
    {
        AudioPacer pacer(kRate, kClock, kLowWater, kCap);
        (void)pacer.plan(kFrameCycles, kLowWater);
        const AudioPacingDecision d = pacer.plan(kFrameCycles, kLowWater);
        expect(d.samples_to_pump == 0 && d.samples_to_push == 0 && d.silence_samples_to_push == 0,
               "Plan_RepeatedCycleCount_PumpsNothing");
    }

    // --- Case 6: Host-stall re-convergence -- a 2-second cycle jump with a
    // drained queue pushes only up to the cap, drops the rest, and generator
    // time still tracks emulated time exactly (latency stays bounded instead
    // of accumulating permanently -- the fix's core guarantee). ---
    {
        AudioPacer pacer(kRate, kClock, kLowWater, kCap);
        const std::uint64_t two_seconds = 2 * kClock;
        const AudioPacingDecision d = pacer.plan(two_seconds, /*queued_samples=*/0);
        expect(d.samples_to_pump == 2 * kRate, "Plan_TwoSecondStall_PumpsFullExactDelta");
        expect(d.samples_to_push == kCap, "Plan_TwoSecondStall_PushCappedAtMaxQueue");
        expect(d.silence_samples_to_push == 0, "Plan_TwoSecondStall_NoSilenceWhenPushAlreadyAboveLowWater");
        expect(pacer.samples_dropped() == 2 * kRate - kCap, "Plan_TwoSecondStall_ExcessDroppedExactly");
        expect(pacer.samples_produced() == 2 * kRate,
               "Plan_TwoSecondStall_GeneratorTimeStillTracksEmulatedTime");
    }

    // --- Case 7: Low-water normalization -- a low-water above the cap is
    // clamped to the cap, and silence + push never exceed the cap. ---
    {
        AudioPacer pacer(kRate, kClock, /*low_water_samples=*/kCap + 500, /*max_queued_samples=*/kCap);
        expect(pacer.low_water_samples() == kCap, "Ctor_LowWaterAboveCap_ClampedToCap");
        const AudioPacingDecision d = pacer.plan(kFrameCycles, /*queued_samples=*/0);
        expect(d.samples_to_push + d.silence_samples_to_push <= kCap,
               "Plan_LowWaterClamped_SilencePlusPushNeverExceedCap");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_AudioPacer_Unit cases passed\n";
    return 0;
}
