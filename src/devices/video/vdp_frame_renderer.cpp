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

// V9958 table index-mask fills (DEF-M43-FMPAC-SCREEN / DEC-0063): the high-bit
// "unused bits set to 1" fill that openMSX's readNP index carries
// (VDPVRAM.hh:263-269), pre-masked to the 17-bit (128 KB) VRAM space. Each is
// `(~0u << N) & 0x1FFFF`, where N is the per-table/per-mode index width from
// openMSX's updatePatternBase/updateColorBase (VDP.cc:1299-1355):
//   pattern -- Text1/Text2/G1/Multicolor: ~0u<<11 ; G2/G3: ~0u<<13
//   color   -- Text2: ~0u<<9 ; G1: ~0u<<6 ; G2/G3: ~0u<<13
// ANDing the base register's effectiveBaseMask (pattern_table_mask()/
// color_table_mask()) with (fill | actualIndex) reproduces readNP's
// `data[effectiveBaseMask & index]` exactly, so the low mirror bits of R#3/R#4
// mask the index rather than adding to it.
constexpr std::uint32_t kFill6 = 0x1FFC0u;   // ~0u<<6  (G1 color)
constexpr std::uint32_t kFill9 = 0x1FE00u;   // ~0u<<9  (Text2 color)
constexpr std::uint32_t kFill11 = 0x1F800u;  // ~0u<<11 (Text/G1/Multicolor pattern)
constexpr std::uint32_t kFill13 = 0x1E000u;  // ~0u<<13 (G2/G3 pattern+color, quarter boundary)

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

std::uint32_t VdpFrameRenderer::pattern_table_mask() const {
    // Pattern-table effectiveBaseMask = (R#4<<11) | ~(~0u<<11), ANDed with the
    // 128 KB size mask (openMSX VDP.cc:1323-1355 updatePatternBase setMask;
    // VDPVRAM.hh:153-180/:266-269 readNP). The forced-1 low 11 bits are the
    // legacy TMS9918 mirror bits, NOT additive address bits: canonical SCREEN 2
    // R#4=0x03 keeps the pattern generator at 0x0000 (only the two quarter-select
    // bits 11-12 must be set to let the 3-quarter index through), whereas the old
    // additive `R#4<<11` wrongly returned 0x1800 (the name table) -- the DEF-M43
    // blank-screen defect.
    return ((static_cast<std::uint32_t>(vdp_->control_register(4)) << 11) | 0x7FFu) & 0x1FFFFu;
}

std::uint32_t VdpFrameRenderer::color_table_mask() const {
    // Color-table effectiveBaseMask = (R#10<<14) | (R#3<<6) | ~(~0u<<6), ANDed
    // with the size mask (openMSX VDP.cc:1299-1321 updateColorBase). Canonical
    // SCREEN 2 R#3=0xFF -> colour base 0x2000 (bit 13 comes from R#3 bit 7; the
    // low 6 bits are mask bits), NOT the additive 0x3FC0 that read past the colour
    // table into the backdrop.
    return ((static_cast<std::uint32_t>(vdp_->control_register(10)) << 14) |
            (static_cast<std::uint32_t>(vdp_->control_register(3)) << 6) | 0x3Fu) & 0x1FFFFu;
}

int VdpFrameRenderer::vertical_scroll_wrap(const int line) const {
    return (line + vdp_->control_register(23)) & 0xFF;  // VDP.hh:328-333 + PixelRenderer.cc:44-49
}

int VdpFrameRenderer::scrolled_name_col(const int col) const {
    return (col + vdp_->control_register(26)) & 0x1F;  // CharacterConverter.cc:255-270
}

int VdpFrameRenderer::bitmap_coarse_shift(const int width) const {
    // R#26 coarse horizontal scroll (SDLRasterizer.cc:468-471): rotates the
    // displayed bitmap line LEFT in 8-dot steps -- `8 * (lineWidth/256) *
    // (R#26 & 0x1F)`, i.e. 8-dot units for 256-wide modes and 16-dot units for
    // the 512-wide G5/G6. Range [0, 248] (256-wide) / [0, 496] (512-wide),
    // always < width, so no wrap of the shift value itself is needed. This is
    // the ORIGINAL, correct coarse behavior (M38 Phase-A scenarios s02/s07
    // already MATCHED); the fine component is a SEPARATE mechanism.
    const int mult = (width >= 512) ? 2 : 1;
    return (vdp_->control_register(26) & 0x1F) * 8 * mult;
}

