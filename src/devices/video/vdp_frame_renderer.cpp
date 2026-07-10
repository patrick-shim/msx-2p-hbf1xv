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

#include "devices/video/vdp_frame_renderer.h"

#include <algorithm>
#include <array>

#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_palette.h"

namespace sony_msx::devices::video {

namespace {

// draw6/draw8: unpack a pattern byte's leading N bits (MSB first) into N
// foreground/background pixels. Independently re-expressed from the shape of
// CharacterConverter.cc's draw6/draw8 helpers (:100-120) -- never copied.
void draw6(std::uint16_t* dst, const std::uint16_t fg, const std::uint16_t bg, const std::uint8_t pattern) {
    static constexpr std::uint8_t kMasks[6] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04};
    for (int i = 0; i < 6; ++i) {
        dst[i] = (pattern & kMasks[i]) ? fg : bg;
    }
}

void draw8(std::uint16_t* dst, const std::uint16_t fg, const std::uint16_t bg, const std::uint8_t pattern) {
    static constexpr std::uint8_t kMasks[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    for (int i = 0; i < 8; ++i) {
        dst[i] = (pattern & kMasks[i]) ? fg : bg;
    }
}

}  // namespace

PlanarRowSpans planar_row_spans(const std::uint32_t row_base, const std::size_t length) {
    // A-M21-11: for an even logical row_base, the 17-bit rotate-right-by-1
    // (A-M21-10, `physical = (logical >> 1) | ((logical & 1) << 16)`) applied
    // to every byte in [row_base, row_base+length) degenerates to two
    // contiguous half-length spans: bank0[i] = vram[(row_base>>1)+i] (even
    // bytes), bank1[i] = vram[0x10000+(row_base>>1)+i] (odd bytes).
    const std::uint32_t masked = row_base & 0x1FFFF;
    const std::uint32_t bank0 = masked >> 1;
    return PlanarRowSpans{bank0, bank0 + 0x10000u, length / 2};
}

VdpFrameRenderer::VdpFrameRenderer(const V9958Vdp& vdp) : vdp_(&vdp) {}

int VdpFrameRenderer::width() const {
    // Independently derived dimension table (planner package §2.2), NOT the
    // fact-sheet's mode table alone (which conflates cell-grid and
    // pixel-canvas resolution for MULTICOLOR, A-M21-9).
    switch (vdp_->mode().mode) {
    case VdpMode::Text1:
        return 240;  // CharacterConverter.cc:142-160: 40 chars x 6px
    case VdpMode::Text2:
        return 480;  // CharacterConverter.cc:183-231: 80 chars x 6px
    case VdpMode::Graphic5:
    case VdpMode::Graphic6:
        return 512;  // DisplayMode.hh:176-186 (the 512-wide list)
    default:
        return 256;  // Graphic1/2/3/Multicolor/Graphic4/Graphic7/YJK/YJKYAE/
                      // Text1Q/MulticolorQ/Unknown (A-M21-6/A-M21-9)
    }
}

int VdpFrameRenderer::height() const {
    switch (vdp_->mode().mode) {
    case VdpMode::Text1:
    case VdpMode::Text2:
    case VdpMode::Text1Q:
    case VdpMode::MulticolorQ:
    case VdpMode::Unknown:
        return 192;  // text modes and the flat-blank fallbacks ignore LN
    default:
        return (vdp_->control_register(9) & 0x80) ? 212 : 192;  // R#9 LN
    }
}

std::uint8_t VdpFrameRenderer::vram_read(const std::uint32_t addr) const {
    return vdp_->vram().read(addr & 0x1FFFF);
}

std::uint16_t VdpFrameRenderer::pal16(const int index) const {
    return palette_entry_to_rgb555(vdp_->palette_entry(index & 0x0F));
}

bool VdpFrameRenderer::color0_transparent() const {
    // VDP.hh:189-191 getTransparency(): TP bit (R#8 bit 5) CLEAR ->
    // transparency ON (fact-sheet: "TP colour0 transparent").
    return (vdp_->control_register(8) & 0x20) == 0;
}

std::uint16_t VdpFrameRenderer::content_pal16(const int index) const {
    // palFg-equivalent lookup (see the header doc comment): content color 0
    // resolves to the backdrop color (R#7 low nibble) when transparent.
    // When transparency is OFF the substitution index is 0 itself
    // (SDLRasterizer.cc:354 `tpIndex = transparency ? bgColorIndex : 0`),
    // i.e. the raw palette entry 0 -- so only the transparent case differs.
    if ((index & 0x0F) == 0 && color0_transparent()) {
        return pal16(vdp_->control_register(7) & 0x0F);
    }
    return pal16(index);
}

std::uint16_t VdpFrameRenderer::content_pal16_g5(const int two_bit_index, const bool odd_pixel) const {
    // GRAPHIC5 variant (SDLRasterizer.cc:364-372): the 4-bit backdrop nibble
    // splits into a 2-bit even-pixel half (bits 3-2) and a 2-bit odd-pixel
    // half (bits 1-0), mirroring BitmapConverter.cc:145-157's
    // palette16[0..3] (even) / palette16[16..19] (odd) banks.
    const int index = two_bit_index & 0x03;
    if (index == 0 && color0_transparent()) {
        const int backdrop = vdp_->control_register(7) & 0x0F;
        return pal16(odd_pixel ? (backdrop & 0x03) : ((backdrop >> 2) & 0x03));
    }
    return pal16(index);
}

std::uint32_t VdpFrameRenderer::name_table_base() const {
    return static_cast<std::uint32_t>(vdp_->control_register(2)) << 10;  // VDP.hh:255-257
}

std::uint32_t VdpFrameRenderer::pattern_table_base() const {
    return static_cast<std::uint32_t>(vdp_->control_register(4)) << 11;  // VDP.hh:249-251
}

std::uint32_t VdpFrameRenderer::color_table_base() const {
    // VDP.hh:252-254.
    return (static_cast<std::uint32_t>(vdp_->control_register(10)) << 14) |
           (static_cast<std::uint32_t>(vdp_->control_register(3)) << 6);
}

int VdpFrameRenderer::vertical_scroll_wrap(const int line) const {
    return (line + vdp_->control_register(23)) & 0xFF;  // VDP.hh:328-333 + PixelRenderer.cc:44-49
}

int VdpFrameRenderer::scrolled_name_col(const int col) const {
    return (col + vdp_->control_register(26)) & 0x1F;  // CharacterConverter.cc:255-270
}

int VdpFrameRenderer::bitmap_horizontal_shift(const int width) const {
    const int mult = (width >= 512) ? 2 : 1;  // fact-sheet §4: G5/G6 double the scroll units
    const int coarse = (vdp_->control_register(26) & 0x1F) * 8 * mult;  // SDLRasterizer.cc:465-471
    const int fine = (vdp_->control_register(27) & 0x07) * mult;       // PixelRenderer.cc:60-69
    return (coarse + fine) % width;
}

void VdpFrameRenderer::apply_bitmap_scroll(std::span<std::uint16_t> out, const std::uint16_t* unshifted,
                                           const int width) const {
    const int shift = bitmap_horizontal_shift(width);
    for (int x = 0; x < width; ++x) {
        out[static_cast<std::size_t>(x)] = unshifted[(x + shift) % width];
    }
}

bool VdpFrameRenderer::multi_page_scrolling() const {
    // VDP.hh:362-370 isMultiPageScrolling(): "considered enabled if both the
    // multi page scrolling flag is enabled [R#25 bit0] and the current page
    // is odd [R#2 bit5]. Scrolling wraps into the lower even page."
    return (vdp_->control_register(25) & 0x01) != 0 && (vdp_->control_register(2) & 0x20) != 0;
}

bool VdpFrameRenderer::use_alternate_page(const Field field) const {
    // A-M21-7: deliberately-narrower model, NOT a bit-for-bit reproduction
    // of VDP.hh:443-459's getEvenOddMask().
    //
    // getEvenOddMask() reduces to: false when blinkState, else true when
    // EITHER EO (R#9 bit2) is CLEAR or the chip's live eo_field_ toggle is
    // set. Naively substituting "field == Field::Odd" for that toggle looks
    // tempting, but its only call site (SDLRasterizer.cc:490-497) feeds
    // `pageMaskOdd`/`pageMaskEven`, which distinguish SCANLINE PARITY within
    // a frame for the multi-page-scroll smooth-blit effect -- NOT interlace
    // FIELD selection (a different feature this renderer doesn't attempt).
    // That substitution would make "EO clear" (the power-on default) ALWAYS
    // force alternation regardless of the requested Field -- silently
    // flipping every bitmap-mode page read for callers that never asked for
    // interlace.
    //
    // Instead: alternation only engages when EO is explicitly SET (R#9
    // bit2) and not suppressed by blink; Field::Odd then selects the
    // alternate page, Field::Even/Progressive the configured one. Acceptance
    // bar: parity with openMSX's MODELED CONCEPT (itself flagged "TODO:
    // verify" by the reference authors), not bit-for-bit getEvenOddMask()
    // reproduction or independently-proven hardware ground truth.
    if (vdp_->blink_state()) {
        return false;
    }
    const bool eo_enabled = (vdp_->control_register(9) & 0x04) != 0;
    return eo_enabled && field == Field::Odd;
}

std::uint32_t VdpFrameRenderer::resolve_bitmap_page(const std::uint32_t page_mask, const Field field) const {
    std::uint32_t page = (static_cast<std::uint32_t>(vdp_->control_register(2)) >> 5) & page_mask;
    if (use_alternate_page(field)) {
        page ^= 0x01u;
    }
    if (multi_page_scrolling()) {
        page &= ~0x01u;
    }
    return page;
}

std::uint32_t VdpFrameRenderer::bitmap_row_base_nonplanar(const int line, const Field field) const {
    // G4/G5: 128 logical bytes/row x 256 rows = 0x8000/page, 2-bit page
    // selector (4 pages x 0x8000 = 128 KB, matches this machine's fixed
    // VRAM capacity).
    return resolve_bitmap_page(0x03, field) * 0x8000u + static_cast<std::uint32_t>(line) * 128u;
}

std::uint32_t VdpFrameRenderer::bitmap_row_base_planar(const int line, const Field field) const {
    // G6/G7/YJK: 256 logical bytes/row (both banks combined, pre-transform)
    // x 256 rows = 0x10000/page, 1-bit page selector (2 pages x 0x10000 =
    // 128 KB) -- the well-known MSX2 fact that SCREEN7/8 support only 2
    // pages where SCREEN5/6 support 4, independently re-derived here from
    // VRAM-capacity arithmetic.
    return resolve_bitmap_page(0x01, field) * 0x10000u + static_cast<std::uint32_t>(line) * 256u;
}

std::uint16_t VdpFrameRenderer::border_color() const {
    const VdpModeState& m = vdp_->mode();
    const std::uint8_t reg7 = vdp_->control_register(7);
    if (m.mode == VdpMode::Graphic7) {
        // VDP.hh:216-226 getBackgroundColor() / SDLRasterizer.cc:384-393
        // getBorderColors(): full byte through the fixed 256-color decode.
        // Pure GRAPHIC7 only -- YJK/YJK+YAE (sharing base 0x1C) use the
        // plain 16-color palette instead, matching the fact-sheet's
        // "sprites use the 16-colour palette" note for YJK modes.
        return graphic7_fixed_color_to_rgb555(reg7);
    }
    if (m.base == 0x10) {
        // GRAPHIC5 (SCREEN6): real hardware alternates two border colors
        // (SDLRasterizer.cc:378-383, bits3-2 even / bits1-0 odd); since
        // FrameBuffer.border_color is a single value (§2.2), the even-pixel
        // half is used as a documented simplification.
        return pal16((reg7 & 0x0C) >> 2);
    }
    return pal16(reg7 & 0x0F);  // VDP.hh:216-226, the common case
}

void VdpFrameRenderer::dispatch_content(const int line, const Field field, std::span<std::uint16_t> out) const {
    switch (vdp_->mode().mode) {
    case VdpMode::Text1:
        render_text1(line, out);
        break;
    case VdpMode::Text2:
        render_text2(line, out);
        break;
    case VdpMode::Graphic1:
        render_graphic1(vertical_scroll_wrap(line), out);
        break;
    case VdpMode::Graphic2:
    case VdpMode::Graphic3:
        render_graphic2_or_3(vertical_scroll_wrap(line), out);
        break;
    case VdpMode::Multicolor:
        render_multicolor(vertical_scroll_wrap(line), out);
        break;
    case VdpMode::Graphic4:
        render_graphic4(vertical_scroll_wrap(line), out);
        break;
    case VdpMode::Graphic5:
        render_graphic5(vertical_scroll_wrap(line), out);
        break;
    case VdpMode::Graphic6:
        render_graphic6(vertical_scroll_wrap(line), field, out);
        break;
    case VdpMode::Graphic7:
        render_graphic7(vertical_scroll_wrap(line), field, out);
        break;
    case VdpMode::ScreenYjk:
        render_yjk(vertical_scroll_wrap(line), field, out);
        break;
    case VdpMode::ScreenYjkYae:
        render_yjk_yae(vertical_scroll_wrap(line), field, out);
        break;
    case VdpMode::Text1Q:
    case VdpMode::MulticolorQ:
    case VdpMode::Unknown:
    default:
        render_blank(out);
        break;
    }
}

void VdpFrameRenderer::composite_sprites(const int line, const Field /*field*/, std::span<std::uint16_t> out) const {
    // Sprite mode determination shared with SpriteEngine::recompute_frame()
    // (vdp_sprite_mode(), vdp_mode.h) -- a deliberate anti-drift measure.
    const VdpModeState& m = vdp_->mode();
    const int sprite_mode = vdp_sprite_mode(m.base);
    if (sprite_mode == 0) {
        return;
    }
    const std::span<const SpriteEngine::VisibleSprite> visible = vdp_->sprite_engine().visible_sprites(line);
    if (visible.empty()) {
        return;
    }

    // Color-0 transparency is conditioned on R#8 TP (A-M22-12, a genuine
    // fact-sheet-vs-source discrepancy resolved in favor of the read
    // source): TP bit CLEAR -> transparency ON (skip color-0 pixels).
    const bool tp_enabled = (vdp_->control_register(8) & 0x20) == 0;
    const int w = width();
    // Sprite X coordinates live in a 256-wide space even on the 512-wide
    // GRAPHIC5/GRAPHIC6 canvases -- each sprite pixel spans TWO canvas pixels
    // (out[x*2+0/1] below), so the per-pixel loop must clip at the
    // sprite-space limit (256), NOT the canvas width. Reference contract:
    // SpriteConverter.hh:134-143 documents the mode-2 buffer as "256 pixels
    // wide" with maxX clipping via clipPattern(), writing pixelPtr[x*2+0/1]
    // for GRAPHIC5/6 (:186-198). Using canvas width here let a right-edge
    // sprite (SCREEN 6 boot-logo sliding letters) write out[x*2+1] past the
    // row span -- an out-of-bounds write (debug-CRT abort during boot logo).
    const bool doubled = (m.mode == VdpMode::Graphic5 || m.mode == VdpMode::Graphic6);
    const int x_limit = doubled ? w / 2 : w;

    if (sprite_mode == 1) {
        // Mode 1: reverse-priority overdraw -- draw the highest sprite index
        // FIRST, sprite 0 LAST, so sprite 0 visually wins overlaps
        // (SpriteConverter.hh:99-132 drawMode1).
        for (std::size_t idx = visible.size(); idx-- > 0;) {
            const SpriteEngine::VisibleSprite& s = visible[idx];
            const std::uint8_t color_index = static_cast<std::uint8_t>(s.color_attrib & 0x0F);
            if (color_index == 0 && tp_enabled) continue;
            const std::uint16_t color = pal16(color_index);
            int x = s.x;
            std::uint32_t pattern = s.pattern;
            if (x < 0) {
                if (x <= -32) continue;
                pattern <<= static_cast<unsigned>(-x);
                x = 0;
            }
            for (; pattern != 0 && x < x_limit; pattern <<= 1, ++x) {
                if (pattern & 0x8000'0000u) {
                    out[static_cast<std::size_t>(x)] = color;
                }
            }
        }
        return;
    }

    // Mode 2: find the first (lowest index) sprite with CC=0
    // (SpriteConverter.hh:144-205 drawMode2); sprites before it (all CC=1,
    // with no preceding CC=0 sprite) are never drawn at all.
    std::size_t first = 0;
    while (first < visible.size() && (visible[first].color_attrib & 0x40) != 0) {
        ++first;
    }
    for (std::size_t idx = visible.size(); idx-- > first;) {
        const SpriteEngine::VisibleSprite& s = visible[idx];
        int x = s.x;
        std::uint32_t pattern = s.pattern;
        if (x < 0) {
            if (x <= -32) continue;
            pattern <<= static_cast<unsigned>(-x);
            x = 0;
        }
        const std::uint8_t base_color = static_cast<std::uint8_t>(s.color_attrib & 0x0F);
        if (base_color == 0 && tp_enabled) continue;
        for (; pattern != 0 && x < x_limit; pattern <<= 1, ++x) {
            if (!(pattern & 0x8000'0000u)) continue;
            std::uint8_t color = base_color;
            // OR-merge any immediately-following (higher index, contiguous)
            // CC=1 sprites' color bits into this drawn pixel (A-M22-10).
            for (std::size_t j = idx + 1; j < visible.size(); ++j) {
                const SpriteEngine::VisibleSprite& s2 = visible[j];
                if ((s2.color_attrib & 0x40) == 0) break;
                const int shift2 = x - s2.x;
                if (shift2 >= 0 && shift2 < 32 && ((s2.pattern << static_cast<unsigned>(shift2)) & 0x8000'0000u)) {
                    color = static_cast<std::uint8_t>(color | (s2.color_attrib & 0x0F));
                }
            }
            if (m.mode == VdpMode::Graphic5) {
                // GRAPHIC5 (SCREEN6): split the 4-bit merged color into two
                // 2-bit half-pixel palette lookups (SpriteConverter.hh:186-190).
                out[static_cast<std::size_t>(x) * 2 + 0] = pal16(color >> 2);
                out[static_cast<std::size_t>(x) * 2 + 1] = pal16(color & 0x03);
            } else if (m.mode == VdpMode::Graphic6) {
                // GRAPHIC6 (SCREEN7): the same pixel written twice (512-wide
                // canvas, 256-wide sprite coordinate space).
                const std::uint16_t pix = pal16(color);
                out[static_cast<std::size_t>(x) * 2 + 0] = pix;
                out[static_cast<std::size_t>(x) * 2 + 1] = pix;
            } else {
                out[static_cast<std::size_t>(x)] = pal16(color);
            }
        }
    }
}

void VdpFrameRenderer::render_line(const int line, const Field field, std::span<std::uint16_t> out) const {
    // M34 Defect B (DEC-0043; docs/m34-planner-package.md §3.2): R#1 bit6
    // (BL, display enable) gates the ENTIRE active line. BL=0 => pure
    // backdrop -- content dispatch, sprite compositing, and the R#25 MSK
    // step are all skipped (sprites already honored this bit,
    // sprite_engine.cpp:90-94; this closes the pre-M34 asymmetry where
    // sprites obeyed BL but the background didn't). Grounding (both
    // references agree, §3.1):
    //   * openMSX references/openmsx-21.0/src/video/PixelRenderer.cc:608-611
    //     (!displayEnabled => whole line draws DRAW_BORDER) and :580-584
    //     (sprite checking only under displayEnabled); VDP.cc:435-442
    //     (displayEnabled derives from controlRegs[1] & 0x40).
    //   * fMSX references/fmsx-60/source/MSX.h:216 (#define ScreenON
    //     (VDP[1]&0x40)) with every per-line refresh in Common.h (:463,
    //     :497, :533, ...) starting `if(!ScreenON) ClearLine(background)`.
    // Fill color = mode-aware border_color() (VDP.hh:211-226
    // getBackgroundColor, the real border's color), NOT render_blank()'s
    // undefined-mode palette-15 fallback (different semantic,
    // CharacterConverter.cc:368-373). render_line() reads R#1 LIVE and is
    // shared by render_frame() and the M32 scanline accumulator, so the L+1
    // write-latch (hbf1xv_machine.cpp write hook) applies to BL like any
    // other register -- matching VDP.cc:1080-1082/:1260-1269 (an R#1 bit6
    // change takes effect at the NEXT line; syncAtNextLine). Mid-LINE BL
    // precision is the pre-existing D8 remainder; BL=1-mid-frame
    // sprite-table liveness is D9 (cross-notes in the ledger).
    if ((vdp_->control_register(1) & 0x40) == 0) {
        const std::uint16_t bc = border_color();
        const int w = width();
        for (int x = 0; x < w; ++x) {
            out[static_cast<std::size_t>(x)] = bc;
        }
        return;
    }

    dispatch_content(line, field, out);
    composite_sprites(line, field, out);

    // Border mask (R#25 bit1 MSK, VDP.hh:353-360): "extends the left border
    // by 8 pixels." Since border is modeled as a single color, not extra
    // canvas columns (§2.2), this overwrites the leftmost 8 (or fewer, if
    // narrower) active-display pixels with the border color.
    if (vdp_->control_register(25) & 0x02) {
        const int n = std::min(8, width());
        const std::uint16_t bc = border_color();
        for (int x = 0; x < n; ++x) {
            out[static_cast<std::size_t>(x)] = bc;
        }
    }
}

FrameBuffer VdpFrameRenderer::render_frame(const Field field) const {
    FrameBuffer fb;
    fb.width = width();
    fb.height = height();
    fb.pixels.assign(static_cast<std::size_t>(fb.width) * static_cast<std::size_t>(fb.height), 0);
    fb.border_color = border_color();
    for (int line = 0; line < fb.height; ++line) {
        std::span<std::uint16_t> row(fb.pixels.data() + static_cast<std::size_t>(line) * static_cast<std::size_t>(fb.width),
                                     static_cast<std::size_t>(fb.width));
        render_line(line, field, row);
    }
    return fb;
}

// --- character/tile modes ----------------------------------------------

void VdpFrameRenderer::render_text1(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:142-160 (fg/bg via palFg, :144-145 -> content_pal16).
    const std::uint16_t fg = content_pal16(vdp_->control_register(7) >> 4);
    const std::uint16_t bg = content_pal16(vdp_->control_register(7) & 0x0F);
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pattern_base = pattern_table_base();
    const int row_in_cell = (line + vdp_->control_register(23)) & 0x07;
    const int name_row = (line / 8) * 40;
    for (int col = 0; col < 40; ++col) {
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + col));
        const std::uint8_t pattern = vram_read(
            pattern_base + static_cast<std::uint32_t>(char_code) * 8 + static_cast<std::uint32_t>(row_in_cell));
        draw6(out.data() + col * 6, fg, bg, pattern);
    }
}

void VdpFrameRenderer::render_text2(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:183-231, incl. the blink color-flip (:184-190).
    // Plain colors go through palFg (:189-190 -> content_pal16); the blink
    // colors go through palBg (:194-195), the RAW palette -- deliberately
    // NOT substituted.
    const std::uint16_t plain_fg = content_pal16(vdp_->control_register(7) >> 4);
    const std::uint16_t plain_bg = content_pal16(vdp_->control_register(7) & 0x0F);
    std::uint16_t blink_fg = plain_fg;
    std::uint16_t blink_bg = plain_bg;
    if (vdp_->blink_state()) {
        const int bfg = vdp_->control_register(12) >> 4;
        const int bbg = vdp_->control_register(12) & 0x0F;
        blink_fg = pal16(bfg != 0 ? bfg : bbg);
        blink_bg = pal16(bbg);
    }
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pattern_base = pattern_table_base();
    const std::uint32_t color_base = color_table_base();
    const int row_in_cell = (line + vdp_->control_register(23)) & 0x07;
    const int name_row = (line / 8) * 80;
    const int color_row = (line / 8) * 10;
    for (int block = 0; block < 10; ++block) {
        const std::uint8_t color_byte = vram_read(color_base + static_cast<std::uint32_t>(color_row + block));
        for (int bit = 0; bit < 8; ++bit) {
            const int col = block * 8 + bit;
            const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + col));
            const std::uint8_t pattern = vram_read(
                pattern_base + static_cast<std::uint32_t>(char_code) * 8 + static_cast<std::uint32_t>(row_in_cell));
            const bool blinking = (color_byte & (0x80 >> bit)) != 0;
            draw6(out.data() + col * 6, blinking ? blink_fg : plain_fg, blinking ? blink_bg : plain_bg, pattern);
        }
    }
}

