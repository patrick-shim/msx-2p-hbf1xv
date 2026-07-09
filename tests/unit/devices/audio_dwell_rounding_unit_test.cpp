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

#include "devices/audio/dwell_rounding.h"

// Suite: Devices_AudioDwellRounding_Unit (M34-S1, DEC-0043 Defect A,
// docs/m34-planner-package.md §2.3.4 / test matrix row 3)
//
// The ONE shared integer rounding helper both M34 take_integrated_sample()
// APIs (PsgYm2149, SccWavetable) divide through. Every expectation below is
// hand-computed arithmetic (authored BEFORE execution, R-M34-9 anti-tautology
// rule): round-half-away-from-zero, (2*sum + sign(sum)*window)/(2*window).

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    using sony_msx::devices::audio::round_div_half_away_from_zero;

    // --- Exact divisions pass through unchanged. ---
    expect(round_div_half_away_from_zero(0, 5) == 0, "Zero_AnyWindow_IsZero");
    expect(round_div_half_away_from_zero(81, 81) == 1, "Exact_81Over81_Is1");
    expect(round_div_half_away_from_zero(-81, 81) == -1, "Exact_Neg81Over81_IsNeg1");
    expect(round_div_half_away_from_zero(62 * 81, 81) == 62, "Exact_MaxPsgConstant_Is62");

    // --- Nearest-integer behavior away from ties (hand-computed):
    //     1/3 = 0.333 -> 0; 2/3 = 0.667 -> 1; mirrored for negatives. ---
    expect(round_div_half_away_from_zero(1, 3) == 0, "Nearest_1Over3_RoundsDownTo0");
    expect(round_div_half_away_from_zero(2, 3) == 1, "Nearest_2Over3_RoundsUpTo1");
    expect(round_div_half_away_from_zero(-1, 3) == 0, "Nearest_Neg1Over3_RoundsTo0");
    expect(round_div_half_away_from_zero(-2, 3) == -1, "Nearest_Neg2Over3_RoundsToNeg1");
    // 1023/81 = 12.63 -> 13; 1457/81 = 17.99 -> 18 (the PSG period-1 W=81
    // dwell oracle's own first two values, cross-anchored here).
    expect(round_div_half_away_from_zero(1023, 81) == 13, "Nearest_1023Over81_Is13");
    expect(round_div_half_away_from_zero(1457, 81) == 18, "Nearest_1457Over81_Is18");

    // --- The ±ties (only possible for even windows): half rounds AWAY from
    //     zero. 3/2 = 1.5 -> 2; 5/2 = 2.5 -> 3; -3/2 -> -2; -5/2 -> -3. ---
    expect(round_div_half_away_from_zero(3, 2) == 2, "TiePositive_3Over2_RoundsAwayTo2");
    expect(round_div_half_away_from_zero(5, 2) == 3, "TiePositive_5Over2_RoundsAwayTo3");
    expect(round_div_half_away_from_zero(-3, 2) == -2, "TieNegative_Neg3Over2_RoundsAwayToNeg2");
    expect(round_div_half_away_from_zero(-5, 2) == -3, "TieNegative_Neg5Over2_RoundsAwayToNeg3");
    // Tie at window 16 (the PSG generator step): 8/16 = 0.5 -> 1; 24/16 =
    // 1.5 -> 2.
    expect(round_div_half_away_from_zero(8, 16) == 1, "Tie_8Over16_RoundsAwayTo1");
    expect(round_div_half_away_from_zero(24, 16) == 2, "Tie_24Over16_RoundsAwayTo2");

    // --- THE FIXED-POINT PROPERTY (§2.3.4, the property the whole M34
    //     oracle re-baseline rests on): for EVERY constant level L held over
    //     the whole window, round(L*W, W) == L exactly -- proven across the
    //     full PSG summed-level range [0, 62] and the SCC AC extremes
    //     [-600, 595], for both an odd (81) and an even (16) window. ---
    {
        bool fixed_point = true;
        for (std::int64_t level = -600; level <= 600; ++level) {
            if (round_div_half_away_from_zero(level * 81, 81) != level ||
                round_div_half_away_from_zero(level * 16, 16) != level) {
                fixed_point = false;
            }
        }
        expect(fixed_point, "FixedPoint_ConstantLevel_IntegratesToExactlyItself_AllLevels");
    }

    // --- W = 1 degenerates to the identity (the psg_audio_dump per-cycle
    //     case, §2.3.6). ---
    expect(round_div_half_away_from_zero(31, 1) == 31 &&
               round_div_half_away_from_zero(-120, 1) == -120,
           "WindowOne_Identity");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_AudioDwellRounding_Unit cases passed\n";
    return 0;
}
