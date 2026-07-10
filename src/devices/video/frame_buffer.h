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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sony_msx::devices::video {

// Deterministic, SDL3-independent VDP rendering output contract (M21-S1,
// backlog D1). Tests/tools drive VRAM + registers through the existing M14
// V9958Vdp port contract, call `VdpFrameRenderer::render_frame()`, and read
// `pixels` directly (raw std::uint16_t RGB555 values) -- never PNG, never
// SDL3.
//
// Field selects which of the two interlace fields to decode (§2.2 of
// docs/m21-planner-package.md); interleaving both fields into one 424-line
// raster (the odd field's documented "half a line lower" timing position)
// is a raster-positioning/timing concern, explicitly out of scope this
// milestone (backlog D4).
enum class Field : std::uint8_t {
    Progressive,
    Even,
    Odd,
};

// One decoded frame: `width`/`height` are mode-dependent (see
// docs/m21-planner-package.md §2.2's dimension table); `pixels` is a flat,
// row-major buffer of `width*height` native RGB555 values (bits 14-10=R,
// 9-5=G, 4-0=B); `border_color` is a single RGB555 value per frame (border
// geometry -- extra canvas rows/columns -- is out of scope this milestone,
// a D4 raster-geometry concern).
struct FrameBuffer {
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> pixels;
    std::uint16_t border_color = 0;

    [[nodiscard]] std::uint16_t at(const int x, const int y) const {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
    }
};

}  // namespace sony_msx::devices::video