int VdpFrameRenderer::bitmap_fine_shift(const int width) const {
    // R#27 fine horizontal scroll (VDP.hh:335-342 getHorizontalScrollLow;
    // VDP.hh:629-631 getLeftBackground() = getLeftSprites() + R#27*4;
    // SDLRasterizer.cc:464-465). Shifts the WHOLE displayed image to the RIGHT
    // by 0..7 dots (doubled for 512-wide modes) and exposes the vacated left
    // edge as BORDER/backdrop -- NOT a circular wrap, NOT the same sign as the
    // coarse rotate (M38 Phase-A root cause, s03/s04).
    const int mult = (width >= 512) ? 2 : 1;
    return (vdp_->control_register(27) & 0x07) * mult;
}

void VdpFrameRenderer::compose_bitmap_scroll(std::span<std::uint16_t> out, const std::uint16_t* page_first,
                                             const std::uint16_t* page_wrap, const int width,
                                             const int content_lead) const {
    // Compose one bitmap scanline from the decoded page content, applying the
    // V9958 horizontal-scroll model re-derived (never copied) from openMSX
    // SDLRasterizer.cc:464-538 + PixelRenderer.cc:586-604:
    //   1. COARSE (R#26): a LEFT rotation by `coarse` dots. Output column x in
    //      [0, width-coarse) reads page_first[x+coarse]; the wrap tail x in
    //      [width-coarse, width) reads page_wrap[x+coarse-width]. For a single
    //      page (non multi-page) page_wrap == page_first, so this degenerates to
    //      the pre-M38 circular left rotation. For multi-page scroll (R#25 bit0
    //      + R#2 bit5) page_wrap is the ADJACENT page, so the tail shows the
    //      neighbouring page instead of a self-wrap (SDLRasterizer.cc:530-537).
    //   2. FINE (R#27): the coarse-rotated line is then shifted RIGHT by `fine`
    //      dots; the vacated left `fine` columns show the BORDER/backdrop
    //      ("the 0..7 extra horizontal scroll low pixels should be drawn in
    //      border color", PixelRenderer.cc:587-594) and the rightmost `fine`
    //      content dots fall off the right edge (clipped -- they would land in
    //      the right border on the full bordered canvas).
    const int coarse = bitmap_coarse_shift(width);
    const int fine = bitmap_fine_shift(width);
    // The R#27 fine right-shift and the mode-intrinsic YJK/YAE content_lead are
    // BOTH rightward content displacements that expose BORDER on the vacated
    // left edge, so they add: the leftmost `left` output dots are border and
    // the decoded page is registered `content_lead` dots right of its G7 base
    // (content_lead == 0 for every non-YJK caller => byte-identical to before).
    const int left = fine + content_lead;
    const std::uint16_t bc = border_color();
    for (int x = 0; x < width; ++x) {
        if (x < left) {
            out[static_cast<std::size_t>(x)] = bc;  // exposed left border strip (R#27 fine + YJK lead)
            continue;
        }
        const int cx = x - left;      // column within the coarse-rotated line
        const int src = cx + coarse;  // source column before the left rotation
        out[static_cast<std::size_t>(x)] =
            (src < width) ? page_first[src] : page_wrap[src - width];
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

std::pair<std::uint32_t, std::uint32_t> VdpFrameRenderer::bitmap_scroll_pages(const std::uint32_t page_mask,
                                                                             const Field field) const {
    // The {first, wrap} page pair fed to compose_bitmap_scroll(). page_mask is
    // 0x03 for G4/G5 (2-bit page, 4 x 0x8000 = 128 KB) or 0x01 for G6/G7/YJK
    // (1-bit page, 2 x 0x10000 = 128 KB) -- re-derived from VRAM capacity,
    // matching SCREEN5/6's 4 pages vs SCREEN7/8's 2.
    const std::uint32_t page = resolve_bitmap_page(page_mask, field);
    if (!multi_page_scrolling()) {
        return {page, page};
    }
    // Multi-page scroll pair (VDP.hh:362-370 isMultiPageScrolling(), "wraps into
    // the lower even page"; SDLRasterizer.cc:479-538). resolve_bitmap_page()
    // already forced the low page bit clear when multi-page is active, so `page`
    // is the EVEN member of the pair and `page | 1` its ODD neighbour. R#26 bit5
    // (openMSX `p1 = getHorizontalScrollHigh() >> 5`) selects which member is
    // drawn first; the other supplies the coarse wrap tail. So even with R#2
    // selecting the odd page, the primary content is the even page and the tail
    // wraps in from the odd page (or vice-versa when p1 is set).
    const std::uint32_t even = page;
    const std::uint32_t odd = (page | 0x01u) & page_mask;
    const int p1 = (vdp_->control_register(26) >> 5) & 0x01;
    if (p1 != 0) {
        return {odd, even};
    }
    return {even, odd};
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
    const std::uint32_t pat_mask = pattern_table_mask();
    const std::uint32_t row_in_cell = static_cast<std::uint32_t>((line + vdp_->control_register(23)) & 0x07);
    const int name_row = (line / 8) * 40;
    for (int col = 0; col < 40; ++col) {
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + col));
        // Text1 pattern index (openMSX renderText1 / VDP.cc:1328-1335 ~0u<<11).
        const std::uint8_t pattern =
            vram_read(pat_mask & (kFill11 | (static_cast<std::uint32_t>(char_code) * 8u) | row_in_cell));
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
    const std::uint32_t pat_mask = pattern_table_mask();
    const std::uint32_t col_mask = color_table_mask();
    const std::uint32_t row_in_cell = static_cast<std::uint32_t>((line + vdp_->control_register(23)) & 0x07);
    const int name_row = (line / 8) * 80;
    const int color_row = (line / 8) * 10;
    for (int block = 0; block < 10; ++block) {
        // Text2 colour index (openMSX renderText2 CharacterConverter.cc:205-210:
        // readNP((colorStart+i) | (~0u<<9))).
        const std::uint8_t color_byte =
            vram_read(col_mask & (kFill9 | static_cast<std::uint32_t>(color_row + block)));
        for (int bit = 0; bit < 8; ++bit) {
            const int col = block * 8 + bit;
            const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + col));
            const std::uint8_t pattern =
                vram_read(pat_mask & (kFill11 | (static_cast<std::uint32_t>(char_code) * 8u) | row_in_cell));
            const bool blinking = (color_byte & (0x80 >> bit)) != 0;
            draw6(out.data() + col * 6, blinking ? blink_fg : plain_fg, blinking ? blink_bg : plain_bg, pattern);
        }
    }
}

