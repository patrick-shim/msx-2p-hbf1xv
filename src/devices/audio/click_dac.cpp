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

#include "devices/audio/click_dac.h"

#include "devices/audio/dwell_rounding.h"

namespace sony_msx::devices::audio {

void ClickDac::reset() {
    edges_.clear();
    consumed_cycle_ = 0;
    current_level_ = false;
    level_dwell_integral_ = 0;
    dc_acc_ = 0;
    edge_count_ = 0;
    // capture_enabled_ intentionally preserved (wiring config, not per-boot
    // device state -- see the header).
}

void ClickDac::record_edge(const std::uint64_t cycle, const bool level) {
    if (!capture_enabled_) {
        return;
    }
    edges_.push_back(Edge{cycle, level});
    ++edge_count_;
    if (edges_.size() > kMaxPendingEdges) {
        // Non-consuming safety net (see kMaxPendingEdges): fast-forward past
        // the oldest edge, keeping consumed_cycle_/current_level_ consistent
        // for any later advance. Only reachable when capture is on but nothing
        // consumes -- an inaudible configuration.
        const Edge oldest = edges_.front();
        if (oldest.cycle > consumed_cycle_) {
            consumed_cycle_ = oldest.cycle;
        }
        current_level_ = oldest.level;
        edges_.pop_front();
    }
}

void ClickDac::advance_cycles(const std::uint64_t window_cycles) {
    const std::uint64_t end = consumed_cycle_ + window_cycles;
    std::uint64_t t = consumed_cycle_;
    // Walk every edge that falls strictly inside [t, end): integrate the held
    // level over each dwell segment, then apply the edge's new level. An edge
    // exactly at `end` belongs to the NEXT window (boundary convention).
    while (!edges_.empty() && edges_.front().cycle < end) {
        const Edge edge = edges_.front();
        // Clamp: edges are recorded monotonically, so edge.cycle >= t, but
        // guard against any out-of-order stamp (dwell 0, level applied now).
        const std::uint64_t edge_cycle = edge.cycle < t ? t : edge.cycle;
        level_dwell_integral_ += static_cast<std::int64_t>(current_level_ ? kUnit : 0) *
                                 static_cast<std::int64_t>(edge_cycle - t);
        current_level_ = edge.level;
        t = edge_cycle;
        edges_.pop_front();
    }
    // Tail segment from the last edge (or the window start) to the window end.
    level_dwell_integral_ +=
        static_cast<std::int64_t>(current_level_ ? kUnit : 0) * static_cast<std::int64_t>(end - t);
    consumed_cycle_ = end;
}

std::int32_t ClickDac::take_integrated_sample(const std::uint64_t window_cycles) {
    if (window_cycles == 0) {
        // Idle-pump guard: no division, and DO NOT perturb the DC estimate
        // (the M34 take-API zero-window contract). integral is already 0.
        return 0;
    }
    // Box average of the level over the window -> [0, kUnit]. round-half-away-
    // from-zero via the shared M34 helper (integral is non-negative).
    const std::int64_t avg =
        round_div_half_away_from_zero(level_dwell_integral_, static_cast<std::int64_t>(window_cycles));
    level_dwell_integral_ = 0;

    // One-pole integer DC-blocker (AC coupling): subtract the slow DC estimate,
    // then relax the estimate toward `avg`. With avg constant, dc converges to
    // avg and the output y -> ~0 (a held level contributes nothing, mirroring
    // openMSX's blip-delta AC coupling); with avg == 0 forever, dc_acc_ stays
    // exactly 0 and y is exactly 0 (idle byte-identity).
    const std::int64_t dc = dc_acc_ >> kDcShift;  // dc_acc_ >= 0 so shift == floor
    const std::int64_t y = avg - dc;
    // Truncating (toward-zero) integer division -- well-defined on every
    // compiler, so replays stay byte-identical cross-platform.
    dc_acc_ += ((avg << kDcShift) - dc_acc_) / (static_cast<std::int64_t>(1) << kDcLog2Tau);
    return static_cast<std::int32_t>(y);
}

}  // namespace sony_msx::devices::audio
