#include "frontend/sdl3_video_presenter.h"

#include "frontend/border_composer.h"

namespace sony_msx::frontend {

Sdl3VideoPresenter::Sdl3VideoPresenter(SDL_Renderer* renderer) : renderer_(renderer) {}

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
    // Border-box composition (border fix): a real display shows the active
    // area surrounded by the R#7 border color, so the presented texture is
    // the composed 320x240 / 640x240 canvas (border_composer.h documents the
    // raster-true geometry), not the bare active area stretched edge to
    // edge. The border color is live per frame (frame.border_color).
    const devices::video::FrameBuffer canvas = compose_border_canvas(frame);
    if (!ensure_texture(canvas.width, canvas.height)) {
        return false;
    }

    // Zero per-pixel conversion (A-M26-3): the composed canvas's pixels are
    // handed to SDL3 as-is, a raw uint16_t buffer, pitch = width *
    // sizeof(uint16_t). (Composition copies pixels but never converts them.)
    const int pitch = canvas.width * static_cast<int>(sizeof(std::uint16_t));
    if (!SDL_UpdateTexture(texture_, nullptr, canvas.pixels.data(), pitch)) {
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
