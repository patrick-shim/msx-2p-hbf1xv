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

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "devices/video/frame_buffer.h"
#include "frontend/phosphor_blend.h"  // PhosphorMode (Average / Peak)

namespace sony_msx::frontend {

// Owns the SDL_Texture presenting the decoded FrameBuffer to a real SDL3 renderer.
// Created with `SDL_PIXELFORMAT_XRGB1555` -- a
// bit-for-bit match with FrameBuffer's RGB555 layout (confirmed against both
// `devices/video/frame_buffer.h` and `src/external/sdl3/include/SDL3/SDL_pixels.h`: bits
// 14-10=R, 9-5=G, 4-0=B, bit15/X unused, both plain host-native uint16_t values).
//
// Border composition is controlled by `border_enabled`. When true, `blit_frame()` first
// composes the frame into its border-colored presentation canvas (frontend/border_composer.h,
// raster-true geometry, live per-frame R#7 border color) and uploads that; when false it
// uploads the bare active-area FrameBuffer edge-to-edge (the pre-border behavior,
// byte-for-byte). Either way the uploaded buffer is raw uint16_t pixels via
// `SDL_UpdateTexture` with zero per-pixel conversion in this project's own code
// (composition copies pixel values, never converts them).
//
// The CONSTRUCTOR default is false (bare) -- direct/test construction stays byte-for-byte
// the pre-border presentation. The SDL3 APP also defaults border OFF
// (Sdl3AppConfig::border_enabled = false): the bare edge-to-edge active area is the Sony-
// original default look per the owner's explicit preference. `--border` opts into the
// composed canvas, which matches openMSX's vertical framing for BOTH 192- and 212-line
// modes; `--no-border` is the explicit off (matches the default). (Revision 7fac03d had
// briefly defaulted this ON; it was reverted to the owner-chosen Sony default.)
//
// `blit_frame()`/`present()` are split (rather than one combined call) so a test can read
// back the texture's presented pixel data via `SDL_RenderReadPixels` between the two steps
// -- SDL3's documented recommendation (SDL_render.h:2556-2558: "should be called after
// rendering and before SDL_RenderPresent()") -- to prove the "zero conversion" claim is
// genuinely true (the pixel-exact test), not merely plausible.
class Sdl3VideoPresenter {
public:
    // `scale_mode` selects the texture filter applied via
    // SDL_SetTextureScaleMode each time the texture is (re)created. Default
    // SDL_SCALEMODE_LINEAR matches the renderer's own default (SDL_render.h:1260), so an
    // unspecified filter leaves the presentation byte-identical; SDL_SCALEMODE_NEAREST
    // gives crisp pixels (--filter nearest). (DEC-0056)
    //
    // `persistence` (0..100, DEFAULT 0 = OFF) is the phosphor-persistence
    // inter-frame blend weight (--persistence): the percent of the previously
    // presented frame carried into the current one before upload (see
    // frontend/phosphor_blend.h for the CRT rationale). DEFAULT 0 makes
    // blit_frame() byte-for-byte the pre-existing present path -- no blend, no
    // prev-frame buffer, no extra allocation.
    //
    // `persistence_mode` (DEFAULT Average) selects HOW the retained frame
    // combines (--persistence-mode; frontend/phosphor_blend.h): Average = the
    // original weighted-mean blend; Peak = peak-hold-with-decay (a bright pixel
    // in EITHER frame stays bright, so multiplexed sprites keep full brightness
    // instead of the Average mode's dimming). The mode is irrelevant when
    // persistence == 0 (the blend block is skipped entirely).
    explicit Sdl3VideoPresenter(SDL_Renderer* renderer, bool border_enabled = false,
                                SDL_ScaleMode scale_mode = SDL_SCALEMODE_LINEAR, int persistence = 0,
                                PhosphorMode persistence_mode = PhosphorMode::Average);
    ~Sdl3VideoPresenter();

    Sdl3VideoPresenter(const Sdl3VideoPresenter&) = delete;
    Sdl3VideoPresenter& operator=(const Sdl3VideoPresenter&) = delete;

    // Uploads `frame` (bare active area by default; composed border canvas when
    // border_enabled -- see the class doc) to the (recreated-on-mode-switch) texture and
    // draws it to the renderer's current target (SDL_UpdateTexture + SDL_RenderClear +
    // SDL_RenderTexture) -- does NOT call SDL_RenderPresent(). Returns false (and records
    // last_error()) on any SDL3 call failure.
    bool blit_frame(const devices::video::FrameBuffer& frame);

    [[nodiscard]] bool border_enabled() const { return border_enabled_; }