void VdpFrameRenderer::render_graphic1(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:255-270.
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pattern_base = pattern_table_base();
    const std::uint32_t color_base = color_table_base();
    const int l = line & 7;
    const int name_row = (line / 8) * 32;
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint8_t pattern =
            vram_read(pattern_base + static_cast<std::uint32_t>(char_code) * 8 + static_cast<std::uint32_t>(l));
        const std::uint8_t color = vram_read(color_base + static_cast<std::uint32_t>(char_code) / 8);
        draw8(out.data() + col * 8, content_pal16(color >> 4), content_pal16(color & 0x0F), pattern);
    }
}

void VdpFrameRenderer::render_graphic2_or_3(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:272-297 (renderGraphic2, reused for GRAPHIC3):
    // pattern/color tables are "quartered" into 2048-byte blocks, one per 64
    // raster lines (3 quarters span the 192-line canvas).
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pattern_base = pattern_table_base();
    const std::uint32_t color_base = color_table_base();
    const std::uint32_t quarter = static_cast<std::uint32_t>(line / 64) * 2048u;
    const int l7 = line & 7;
    const int name_row = (line / 8) * 32;
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint32_t offset = quarter + static_cast<std::uint32_t>(char_code) * 8 + static_cast<std::uint32_t>(l7);
        const std::uint8_t pattern = vram_read(pattern_base + offset);
        const std::uint8_t color = vram_read(color_base + offset);
        draw8(out.data() + col * 8, content_pal16(color >> 4), content_pal16(color & 0x0F), pattern);
    }
}

