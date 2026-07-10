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

#include <cstdint>

namespace sony_msx::devices::video {

// VDP command-engine coordinate-to-VRAM-address resolution (M22-S3, the
// D7-closing piece, backlog D3/D7). Five NEW, dedicated, pure functions --
// separate from V9958Vdp::effective_address(), which has no X/Y parameters
// at all (it operates on a linear 17-bit CPU-port pointer, a different
// domain). The command engine addresses VRAM directly in X/Y pixel-
// coordinate space, so it needs its own formulas.
//
// Grounding (behavior reference only, GPL isolation -- never copied):
// references/openmsx-21.0/src/video/VDPCmdEngine.cc:175-410 (the
// Graphic4Mode/Graphic5Mode/Graphic6Mode/Graphic7Mode/NonBitmapMode structs'
// `addressOf(x, y, extVRAM)` static functions), `!extVRAM` branch only --
// HB-F1XV has no expansion socket (128 KB VRAM, fixed, matching the
// established M14/M21 scope boundary; A-M22-8). MXS/MXD/R#45-bit6 are
// therefore not modeled: these five functions take only (x, y).
//
// CRITICAL (independently hand-verified, docs/m22-planner-package.md §1.5):
// none of these formulas reference R#2. Commands address BOTH pages of a
// bitmap mode directly through the Y-coordinate's own range (`y & 511` spans
// both G6/G7 pages since each page is 256 lines; `y & 1023` spans all 4
// G4/G5 pages), bypassing R#2's display-page-select bits entirely -- those
// only matter for the DISPLAY/rendering path (VdpFrameRenderer's
// resolve_bitmap_page()). Do NOT gate these formulas on R#2.

// GRAPHIC4 (SCREEN5): 4bpp packed, non-planar, 4 pages x 256 lines (10-bit Y).
[[nodiscard]] constexpr std::uint32_t graphic4_command_address(unsigned x, unsigned y) {
    return ((y & 1023u) << 7) | ((x & 255u) >> 1);
}

// GRAPHIC5 (SCREEN6): 2bpp packed, non-planar, 4 pages x 256 lines (10-bit Y).
[[nodiscard]] constexpr std::uint32_t graphic5_command_address(unsigned x, unsigned y) {
    return ((y & 1023u) << 7) | ((x & 511u) >> 2);
}

// GRAPHIC6 (SCREEN7): 4bpp packed, PLANAR (two-bank VRAM interleave), 2 pages
// x 256 lines (9-bit Y). Bank-select bit is bit1 of x (the byte index's own
// LSB, since 2 pixels/byte).
[[nodiscard]] constexpr std::uint32_t graphic6_command_address(unsigned x, unsigned y) {
    return ((x & 2u) << 15) | ((y & 511u) << 7) | ((x & 511u) >> 2);
}

// GRAPHIC7 (SCREEN8, and the V9958 YJK/YJK+YAE overlays which share
// GRAPHIC7's scrMode bucket): 1 byte/pixel, PLANAR, 2 pages x 256 lines
// (9-bit Y). Bank-select bit is bit0 of x.
[[nodiscard]] constexpr std::uint32_t graphic7_command_address(unsigned x, unsigned y) {
    return ((x & 1u) << 16) | ((y & 511u) << 7) | ((x & 255u) >> 1);
}

// V9958-only "commands in non-bitmap modes" addressing (R#25 CMD bit), used
// when the display mode is anything other than G4-G7: flat, non-planar, one
// byte per pixel, over the SAME shared VdpVram the renderer already reads.
[[nodiscard]] constexpr std::uint32_t non_bitmap_command_address(unsigned x, unsigned y) {
    return ((y & 511u) << 8) | (x & 255u);
}

}  // namespace sony_msx::devices::video
