#include "frontend/sdl3_video_presenter.h"

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
    if (!ensure_texture(frame.width, frame.height)) {
        return false;
    }

    // Zero per-pixel conversion (A-M26-3): FrameBuffer::pixels is handed to
    // SDL3 as-is, a raw uint16_t buffer, pitch = width * sizeof(uint16_t).
    const int pitch = frame.width * static_cast<int>(sizeof(std::uint16_t));
    if (!SDL_UpdateTexture(texture_, nullptr, frame.pixels.data(), pitch)) {
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