void VdpFrameRenderer::render_multicolor(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:299-325 (renderMultiHelper/renderMulti). The
    // real pixel canvas is 256x192 with 4x4-pixel color-cell granularity
    // (A-M21-9), NOT a literal 64x48-pixel image.
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pattern_base = pattern_table_base();
    const int name_row = (line / 8) * 32;
    const int pattern_sub = (line / 4) & 7;
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint8_t color = vram_read(
            pattern_base + static_cast<std::uint32_t>(char_code) * 8 + static_cast<std::uint32_t>(pattern_sub));
        const std::uint16_t cl = content_pal16(color >> 4);
        const std::uint16_t cr = content_pal16(color & 0x0F);
        std::uint16_t* p = out.data() + col * 8;
        p[0] = p[1] = p[2] = p[3] = cl;
        p[4] = p[5] = p[6] = p[7] = cr;
    }
}

void VdpFrameRenderer::render_blank(std::span<std::uint16_t> out) const {
    // A-M21-6: TEXT1Q/MULTIQ/Unknown render a flat fill of palette entry 15
    // (CharacterConverter.cc:368-373 renderBlank), never TMS9918-compatible
    // striped content -- HB-F1XV's V9958 is never isMSX1VDP().
    const std::uint16_t color = pal16(15);
    const int w = width();
    for (int x = 0; x < w; ++x) {
        out[static_cast<std::size_t>(x)] = color;
    }
}

