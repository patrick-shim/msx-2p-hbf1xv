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

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>
#include <iostream>

#include "devices/video/frame_buffer.h"
#include "frontend/border_composer.h"
#include "frontend/sdl3_video_presenter.h"

// Suite: Frontend_Sdl3VideoPresenterPixel_Integration (M26-S3, docs/m26-
// planner-package.md §2.3/A-M26-3; updated for the opt-in border-box
// presentation -- docs/konami-splash-regression-investigation.md).
//
// Independently PROVES (not merely assumes) the real
// Sdl3VideoPresenter::blit_frame() path in BOTH presentation modes:
//   1. DEFAULT (border off): the presented pixel data is the BARE
//      active-area FrameBuffer, bit-for-bit at (0,0) with a window/render
//      target of exactly the frame's own size -- byte-for-byte the
//      pre-border behavior (A-M26-3 "zero conversion").
//   2. OPT-IN (--border / border_enabled=true): the active area sits at its
//      raster-true border_geometry() anchor inside a border_color surround,
//      asserted against the SAME border_composer geometry the presenter
//      uses, with the border color live per frame.
//
// The readback surface is normalized to SDL_PIXELFORMAT_XRGB1555 via SDL3's
// OWN SDL_ConvertSurface (SDL_RenderReadPixels returns whatever format the
// current render target uses, SDL_render.h:2546-2571); each window/render
// target is created at EXACTLY the presented source's size so the
// SDL_RenderTexture(..., nullptr, nullptr) full-target blit is 1:1 with no
// scaling ambiguity.

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
    frame.width = 256;
    frame.height = 192;
    frame.border_color = 0x2E5F;  // a distinctive, non-zero live border color
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

std::uint16_t surface_pixel(const SDL_Surface* surface, const int x, const int y) {
    const auto* row =
        static_cast<const std::uint8_t*>(surface->pixels) + static_cast<std::size_t>(y) * surface->pitch;
    std::uint16_t value = 0;
    std::memcpy(&value, row + static_cast<std::size_t>(x) * 2, 2);
    return value;
}

// Blit `frame` through a presenter constructed with `border_enabled`, read
// the render target back, normalize to XRGB1555, and hand the surface to
// `verify`. The window/render target is created at exactly (target_w,
// target_h) -- the presented source's own size -- for a 1:1 readback.
template <typename VerifyFn>
void run_presenter_case(const sony_msx::devices::video::FrameBuffer& frame, const bool border_enabled,
                        const int target_w, const int target_h, const char* window_title, VerifyFn verify) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const bool created =
        SDL_CreateWindowAndRenderer(window_title, target_w, target_h, SDL_WINDOW_HIDDEN, &window, &renderer);
    expect(created, "CreateWindowAndRenderer_SucceedsUnderDummyDriver");
    if (!created) {
        std::cerr << "  " << SDL_GetError() << "\n";
        return;
    }

    {
        sony_msx::frontend::Sdl3VideoPresenter presenter(renderer, border_enabled);
        expect(presenter.border_enabled() == border_enabled, "Presenter_ReportsConfiguredBorderMode");

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
                expect(normalized->w >= target_w && normalized->h >= target_h,
                       "PresentedSurface_AtLeastTargetSize");
                verify(normalized);
            }

            if (owns_normalized && normalized != nullptr) {
                SDL_DestroySurface(normalized);
            }
            SDL_DestroySurface(readback);
        }

        expect(presenter.present(), "Present_Succeeds");
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

}  // namespace