void VdpFrameRenderer::render_graphic1(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:255-272; pattern index ~0u<<11, colour index ~0u<<6
    // (VDP.cc:1308/1331).
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pat_mask = pattern_table_mask();
    const std::uint32_t col_mask = color_table_mask();
    const std::uint32_t l = static_cast<std::uint32_t>(line & 7);
    const int name_row = (line / 8) * 32;
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint8_t pattern =
            vram_read(pat_mask & (kFill11 | (static_cast<std::uint32_t>(char_code) * 8u) | l));
        const std::uint8_t color = vram_read(col_mask & (kFill6 | (static_cast<std::uint32_t>(char_code) / 8u)));
        draw8(out.data() + col * 8, content_pal16(color >> 4), content_pal16(color & 0x0F), pattern);
    }
}

void VdpFrameRenderer::render_graphic2_or_3(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:299-315 (renderGraphic2 slow path, reused for
    // GRAPHIC3): the shared table index is baseLine | (charCode*8), baseLine =
    // (~0u<<13) | quarter8 | line7 (kFill13 in 17-bit VRAM space). The 2048-byte
    // quarter (bit 11-12) selects one of the 3 blocks per 64 raster lines; readNP
    // AND-masks the index by each table's effectiveBaseMask (pat_mask / col_mask)
    // -- the V9958 AND-mask model (DEF-M43-FMPAC-SCREEN / DEC-0063). With
    // canonical SCREEN 2 registers R#4=0x03 (pat_mask=0x1FFF) and R#3=0xFF
    // (col_mask=0x3FFF) this addresses pattern @ 0x0000 and colour @ 0x2000; the
    // former additive model wrongly hit 0x1800 / 0x3FC0 and rendered blank.
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pat_mask = pattern_table_mask();
    const std::uint32_t col_mask = color_table_mask();
    const std::uint32_t quarter8 = static_cast<std::uint32_t>(line / 64) * 2048u;
    const std::uint32_t line7 = static_cast<std::uint32_t>(line & 7);
    const std::uint32_t base_line = kFill13 | quarter8 | line7;
    const int name_row = (line / 8) * 32;
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint32_t index = base_line | (static_cast<std::uint32_t>(char_code) * 8u);
        const std::uint8_t pattern = vram_read(pat_mask & index);
        const std::uint8_t color = vram_read(col_mask & index);
        draw8(out.data() + col * 8, content_pal16(color >> 4), content_pal16(color & 0x0F), pattern);
    }
}