// --- non-planar bitmap modes ---------------------------------------------

void VdpFrameRenderer::render_graphic4(const int line, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:104-133 (renderGraphic4): 4bpp packed, high nibble
    // = even pixel.
    const std::uint32_t row_base = bitmap_row_base_nonplanar(line, Field::Progressive);
    std::array<std::uint16_t, 256> temp{};
    for (int i = 0; i < 128; ++i) {
        const std::uint8_t b = vram_read(row_base + static_cast<std::uint32_t>(i));
        temp[static_cast<std::size_t>(2 * i + 0)] = content_pal16(b >> 4);
        temp[static_cast<std::size_t>(2 * i + 1)] = content_pal16(b & 0x0F);
    }
    apply_bitmap_scroll(out, temp.data(), 256);
}

void VdpFrameRenderer::render_graphic5(const int line, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:135-147 (renderGraphic5): 2bpp packed. Real
    // hardware doubles palette16 (even/odd halves), but absent superimpose
    // (no digitizer, fact-sheet §9) the two halves are always identical
    // (SDLRasterizer.cc:104-109: `palFg[i] = palFg[i+16] = palBg[i] = ...`),
    // so both parities read the SAME 4 palette registers (0-3) directly.
    const std::uint32_t row_base = bitmap_row_base_nonplanar(line, Field::Progressive);
    std::array<std::uint16_t, 512> temp{};
    for (int i = 0; i < 128; ++i) {
        const std::uint8_t b = vram_read(row_base + static_cast<std::uint32_t>(i));
        temp[static_cast<std::size_t>(4 * i + 0)] = content_pal16_g5((b >> 6) & 0x03, false);
        temp[static_cast<std::size_t>(4 * i + 1)] = content_pal16_g5((b >> 4) & 0x03, true);
        temp[static_cast<std::size_t>(4 * i + 2)] = content_pal16_g5((b >> 2) & 0x03, false);
        temp[static_cast<std::size_t>(4 * i + 3)] = content_pal16_g5(b & 0x03, true);
    }
    apply_bitmap_scroll(out, temp.data(), 512);
}

