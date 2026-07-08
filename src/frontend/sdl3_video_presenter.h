#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "devices/video/frame_buffer.h"

namespace sony_msx::frontend {

// Owns the SDL_Texture presenting the decoded FrameBuffer (M21) to a real
// SDL3 renderer (M26-S3, docs/m26-planner-package.md §2.3). Created with
// `SDL_PIXELFORMAT_XRGB1555` -- a BIT-FOR-BIT match with FrameBuffer's own
// documented RGB555 layout (A-M26-3, independently confirmed by reading both
// `devices/video/frame_buffer.h` and `references/sdl3/include/SDL3/
// SDL_pixels.h`: bits 14-10=R, 9-5=G, 4-0=B, bit15/X unused, both plain
// host-native uint16_t values in memory). `blit_frame()` first composes the
// frame into its border-colored presentation canvas (frontend/
// border_composer.h, the border-box fix), then uploads that canvas's raw
// uint16_t pixel buffer via `SDL_UpdateTexture` with ZERO per-pixel
// conversion in this project's own code (composition copies pixel values,
// never converts them).
//
// `blit_frame()`/`present()` are split (rather than one combined call) so a
// test can read back the texture's presented pixel data via
// `SDL_RenderReadPixels` BETWEEN the two steps -- SDL3's own documented
// recommendation (SDL_render.h:2556-2558: "should be called after rendering
// and before SDL_RenderPresent()") -- to prove the "zero conversion" claim is
// genuinely true (S3 pixel-exact test), not merely plausible.
class Sdl3VideoPresenter {
public:
    explicit Sdl3VideoPresenter(SDL_Renderer* renderer);
    ~Sdl3VideoPresenter();

    Sdl3VideoPresenter(const Sdl3VideoPresenter&) = delete;
    Sdl3VideoPresenter& operator=(const Sdl3VideoPresenter&) = delete;

    // Composes `frame` into its border-colored presentation canvas
    // (frontend/border_composer.h -- active area at its raster-true position
    // inside a live-per-frame R#7 border surround, 320x240 or 640x240),
    // uploads the canvas to the (recreated-on-mode-switch) texture and draws
    // it to the renderer's current target (SDL_UpdateTexture +
    // SDL_RenderClear + SDL_RenderTexture) -- does NOT call
    // SDL_RenderPresent(). Returns false (and records last_error()) on any
    // SDL3 call failure.
    bool blit_frame(const devices::video::FrameBuffer& frame);

    // SDL_RenderPresent() -- the real-time loop's per-frame swap. Kept
    // separate from blit_frame() (see class doc comment).
    bool present();

    [[nodiscard]] SDL_Texture* texture() const { return texture_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

private:
    bool ensure_texture(int width, int height);

    SDL_Renderer* renderer_;
    SDL_Texture* texture_ = nullptr;
    int texture_width_ = 0;
    int texture_height_ = 0;
    std::string last_error_;
};

}  // namespace sony_msx::frontend
