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

namespace sony_msx::frontend {

// Phosphor-persistence inter-frame blend (PRESENTATION-ONLY, SDL3 frontend).
//
// WHY (CRT phosphor persistence): many MSX games multiplex sprites -- they cycle
// which sprites are drawn on alternate frames to beat the V9958's hard
// 8-sprites-per-scanline limit, so a sprite is present on frame N and absent on
// frame N+1. On a real CRT the phosphor keeps glowing for a few ms after the
// electron beam passes, so the eye integrates the ON and OFF frames into one
// steady (slightly dimmer) sprite. An LCD has no such persistence and snaps
// instantly, so the same content reads as harsh flicker. This blend simulates
// the phosphor's decay by carrying a weighted fraction of the previously
// presented frame into the current one -- exactly openMSX's inter-frame
// "blur"/glow presentation option. It is a DISPLAY post-process only: it never
// touches emulated VDP/sprite/game state and has zero effect on determinism or
// headless output.
//
// Two BLEND MODES (--persistence-mode; PhosphorMode below) select how the
// retained previous frame combines with the current one:
//
//   * Average (blend_rgb555, the original mode) -- a weighted MEAN of prev and
//     cur. It steadies flicker but DIMS a sprite that is bright on only one of
//     the two frames: a multiplexed sprite present on frame N and absent on
//     frame N+1 is pulled toward the (darker) neighbour, so at 25% it still
//     flickers and at 75% the whole scrolling background ghosts (the owner's
//     exact complaint).
//   * Peak (peak_rgb555) -- peak-hold-with-decay, the classic sprite-flicker
//     "max" technique tempered with a phosphor decay. A pixel bright in the
//     CURRENT frame is presented at FULL brightness (never dimmed toward a
//     darker retained value), so a multiplexed sprite reads full-brightness and
//     steady; a pixel that WAS bright and is now dark fades toward the current
//     value at the decay rate, so an old trail clears instead of smearing. This
//     is the fix for the flicker-vs-ghost tradeoff.
enum class PhosphorMode { Average, Peak };

// AVERAGE mode. Blends one XRGB1555 pixel channel-wise (5-bit R/G/B):
//     out_ch = round( (p*prev_ch + (100-p)*cur_ch) / 100 )
// where `persistence` (p) is the PERCENT of the previous presented frame
// retained, clamped to [0, 100].
//
// Contract (unit-tested, non-tautological):
//   * p == 0   -> returns `cur` EXACTLY (byte-identical -> the default/off path
//                 stays bit-for-bit the current presentation).
//   * p == 100 -> returns `prev` EXACTLY.
//   * monotonic in p for any fixed (prev, cur) pair.
// Bit 15 (the unused X of XRGB1555) is always cleared.
[[nodiscard]] constexpr std::uint16_t blend_rgb555(const std::uint16_t prev, const std::uint16_t cur,
                                                   const int persistence) {
    const int p = persistence < 0 ? 0 : (persistence > 100 ? 100 : persistence);
    const int inv = 100 - p;
    const int r = (p * ((prev >> 10) & 0x1F) + inv * ((cur >> 10) & 0x1F) + 50) / 100;
    const int g = (p * ((prev >> 5) & 0x1F) + inv * ((cur >> 5) & 0x1F) + 50) / 100;
    const int b = (p * (prev & 0x1F) + inv * (cur & 0x1F) + 50) / 100;
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}

// PEAK mode -- peak-hold-with-decay of one XRGB1555 pixel, channel-wise. For
// each 5-bit channel:
//     decayed = FLOOR( (p*prev_ch + (100-p)*cur_ch) / 100 )  // retained peak fades toward cur
//     out_ch  = max( decayed, cur_ch )                       // brighter-of; cur is NEVER dimmed
// where `persistence` (p) is the PERCENT of the retained previous-frame peak
// carried forward, clamped to [0, 100].
//
// WHY decayed-peak instead of a plain max: a plain `max(prev,cur)` never dims a
// sprite (great) but leaves a PERMANENT trail on a bright scrolling background
// (a pixel bright once stays bright forever). Fading the retained peak toward
// the current value each frame lets those trails clear, at a rate the SAME
// persistence knob controls: p==100 is pure max (no decay -> permanent trail, by
// design), and lower p fades faster. The decay uses FLOOR (not the +50 rounding
// the Average mode uses) on PURPOSE: floor makes `decayed` STRICTLY smaller than
// a brighter retained value every off-frame (for p<100), so a retained trail
// provably decreases to the current value and fully clears, instead of sticking
// on a rounding floor (round-to-nearest can pin a faint residual at high p).
//
// Contract (unit-tested, non-tautological):
//   * peak(x, x, p) == x for all p            (idempotent -- a static image is
//                                              untouched, no shimmer).
//   * peak(black, x, p) == x for all p        (a pixel bright in the CURRENT
//                                              frame is presented at FULL
//                                              brightness -- the anti-DIM
//                                              property that fixes flicker
//                                              WITHOUT darkening sprites; this is
//                                              the key contrast with Average,
//                                              which would return (100-p)% of x).
//   * peak(x, black, 100) == x                (p==100 is pure peak/max: the
//                                              retained peak never decays).
//   * peak(x, black, p) with 0<=p<100 decays: 0 <= out < x (strict for x>0), and
//     peak(x, black, 0) == 0                  (p==0 -> identity to cur -> the
//                                              byte-identical OFF path).
//   * peak(a, b, 100) == channel-wise max(a, b)   (brighter-of).
//   * channel-independent; bit 15 (the X of XRGB1555) is always cleared.
[[nodiscard]] constexpr std::uint16_t peak_rgb555(const std::uint16_t prev, const std::uint16_t cur,
                                                  const int persistence) {
    const int p = persistence < 0 ? 0 : (persistence > 100 ? 100 : persistence);
    const int inv = 100 - p;
    const int cr = (cur >> 10) & 0x1F, cg = (cur >> 5) & 0x1F, cb = cur & 0x1F;
    const int pr = (prev >> 10) & 0x1F, pg = (prev >> 5) & 0x1F, pb = prev & 0x1F;
    // FLOOR decay of the retained peak toward the current value (see WHY above).
    const int dr = (p * pr + inv * cr) / 100;
    const int dg = (p * pg + inv * cg) / 100;
    const int db = (p * pb + inv * cb) / 100;
    // max(decayed, cur): a bright CURRENT pixel is kept at full brightness.
    const int r = dr > cr ? dr : cr;
    const int g = dg > cg ? dg : cg;
    const int b = db > cb ? db : cb;
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}

}  // namespace sony_msx::frontend