void VdpFrameRenderer::render_multicolor(const int line, std::span<std::uint16_t> out) const {
    // CharacterConverter.cc:318-342 (renderMultiHelper/renderMulti). The
    // real pixel canvas is 256x192 with 4x4-pixel color-cell granularity
    // (A-M21-9), NOT a literal 64x48-pixel image. Pattern index mask ~0u<<11
    // (VDP.cc:1332-1335), index = charCode*8 | ((line/4)&7).
    const std::uint32_t name_base = name_table_base();
    const std::uint32_t pat_mask = pattern_table_mask();
    const int name_row = (line / 8) * 32;
    const std::uint32_t pattern_sub = static_cast<std::uint32_t>((line / 4) & 7);
    for (int col = 0; col < 32; ++col) {
        const int src_col = scrolled_name_col(col);
        const std::uint8_t char_code = vram_read(name_base + static_cast<std::uint32_t>(name_row + src_col));
        const std::uint8_t color =
            vram_read(pat_mask & (kFill11 | (static_cast<std::uint32_t>(char_code) * 8u) | pattern_sub));
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
//
// Each bitmap renderer decodes the primary page's scanline into `first`, and --
// only when multi-page scroll makes the wrap page distinct -- the adjacent
// page's scanline into `wrap`, then defers the R#26/R#27 scroll composition to
// compose_bitmap_scroll(). The single-page fast path passes `first` for both
// buffers so behavior is byte-identical to the pre-M38 apply_bitmap_scroll()
// for the (overwhelmingly common) non multi-page case.

void VdpFrameRenderer::decode_graphic4_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:104-133 (renderGraphic4): 4bpp packed, high nibble
    // = even pixel.
    for (int i = 0; i < 128; ++i) {
        const std::uint8_t b = vram_read(row_base + static_cast<std::uint32_t>(i));
        temp[static_cast<std::size_t>(2 * i + 0)] = content_pal16(b >> 4);
        temp[static_cast<std::size_t>(2 * i + 1)] = content_pal16(b & 0x0F);
    }
}

void VdpFrameRenderer::render_graphic4(const int line, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x03, Field::Progressive);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 128u;
    std::array<std::uint16_t, 256> first{};
    decode_graphic4_row(page_first * 0x8000u + stride, first);
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 256> wrap{};
        decode_graphic4_row(page_wrap * 0x8000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 256);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 256);
    }
}

void VdpFrameRenderer::decode_graphic5_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:135-147 (renderGraphic5): 2bpp packed. Real
    // hardware doubles palette16 (even/odd halves), but absent superimpose
    // (no digitizer, fact-sheet §9) the two halves are always identical
    // (SDLRasterizer.cc:104-109: `palFg[i] = palFg[i+16] = palBg[i] = ...`),
    // so both parities read the SAME 4 palette registers (0-3) directly.
    for (int i = 0; i < 128; ++i) {
        const std::uint8_t b = vram_read(row_base + static_cast<std::uint32_t>(i));
        temp[static_cast<std::size_t>(4 * i + 0)] = content_pal16_g5((b >> 6) & 0x03, false);
        temp[static_cast<std::size_t>(4 * i + 1)] = content_pal16_g5((b >> 4) & 0x03, true);
        temp[static_cast<std::size_t>(4 * i + 2)] = content_pal16_g5((b >> 2) & 0x03, false);
        temp[static_cast<std::size_t>(4 * i + 3)] = content_pal16_g5(b & 0x03, true);
    }
}

void VdpFrameRenderer::render_graphic5(const int line, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x03, Field::Progressive);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 128u;
    std::array<std::uint16_t, 512> first{};
    decode_graphic5_row(page_first * 0x8000u + stride, first);
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 512> wrap{};
        decode_graphic5_row(page_wrap * 0x8000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 512);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 512);
    }
}

// --- planar bitmap modes (D7 display-path piece) -------------------------

void VdpFrameRenderer::decode_graphic6_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:149-183 (renderGraphic6, per-byte form): 4bpp
    // planar.
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    for (std::size_t i = 0; i < spans.half_length; ++i) {
        const std::uint8_t d0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(i));
        const std::uint8_t d1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(i));
        temp[4 * i + 0] = content_pal16(d0 >> 4);
        temp[4 * i + 1] = content_pal16(d0 & 0x0F);
        temp[4 * i + 2] = content_pal16(d1 >> 4);
        temp[4 * i + 3] = content_pal16(d1 & 0x0F);
    }
}

void VdpFrameRenderer::render_graphic6(const int line, const Field field, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x01, field);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 256u;
    std::array<std::uint16_t, 512> first{};
    decode_graphic6_row(page_first * 0x10000u + stride, first);
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 512> wrap{};
        decode_graphic6_row(page_wrap * 0x10000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 512);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 512);
    }
}

