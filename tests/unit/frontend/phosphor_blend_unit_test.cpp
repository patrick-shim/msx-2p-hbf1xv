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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "frontend/phosphor_blend.h"

// Suite: Frontend_PhosphorBlend_Unit.
//
// Pure math unit test for the phosphor-persistence inter-frame blend
// (frontend/phosphor_blend.h) -- zero SDL3 dependency, headlessly testable, so
// it is registered OUTSIDE the SONY_MSX_ENABLE_SDL3 guard (mirrors the
// sdl3_cli_unit_test / border_composer_unit_test placement).
//
// Non-tautological: asserts CONCRETE blended channel values (not that the
// function echoes its input), the two exact endpoints (p=0 -> cur, p=100 ->
// prev -- the byte-identical off/full extremes), channel independence, and
// monotonicity of the blend across the full 0..100 sweep.

namespace {

using sony_msx::frontend::blend_rgb555;
using sony_msx::frontend::peak_rgb555;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Pack/unpack helpers for the XRGB1555 channels the blend operates on.
constexpr std::uint16_t rgb555(const int r, const int g, const int b) {
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}
constexpr int ch_r(const std::uint16_t px) { return (px >> 10) & 0x1F; }
constexpr int ch_g(const std::uint16_t px) { return (px >> 5) & 0x1F; }
constexpr int ch_b(const std::uint16_t px) { return px & 0x1F; }

}  // namespace

