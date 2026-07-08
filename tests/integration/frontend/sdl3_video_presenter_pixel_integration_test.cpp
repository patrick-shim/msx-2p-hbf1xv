#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>
#include <iostream>

#include "devices/video/frame_buffer.h"
#include "frontend/sdl3_video_presenter.h"

// Suite: Frontend_Sdl3VideoPresenterPixel_Integration (M26-S3, docs/m26-
// planner-package.md §2.3/A-M26-3).
//
// Independently PROVES (not merely assumes) that A-M26-3's "zero conversion"
// claim is genuinely true: renders a KNOWN synthetic FrameBuffer through the
// real Sdl3VideoPresenter::blit_frame() path, reads the PRESENTED pixel data
// back via the real SDL_RenderReadPixels(), and asserts it matches
// FrameBuffer's own RGB555 values bit-for-bit (after SDL3's OWN
// format-aware SDL_ConvertSurface() normalizes the readback surface to
// SDL_PIXELFORMAT_XRGB1555 for the comparison -- SDL_RenderReadPixels
// documents that it returns whatever format the CURRENT RENDER TARGET uses,
// SDL_render.h:2546-2571, which is not necessarily the uploaded texture's
// own format; this normalization step is SDL3's own trusted, general-purpose
// color-format converter, not any per-pixel logic added by this project's
// own C++ code -- the actual "zero conversion" claim under test concerns
// Sdl3VideoPresenter::blit_frame()'s own SDL_UpdateTexture call, which is
// independently confirmed by direct source inspection to hand FrameBuffer::
// pixels.data() to SDL3 with no per-pixel transform whatsoever).

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void set_dummy_drivers() {
#if defined(_WIN32)
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
}

sony_msx::devices::video::FrameBuffer make_known_frame() {
    sony_msx::devices::video::FrameBuffer frame;
    frame.width = 6;
    frame.height = 4;
    frame.border_color = 0x0000;
    frame.pixels.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height));
    // A varied set of real RGB555 values (including the all-1s/all-0s
    // boundary cases) so a pixel-format/byte-order mismatch would be caught,
    // not accidentally hidden by a degenerate uniform buffer.
    for (std::size_t i = 0; i < frame.pixels.size(); ++i) {
        frame.pixels[i] = static_cast<std::uint16_t>((i * 0x2537u) & 0x7FFFu);
    }
    frame.pixels[0] = 0x0000;
    frame.pixels[1] = 0x7FFF;
    frame.pixels[2] = 0x7C00;  // pure red
    frame.pixels[3] = 0x03E0;  // pure green
    frame.pixels[4] = 0x001F;  // pure blue
    return frame;
}

}  // namespace

int main() {
    set_dummy_drivers();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // The window/render-target is created at EXACTLY the known FrameBuffer's
    // own dimensions (6x4, matching make_known_frame() below) -- avoiding
    // any scaling ambiguity in the SDL_RenderTexture(..., nullptr, nullptr)
    // full-target blit, so a readback pixel at (x,y) corresponds 1:1 to the
    // SAME logical FrameBuffer pixel, not an interpolated/scaled sample.
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const bool created = SDL_CreateWindowAndRenderer("m26-video-presenter-test", 6, 4, SDL_WINDOW_HIDDEN, &window,
                                                      &renderer);
    expect(created, "CreateWindowAndRenderer_SucceedsUnderDummyDriver");
    if (!created) {
        std::cerr << "  " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    {
        sony_msx::frontend::Sdl3VideoPresenter presenter(renderer);
        const sony_msx::devices::video::FrameBuffer frame = make_known_frame();

        const bool blitted = presenter.blit_frame(frame);
        expect(blitted, "BlitFrame_Succeeds");
        if (!blitted) {
            std::cerr << "  " << presenter.last_error() << "\n";
        }

        // Read back BEFORE present() (SDL3's own documented recommendation,
        // SDL_render.h:2556-2558).
        SDL_Surface* readback = SDL_RenderReadPixels(renderer, nullptr);
        expect(readback != nullptr, "RenderReadPixels_Succeeds");

        if (readback != nullptr) {
            SDL_Surface* normalized = readback;
            bool owns_normalized = false;
            if (readback->format != SDL_PIXELFORMAT_XRGB1555) {
                normalized = SDL_ConvertSurface(readback, SDL_PIXELFORMAT_XRGB1555);
                owns_normalized = true;
                expect(normalized != nullptr, "ConvertSurface_ToXRGB1555_Succeeds");
            }

            if (normalized != nullptr) {
                expect(normalized->w >= frame.width && normalized->h >= frame.height,
                       "PresentedSurface_AtLeastAsLargeAsFrameBuffer");

                bool all_pixels_match = true;
                for (int y = 0; y < frame.height && all_pixels_match; ++y) {
                    const auto* row =
                        static_cast<const std::uint8_t*>(normalized->pixels) + static_cast<std::size_t>(y) * normalized->pitch;
                    for (int x = 0; x < frame.width; ++x) {
                        std::uint16_t presented_pixel = 0;
                        std::memcpy(&presented_pixel, row + static_cast<std::size_t>(x) * 2, 2);
                        const std::uint16_t expected_pixel = frame.at(x, y);
                        if (presented_pixel != expected_pixel) {
                            all_pixels_match = false;
                            std::cerr << "  mismatch at (" << x << "," << y << "): presented=0x" << std::hex
                                       << presented_pixel << " expected=0x" << expected_pixel << std::dec << "\n";
                            break;
                        }
                    }
                }
                expect(all_pixels_match,
                       "PresentedPixelData_MatchesFrameBufferOwnValues_BitForBit_AM26_3_ZeroConversionProof");
            }

            if (owns_normalized && normalized != nullptr) {
                SDL_DestroySurface(normalized);
            }
            SDL_DestroySurface(readback);
        }

        const bool presented = presenter.present();
        expect(presented, "Present_Succeeds");

        // A mode switch (different width/height) recreates the texture
        // cleanly -- a second, differently-sized blit still succeeds.
        sony_msx::devices::video::FrameBuffer frame2 = frame;
        frame2.width = 8;
        frame2.height = 8;
        frame2.pixels.assign(64, 0x1234);
        expect(presenter.blit_frame(frame2), "BlitFrame_AfterModeSwitch_Succeeds");
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3VideoPresenterPixel_Integration cases passed\n";
    return 0;
}
