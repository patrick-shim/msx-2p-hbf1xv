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
// M63: the pure, SDL/ImGui-free points->pixels corrective scale for the ImGui
// menu render pass. Header-only + SDL-independent (the letterbox_geometry.h /
// window_fit.h precedent), so the math is ctest-provable with NO SDL and
// compiles in BOTH configs (SDL3=ON and SDL3=OFF).
//
// Problem it solves (owner Raspberry Pi 4B + 7" 800x480 panel, TRUE fullscreen
// via Video > Fullscreen / Alt+Enter): ImGui lays out AND hit-tests the menu in
// io.DisplaySize == window POINTS (SDL_GetWindowSize; mouse is also in points,
// so clicks land correctly), but the vendored SDL_Renderer backend
// (src/external/imgui/backends/imgui_impl_sdlrenderer3.cpp,
// ImGui_ImplSDLRenderer3_RenderDrawData) submits vertices RAW at the renderer's
// current scale (1.0 after Sdl3Menu::render disables logical presentation)
// while scaling only the CLIP rects by draw_data->FramebufferScale. When the
// window's point size diverges from the true render-output pixel size (the Pi
// fullscreen case), vertices and clip rects end up in MISMATCHED scales and
// the bar draws mis-scaled/mis-clipped (File/Machine pushed off the left edge
// yet still clickable at their point-space positions).
//
// The fix (backend-sanctioned: imgui_impl_sdlrenderer3.cpp:126-127 explicitly
// supports driving this via SDL_SetRenderScale -- with a non-1.0 renderer
// scale set, the backend's own render_scale collapses to 1.0 at lines 132-133
// so it stops double-scaling, and SDL then applies the renderer scale to BOTH
// vertices and clip rects): scale the whole ImGui submission by
// (out_px / display_pts) per axis. A mathematical NO-OP -- (1,1) -- whenever
// points == pixels (every working windowed case), corrective only when they
// diverge (fullscreen).
// ---------------------------------------------------------------------------

struct RenderScale {
    float x = 1.0f;
    float y = 1.0f;
};

// display_w/h_pts: io.DisplaySize (window points). out_w/h_px: the true render
// output size (SDL_GetRenderOutputSize). Any non-positive input returns the
// identity (1,1) -- never divide by a degenerate size.
[[nodiscard]] inline RenderScale imgui_render_scale(const float display_w_pts,
                                                    const float display_h_pts,
                                                    const int out_w_px, const int out_h_px) {
    RenderScale s;
    if (display_w_pts <= 0.0f || display_h_pts <= 0.0f || out_w_px <= 0 || out_h_px <= 0) {
        return s;  // unknown/degenerate geometry: identity, draw as today
    }
    s.x = static_cast<float>(out_w_px) / display_w_pts;
    s.y = static_cast<float>(out_h_px) / display_h_pts;
    return s;
}

}  // namespace sony_msx::frontend::geometry