int main() {
    // A varied set of real RGB555 values incl. the all-0/all-1 boundary cases.
    const std::vector<std::uint16_t> samples = {
        0x0000, 0x7FFF, 0x7C00, 0x03E0, 0x001F, 0x4210, 0x1234, 0x2E5F, 0x55AA, 0x2A55,
    };

    // --- Case 1: p == 0 returns `cur` EXACTLY for every (prev,cur) pair. This
    // is the load-bearing "default OFF is byte-identical" property: the SDL3
    // present path gates behind persistence>0, but the math itself must also be
    // an exact identity at 0 so a blend that momentarily runs at 0 is a no-op. ---
    {
        bool all_ok = true;
        for (const std::uint16_t prev : samples) {
            for (const std::uint16_t cur : samples) {
                if (blend_rgb555(prev, cur, 0) != cur) {
                    all_ok = false;
                    std::cerr << "  p=0 not identity: prev=0x" << std::hex << prev << " cur=0x" << cur
                              << " -> 0x" << blend_rgb555(prev, cur, 0) << std::dec << "\n";
                }
            }
        }
        expect(all_ok, "P0_ReturnsCurExactly_ByteIdenticalOffPath");
    }

    // --- Case 2: p == 100 returns `prev` EXACTLY (the full-retention extreme). ---
    {
        bool all_ok = true;
        for (const std::uint16_t prev : samples) {
            for (const std::uint16_t cur : samples) {
                if (blend_rgb555(prev, cur, 100) != prev) {
                    all_ok = false;
                }
            }
        }
        expect(all_ok, "P100_ReturnsPrevExactly");
    }

    // --- Case 3: a concrete 50% blend of white over black. round((50*31 +
    // 50*0)/100) = 16 per channel -> 0x4210. Asserts a real computed value. ---
    {
        const std::uint16_t out = blend_rgb555(/*prev=*/0x7FFF, /*cur=*/0x0000, 50);
        expect(out == rgb555(16, 16, 16) && out == 0x4210, "P50_WhiteOverBlack_Is16PerChannel_0x4210");
    }

    // --- Case 4: channel independence -- pure red prev over pure blue cur at
    // p=50 blends R and B but leaves G at 0, with NO cross-channel bleed. ---
    {
        const std::uint16_t out = blend_rgb555(/*prev=*/0x7C00, /*cur=*/0x001F, 50);
        expect(ch_r(out) == 16 && ch_g(out) == 0 && ch_b(out) == 16 && out == 0x4010,
               "P50_RedOverBlue_ChannelIndependent_NoBleed");
    }

    // --- Case 5: output is always a valid RGB555 with bit 15 (the X) cleared,
    // and every channel within [0,31], across a full parameter sweep. ---
    {
        bool all_ok = true;
        for (const std::uint16_t prev : samples) {
            for (const std::uint16_t cur : samples) {
                for (int p = 0; p <= 100; ++p) {
                    const std::uint16_t out = blend_rgb555(prev, cur, p);
                    if ((out & 0x8000) != 0) all_ok = false;
                    if (ch_r(out) > 31 || ch_g(out) > 31 || ch_b(out) > 31) all_ok = false;
                }
            }
        }
        expect(all_ok, "AllP_Bit15Clear_ChannelsInRange");
    }

    // --- Case 6: MONOTONICITY. For a fixed (prev,cur), the per-channel blend is
    // monotonic in p: non-decreasing where prev_ch >= cur_ch, non-increasing
    // where prev_ch <= cur_ch. Swept over a prev whose channels straddle cur's
    // (R rising, G flat, B falling) so all three directions are exercised. ---
    {
        const std::uint16_t prev = rgb555(31, 10, 0);   // R high, G ==, B low  vs cur
        const std::uint16_t cur = rgb555(0, 10, 31);    // R low,  G ==, B high
        bool mono_ok = true;
        int prev_r = -1;
        int prev_b = 32;
        for (int p = 0; p <= 100; ++p) {
            const std::uint16_t out = blend_rgb555(prev, cur, p);
            if (ch_r(out) < prev_r) mono_ok = false;   // R must be non-decreasing
            if (ch_b(out) > prev_b) mono_ok = false;   // B must be non-increasing
            if (ch_g(out) != 10) mono_ok = false;      // G equal -> constant
            prev_r = ch_r(out);
            prev_b = ch_b(out);
        }
        // Endpoints anchor the sweep (p=0 -> cur, p=100 -> prev).
        expect(mono_ok, "Monotonic_R_NonDecreasing_B_NonIncreasing_G_Constant");
        expect(blend_rgb555(prev, cur, 0) == cur && blend_rgb555(prev, cur, 100) == prev,
               "Monotonic_Endpoints_AnchorCurAndPrev");
    }

    // --- Case 7: a value beyond [0,100] is clamped by the function (defensive:
    // the CLI already validates, but the pure fn must be total). ---
    {
        expect(blend_rgb555(0x7FFF, 0x0000, 200) == 0x7FFF, "OverMax_ClampedTo100_ReturnsPrev");
        expect(blend_rgb555(0x7FFF, 0x0000, -5) == 0x0000, "UnderMin_ClampedTo0_ReturnsCur");
    }

    // ======================================================================
    //  PEAK mode (peak_rgb555): peak-hold-with-decay. The contract is in
    //  frontend/phosphor_blend.h; every case below asserts a CONCRETE value or
    //  a strict inequality (non-tautological), never that the fn echoes input.
    // ======================================================================

    // --- Case 8 (PEAK idempotent): peak(x, x, p) == x for EVERY sample and
    // EVERY p. A static image must be untouched under peak-hold (no shimmer). ---
    {
        bool all_ok = true;
        for (const std::uint16_t x : samples) {
            for (int p = 0; p <= 100; ++p) {
                if (peak_rgb555(x, x, p) != x) {
                    all_ok = false;
                    std::cerr << "  peak(x,x,p) != x: x=0x" << std::hex << x << " p=" << std::dec << p << "\n";
                }
            }
        }
        expect(all_ok, "Peak_XX_IsX_ForAllP_Idempotent");
    }

    // --- Case 9 (PEAK anti-DIM, THE load-bearing property): a pixel bright in the
    // CURRENT frame over a BLACK retained is presented at FULL brightness for
    // every p -- peak(black, cur, p) == cur. This is exactly what fixes flicker
    // WITHOUT dimming, and it is the sharp contrast with Average mode, which at
    // the same inputs would return (100-p)% of cur (asserted alongside for p=50:
    // peak keeps full white 0x7FFF while average dims to 0x4210). ---
    {
        bool all_ok = true;
        for (const std::uint16_t cur : samples) {
            for (int p = 0; p <= 100; ++p) {
                if (peak_rgb555(/*prev=*/0x0000, cur, p) != cur) all_ok = false;
            }
        }
        expect(all_ok, "Peak_BlackPrev_Cur_IsCur_FullBrightness_AntiDim");
        // The concrete contrast at p=50: peak keeps full brightness, average dims.
        expect(peak_rgb555(0x0000, 0x7FFF, 50) == 0x7FFF, "Peak_BlackThenWhite_StaysFullWhite_0x7FFF");
        expect(blend_rgb555(0x0000, 0x7FFF, 50) == 0x4210,
               "Average_BlackThenWhite_DimsToHalf_0x4210_Contrast");
    }

    // --- Case 10 (PEAK == pure max at p=100): with no decay the retained peak is
    // held forever, so peak(a, b, 100) == channel-wise max(a, b). Swept over all
    // sample pairs against an independent max computed here. ---
    {
        bool all_ok = true;
        for (const std::uint16_t a : samples) {
            for (const std::uint16_t b : samples) {
                const int r = std::max(ch_r(a), ch_r(b));
                const int g = std::max(ch_g(a), ch_g(b));
                const int bl = std::max(ch_b(a), ch_b(b));
                if (peak_rgb555(a, b, 100) != rgb555(r, g, bl)) all_ok = false;
            }
        }
        expect(all_ok, "Peak_P100_IsChannelwiseMax_BrighterOf");
    }

    // --- Case 11 (PEAK identity at p=0): peak(prev, cur, 0) == cur for every
    // pair -- p==0 is the byte-identical OFF path (decayed collapses to cur, and
    // max(cur,cur)==cur), same guarantee the Average mode gives at 0. ---
    {
        bool all_ok = true;
        for (const std::uint16_t prev : samples) {
            for (const std::uint16_t cur : samples) {
                if (peak_rgb555(prev, cur, 0) != cur) all_ok = false;
            }
        }
        expect(all_ok, "Peak_P0_IsCur_ByteIdenticalOffPath");
    }

    // --- Case 12 (PEAK DECAY over frames): a retained bright pixel with a BLACK
    // current fades strictly toward black frame after frame and reaches 0 (the
    // trail fully clears -- the whole point of the floor decay). Iterate the IIR
    // exactly as the presenter does (retained := peak(retained, black, p)) from
    // full white at p=90, and assert (a) strict monotone decrease while >0, and
    // (b) it reaches 0 within a bounded number of frames. Also assert a HIGHER p
    // decays SLOWER (more retention) at the first step. ---
    {
        const int p = 90;
        int retained = 31;  // one channel, full brightness
        bool strictly_decreasing = true;
        int frames = 0;
        while (retained > 0 && frames < 500) {
            const std::uint16_t out = peak_rgb555(rgb555(retained, retained, retained), 0x0000, p);
            const int next = ch_r(out);
            if (next >= retained) strictly_decreasing = false;  // must strictly fall each off-frame
            retained = next;
            ++frames;
        }
        expect(strictly_decreasing, "Peak_Decay_BlackCur_StrictlyDecreasesEachFrame");
        expect(retained == 0 && frames < 500, "Peak_Decay_ReachesZero_TrailClears");
        // Retention rate is monotone in p: a bigger p keeps more of the peak on
        // the first off-frame (slower decay). peak(white, black, p) channel ==
        // floor(p*31/100); 90 -> 27, 50 -> 15, 10 -> 3.
        expect(ch_r(peak_rgb555(0x7FFF, 0x0000, 90)) == 27 &&
                   ch_r(peak_rgb555(0x7FFF, 0x0000, 50)) == 15 &&
                   ch_r(peak_rgb555(0x7FFF, 0x0000, 10)) == 3,
               "Peak_Decay_HigherP_RetainsMore_FloorRate");
    }

    // --- Case 13 (PEAK channel independence + validity): pure-red retained over
    // pure-blue current at p=100 -> max keeps R (from prev) and B (from cur),
    // leaves G at 0, no cross-channel bleed; and across a full sweep every output
    // is a valid RGB555 with bit 15 clear and channels in [0,31]. ---
    {
        const std::uint16_t out = peak_rgb555(/*prev=*/0x7C00, /*cur=*/0x001F, 100);
        expect(ch_r(out) == 31 && ch_g(out) == 0 && ch_b(out) == 31 && out == 0x7C1F,
               "Peak_RedPrev_BlueCur_ChannelIndependent_NoBleed");
        bool all_ok = true;
        for (const std::uint16_t prev : samples) {
            for (const std::uint16_t cur : samples) {
                for (int p = 0; p <= 100; ++p) {
                    const std::uint16_t px = peak_rgb555(prev, cur, p);
                    if ((px & 0x8000) != 0) all_ok = false;
                    if (ch_r(px) > 31 || ch_g(px) > 31 || ch_b(px) > 31) all_ok = false;
                }
            }
        }
        expect(all_ok, "Peak_AllP_Bit15Clear_ChannelsInRange");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_PhosphorBlend_Unit cases passed\n";
    return 0;
}
