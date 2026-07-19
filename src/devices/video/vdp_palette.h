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

// V9958 color-decode primitives.
//
// Pure, header-only, side-effect-free functions shared by every render path
// (character/tile modes' 16-entry palette, GRAPHIC7's fixed 256-color table,
// and YJK/YJK+YAE's computed R/G/B). Grounded in
// openMSX 21.0: src/video/SDLRasterizer.cc (behavior reference
// only, GPL — never copied; formulas independently re-derived and re-
// expressed here) and the fact sheet's own restatement of the same table
// (Yamaha V9958 VDP fact sheet §5).
//
// Pixel format: native RGB555 (bits 14-10 = R, 9-5 = G, 4-0 = B), matching
// the V9958's physical 15-bit (5:5:5) DAC (V9958 fact sheet §9) — chosen so
// YJK's already-clamped 0..31 R/G/B values pack directly, no 32768-entry
// host lookup table needed. Bit 15 is reserved, unused.

// 3-bit -> 5-bit component expansion, independently re-derived and
// cross-checked against SDLRasterizer.cc:286-296 (`r5 = (r3 << 2) | (r3 >> 1)`):
// 0->0, 1->4, 2->9, 3->13, 4->18, 5->22, 6->27, 7->31.
[[nodiscard]] constexpr std::uint8_t expand3to5(const std::uint8_t c3) {
    const std::uint8_t v = static_cast<std::uint8_t>(c3 & 0x07);
    return static_cast<std::uint8_t>((v << 2) | (v >> 1));
}

// Pack 5-bit R/G/B components into a native RGB555 pixel.
[[nodiscard]] constexpr std::uint16_t pack_rgb555(const std::uint8_t r5, const std::uint8_t g5,
                                                  const std::uint8_t b5) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(r5 & 0x1F) << 10) |
                                       (static_cast<std::uint16_t>(g5 & 0x1F) << 5) |
                                       static_cast<std::uint16_t>(b5 & 0x1F));
}

[[nodiscard]] constexpr std::uint8_t rgb555_red(const std::uint16_t pixel) {
    return static_cast<std::uint8_t>((pixel >> 10) & 0x1F);
}

[[nodiscard]] constexpr std::uint8_t rgb555_green(const std::uint16_t pixel) {
    return static_cast<std::uint8_t>((pixel >> 5) & 0x1F);
}

[[nodiscard]] constexpr std::uint8_t rgb555_blue(const std::uint16_t pixel) {
    return static_cast<std::uint8_t>(pixel & 0x1F);
}

// Unpack the VDP's stored 9-bit GRB palette-register format (bits 10-8 = G,
// 6-4 = R, 2-0 = B; VDP.hh:274-278's documented `getPalette()` layout) and
// expand every 3-bit component to 5 bits.
[[nodiscard]] constexpr std::uint16_t palette_entry_to_rgb555(const std::uint16_t grb9) {
    const std::uint8_t g3 = static_cast<std::uint8_t>((grb9 >> 8) & 0x07);
    const std::uint8_t r3 = static_cast<std::uint8_t>((grb9 >> 4) & 0x07);
    const std::uint8_t b3 = static_cast<std::uint8_t>(grb9 & 0x07);
    return pack_rgb555(expand3to5(r3), expand3to5(g3), expand3to5(b3));
}

// GRAPHIC7 (SCREEN8) fixed 256-color byte decode: the byte layout is
// GGG RRR BB (green in the TOP 3 bits), NOT the naively-expected RRR GGG
// BB. Independently re-derived and cross-checked against
// SDLRasterizer.cc:330-336:
//   PALETTE256[i] = V9938_COLORS[(i&0x1C)>>2][(i&0xE0)>>5][(i&0x03)==3?7:(i&0x03)*2]
// where V9938_COLORS is indexed [r3][g3][b3] (SDLRasterizer.cc:304-314's
// construction loop). So bits 4-2 = red, bits 7-5 = green, and bits 1-0 give
// a pseudo-3-bit blue via the map {0->0, 1->2, 2->4, 3->7}.
[[nodiscard]] constexpr std::uint16_t graphic7_fixed_color_to_rgb555(const std::uint8_t byte) {
    const std::uint8_t g3 = static_cast<std::uint8_t>((byte >> 5) & 0x07);
    const std::uint8_t r3 = static_cast<std::uint8_t>((byte >> 2) & 0x07);
    const std::uint8_t b2 = static_cast<std::uint8_t>(byte & 0x03);
    const std::uint8_t b3 = (b2 == 3) ? std::uint8_t{7} : static_cast<std::uint8_t>(b2 * 2);
    return pack_rgb555(expand3to5(r3), expand3to5(g3), expand3to5(b3));
}

// YJK / YJK+YAE per-pixel R/G/B decode, independently re-derived and
// cross-checked against BitmapConverter.cc:217-228 (`yjk2rgb`). `y` is
// 0..31 (YJK) or an even value 0..30 (YJK+YAE, LSB stolen for the attribute
// bit); `j`/`k` are signed, range -32..31. The B-channel division is PLAIN
// C++ `int` division (truncating toward zero) — NEVER
// `std::floor()`: openMSX is also C++ with identical `int` truncation
// semantics for `/`, so plain `/` is automatically byte-exact; a "helpful"
// refactor to `std::floor()` would silently diverge for negative,
// non-multiple-of-4 numerators.
struct YjkRgb {
    int r;
    int g;
    int b;
};

[[nodiscard]] constexpr int clamp5(const int v) {
    if (v < 0) return 0;
    if (v > 31) return 31;
    return v;
}

[[nodiscard]] constexpr YjkRgb yjk_to_rgb(const int y, const int j, const int k) {
    const int r = clamp5(y + j);
    const int g = clamp5(y + k);
    const int b = clamp5((5 * y - 2 * j - k + 2) / 4);  // plain int division (see note above)
    return YjkRgb{r, g, b};
}

}  // namespace sony_msx::devices::video
