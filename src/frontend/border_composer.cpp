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

#include "frontend/border_composer.h"

#include <algorithm>

namespace sony_msx::frontend {

BorderGeometry border_geometry(const int active_width, const int active_height) {
    BorderGeometry g;
    // 512-pixel-wide modes (GRAPHIC5/6 = 512, TEXT2 = 480) present on a
    // 640-wide canvas; 256-wide modes (and TEXT1's 240) on a 320-wide one
    // (SDLRasterizer.cc:44-46 translateX() maxX narrow/non-narrow).
    const bool narrow = active_width > 320;
    g.canvas_width = narrow ? 640 : 320;
    g.canvas_height = 240;

    switch (active_width) {
    case 240:  // TEXT1: 32 border + 9 text-mode display offset (VDP.hh:598-604)
        g.x0 = 41;
        break;
    case 256:  // graphics modes: 32 border pixels each side
        g.x0 = 32;
        break;
    case 480:  // TEXT2: (64 + 18) in 512-pixel-mode units
        g.x0 = 82;
        break;
    case 512:  // GRAPHIC5/6: 64 border pixels each side
        g.x0 = 64;
        break;
    default:  // defensive fallback: plain centering
        g.x0 = std::max(0, (g.canvas_width - active_width) / 2);
        break;
    }

    switch (active_height) {
    case 192:  // LN=0: display centered on the 240-line canvas -> 24/24
        g.y0 = 24;
        break;
    case 212:  // LN=1: 14/14 (SDLRasterizer.cc:174-179)
        g.y0 = 14;
        break;
    default:  // defensive fallback: plain centering
        g.y0 = std::max(0, (g.canvas_height - active_height) / 2);
        break;
    }
    return g;
}

devices::video::FrameBuffer compose_border_canvas(const devices::video::FrameBuffer& frame) {
    const BorderGeometry g = border_geometry(frame.width, frame.height);

    devices::video::FrameBuffer out;
    out.width = g.canvas_width;
    out.height = g.canvas_height;
    out.border_color = frame.border_color;
    out.pixels.assign(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height),
                      frame.border_color);

    const int copy_width = std::min(frame.width, out.width - g.x0);
    const int copy_height = std::min(frame.height, out.height - g.y0);
    for (int y = 0; y < copy_height; ++y) {
        const std::uint16_t* src = frame.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width);
        std::uint16_t* dst = out.pixels.data() +
                             (static_cast<std::size_t>(y + g.y0) * static_cast<std::size_t>(out.width) +
                              static_cast<std::size_t>(g.x0));
        std::copy_n(src, static_cast<std::size_t>(copy_width), dst);
    }
    return out;
}

}  // namespace sony_msx::frontend
