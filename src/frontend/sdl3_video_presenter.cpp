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

#include "frontend/sdl3_video_presenter.h"

#include <cstddef>

#include "frontend/border_composer.h"
#include "frontend/letterbox_geometry.h"
#include "frontend/phosphor_blend.h"

namespace sony_msx::frontend {

Sdl3VideoPresenter::Sdl3VideoPresenter(SDL_Renderer* renderer, const bool border_enabled,
                                       const SDL_ScaleMode scale_mode, const int persistence,
                                       const PhosphorMode persistence_mode)
    : renderer_(renderer),
      border_enabled_(border_enabled),
      scale_mode_(scale_mode),
      persistence_(persistence < 0 ? 0 : (persistence > 100 ? 100 : persistence)),
      persistence_mode_(persistence_mode) {}

void Sdl3VideoPresenter::set_scale_mode(const SDL_ScaleMode mode) {
    // M55 (DEC-0083): live filter swap. Update the stored mode (so any later
    // texture recreation honors it) and re-apply to the current texture now.
    // A failure only records last_error_ -- the next blit still presents.
    scale_mode_ = mode;
    if (texture_ != nullptr) {
        if (!SDL_SetTextureScaleMode(texture_, scale_mode_)) {
            last_error_ = SDL_GetError();
        }
    }
}

void Sdl3VideoPresenter::set_persistence(const int persistence) {
    const int clamped = persistence < 0 ? 0 : (persistence > 100 ? 100 : persistence);
    if (clamped != persistence_) {
        persistence_ = clamped;
        // Restart the accumulation: drop the retained previous frame so the next
        // blit re-seeds cleanly (a freshly (dis|en)abled blend never mixes a
        // stale frame, and turning it OFF leaves no dangling state).
        prev_width_ = 0;
        prev_height_ = 0;
        prev_pixels_.clear();
    }
}

Sdl3VideoPresenter::~Sdl3VideoPresenter() {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

bool Sdl3VideoPresenter::ensure_texture(const int width, const int height) {
    if (texture_ != nullptr && texture_width_ == width && texture_height_ == height) {
        return true;
    }
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    // A-M26-3: SDL_PIXELFORMAT_XRGB1555 is bit-for-bit identical to FrameBuffer's RGB555
    // layout. STREAMING access matches SDL_UpdateTexture's intended per-frame full-buffer
    // upload pattern (SDL_render.h:1406-1414).
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB1555, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture_ == nullptr) {
        last_error_ = SDL_GetError();
        return false;
    }
    // M37 Slice E (DEC-0056): apply the configured filter (--filter) to the freshly-created
    // texture via SDL_SetTextureScaleMode (references/sdl3/include/SDL3/SDL_render.h:1275).
    // Default LINEAR matches the renderer's own default (SDL_render.h:1260), so an
    // unspecified filter is byte-identical to the pre-M37 presentation.
    if (!SDL_SetTextureScaleMode(texture_, scale_mode_)) {
        last_error_ = SDL_GetError();
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        return false;
    }
    texture_width_ = width;
    texture_height_ = height;
    return true;
}

bool Sdl3VideoPresenter::blit_frame(const devices::video::FrameBuffer& frame) {
    // border_enabled_ (app default ON, M39-B): compose into the border-colored
    // 320x240 / 640x240 canvas so the active area sits at its raster-true,
    // openMSX-matching position (border_composer.h documents the geometry; border
    // color is live per frame) -- the only present correct for BOTH 192- and
    // 212-line modes. false (--no-border): bare active area edge-to-edge
    // (byte-for-byte the pre-border behavior; squishes 212-line vertically).
    devices::video::FrameBuffer composed;
    const devices::video::FrameBuffer* source = &frame;
    if (border_enabled_) {
        composed = compose_border_canvas(frame);
        source = &composed;
    }
    if (!ensure_texture(source->width, source->height)) {
        return false;
    }

    // Zero per-pixel conversion (A-M26-3): pixels are handed to SDL3 as-is (raw uint16_t
    // buffer, pitch = width * sizeof(uint16_t)); composition copies pixels but never
    // converts them.
    const int pitch = source->width * static_cast<int>(sizeof(std::uint16_t));
    const std::uint16_t* upload = source->pixels.data();

    // Phosphor-persistence inter-frame blend (PRESENTATION-ONLY, frontend/
    // phosphor_blend.h): simulate a CRT phosphor's decay so multiplexed
    // (per-frame-cycled) sprites read as steady dimmer sprites instead of LCD
    // flicker. Entirely GATED behind persistence_>0: when 0 (the default) this
    // block is skipped, `upload` stays source->pixels.data(), and the present
    // path is byte-for-byte the pre-existing path (no prev buffer, no
    // allocation). This is downstream-neutral to the scale/filter path: the
    // blended pixels are handed to the SAME SDL_UpdateTexture -> scale_mode_
    // texture the un-blended pixels would be.
    if (persistence_ > 0) {
        const std::size_t n = source->pixels.size();
        const bool prev_matches = prev_width_ == source->width && prev_height_ == source->height &&
                                  prev_pixels_.size() == n;
        if (prev_matches) {
            // Combine the retained (previously PRESENTED) output with this frame,
            // per pixel, under the selected mode (frontend/phosphor_blend.h):
            //   Average -> weighted mean (blend_rgb555): steadies flicker, dims.
            //   Peak    -> peak-hold-with-decay (peak_rgb555): a pixel bright in
            //              either frame stays bright -> no sprite dimming.
            // The mode branch is HOISTED out of the per-pixel loop (one test per
            // frame, not per pixel).
            blended_pixels_.resize(n);
            if (persistence_mode_ == PhosphorMode::Peak) {
                for (std::size_t i = 0; i < n; ++i) {
                    blended_pixels_[i] = peak_rgb555(prev_pixels_[i], source->pixels[i], persistence_);
                }
            } else {
                for (std::size_t i = 0; i < n; ++i) {
                    blended_pixels_[i] = blend_rgb555(prev_pixels_[i], source->pixels[i], persistence_);
                }
            }
            prev_pixels_.swap(blended_pixels_);  // prev_pixels_ now holds this frame's output (O(1))
            upload = prev_pixels_.data();
        } else {
            // First blended frame (or a mode-switch changed the dimensions):
            // present the current frame as-is and seed the accumulator.
            prev_pixels_.assign(source->pixels.begin(), source->pixels.end());
        }
        prev_width_ = source->width;
        prev_height_ = source->height;
    }

    if (!SDL_UpdateTexture(texture_, nullptr, upload, pitch)) {
        last_error_ = SDL_GetError();
        return false;
    }

    if (!SDL_RenderClear(renderer_)) {
        last_error_ = SDL_GetError();
        return false;
    }

    // M57 (DEC-0085, §4.2): DEF-2 inset. top_inset_px_ == 0 (hidden-window / no
    // menu / the pixel-integration test) keeps the LEGACY path VERBATIM -- the
    // 320x240 LETTERBOX logical presentation (init sdl3_app.cpp) fills+letterboxes
    // the whole output via a nullptr dst. This branch is byte-identical to the
    // pre-M57 present.
    if (top_inset_px_ <= 0 && bottom_inset_px_ <= 0) {
        if (!SDL_RenderTexture(renderer_, texture_, nullptr, nullptr)) {
            last_error_ = SDL_GetError();
            return false;
        }
        return true;
    }

    // top_inset_px_ > 0 (interactive, menu present): draw the MSX picture with an
    // EXPLICIT destination rect letterboxed into the band BELOW the strip, so no
    // MSX pixel hides behind the menu. Explicit dst is in RAW OUTPUT PIXELS, so we
    // temporarily disable the logical presentation (save + restore, self-contained
    // -- the menu render bracket then behaves as before). SDL_GetRenderOutputSize
    // is the pixel output size (A4, references/sdl3/include/SDL3/SDL_render.h).
    int lw = 0;
    int lh = 0;
    SDL_RendererLogicalPresentation lmode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer_, &lw, &lh, &lmode);
    SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    int out_w = 0;
    int out_h = 0;
    bool ok = SDL_GetRenderOutputSize(renderer_, &out_w, &out_h);
    if (ok) {
        const geometry::Rect band =
            geometry::letterbox_into_band(out_w, out_h, top_inset_px_, bottom_inset_px_);
        const SDL_FRect dst{static_cast<float>(band.x), static_cast<float>(band.y),
                            static_cast<float>(band.w), static_cast<float>(band.h)};
        ok = SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
    }
    // Restore the caller's logical presentation regardless of the draw outcome.
    SDL_SetRenderLogicalPresentation(renderer_, lw, lh, lmode);
    if (!ok) {
        last_error_ = SDL_GetError();
        return false;
    }
    return true;
}

bool Sdl3VideoPresenter::present() {
    if (!SDL_RenderPresent(renderer_)) {
        last_error_ = SDL_GetError();
        return false;
    }
    return true;
}

}  // namespace sony_msx::frontend
