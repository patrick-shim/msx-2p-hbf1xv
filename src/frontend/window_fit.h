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

#include <algorithm>

namespace sony_msx::frontend::geometry {

// ---------------------------------------------------------------------------
// The pure, SDL-free display-fit function for the interactive window.
// Header-only + SDL-independent (the letterbox_geometry.h precedent), so the
// WHOLE fit math is ctest-provable with NO SDL and compiles in BOTH configs
// (SDL3=ON and SDL3=OFF). (DEC-0090)
//
// Problem it solves (owner Raspberry Pi 4B + the official 7" 800x480 panel):
// the interactive window used to open at the requested 320N x (240N + bar_h)
// with ZERO display-bounds check, so the 960x739 default overflowed the panel
// and the top-anchored menu bar landed off-screen. This function computes the
// EFFECTIVE window size that never exceeds the display's usable bounds:
//
//   1. If the requested window (picture + bar) already fits -> keep it as-is.
//   2. Otherwise pick the LARGEST integer scale n (never above the requested
//      scale) whose (cw*n) x (ch*n + bar_h) fits the usable bounds -- the
//      picture band stays an exact-4:3 320n x 240n, so the inset-letterbox
//      presenter fills it edge to edge (no bars).
//   3. If even n == 1 overflows, CLAMP the window to min(requested, usable):
//      it fills the screen without exceeding it, and the EXISTING
//      letterbox presenter (set_top_inset + letterbox_into_band) fits the 4:3
//      picture aspect-correct + fully visible into whatever band remains below
//      the bar. This function deliberately adds NO second letterbox path.
//
// Degenerate/unknown usable bounds (w or h <= 0) return the requested size
// unchanged -- the caller's query-failed fallback is thereby total (today's
// behavior, no regression).
// ---------------------------------------------------------------------------

struct WindowFit {
    int w = 0;              // effective window width  (never exceeds a positive usable_w)
    int h = 0;              // effective window height (never exceeds a positive usable_h; INCLUDES bar_h)
    int scale = 1;          // effective integer picture scale n (picture = cw*n x ch*n); 1 when clamped
    bool clamped = false;   // even scale 1 overflowed: window = min(requested, usable), letterbox fits the rest
};

// requested_picture_w/h: the requested PICTURE size (canonically 320N x 240N;
//   the menu bar is NOT included -- pass it separately as bar_h).
// bar_h: the menu-strip height reserved ABOVE the picture band (0 = no menu).
// usable_w/h: the display's usable bounds (SDL_GetDisplayUsableBounds).
// cw/ch: the picture unit (defaults 320x240 = the MSX 4:3 logical canvas).
[[nodiscard]] inline WindowFit fit_window_to_display(const int requested_picture_w,
                                                     const int requested_picture_h,
                                                     const int bar_h_in,
                                                     const int usable_w, const int usable_h,
                                                     const int cw = 320, const int ch = 240) {
    WindowFit out;
    const int bar_h = bar_h_in < 0 ? 0 : bar_h_in;
    const int req_w = requested_picture_w < 0 ? 0 : requested_picture_w;
    const int req_h = requested_picture_h < 0 ? 0 : requested_picture_h;

    // The requested integer scale (exact for the canonical 320N x 240N shapes;
    // floor for any non-canonical programmatic size). The fit NEVER upscales
    // beyond this -- a --scale 1 request on a huge display stays scale 1.
    int n_req = 1;
    if (cw > 0 && ch > 0) {
        n_req = std::max(1, std::min(req_w / cw, req_h / ch));
    }

    // Degenerate canvas unit or unknown usable bounds: requested size unchanged
    // (the caller's no-display-info fallback == today's behavior).
    if (cw <= 0 || ch <= 0 || usable_w <= 0 || usable_h <= 0) {
        out.w = req_w;
        out.h = req_h + bar_h;
        out.scale = n_req;
        return out;
    }

    // 1) The request fits as-is (picture + bar within the usable bounds).
    if (req_w <= usable_w && req_h + bar_h <= usable_h) {
        out.w = req_w;
        out.h = req_h + bar_h;
        out.scale = n_req;
        return out;
    }

    // 2) Largest integer scale n in [1, n_req] with cw*n <= usable_w AND
    //    ch*n + bar_h <= usable_h. (usable_h - bar_h can be <= 0 -> n_by_h < 1.)
    const int n_by_w = usable_w / cw;
    const int n_by_h = (usable_h - bar_h) / ch;
    const int n_fit = std::min(n_req, std::min(n_by_w, n_by_h));
    if (n_fit >= 1) {
        out.w = cw * n_fit;
        out.h = ch * n_fit + bar_h;
        out.scale = n_fit;
        return out;
    }

    // 3) Even scale 1 overflows: clamp to min(requested window, usable). The
    //    letterbox presenter fits the 4:3 picture into the band below the bar.
    out.w = std::min(req_w, usable_w);
    out.h = std::min(req_h + bar_h, usable_h);
    out.scale = 1;
    out.clamped = true;
    return out;
}

}  // namespace sony_msx::frontend::geometry
