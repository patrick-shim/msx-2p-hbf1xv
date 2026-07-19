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

#include "devices/video/frame_buffer.h"

namespace sony_msx::frontend {

// Border/backdrop canvas composition for presentation -- the border-box defect
// fix (DEC-0026).
//
// The FrameBuffer contract deliberately carries ONLY the active display area plus a
// single per-frame `border_color` (frame_buffer.h: border geometry is explicitly out of
// scope). A real display shows that active area surrounded by a border-colored surround
// (the V9958 emits border pixels for the whole visible raster). This module composes the
// presented canvas from a FrameBuffer: a border-colored canvas with the active area copied
// at its raster-true position. Pure, SDL-free, deterministic math, usable by the SDL3
// presenter and by headless tools/tests alike.
//
// Geometry grounding (independently derived, then verified against a measured openMSX
// 320x240 `screenshot -raw` of the Sony_HB-F1XV BASIC screen -- active area x in [32,287],
// y in [24,215] for a 256x192 mode):
//
//  * One "wide pixel" (256-wide modes) = 4 VDP cycles; 512-wide modes use 2-cycle pixels
//    (Yamaha V9958 VDP fact sheet §7: display region [258,1282) = 1024
//    cycles for 256/512 pixels).
//  * The true visible line (left border [202,258) + display + right border [1282,1341)) is
//    1139 cycles = 284.75 wide pixels. The reference presentation
//    (openMSX 21.0: src/video/SDLRasterizer.cc:38-52 translateX(): "Because it
//    looks better, the borders are extended") extends the border to a 320-wide (or 640 for
//    512-pixel modes) canvas centered on the visible-region midpoint; this module adopts
//    the same convention, giving 32 border pixels each side for a 256-wide mode.
//  * TEXT modes start 9 wide pixels later than graphics modes and are only 240/480 pixels
//    wide (openMSX 21.0: src/video/VDP.hh:598-604 getLeftSprites(): +9*4 cycles
//    for V99x8 text; getRightBorder(): 960-cycle text display region), so a 240-wide TEXT1
//    frame anchors at x=41 (= 32 + 9); empirically confirmed by the openMSX SCREEN 0 raw
//    screenshot's leftmost text pixel at x=41.
//  * Vertically the canvas is 240 lines with the display area centered
//    (SDLRasterizer.cc:174-179: "240 - 212 = 28 lines available for top/bottom border; 14
//    each" -> lineRenderTop = 32 - 14 for NTSC): a 212-line frame anchors at y=14, a
//    192-line frame at y=24. Matches the fact-sheet's NTSC frame table (LN=0: 26 top/25
//    bottom border lines; LN=1: 16/15), outermost lines cropped symmetrically by the
//    240-line canvas.
//
// R#18 set-adjust display repositioning is not modeled (matches the existing renderer's
// scope). Border COLOR is live per frame, read from FrameBuffer::border_color, which
// VdpFrameRenderer::render_frame() derives from R#7 (mode-aware) at render time.
struct BorderGeometry {
    int canvas_width = 0;
    int canvas_height = 0;
    int x0 = 0;  // left edge of the active area on the canvas
    int y0 = 0;  // top edge of the active area on the canvas
};

// Geometry for a given active-area size. Known widths anchor per the table
// above (240 -> 41, 256 -> 32, 480 -> 82, 512 -> 64); unknown sizes fall
// back to plain centering (defensive only -- the renderer emits only the
// four known widths and heights 192/212).
[[nodiscard]] BorderGeometry border_geometry(int active_width, int active_height);

// Compose the presented canvas: a border_color-filled canvas of
// border_geometry() size with `frame`'s pixels copied at (x0, y0).
// border_color is propagated unchanged. Deterministic: identical input
// frames produce byte-identical composed frames.
[[nodiscard]] devices::video::FrameBuffer compose_border_canvas(const devices::video::FrameBuffer& frame);

}  // namespace sony_msx::frontend
