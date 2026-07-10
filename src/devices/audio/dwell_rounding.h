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

namespace sony_msx::devices::audio {

// Shared integer rounding helper for the M34 level x dwell box-average
// integration APIs (docs/m34-planner-package.md §2.3.4; DEC-0043 Defect A).
//
// EXACT FORM (per the §2.3.4 contract): round-half-away-from-zero,
//
//   round(sum / window) = (2*sum + sign(sum)*window) / (2*window)
//
// evaluated in exact int64 arithmetic (no floats in the sample path,
// DEC-0043 risk note 3). Examples:  3/2 -> 2,  -3/2 -> -2,  1/3 -> 0,
// 2/3 -> 1, -2/3 -> -1 (ties move AWAY from zero; all other values to the
// nearest integer).
//
// Preconditions (asserted by construction at every call site):
//   * window > 0 (the zero-window guard lives in the take-APIs, which return
//     silence WITHOUT dividing -- §2.3.5);
//   * |sum| <= 62 * window (PSG: two 5-bit channel levels per stereo side)
//     or |sum| <= 600 * window (SCC: five channels of |adjust| <= 120), so
//     2*|sum| + window never approaches INT64_MAX for any realistic window.
//
// FIXED-POINT PROPERTY (the §2.3.4 guarantee the M34 oracle re-baseline
// rests on): for any constant summed level L held across the whole window,
// sum == L * window, and
//   (2*L*window + window) / (2*window) == L   exactly
// (unit-proven in tests/unit/devices/audio_dwell_rounding_unit_test.cpp) --
// a constant/silent source integrates to EXACTLY its point-sample value.
[[nodiscard]] constexpr std::int64_t round_div_half_away_from_zero(const std::int64_t sum,
                                                                   const std::int64_t window) {
    if (sum >= 0) {
        return (2 * sum + window) / (2 * window);
    }
    return -((-2 * sum + window) / (2 * window));
}

}  // namespace sony_msx::devices::audio