// --- planar bitmap modes (D7 display-path piece) -------------------------

void VdpFrameRenderer::render_graphic6(const int line, const Field field, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:149-183 (renderGraphic6, per-byte form): 4bpp
    // planar.
    const std::uint32_t row_base = bitmap_row_base_planar(line, field);
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    std::array<std::uint16_t, 512> temp{};
    for (std::size_t i = 0; i < spans.half_length; ++i) {
        const std::uint8_t d0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(i));
        const std::uint8_t d1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(i));
        temp[4 * i + 0] = content_pal16(d0 >> 4);
        temp[4 * i + 1] = content_pal16(d0 & 0x0F);
        temp[4 * i + 2] = content_pal16(d1 >> 4);
        temp[4 * i + 3] = content_pal16(d1 & 0x0F);
    }
    apply_bitmap_scroll(out, temp.data(), 512);
}

void VdpFrameRenderer::render_graphic7(const int line, const Field field, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:185-195 (renderGraphic7): one fixed-256-color byte
    // per pixel (A-M21-4), banks alternate even/odd output pixels.
    const std::uint32_t row_base = bitmap_row_base_planar(line, field);
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    std::array<std::uint16_t, 256> temp{};
    for (std::size_t i = 0; i < spans.half_length; ++i) {
        const std::uint8_t d0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(i));
        const std::uint8_t d1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(i));
        temp[2 * i + 0] = graphic7_fixed_color_to_rgb555(d0);
        temp[2 * i + 1] = graphic7_fixed_color_to_rgb555(d1);
    }
    apply_bitmap_scroll(out, temp.data(), 256);
}

