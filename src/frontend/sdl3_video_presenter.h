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

#include <string>

#include "devices/video/frame_buffer.h"

namespace sony_msx::frontend {

// Owns the SDL_Texture presenting the decoded FrameBuffer (M21) to a real SDL3 renderer
// (M26-S3, docs/m26-planner-package.md §2.3). Created with `SDL_PIXELFORMAT_XRGB1555` -- a
// bit-for-bit match with FrameBuffer's RGB555 layout (A-M26-3, confirmed against both
// `devices/video/frame_buffer.h` and `references/sdl3/include/SDL3/SDL_pixels.h`: bits
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
// the pre-border presentation. The SDL3 APP also defaults border OFF (M39-D,
// Sdl3AppConfig::border_enabled = false): the bare edge-to-edge active area is the Sony-
// original default look per the human's explicit preference. `--border` opts into the
// composed canvas, which matches openMSX's vertical framing for BOTH 192- and 212-line
// modes; `--no-border` is the explicit off (matches the default). (7fac03d/M39-B had
// briefly defaulted this ON; M39-D reverts it to the human-chosen Sony default.)
//
// `blit_frame()`/`present()` are split (rather than one combined call) so a test can read
// back the texture's presented pixel data via `SDL_RenderReadPixels` between the two steps
// -- SDL3's documented recommendation (SDL_render.h:2556-2558: "should be called after
// rendering and before SDL_RenderPresent()") -- to prove the "zero conversion" claim is
// genuinely true (S3 pixel-exact test), not merely plausible.
class Sdl3VideoPresenter {
public:
    // M37 Slice E (DEC-0056): `scale_mode` selects the texture filter applied via
    // SDL_SetTextureScaleMode each time the texture is (re)created. Default
    // SDL_SCALEMODE_LINEAR matches the renderer's own default (SDL_render.h:1260), so an
    // unspecified filter is byte-identical to the pre-M37 presentation; SDL_SCALEMODE_NEAREST
    // gives crisp pixels (--filter nearest).
    explicit Sdl3VideoPresenter(SDL_Renderer* renderer, bool border_enabled = false,
                                SDL_ScaleMode scale_mode = SDL_SCALEMODE_LINEAR);
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

    // M37 Slice E (DEC-0056): the configured texture scale mode (--filter).
    [[nodiscard]] SDL_ScaleMode scale_mode() const { return scale_mode_; }

    // SDL_RenderPresent() -- the real-time loop's per-frame swap. Kept separate from
    // blit_frame() (see class doc).
    bool present();

    [[nodiscard]] SDL_Texture* texture() const { return texture_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

private:
    bool ensure_texture(int width, int height);

    SDL_Renderer* renderer_;
    bool border_enabled_ = false;
    SDL_ScaleMode scale_mode_ = SDL_SCALEMODE_LINEAR;  // M37 Slice E (DEC-0056): --filter
    SDL_Texture* texture_ = nullptr;
    int texture_width_ = 0;
    int texture_height_ = 0;
    std::string last_error_;
};

}  // namespace sony_msx::frontend
