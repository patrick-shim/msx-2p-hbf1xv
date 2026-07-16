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

namespace sony_msx::frontend::geometry {

// ---------------------------------------------------------------------------
// M57 (DEC-0085, docs/m57-planner-package.md §4.3): the SDL-free inset-letterbox
// geometry helper for DEF-2 (the ImGui menu strip must not obscure the top of
// the emulated picture). Header-only + SDL-independent (the master_volume.h /
// phosphor_blend.h precedent), so the WHOLE fit math is ctest-provable with NO
// SDL renderer -- it compiles in BOTH configs (SDL3=ON and SDL3=OFF) and the
// geometry oracle targets this pure function directly.
//
// The menu bar owns a `top_inset`-pixel-tall strip along the window top. This
// function fits a cw:ch (default 320x240 = MSX 4:3) rectangle into the BAND
// BELOW the strip -- band = {x:0, y:top_inset, w:win_w, h:win_h - top_inset} --
// aspect-preserved and CENTERED in the band. The returned rect never overlaps
// [0, top_inset) (the strip), so zero MSX pixels hide behind the menu.
//
// top_inset == 0 (hidden-window / no menu) degenerates to the FULL-WINDOW
// letterbox -- byte-identical geometry to SDL's LOGICAL_PRESENTATION_LETTERBOX
// of the same 320x240 canvas -- so the legacy path (which passes 0) is neutral.
// ---------------------------------------------------------------------------

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

// Fit a cw:ch rect into the band below `top_inset`, aspect-preserved + centered.
// Degenerate inputs (non-positive window/canvas, or an inset >= win_h leaving no
// band) return a zero-size rect anchored at the band top -- the caller draws
// nothing rather than inverting the geometry.
[[nodiscard]] inline Rect letterbox_into_band(const int win_w, const int win_h, const int top_inset,
                                              const int cw = 320, const int ch = 240) {
    Rect r;
    r.y = static_cast<double>(top_inset);
    if (win_w <= 0 || win_h <= 0 || cw <= 0 || ch <= 0) {
        return r;  // degenerate window/canvas
    }
    const int band_h_i = win_h - top_inset;
    if (band_h_i <= 0) {
        return r;  // the inset is taller than the window: no band to draw into
    }
    const double band_w = static_cast<double>(win_w);
    const double band_h = static_cast<double>(band_h_i);
    const double target_aspect = static_cast<double>(cw) / static_cast<double>(ch);
    const double band_aspect = band_w / band_h;

    double out_w = 0.0;
    double out_h = 0.0;
    if (band_aspect > target_aspect) {
        // Band is WIDER than 4:3 -> height-bound (pillarbox within the band): the
        // picture fills the band height, so its top sits exactly at top_inset.
        out_h = band_h;
        out_w = out_h * target_aspect;
    } else {
        // Band is NARROWER/taller than 4:3 -> width-bound (letterbox within the
        // band): the picture fills the band width and is centered vertically.
        out_w = band_w;
        out_h = out_w / target_aspect;
    }
    r.w = out_w;
    r.h = out_h;
    r.x = (band_w - out_w) / 2.0;                                  // centered horizontally
    r.y = static_cast<double>(top_inset) + (band_h - out_h) / 2.0;  // centered in the band
    return r;
}

}  // namespace sony_msx::frontend::geometry