namespace {

// Shared J/K unpack for the YJK family (BitmapConverter.cc:217-249), per
// 4-pixel group: p[0]=plane0-even, p[1]=plane1-even, p[2]=plane0-odd,
// p[3]=plane1-odd.
struct YjkGroup {
    int j;
    int k;
    std::array<std::uint8_t, 4> p;
};

[[nodiscard]] YjkGroup unpack_yjk_group(const std::uint8_t p0, const std::uint8_t p1, const std::uint8_t p2,
                                        const std::uint8_t p3) {
    const int k = (p0 & 7) + ((p1 & 3) << 3) - ((p1 & 4) << 3);
    const int j = (p2 & 7) + ((p3 & 3) << 3) - ((p3 & 4) << 3);
    return YjkGroup{j, k, {p0, p1, p2, p3}};
}

}  // namespace

void VdpFrameRenderer::render_yjk(const int line, const Field field, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:230-249 (renderYJK).
    const std::uint32_t row_base = bitmap_row_base_planar(line, field);
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    std::array<std::uint16_t, 256> temp{};
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint8_t p0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(2 * i + 0));
        const std::uint8_t p1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(2 * i + 0));
        const std::uint8_t p2 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(2 * i + 1));
        const std::uint8_t p3 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(2 * i + 1));
        const YjkGroup group = unpack_yjk_group(p0, p1, p2, p3);
        for (int n = 0; n < 4; ++n) {
            const int y = group.p[static_cast<std::size_t>(n)] >> 3;
            const YjkRgb rgb = yjk_to_rgb(y, group.j, group.k);
            temp[4 * i + static_cast<std::size_t>(n)] =
                pack_rgb555(static_cast<std::uint8_t>(rgb.r), static_cast<std::uint8_t>(rgb.g),
                            static_cast<std::uint8_t>(rgb.b));
        }
    }
    apply_bitmap_scroll(out, temp.data(), 256);
}

