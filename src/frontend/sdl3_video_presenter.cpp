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

#include "frontend/border_composer.h"

namespace sony_msx::frontend {

Sdl3VideoPresenter::Sdl3VideoPresenter(SDL_Renderer* renderer, const bool border_enabled)
    : renderer_(renderer), border_enabled_(border_enabled) {}

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
    // A-M26-3: SDL_PIXELFORMAT_XRGB1555 is bit-for-bit identical to
    // FrameBuffer's own documented RGB555 layout -- STREAMING access so
    // SDL_UpdateTexture (a per-frame full-buffer upload) is the intended,
    // efficient access pattern (SDL_render.h:1406-1414).
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB1555, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture_ == nullptr) {
        last_error_ = SDL_GetError();
        return false;
    }
    texture_width_ = width;
    texture_height_ = height;
    return true;
}

bool Sdl3VideoPresenter::blit_frame(const devices::video::FrameBuffer& frame) {
    // Default: the BARE active area, edge to edge (the human-preferred
    // presentation; byte-for-byte the pre-border behavior). Opt-in
    // (border_enabled_, the --border flag): compose the frame into the
    // border-colored 320x240 / 640x240 canvas first (border_composer.h
    // documents the raster-true geometry; border color live per frame).
    devices::video::FrameBuffer composed;
    const devices::video::FrameBuffer* source = &frame;
    if (border_enabled_) {
        composed = compose_border_canvas(frame);
        source = &composed;
    }
    if (!ensure_texture(source->width, source->height)) {
        return false;
    }

    // Zero per-pixel conversion (A-M26-3): the source's pixels are handed
    // to SDL3 as-is, a raw uint16_t buffer, pitch = width *
    // sizeof(uint16_t). (Composition copies pixels but never converts them.)
    const int pitch = source->width * static_cast<int>(sizeof(std::uint16_t));
    if (!SDL_UpdateTexture(texture_, nullptr, source->pixels.data(), pitch)) {
        last_error_ = SDL_GetError();
        return false;
    }

    if (!SDL_RenderClear(renderer_)) {
        last_error_ = SDL_GetError();
        return false;
    }
    if (!SDL_RenderTexture(renderer_, texture_, nullptr, nullptr)) {
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