void VdpFrameRenderer::decode_graphic7_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:185-195 (renderGraphic7): one fixed-256-color byte
    // per pixel (A-M21-4), banks alternate even/odd output pixels.
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
    for (std::size_t i = 0; i < spans.half_length; ++i) {
        const std::uint8_t d0 = vram_read(spans.bank0_base + static_cast<std::uint32_t>(i));
        const std::uint8_t d1 = vram_read(spans.bank1_base + static_cast<std::uint32_t>(i));
        temp[2 * i + 0] = graphic7_fixed_color_to_rgb555(d0);
        temp[2 * i + 1] = graphic7_fixed_color_to_rgb555(d1);
    }
}

void VdpFrameRenderer::render_graphic7(const int line, const Field field, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x01, field);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 256u;
    std::array<std::uint16_t, 256> first{};
    decode_graphic7_row(page_first * 0x10000u + stride, first);
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 256> wrap{};
        decode_graphic7_row(page_wrap * 0x10000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 256);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 256);
    }
}

namespace {

// V9958 YJK/YAE horizontal display latency, in 256-wide-mode dots.
//
// Unlike GRAPHIC7 (each byte decodes to one pixel independently), a YJK/YAE
// 4-pixel group cannot be resolved until ALL FOUR of its bytes are fetched:
// the shared chroma J is packed into the group's 3rd/4th bytes (fact-sheet
// references/fact-sheets/Yamaha V9958 VDP.md:104-105, "pixel3 = Y3 J[low],
// pixel4 = Y4 J[high]"), so the group's first displayable pixel trails the G7
// base position by one whole group -- four dots. The vacated left edge shows
// backdrop and the last (clipped) group falls off the right. This four-dot
// rightward registration is corroborated three ways (DEF-M41-YJKOFFSET, an
// M41 production-QA finding): (1) openMSX 19.1 A/B -- YJK content lands 4 dots
// right of the IDENTICAL-base GRAPHIC7 control (which has zero offset); (2)
// fMSX 6.0 models it EXPLICITLY -- references/fmsx-60/source/fMSX/Common.h:
// 732-737 (RefreshLine10/YAE) and :778-783 (RefreshLine12/YJK) draw the first
// four pixels as backdrop (BPal[VDP[7]]) before the YJK groups; (3) the
// fact-sheet's YJK packing above. GRAPHIC7 keeps content_lead == 0. (openMSX
// 21.0's BitmapConverter -- the read-only source reference -- does not model
// this latency; the 19.1 RUNTIME used for the A/B and fMSX both do, and the
// hardware packing corroborates it, so the corroborated interpretation wins
// per the two-reference-disagreement guardrail.)
constexpr int kYjkDisplayLead = 4;

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

void VdpFrameRenderer::decode_yjk_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:230-249 (renderYJK).
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
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
}

void VdpFrameRenderer::render_yjk(const int line, const Field field, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x01, field);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 256u;
    std::array<std::uint16_t, 256> first{};
    decode_yjk_row(page_first * 0x10000u + stride, first);
    // kYjkDisplayLead: register the decoded page 4 dots right of the G7 base
    // (DEF-M41-YJKOFFSET). The R#26/R#27 scroll model is unchanged.
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 256> wrap{};
        decode_yjk_row(page_wrap * 0x10000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 256, kYjkDisplayLead);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 256, kYjkDisplayLead);
    }
}

void VdpFrameRenderer::decode_yjk_yae_row(const std::uint32_t row_base, std::span<std::uint16_t> temp) const {
    // BitmapConverter.cc:251-276 (renderYAE): identical J/K unpack, but each
    // pixel's bit3 (0x08) selects the 16-color palette branch instead of the
    // computed YJK color.
    const PlanarRowSpans spans = planar_row_spans(row_base, 256);
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
}

void VdpFrameRenderer::render_yjk_yae(const int line, const Field field, std::span<std::uint16_t> out) const {
    const auto [page_first, page_wrap] = bitmap_scroll_pages(0x01, field);
    const std::uint32_t stride = static_cast<std::uint32_t>(line) * 256u;
    std::array<std::uint16_t, 256> first{};
    decode_yjk_yae_row(page_first * 0x10000u + stride, first);
    // kYjkDisplayLead: same 4-dot right registration as plain YJK (the YAE
    // attribute branch does not change the group's display latency).
    if (page_first != page_wrap) {
        std::array<std::uint16_t, 256> wrap{};
        decode_yjk_yae_row(page_wrap * 0x10000u + stride, wrap);
        compose_bitmap_scroll(out, first.data(), wrap.data(), 256, kYjkDisplayLead);
    } else {
        compose_bitmap_scroll(out, first.data(), first.data(), 256, kYjkDisplayLead);
    }
}

}  // namespace sony_msx::devices::video
