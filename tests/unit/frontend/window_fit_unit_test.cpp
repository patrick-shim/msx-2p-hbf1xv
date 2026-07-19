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
#include "frontend/window_fit.h"

// Suite: Frontend_WindowFit_Unit (M61, DEC-0090). The PERMANENT display-fit
// oracle -- the pure, SDL-free fit_window_to_display() math that keeps the
// interactive SDL3 window from opening larger than the display's usable bounds
// (owner Pi 4B + 800x480 panel: the 960x739 default overflowed and the menu bar
// landed off-screen). SDL-free -> registered OUTSIDE the SONY_MSX_ENABLE_SDL3
// guard (counts in BOTH the ON and OFF denominators).
//
// The interactive/visual confirmation on the actual Pi panel is deliberately
// OUT of scope here (owner manual smoke, the M59-device evidence boundary);
// this test proves the fit math + its hand-off contract with the M57 letterbox.

namespace {

using sony_msx::frontend::geometry::fit_window_to_display;
using sony_msx::frontend::geometry::letterbox_into_band;
using sony_msx::frontend::geometry::Rect;
using sony_msx::frontend::geometry::WindowFit;

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

// The universal M61 safety invariant: the effective window never exceeds
// positive usable bounds, and never exceeds the requested window either
// (the fit shrinks or keeps -- it NEVER grows a request).
void expect_never_exceeds(const WindowFit& f, const int req_w, const int req_h_with_bar,
                          const int usable_w, const int usable_h, const std::string& tag) {
    expect(f.w <= usable_w, tag + ":w<=usable_w");
    expect(f.h <= usable_h, tag + ":h<=usable_h");
    expect(f.w <= req_w, tag + ":w<=requested_w");
    expect(f.h <= req_h_with_bar, tag + ":h<=requested_h");
    expect(f.w >= 0 && f.h >= 0, tag + ":non_negative");
    expect(f.scale >= 1, tag + ":scale>=1");
}

}  // namespace

