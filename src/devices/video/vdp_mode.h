#pragma once

#include <cstdint>

namespace sony_msx::devices::video {

// V9958 display-mode identity (M14-S3, bit-level decode ONLY — no pixels).
//
// The V9958 display mode is selected by M1..M5 (R#1 b4,b3 and R#0 b3,b2,b1) plus
// the V9958 YJK/YAE bits (R#25 b3/b4). LN (R#9 b7) and IL (R#9 b3) only change
// vertical resolution/interlace, not the mode identity, so they are NOT part of
// this enum. This decode reproduces the canonical MSX mode-bit encoding grounded
// in references/openmsx-21.0/src/video/DisplayMode.hh (behavior reference only —
// never copied; the numeric layout below is the public MSX2 register encoding
// from the fact-sheet §3 lines 45-64, independently expressed here).
//
// Base byte layout (M5..M1), bit0=M1 .. bit4=M5:
//   base = ((reg0 & 0x0E) << 1) | ((reg1 & 0x08) >> 2) | ((reg1 & 0x10) >> 4)
//
// M14 stores this identity + the raw V9958 feature bits; it computes NO output
// pixels or colors (rendering is deferred, backlog D1/D5/D6).

enum class VdpMode : std::uint8_t {
    Graphic1,     // SCREEN 1        (base 0x00)
    Text1,        // SCREEN 0 W40    (base 0x01)
    Multicolor,   // SCREEN 3        (base 0x02)
    Graphic2,     // SCREEN 2        (base 0x04)
    Text1Q,       // undocumented    (base 0x05)
    MulticolorQ,  // undocumented    (base 0x06)
    Graphic3,     // SCREEN 4        (base 0x08)
    Text2,        // SCREEN 0 W80    (base 0x09)
    Graphic4,     // SCREEN 5        (base 0x0C)
    Graphic5,     // SCREEN 6        (base 0x10)
    Graphic6,     // SCREEN 7        (base 0x14)
    Graphic7,     // SCREEN 8        (base 0x1C)
    // V9958 YJK overlays on GRAPHIC7 (fact-sheet §3):
    ScreenYjk,      // SCREEN 12   (YJK, no YAE)
    ScreenYjkYae,   // SCREEN 10/11 (YJK + YAE)
    Unknown,      // any base combination not in the Target-Spec set
};

// Decoded mode identity plus the raw V9958 feature bits (stored, no effect).
struct VdpModeState {
    VdpMode mode = VdpMode::Graphic1;
    std::uint8_t base = 0x00;  // M5..M1 base byte (0..0x1F)
    bool yjk = false;          // R#25 bit3
    bool yae = false;          // R#25 bit4 (only meaningful when yjk)

    [[nodiscard]] constexpr bool operator==(const VdpModeState&) const = default;
};

// Compute the M5..M1 base byte from R#0 and R#1.
[[nodiscard]] constexpr std::uint8_t vdp_mode_base(std::uint8_t reg0, std::uint8_t reg1) {
    return static_cast<std::uint8_t>(((reg0 & 0x0E) << 1) | ((reg1 & 0x08) >> 2) |
                                     ((reg1 & 0x10) >> 4));
}

// True iff this base mode is a V9938-and-later mode (M4 or M5 set) — the modes
// in which the CPU-port VRAM auto-increment carries into R#14 (full 128 KB
// counting). The legacy TMS9918 modes (G1/G2/MC/T1: M4=M5=0) do NOT carry
// (fact-sheet §2 line 40; openMSX executeCpuVramAccess VDP.cc:884 isV9938Mode).
[[nodiscard]] constexpr bool vdp_base_is_v9938_mode(std::uint8_t base) {
    return (base & 0x18) != 0;
}

// True iff this base mode uses the G6/G7 planar VRAM interleave (M21-S4,
// backlog D7). Independently re-derived from
// references/openmsx-21.0/src/video/DisplayMode.hh:140-143 (`isPlanar`):
// `(base & 0x14) == 0x14`. Correctly identifies GRAPHIC6 (0x14) and
// GRAPHIC7 (0x1C), and -- since YJK/YAE only set bits 5/6 of the full mode
// byte, never touching base bits 2/4 -- both V9958 YJK overlay modes too
// (their `base` stays 0x1C), while excluding GRAPHIC5 (0x10).
[[nodiscard]] constexpr bool vdp_base_is_planar(std::uint8_t base) {
    return (base & 0x14) == 0x14;
}

// Sprite mode for this base (M22-S1, backlog D2; independently re-derived
// from references/openmsx-21.0/src/video/DisplayMode.hh:158-174
// `getSpriteMode()`, restricted to the V9958 case since this project's VDP
// is never `isMSX1VDP()`): 0 = no sprites, 1 = sprite mode 1 (TMS9918-
// compatible, max 4/line), 2 = sprite mode 2 (MSX2+, max 8/line). Shared by
// `SpriteEngine`'s own per-line check/collision algorithm and
// `VdpFrameRenderer`'s sprite-compositing gate so both stay byte-identical
// without duplicating the table (a deliberate, low-risk anti-drift measure).
[[nodiscard]] constexpr int vdp_sprite_mode(std::uint8_t base) {
    switch (base) {
    case 0x00: case 0x02: case 0x04:
        // Graphic1, Multicolor, Graphic2.
        return 1;
    case 0x08: case 0x0C: case 0x10: case 0x14: case 0x1C:
        // Graphic3, Graphic4, Graphic5, Graphic6, Graphic7 (+ YJK/YJK+YAE,
        // which share GRAPHIC7's base).
        return 2;
    default:
        // TEXT1/TEXT1Q/TEXT2/MulticolorQ/Unknown: never show sprites
        // (verified on real V9958 per the reference's own comment).
        return 0;
    }
}

[[nodiscard]] constexpr VdpModeState decode_vdp_mode(std::uint8_t reg0, std::uint8_t reg1,
                                                     std::uint8_t reg25) {
    VdpModeState state;
    state.base = vdp_mode_base(reg0, reg1);
    // If YJK is off, YAE is ignored (DisplayMode.hh:69).
    state.yjk = (reg25 & 0x08) != 0;
    state.yae = state.yjk && ((reg25 & 0x10) != 0);

    switch (state.base) {
    case 0x00: state.mode = VdpMode::Graphic1; break;
    case 0x01: state.mode = VdpMode::Text1; break;
    case 0x02: state.mode = VdpMode::Multicolor; break;
    case 0x04: state.mode = VdpMode::Graphic2; break;
    case 0x05: state.mode = VdpMode::Text1Q; break;
    case 0x06: state.mode = VdpMode::MulticolorQ; break;
    case 0x08: state.mode = VdpMode::Graphic3; break;
    case 0x09: state.mode = VdpMode::Text2; break;
    case 0x0C: state.mode = VdpMode::Graphic4; break;
    case 0x10: state.mode = VdpMode::Graphic5; break;
    case 0x14: state.mode = VdpMode::Graphic6; break;
    case 0x1C: state.mode = VdpMode::Graphic7; break;
    default:   state.mode = VdpMode::Unknown; break;
    }

    // V9958 YJK/YJK+YAE overlay GRAPHIC7 (base 0x1C).
    if (state.base == 0x1C && state.yjk) {
        state.mode = state.yae ? VdpMode::ScreenYjkYae : VdpMode::ScreenYjk;
    }
    return state;
}

}  // namespace sony_msx::devices::video
