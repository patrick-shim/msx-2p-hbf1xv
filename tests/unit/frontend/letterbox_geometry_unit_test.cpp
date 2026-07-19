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

#include <cmath>
#include <iostream>
#include <string>

#include "frontend/letterbox_geometry.h"

// Suite: Frontend_LetterboxGeometry_Unit (M57, DEC-0085, docs/m57-planner-
// package.md §4.3). The PERMANENT DEF-2 inset-geometry oracle -- the pure,
// SDL-free letterbox_into_band() fit math. Closes the "tests green but the menu
// obscures the picture" blind spot: it proves the picture rect always sits BELOW
// the menu strip (zero overlap), aspect-preserved, and that the inset==0 fallback
// reproduces the legacy full-window letterbox. SDL-free -> registered OUTSIDE the
// SONY_MSX_ENABLE_SDL3 guard (counts in BOTH denominators).

namespace {

using sony_msx::frontend::geometry::letterbox_into_band;
using sony_msx::frontend::geometry::Rect;

int g_failures = 0;

void expect(const bool ok, const std::string& case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

bool near(const double a, const double b, const double eps = 1e-6) {
    return std::fabs(a - b) <= eps;
}

// The universal DEF-2 safety invariants for a menu-present (top_inset > 0) fit:
//   * the rect never overlaps the strip band [0, top_inset)  (zero hidden pixels)
//   * the rect stays fully inside the window
//   * the aspect is exactly 4:3 (320:240)
void expect_below_strip_and_4_3(const Rect& r, const int win_w, const int win_h,
                                const int top_inset, const std::string& tag) {
    expect(r.y >= static_cast<double>(top_inset) - 1e-6, tag + ":y>=top_inset(zero overlap)");
    expect(r.y + r.h <= static_cast<double>(win_h) + 1e-6, tag + ":bottom<=win_h");
    expect(r.x >= -1e-6, tag + ":x>=0");
    expect(r.x + r.w <= static_cast<double>(win_w) + 1e-6, tag + ":right<=win_w");
    expect(r.h > 0.0 && near(r.w / r.h, 320.0 / 240.0, 1e-4), tag + ":aspect==4:3");
}

}  // namespace

int main() {
    // --- Case A: NOMINAL grown window (the --scale N default) --------------
    // set_scale grows the window to 320N x (240N + h); the band {0,h,320N,240N} is
    // EXACTLY 4:3, so the picture fills the band: y == top_inset, x == 0, no bars.
    {
        const Rect r = letterbox_into_band(960, 739, 19);  // scale 3 + 19px strip
        expect(near(r.x, 0.0), "A_grown_x==0");
        expect(near(r.y, 19.0), "A_grown_y==top_inset");
        expect(near(r.w, 960.0), "A_grown_w==band_w");
        expect(near(r.h, 720.0), "A_grown_h==240N");
        expect_below_strip_and_4_3(r, 960, 739, 19, "A_grown");
    }
    // Every --scale 1..8 grown window is an exact-4:3 band -> picture fills it.
    for (int n = 1; n <= 8; ++n) {
        const int inset = 19;
        const Rect r = letterbox_into_band(320 * n, 240 * n + inset, inset);
        const std::string tag = "A_scale" + std::to_string(n);
        expect(near(r.x, 0.0), tag + "_x==0");
        expect(near(r.y, static_cast<double>(inset)), tag + "_y==top_inset");
        expect(near(r.w, static_cast<double>(320 * n)), tag + "_w==320N");
        expect(near(r.h, static_cast<double>(240 * n)), tag + "_h==240N");
        expect_below_strip_and_4_3(r, 320 * n, 240 * n + inset, inset, tag);
    }

    // --- Case B: LANDSCAPE/WIDE band (fullscreen on a 16:9 display) --------
    // Band wider than 4:3 -> height-bound: picture fills band height (y==top_inset)
    // and pillarboxes horizontally. This is the fullscreen case where the window
    // CANNOT grow -- the band math insets within the fixed surface.
    {
        const int W = 1920, H = 1080, inset = 19;
        const Rect r = letterbox_into_band(W, H, inset);
        expect(near(r.y, static_cast<double>(inset)), "B_wide_y==top_inset");
        expect(near(r.h, static_cast<double>(H - inset)), "B_wide_h==band_h(height-bound)");
        expect(r.w < static_cast<double>(W), "B_wide_pillarboxed");
        expect(near(r.x, (static_cast<double>(W) - r.w) / 2.0), "B_wide_x_centered");
        expect_below_strip_and_4_3(r, W, H, inset, "B_wide");
    }

    // --- Case C: PORTRAIT/NARROW band (a tall dragged window) --------------
    // Band narrower than 4:3 -> width-bound: picture fills band width and is
    // centered vertically WITHIN the band (y > top_inset), still zero overlap.
    {
        const int W = 400, H = 900, inset = 19;
        const Rect r = letterbox_into_band(W, H, inset);
        expect(near(r.w, static_cast<double>(W)), "C_portrait_w==band_w(width-bound)");
        expect(near(r.h, static_cast<double>(W) * 240.0 / 320.0), "C_portrait_h==w*3/4");
        expect(r.y > static_cast<double>(inset) + 1e-6, "C_portrait_y>top_inset(centered)");
        expect_below_strip_and_4_3(r, W, H, inset, "C_portrait");
    }

    // --- Case D: inset == 0 REPRODUCES the FULL-WINDOW letterbox -----------
    // The hidden-window / no-menu fallback: band == whole window, centered fit ==
    // exactly what SDL's LOGICAL_PRESENTATION_LETTERBOX of 320x240 produces.
    {
        // Exact 4:3 window -> fills it entirely (x=y=0).
        const Rect r43 = letterbox_into_band(800, 600, 0);
        expect(near(r43.x, 0.0) && near(r43.y, 0.0) && near(r43.w, 800.0) && near(r43.h, 600.0),
               "D_inset0_exact43_fills_window");
        // Wider than 4:3 -> height-bound, horizontal bars: pillarbox centered.
        const Rect rw = letterbox_into_band(1000, 600, 0);
        expect(near(rw.h, 600.0) && near(rw.w, 800.0) && near(rw.x, 100.0) && near(rw.y, 0.0),
               "D_inset0_wide_pillarbox_centered");
        // Taller than 4:3 -> width-bound, vertical bars: letterbox centered.
        const Rect rt = letterbox_into_band(600, 800, 0);
        expect(near(rt.w, 600.0) && near(rt.h, 450.0) && near(rt.x, 0.0) && near(rt.y, 175.0),
               "D_inset0_tall_letterbox_centered");
        // inset 0 must equal the SAME window with the standard 4:3 fit (legacy).
        expect(near(rw.y, 0.0), "D_inset0_top_at_zero(legacy geometry)");
    }

    // --- Case E: degenerate inputs never invert the geometry ---------------
    {
        const Rect z1 = letterbox_into_band(0, 600, 0);       // zero width
        const Rect z2 = letterbox_into_band(800, 10, 19);     // inset taller than window
        const Rect z3 = letterbox_into_band(800, 19, 19);     // inset == window height (no band)
        expect(near(z1.w, 0.0) && near(z1.h, 0.0), "E_zero_width_no_draw");
        expect(near(z2.w, 0.0) && near(z2.h, 0.0), "E_inset_gt_window_no_draw");
        expect(near(z3.w, 0.0) && near(z3.h, 0.0), "E_inset_eq_window_no_draw");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_LetterboxGeometry_Unit cases passed\n";
    return 0;
}