int main() {
    // --- Case A: FITS-AS-IS -- the request fits, so it is returned UNCHANGED --
    {
        // The Windows-desktop default: 960x720 picture + 19px bar on 1920x1040.
        const WindowFit f = fit_window_to_display(960, 720, 19, 1920, 1040);
        expect(f.w == 960 && f.h == 739, "A_fits_unchanged_960x739");
        expect(f.scale == 3, "A_fits_scale3");
        expect(!f.clamped, "A_fits_not_clamped");
        expect_never_exceeds(f, 960, 739, 1920, 1040, "A_fits");
    }
    {
        // EXACT-fit boundary: usable == requested window exactly -> unchanged.
        const WindowFit f = fit_window_to_display(960, 720, 19, 960, 739);
        expect(f.w == 960 && f.h == 739 && f.scale == 3 && !f.clamped, "A_exact_boundary_unchanged");
    }
    {
        // A small request on a huge display NEVER upscales (scale 1 stays 1).
        const WindowFit f = fit_window_to_display(320, 240, 19, 3840, 2160);
        expect(f.w == 320 && f.h == 259 && f.scale == 1 && !f.clamped, "A_no_upscale_scale1");
    }
    {
        // Non-canonical programmatic size that fits -> preserved verbatim.
        const WindowFit f = fit_window_to_display(1000, 700, 19, 1920, 1040);
        expect(f.w == 1000 && f.h == 719 && !f.clamped, "A_noncanonical_preserved");
    }
    {
        // bar_h == 0 (no menu): pure picture fit.
        const WindowFit f = fit_window_to_display(960, 720, 0, 1920, 1040);
        expect(f.w == 960 && f.h == 720 && f.scale == 3 && !f.clamped, "A_bar0_unchanged");
    }

    // --- Case B: THE OWNER'S PI CASE -- 960x739 requested on 800x480 usable ---
    // Largest fitting integer scale: n_by_w = 800/320 = 2, n_by_h = (480-19)/240
    // = 1 -> scale 1 -> window 320x259. This is the exact diagnostic-line
    // scenario from DEC-0090: "display usable 800x480; requested 960x739;
    // window 320x259 (scale 1)".
    {
        const WindowFit f = fit_window_to_display(960, 720, 19, 800, 480);
        expect(f.w == 320, "B_pi_w==320");
        expect(f.h == 259, "B_pi_h==259");
        expect(f.scale == 1, "B_pi_scale==1");
        expect(!f.clamped, "B_pi_not_clamped(scale1_fits)");
        expect_never_exceeds(f, 960, 739, 800, 480, "B_pi");
        // The fitted window's picture band {0,19,320,240} is EXACT 4:3, so the
        // M57 letterbox fills it edge to edge: the whole picture is visible.
        const Rect r = letterbox_into_band(f.w, f.h, 19);
        expect(near(r.x, 0.0) && near(r.y, 19.0) && near(r.w, 320.0) && near(r.h, 240.0),
               "B_pi_letterbox_fills_band");
    }

    // --- Case C: TOO WIDE -- width bounds the scale-down --------------------
    {
        // 960x739 requested; usable 700x1080: n_by_w = 700/320 = 2 binds
        // (n_by_h = (1080-19)/240 = 4) -> 640x499 scale 2.
        const WindowFit f = fit_window_to_display(960, 720, 19, 700, 1080);
        expect(f.w == 640 && f.h == 499 && f.scale == 2 && !f.clamped, "C_width_bound_scale2");
        expect_never_exceeds(f, 960, 739, 700, 1080, "C_width_bound");
    }

    // --- Case D: TOO TALL -- height bounds the scale-down -------------------
    {
        // 960x739 requested; usable 1920x500: n_by_h = (500-19)/240 = 2 binds
        // (n_by_w = 6) -> 640x499 scale 2.
        const WindowFit f = fit_window_to_display(960, 720, 19, 1920, 500);
        expect(f.w == 640 && f.h == 499 && f.scale == 2 && !f.clamped, "D_height_bound_scale2");
        expect_never_exceeds(f, 960, 739, 1920, 500, "D_height_bound");
    }

    // --- Case E: CLAMP -- even scale 1 overflows ----------------------------
    {
        // usable 300x200: 320 > 300 -> n_by_w = 0 -> clamp to min(requested,
        // usable) = 300x200 (fills the screen without exceeding it).
        const WindowFit f = fit_window_to_display(960, 720, 19, 300, 200);
        expect(f.w == 300 && f.h == 200, "E_clamped_300x200");
        expect(f.scale == 1 && f.clamped, "E_clamped_flag_scale1");
        expect_never_exceeds(f, 960, 739, 300, 200, "E_clamped");
        // THE M57 HAND-OFF CONTRACT: the letterbox fits a fully-visible 4:3
        // picture into the band below the bar of the CLAMPED window. Band =
        // 300x181, wider than 4:3 -> height-bound: h = 181, w = 241.33, centered
        // -- below the strip, inside the window, exact 4:3.
        const Rect r = letterbox_into_band(f.w, f.h, 19);
        expect(r.y >= 19.0 - 1e-6, "E_letterbox_below_strip");
        expect(r.x >= -1e-6 && r.x + r.w <= 300.0 + 1e-6, "E_letterbox_inside_w");
        expect(r.y + r.h <= 200.0 + 1e-6, "E_letterbox_inside_h");
        expect(r.h > 0.0 && near(r.w / r.h, 320.0 / 240.0, 1e-4), "E_letterbox_aspect_4_3");
        expect(near(r.h, 181.0), "E_letterbox_height_bound_band");
    }
    {
        // Width fits scale 1 but the band is squeezed: usable 310x1000 ->
        // n_by_w = 0 (310 < 320) -> clamp w to 310; h = min(739, 1000) = 739.
        // Band 310x720 is NARROWER than 4:3 -> width-bound letterbox: w = 310,
        // h = 232.5, centered in the band (the M57 Case-C geometry).
        const WindowFit f = fit_window_to_display(960, 720, 19, 310, 1000);
        expect(f.w == 310 && f.h == 739 && f.clamped, "E2_clamped_narrow");
        expect_never_exceeds(f, 960, 739, 310, 1000, "E2_clamped_narrow");
        const Rect r = letterbox_into_band(f.w, f.h, 19);
        expect(near(r.w, 310.0), "E2_letterbox_width_bound");
        expect(near(r.h, 310.0 * 240.0 / 320.0), "E2_letterbox_h==w*3/4");
        expect(r.y > 19.0 + 1e-6, "E2_letterbox_centered_below_strip");
    }
    {
        // Pathological: the bar alone is taller than the usable height. Clamp
        // yields a bar-only sliver; the letterbox degenerates to a zero-size
        // rect (draw nothing) -- the M57 Case-E discipline, never inverted.
        const WindowFit f = fit_window_to_display(960, 720, 19, 400, 15);
        expect(f.w == 400 && f.h == 15 && f.clamped, "E3_bar_taller_than_display");
        expect_never_exceeds(f, 960, 739, 400, 15, "E3_bar_taller");
        const Rect r = letterbox_into_band(f.w, f.h, 19);
        expect(near(r.w, 0.0) && near(r.h, 0.0), "E3_letterbox_degenerate_no_draw");
    }

    // --- Case F: NEVER-EXCEEDS property sweep -------------------------------
    // Every requested scale x bar x usable combination on a coarse grid obeys
    // the invariant; and whenever the result is NOT clamped, the picture band
    // (h - bar) is an exact multiple of 240 with w the matching multiple of 320.
    {
        const int usable_ws[] = {200, 320, 480, 640, 800, 1024, 1920, 3840};
        const int usable_hs[] = {150, 240, 259, 480, 600, 768, 1080, 2160};
        const int bars[] = {0, 19, 24, 48};
        for (int n = 1; n <= 8; ++n) {
            for (const int bw : usable_ws) {
                for (const int bh : usable_hs) {
                    for (const int bar : bars) {
                        const WindowFit f = fit_window_to_display(320 * n, 240 * n, bar, bw, bh);
                        const std::string tag = "F_n" + std::to_string(n) + "_" + std::to_string(bw) +
                                                "x" + std::to_string(bh) + "_b" + std::to_string(bar);
                        expect_never_exceeds(f, 320 * n, 240 * n + bar, bw, bh, tag);
                        if (!f.clamped) {
                            expect(f.scale <= n, tag + ":scale<=requested");
                            expect(f.w == 320 * f.scale && f.h == 240 * f.scale + bar,
                                   tag + ":canonical_320Nx240N+bar");
                        }
                    }
                }
            }
        }
    }

    // --- Case G: DEGENERATE usable bounds -> requested unchanged (fallback) --
    {
        const WindowFit f0 = fit_window_to_display(960, 720, 19, 0, 0);
        expect(f0.w == 960 && f0.h == 739 && f0.scale == 3 && !f0.clamped, "G_zero_usable_fallback");
        const WindowFit fn = fit_window_to_display(960, 720, 19, -1, 480);
        expect(fn.w == 960 && fn.h == 739 && !fn.clamped, "G_negative_usable_fallback");
        // Negative bar sanitizes to 0.
        const WindowFit fb = fit_window_to_display(960, 720, -5, 1920, 1040);
        expect(fb.w == 960 && fb.h == 720, "G_negative_bar_sanitized");
    }

    // --- Case H: THE RESIZE/MAXIMIZE CLAMP REUSE (DEC-0090-AMENDMENT-A) -----
    // The event-loop clamp (Sdl3App::clamp_window_to_display) calls the SAME
    // fit function with the CURRENT window as its own canvas unit (cw = cur_w,
    // ch = cur picture h), which reduces it to exactly "keep when it fits,
    // else min(current, usable)" -- a user-driven resize/maximize is NEVER
    // snapped back to an integer 320Nx240N scale (the owner goal on the 7"
    // panel is the BIGGEST picture that keeps the menu bar visible).
    {
        // The owner-Pi maximize: the WM grows the window to 800x480 (bar 19 ->
        // picture band 461) but the display usable area is 800x444 (a ~36px
        // desktop panel). Clamp = min(current, usable) = 800x444.
        const WindowFit f = fit_window_to_display(800, 461, 19, 800, 444, 800, 461);
        expect(f.w == 800 && f.h == 444, "H_pi_maximize_clamp_800x444");
        expect(f.clamped && f.scale == 1, "H_pi_maximize_clamped_flag");
        expect_never_exceeds(f, 800, 480, 800, 444, "H_pi_maximize");
        // The M57 hand-off after the clamp: the letterbox fills the 800x425
        // band below the bar height-bound -- picture 566.67x425, exact 4:3,
        // fully below the strip. BIG picture WITH the menu visible.
        const Rect r = letterbox_into_band(f.w, f.h, 19);
        expect(r.y >= 19.0 - 1e-6, "H_letterbox_below_strip");
        expect(near(r.h, 425.0), "H_letterbox_fills_band_height");
        expect(near(r.w, 425.0 * 320.0 / 240.0), "H_letterbox_aspect_4_3");
        expect(r.x >= -1e-6 && r.x + r.w <= 800.0 + 1e-6, "H_letterbox_inside_w");
    }
    {
        // A current window that already fits is returned UNCHANGED -- the
        // event-loop predicate would not even fire; the math agrees (identity),
        // which is what makes the clamp settle in one step (its own corrective
        // resize event re-enters with a fitting window and does nothing).
        const WindowFit f = fit_window_to_display(640, 425, 19, 800, 444, 640, 425);
        expect(f.w == 640 && f.h == 444 && !f.clamped, "H_fitting_current_identity");
    }
    {
        // Width-only overflow of a non-canonical drag size: width clamps to the
        // usable width, height is kept (no integer snap, no height change).
        const WindowFit f = fit_window_to_display(900, 300, 19, 800, 444, 900, 300);
        expect(f.w == 800 && f.h == 319 && f.clamped && f.scale == 1, "H_width_only_clamp");
        expect_never_exceeds(f, 900, 319, 800, 444, "H_width_only");
    }
    {
        // Height-only overflow: height clamps, width kept.
        const WindowFit f = fit_window_to_display(600, 500, 19, 800, 444, 600, 500);
        expect(f.w == 600 && f.h == 444 && f.clamped, "H_height_only_clamp");
        expect_never_exceeds(f, 600, 519, 800, 444, "H_height_only");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_WindowFit_Unit cases passed\n";
    return 0;
}