    // The configured texture scale mode (--filter). (DEC-0056)
    [[nodiscard]] SDL_ScaleMode scale_mode() const { return scale_mode_; }
    // Swap the texture filter live (menu Video > Filter). Applied
    // immediately to the existing texture via SDL_SetTextureScaleMode, and also
    // used for any future texture (re)creation. Presentation-only. (DEC-0083)
    void set_scale_mode(SDL_ScaleMode mode);

    // Phosphor-persistence blend weight (--persistence / Alt+B). 0 = OFF (the
    // present path is byte-for-byte the current path). Setting a NEW value
    // restarts the accumulation cleanly (the previous-frame buffer is dropped
    // so a freshly (dis|en)abled blend never mixes a stale frame).
    [[nodiscard]] int persistence() const { return persistence_; }
    void set_persistence(int persistence);

    // Phosphor blend MODE (--persistence-mode / Alt+M). Average = weighted mean;
    // Peak = peak-hold-with-decay (frontend/phosphor_blend.h). Switching the mode
    // does NOT drop the retained frame -- the buffer is just "the last presented
    // output" either way, so the next blit simply combines it under the new rule
    // (no reseed needed, unlike a persistence VALUE change which restarts the
    // accumulation). Irrelevant while persistence == 0.
    [[nodiscard]] PhosphorMode persistence_mode() const { return persistence_mode_; }
    void set_persistence_mode(PhosphorMode mode) { persistence_mode_ = mode; }

    // The menu-strip TOP INSET
    // in output pixels. DEFAULT 0 => the legacy full-window LETTERBOX path VERBATIM
    // (blit_frame relies on the 320x240 logical presentation set in init(), draws
    // SDL_RenderTexture(nullptr, nullptr)); the hidden-window / pixel-integration
    // path passes 0 and stays byte-identical. > 0 (interactive, menu present) =>
    // blit_frame explicitly letterboxes the picture into the band BELOW the strip
    // (letterbox_geometry.h), so zero MSX pixels hide behind the menu. The app sets
    // this from Sdl3Menu::bar_height() each frame; hidden-window never touches it.
    // (DEC-0085)
    void set_top_inset(int px) { top_inset_px_ = px < 0 ? 0 : px; }
    [[nodiscard]] int top_inset() const { return top_inset_px_; }
    // The status-bar strip reserved along the window BOTTOM (mirror of
    // set_top_inset). The picture letterboxes into the band BETWEEN the two
    // strips. 0 (hidden-window / no status bar) keeps the legacy behavior.
    // (DEC-0096)
    void set_bottom_inset(int px) { bottom_inset_px_ = px < 0 ? 0 : px; }
    [[nodiscard]] int bottom_inset() const { return bottom_inset_px_; }

    // SDL_RenderPresent() -- the real-time loop's per-frame swap. Kept separate from
    // blit_frame() (see class doc).
    bool present();

    [[nodiscard]] SDL_Texture* texture() const { return texture_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

private:
    bool ensure_texture(int width, int height);

    SDL_Renderer* renderer_;
    bool border_enabled_ = false;
    SDL_ScaleMode scale_mode_ = SDL_SCALEMODE_LINEAR;  // --filter (DEC-0056)
    // Phosphor-persistence blend (--persistence): weight in [0,100] of the
    // previously presented frame. 0 = OFF (skips the entire blend block, so no
    // prev buffer is ever touched/allocated -- the byte-identical default path).
    int persistence_ = 0;
    // Phosphor blend mode (--persistence-mode / Alt+M). Average = weighted mean
    // (blend_rgb555); Peak = peak-hold-with-decay (peak_rgb555). Only consulted
    // inside the persistence_>0 block, so it never touches the default OFF path.
    PhosphorMode persistence_mode_ = PhosphorMode::Average;
    // Previous PRESENTED (blended) output, retained only while persistence_>0
    // for the exponential inter-frame blend (out = p*prev + (100-p)*cur, IIR).
    // prev_width_/prev_height_ guard against blending across a mode-switch.
    std::vector<std::uint16_t> prev_pixels_;
    int prev_width_ = 0;
    int prev_height_ = 0;
    // Reused per-frame scratch for the blended result (swapped into prev_pixels_
    // each frame -- avoids a per-frame full copy).
    std::vector<std::uint16_t> blended_pixels_;
    SDL_Texture* texture_ = nullptr;
    int texture_width_ = 0;
    int texture_height_ = 0;
    // Menu-strip top inset in output pixels (0 = legacy path; DEC-0085).
    int top_inset_px_ = 0;
    int bottom_inset_px_ = 0;  // status-bar strip along the window bottom (DEC-0096)
    std::string last_error_;
};

}  // namespace sony_msx::frontend