void VdpFrameRenderer::render_yjk_yae(const int line, const Field field, std::span<std::uint16_t> out) const {
    // BitmapConverter.cc:251-276 (renderYAE): identical J/K unpack, but each
    // pixel's bit3 (0x08) selects the 16-color palette branch instead of the
    // computed YJK color.
    const std::uint32_t row_base = bitmap_row_base_planar(line, field);
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    std::array<std::uint16_t, 256> temp{};
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint8_t p0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(2 * i + 0));
        const std::uint8_t p1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(2 * i + 0));
        const std::uint8_t p2 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(2 * i + 1));
        const std::uint8_t p3 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(2 * i + 1));
        const YjkGroup group = unpack_yjk_group(p0, p1, p2, p3);
        for (int n = 0; n < 4; ++n) {
            const std::uint8_t pn = group.p[static_cast<std::size_t>(n)];
            std::uint16_t pixel;
            if (pn & 0x08) {
                // The YAE palette branch reads palette16 == palFg
                // (BitmapConverter.cc:271-275) -> content_pal16.
                pixel = content_pal16(pn >> 4);
            } else {
                const int y = pn >> 3;
                const YjkRgb rgb = yjk_to_rgb(y, group.j, group.k);
                pixel = pack_rgb555(static_cast<std::uint8_t>(rgb.r), static_cast<std::uint8_t>(rgb.g),
                                    static_cast<std::uint8_t>(rgb.b));
            }
            temp[4 * i + static_cast<std::size_t>(n)] = pixel;
        }
    }
    apply_bitmap_scroll(out, temp.data(), 256);
}

}  // namespace sony_msx::devices::video