int main() {
    set_dummy_drivers();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    const sony_msx::devices::video::FrameBuffer frame = make_known_frame();

    // --- 1. DEFAULT path (border off): bare active area, bit-for-bit at
    //     (0,0) -- the pre-border presentation, byte-for-byte. ---
    run_presenter_case(frame, /*border_enabled=*/false, frame.width, frame.height,
                       "m26-video-presenter-test-default", [&](const SDL_Surface* normalized) {
        bool all_pixels_match = true;
        for (int y = 0; y < frame.height && all_pixels_match; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                const std::uint16_t presented_pixel = surface_pixel(normalized, x, y);
                const std::uint16_t expected_pixel = frame.at(x, y);
                if (presented_pixel != expected_pixel) {
                    all_pixels_match = false;
                    std::cerr << "  default-path mismatch at (" << x << "," << y << "): presented=0x" << std::hex
                               << presented_pixel << " expected=0x" << expected_pixel << std::dec << "\n";
                    break;
                }
            }
        }
        expect(all_pixels_match,
               "Default_PresentedBareActiveArea_MatchesFrameBufferOwnValues_BitForBit_AM26_3_ZeroConversionProof");
    });

    // --- 2. OPT-IN path (--border): composed border-canvas geometry. ---
    const sony_msx::frontend::BorderGeometry geometry =
        sony_msx::frontend::border_geometry(frame.width, frame.height);
    run_presenter_case(frame, /*border_enabled=*/true, geometry.canvas_width, geometry.canvas_height,
                       "m26-video-presenter-test-border", [&](const SDL_Surface* normalized) {
        // 2a. Active area, bit-for-bit at the composed anchor.
        bool all_pixels_match = true;
        for (int y = 0; y < frame.height && all_pixels_match; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                const std::uint16_t presented_pixel =
                    surface_pixel(normalized, geometry.x0 + x, geometry.y0 + y);
                const std::uint16_t expected_pixel = frame.at(x, y);
                if (presented_pixel != expected_pixel) {
                    all_pixels_match = false;
                    std::cerr << "  border-path mismatch at (" << x << "," << y << "): presented=0x" << std::hex
                               << presented_pixel << " expected=0x" << expected_pixel << std::dec << "\n";
                    break;
                }
            }
        }
        expect(all_pixels_match, "Border_PresentedActiveArea_MatchesFrameBufferOwnValues_AtComposedAnchor");

        // 2b. Border surround: corners + just-outside-active-edge pixels
        //     all carry the frame's live border color.
        const std::uint16_t bc = frame.border_color;
        expect(surface_pixel(normalized, 0, 0) == bc &&
                   surface_pixel(normalized, geometry.canvas_width - 1, 0) == bc &&
                   surface_pixel(normalized, 0, geometry.canvas_height - 1) == bc &&
                   surface_pixel(normalized, geometry.canvas_width - 1, geometry.canvas_height - 1) == bc,
               "Border_Corners_AreLiveBorderColor");
        expect(surface_pixel(normalized, geometry.x0 - 1, geometry.y0 + 10) == bc &&
                   surface_pixel(normalized, geometry.x0 + frame.width, geometry.y0 + 10) == bc &&
                   surface_pixel(normalized, geometry.x0 + 10, geometry.y0 - 1) == bc &&
                   surface_pixel(normalized, geometry.x0 + 10, geometry.y0 + frame.height) == bc,
               "Border_JustOutsideActiveEdges_AreLiveBorderColor");
    });

    // --- 3. Mode switch (different active dimensions -> different texture
    //     size in either mode) recreates the texture cleanly -- a second,
    //     differently-sized blit still succeeds on the DEFAULT path. ---
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        const bool created = SDL_CreateWindowAndRenderer("m26-video-presenter-test-resize", frame.width,
                                                          frame.height, SDL_WINDOW_HIDDEN, &window, &renderer);
        expect(created, "CreateWindowAndRenderer_ForModeSwitch_Succeeds");
        if (created) {
            sony_msx::frontend::Sdl3VideoPresenter presenter(renderer);
            expect(!presenter.border_enabled(), "Presenter_DefaultConstruction_BorderOff");
            expect(presenter.blit_frame(frame), "BlitFrame_FirstSize_Succeeds");

            sony_msx::devices::video::FrameBuffer frame2;
            frame2.width = 512;
            frame2.height = 212;
            frame2.border_color = 0x0007;
            frame2.pixels.assign(static_cast<std::size_t>(frame2.width) * static_cast<std::size_t>(frame2.height),
                                 0x1234);
            expect(presenter.blit_frame(frame2), "BlitFrame_AfterModeSwitch_Succeeds");

            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    // --- 4. (M37 Slice E, DEC-0056) The presenter applies the configured
    //     texture scale mode via SDL_SetTextureScaleMode (references/sdl3/
    //     include/SDL3/SDL_render.h:1275) each time the texture is (re)created.
    //     Verified against a LIVE renderer by reading the mode back with
    //     SDL_GetTextureScaleMode (:1291). Default construction -> LINEAR (the
    //     renderer's own default, byte-identical to the pre-M37 look);
    //     explicit NEAREST (--filter nearest) -> NEAREST. This is a real
    //     live-renderer assertion of the filter wiring, not a tautology. ---
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        const bool created = SDL_CreateWindowAndRenderer("m37-filter-test", frame.width, frame.height,
                                                         SDL_WINDOW_HIDDEN, &window, &renderer);
        expect(created, "FilterMode_CreateWindowAndRenderer_Succeeds");
        if (created) {
            // Default construction -> LINEAR.
            {
                sony_msx::frontend::Sdl3VideoPresenter presenter(renderer);
                expect(presenter.scale_mode() == SDL_SCALEMODE_LINEAR, "FilterMode_DefaultConfiguredLinear");
                expect(presenter.blit_frame(frame), "FilterMode_Default_BlitSucceeds");
                SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
                expect(presenter.texture() != nullptr &&
                           SDL_GetTextureScaleMode(presenter.texture(), &mode) && mode == SDL_SCALEMODE_LINEAR,
                       "FilterMode_Default_TextureIsLinear_LiveReadback");
            }
            // Explicit NEAREST.
            {
                sony_msx::frontend::Sdl3VideoPresenter presenter(renderer, /*border_enabled=*/false,
                                                                 SDL_SCALEMODE_NEAREST);
                expect(presenter.scale_mode() == SDL_SCALEMODE_NEAREST, "FilterMode_NearestConfigured");
                expect(presenter.blit_frame(frame), "FilterMode_Nearest_BlitSucceeds");
                SDL_ScaleMode mode = SDL_SCALEMODE_LINEAR;
                expect(presenter.texture() != nullptr &&
                           SDL_GetTextureScaleMode(presenter.texture(), &mode) && mode == SDL_SCALEMODE_NEAREST,
                       "FilterMode_Nearest_TextureIsNearest_LiveReadback");
            }
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    // --- 5. Phosphor-persistence inter-frame blend (frontend/phosphor_blend.h)
    //     in the LIVE present path. Reads back a center pixel through the SAME
    //     SDL_RenderReadPixels seam the earlier cases use, at a 1:1 window/
    //     texture size (no scaling ambiguity). Proves:
    //       * persistence>0 blends the PREVIOUS presented frame into the current
    //         one (white then black at p=50 -> 0x4210 = 16/16/16, i.e.
    //         round(0.5*31 + 0.5*0));
    //       * the FIRST blended frame (empty accumulator) is presented as-is;
    //       * persistence==0 (the DEFAULT) is byte-identical -- the identical two
    //         blits present the current frame unchanged, NO blend;
    //       * a live set_persistence() change RESTARTS the accumulation (the next
    //         frame re-seeds rather than blending against a stale frame). ---
    {
        // Read the center pixel of the current render target, normalized to XRGB1555.
        const auto read_center = [](SDL_Renderer* renderer, std::uint16_t& out) -> bool {
            SDL_Surface* readback = SDL_RenderReadPixels(renderer, nullptr);
            if (readback == nullptr) {
                return false;
            }
            SDL_Surface* normalized = readback;
            bool owns = false;
            if (readback->format != SDL_PIXELFORMAT_XRGB1555) {
                normalized = SDL_ConvertSurface(readback, SDL_PIXELFORMAT_XRGB1555);
                owns = true;
            }
            bool ok = false;
            if (normalized != nullptr) {
                out = surface_pixel(normalized, 8, 8);
                ok = true;
            }
            if (owns && normalized != nullptr) {
                SDL_DestroySurface(normalized);
            }
            SDL_DestroySurface(readback);
            return ok;
        };

        sony_msx::devices::video::FrameBuffer white;
        white.width = 64;
        white.height = 64;
        white.border_color = 0;
        white.pixels.assign(static_cast<std::size_t>(64 * 64), 0x7FFF);
        sony_msx::devices::video::FrameBuffer black = white;
        black.pixels.assign(static_cast<std::size_t>(64 * 64), 0x0000);

        // 5a. persistence = 50: frame A (white) seeds the accumulator (presented
        //     as-is); frame B (black) blends white-over-black -> 0x4210.
        {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            const bool created = SDL_CreateWindowAndRenderer("phosphor-blend-50", 64, 64, SDL_WINDOW_HIDDEN,
                                                             &window, &renderer);
            expect(created, "Persistence50_CreateWindow_Succeeds");
            if (created) {
                sony_msx::frontend::Sdl3VideoPresenter presenter(renderer, /*border_enabled=*/false,
                                                                 SDL_SCALEMODE_NEAREST, /*persistence=*/50);
                expect(presenter.persistence() == 50, "Persistence50_ConfiguredWeight");
                expect(presenter.blit_frame(white), "Persistence50_BlitA_White_Succeeds");
                std::uint16_t a = 0xFFFF;
                expect(read_center(renderer, a) && a == 0x7FFF,
                       "Persistence50_FrameA_SeedsAccumulator_PresentedWhite");
                expect(presenter.blit_frame(black), "Persistence50_BlitB_Black_Succeeds");
                std::uint16_t b = 0xFFFF;
                expect(read_center(renderer, b) && b == 0x4210,
                       "Persistence50_FrameB_BlendsWhiteOverBlack_0x4210");
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
            }
        }

        // 5b. persistence = 0 (DEFAULT): identical two blits, NO blend, frame B
        //     presented EXACTLY -- the byte-identical off-path regression guard.
        {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            const bool created = SDL_CreateWindowAndRenderer("phosphor-blend-off", 64, 64, SDL_WINDOW_HIDDEN,
                                                             &window, &renderer);
            expect(created, "PersistenceOff_CreateWindow_Succeeds");
            if (created) {
                sony_msx::frontend::Sdl3VideoPresenter presenter(renderer, /*border_enabled=*/false,
                                                                 SDL_SCALEMODE_NEAREST, /*persistence=*/0);
                expect(presenter.persistence() == 0, "PersistenceOff_ConfiguredWeightZero");
                expect(presenter.blit_frame(white), "PersistenceOff_BlitA_Succeeds");
                expect(presenter.blit_frame(black), "PersistenceOff_BlitB_Succeeds");
                std::uint16_t b = 0xFFFF;
                expect(read_center(renderer, b) && b == 0x0000,
                       "PersistenceOff_FrameB_PresentedExactly_NoBlend_ByteIdentical");
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
            }
        }

        // 5c. a live set_persistence() change restarts the accumulation: after
        //     the change the next frame re-seeds (presented as-is), not blended
        //     against the stale white frame.
        {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            const bool created = SDL_CreateWindowAndRenderer("phosphor-blend-toggle", 64, 64,
                                                             SDL_WINDOW_HIDDEN, &window, &renderer);
            expect(created, "PersistenceToggle_CreateWindow_Succeeds");
            if (created) {
                sony_msx::frontend::Sdl3VideoPresenter presenter(renderer, /*border_enabled=*/false,
                                                                 SDL_SCALEMODE_NEAREST, /*persistence=*/50);
                expect(presenter.blit_frame(white), "PersistenceToggle_BlitWhite_SeedsPrev");
                presenter.set_persistence(75);  // value change -> drop the retained frame
                expect(presenter.persistence() == 75, "PersistenceToggle_WeightUpdatedTo75");
                expect(presenter.blit_frame(black), "PersistenceToggle_BlitBlack_Succeeds");
                std::uint16_t b = 0xFFFF;
                expect(read_center(renderer, b) && b == 0x0000,
                       "PersistenceToggle_ReseedsAfterChange_BlackPresentedAsIs");
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
            }
        }

        // 5d. PEAK mode, persistence = 50: white seeds the accumulator; black
        //     current -> peak-hold-with-decay = max(floor(0.5*31), 0) = 15 per
        //     channel = 0x3DEF (the retained peak FADES rather than averaging to
        //     the Average mode's 0x4210). Proves the mode is selected in the LIVE
        //     present path, not just the pure fn.
        {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            const bool created = SDL_CreateWindowAndRenderer("phosphor-peak-decay", 64, 64, SDL_WINDOW_HIDDEN,
                                                             &window, &renderer);
            expect(created, "PeakDecay_CreateWindow_Succeeds");
            if (created) {
                sony_msx::frontend::Sdl3VideoPresenter presenter(
                    renderer, /*border_enabled=*/false, SDL_SCALEMODE_NEAREST, /*persistence=*/50,
                    sony_msx::frontend::PhosphorMode::Peak);
                expect(presenter.persistence_mode() == sony_msx::frontend::PhosphorMode::Peak,
                       "PeakDecay_ModeConfiguredPeak");
                expect(presenter.blit_frame(white), "PeakDecay_BlitA_White_Seeds");
                expect(presenter.blit_frame(black), "PeakDecay_BlitB_Black_Succeeds");
                std::uint16_t b = 0xFFFF;
                expect(read_center(renderer, b) && b == 0x3DEF,
                       "PeakDecay_FrameB_RetainedPeakFadesTo15PerChannel_0x3DEF");
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
            }
        }

        // 5e. PEAK mode ANTI-DIM (the owner's fix): black seeds; a bright WHITE
        //     current is presented at FULL brightness (0x7FFF), NOT dimmed. The
        //     Average mode presents the identical sequence dimmed to 0x4210 --
        //     asserted side-by-side so the live-path distinction is concrete.
        {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            const bool created = SDL_CreateWindowAndRenderer("phosphor-peak-antidim", 64, 64,
                                                             SDL_WINDOW_HIDDEN, &window, &renderer);
            expect(created, "PeakAntiDim_CreateWindow_Succeeds");
            if (created) {
                sony_msx::frontend::Sdl3VideoPresenter peak(
                    renderer, /*border_enabled=*/false, SDL_SCALEMODE_NEAREST, /*persistence=*/50,
                    sony_msx::frontend::PhosphorMode::Peak);
                expect(peak.blit_frame(black), "PeakAntiDim_BlitA_Black_Seeds");
                expect(peak.blit_frame(white), "PeakAntiDim_BlitB_White_Succeeds");
                std::uint16_t pk = 0xFFFF;
                expect(read_center(renderer, pk) && pk == 0x7FFF,
                       "PeakAntiDim_FrameB_WhiteStaysFullBrightness_0x7FFF");

                // Same black->white sequence in AVERAGE mode dims to 0x4210.
                sony_msx::frontend::Sdl3VideoPresenter avg(
                    renderer, /*border_enabled=*/false, SDL_SCALEMODE_NEAREST, /*persistence=*/50,
                    sony_msx::frontend::PhosphorMode::Average);
                expect(avg.blit_frame(black), "AvgContrast_BlitA_Black_Seeds");
                expect(avg.blit_frame(white), "AvgContrast_BlitB_White_Succeeds");
                std::uint16_t av = 0xFFFF;
                expect(read_center(renderer, av) && av == 0x4210,
                       "AvgContrast_FrameB_WhiteDimmedToHalf_0x4210");
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
            }
        }
    }

    SDL_Quit();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3VideoPresenterPixel_Integration cases passed\n";
    return 0;
}
