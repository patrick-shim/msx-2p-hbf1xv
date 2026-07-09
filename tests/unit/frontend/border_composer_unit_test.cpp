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

// Suite: Frontend_BorderComposer_Unit  (border-box fix, targeted defect cycle)
//
// Presentation border-canvas composition (frontend/border_composer.h): the
// active display area is placed at its raster-true position inside a
// border-colored 320x240 / 640x240 canvas. Geometry grounding: V9958
// fact-sheet §7 line/frame tick tables + openMSX SDLRasterizer.cc
// translateX()/lineRenderTop presentation convention, verified against a
// measured openMSX 320x240 raw screenshot of the Sony_HB-F1XV BASIC screen
// (256x192 active area at x in [32,287], y in [24,215]; SCREEN 0 text at
// x=41). All anchors here are exact-value asserted.

#include <cstdint>
#include <iostream>

#include "devices/video/frame_buffer.h"
#include "frontend/border_composer.h"

namespace {

using sony_msx::devices::video::FrameBuffer;
using sony_msx::frontend::border_geometry;
using sony_msx::frontend::BorderGeometry;
using sony_msx::frontend::compose_border_canvas;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

FrameBuffer make_frame(const int width, const int height, const std::uint16_t border) {
    FrameBuffer f;
    f.width = width;
    f.height = height;
    f.border_color = border;
    f.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t i = 0; i < f.pixels.size(); ++i) {
        f.pixels[i] = static_cast<std::uint16_t>((i * 0x1234u + 7u) & 0x7FFFu);
    }
    return f;
}

}  // namespace

int main() {
    // --- Geometry table: every mode-family anchor, exact values. ---
    {
        const BorderGeometry g256_192 = border_geometry(256, 192);
        expect(g256_192.canvas_width == 320 && g256_192.canvas_height == 240,
               "Geometry_256x192_Canvas320x240");
        expect(g256_192.x0 == 32 && g256_192.y0 == 24, "Geometry_256x192_Anchor_32_24_MeasuredOpenMsx");

        const BorderGeometry g256_212 = border_geometry(256, 212);
        expect(g256_212.x0 == 32 && g256_212.y0 == 14, "Geometry_256x212_Anchor_32_14");

        const BorderGeometry g240_192 = border_geometry(240, 192);
        expect(g240_192.canvas_width == 320, "Geometry_Text1_Canvas320");
        expect(g240_192.x0 == 41 && g240_192.y0 == 24, "Geometry_Text1_Anchor_41_24_MeasuredOpenMsxScreen0");

        const BorderGeometry g512_212 = border_geometry(512, 212);
        expect(g512_212.canvas_width == 640 && g512_212.canvas_height == 240,
               "Geometry_512Wide_Canvas640x240");
        expect(g512_212.x0 == 64 && g512_212.y0 == 14, "Geometry_512x212_Anchor_64_14");

        const BorderGeometry g480_192 = border_geometry(480, 192);
        expect(g480_192.canvas_width == 640, "Geometry_Text2_Canvas640");
        expect(g480_192.x0 == 82 && g480_192.y0 == 24, "Geometry_Text2_Anchor_82_24");
    }

    // --- Composition: border fill + active-area copy + border_color
    //     propagation, byte-exact. ---
    {
        const FrameBuffer frame = make_frame(256, 192, 0x2E5F);
        const FrameBuffer canvas = compose_border_canvas(frame);

        expect(canvas.width == 320 && canvas.height == 240, "Compose_CanvasDimensions");
        expect(canvas.border_color == 0x2E5F, "Compose_BorderColorPropagated");

        // Border pixels: all four corners + the pixel just outside each
        // active-area edge.
        expect(canvas.at(0, 0) == 0x2E5F && canvas.at(319, 0) == 0x2E5F && canvas.at(0, 239) == 0x2E5F &&
                   canvas.at(319, 239) == 0x2E5F,
               "Compose_CornersAreBorderColor");
        expect(canvas.at(31, 100) == 0x2E5F && canvas.at(288, 100) == 0x2E5F && canvas.at(100, 23) == 0x2E5F &&
                   canvas.at(100, 216) == 0x2E5F,
               "Compose_JustOutsideActiveEdges_BorderColor");

        // Active pixels: copied bit-for-bit at (x0+x, y0+y).
        bool active_exact = true;
        for (int y = 0; y < frame.height && active_exact; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                if (canvas.at(32 + x, 24 + y) != frame.at(x, y)) {
                    active_exact = false;
                    break;
                }
            }
        }
        expect(active_exact, "Compose_ActiveAreaCopiedBitForBit");
    }

    // --- The border color is LIVE per frame: two frames differing only in
    //     border_color compose different surrounds but identical active
    //     areas. ---
    {
        FrameBuffer a = make_frame(256, 212, 0x0007);
        FrameBuffer b = a;
        b.border_color = 0x7C00;
        const FrameBuffer ca = compose_border_canvas(a);
        const FrameBuffer cb = compose_border_canvas(b);
        expect(ca.at(0, 0) == 0x0007 && cb.at(0, 0) == 0x7C00, "Compose_LiveBorderColorPerFrame");
        expect(ca.at(32, 14) == cb.at(32, 14), "Compose_ActiveAreaUnaffectedByBorderColor");
    }

    // --- Determinism oracle: identical inputs -> byte-identical canvases. ---
    {
        const FrameBuffer frame = make_frame(512, 212, 0x1234);
        const FrameBuffer c1 = compose_border_canvas(frame);
        const FrameBuffer c2 = compose_border_canvas(frame);
        expect(c1.pixels == c2.pixels && c1.width == c2.width && c1.height == c2.height,
               "Compose_Determinism_ByteIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_BorderComposer_Unit cases passed\n";
    return 0;
}
