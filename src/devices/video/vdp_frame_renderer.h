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
#include <span>
#include <utility>

#include "devices/video/frame_buffer.h"
#include "devices/video/v9958_vdp.h"

namespace sony_msx::devices::video {

// Display-path row-pointer resolution for the G6/G7 planar VRAM interleave.
// For an even LOGICAL row_base (always true
// for G6/G7 display rows: row_base = page*0x10000 + line*256), the CPU
// port's 17-bit rotate-right-by-1 degenerates to two CONTIGUOUS
// half-length spans in flat physical VRAM: even logical bytes at
// [bank0_base, bank0_base+half_length), odd at [bank1_base, +half_length)
// (bank1_base == bank0_base + 0x10000).
struct PlanarRowSpans {
    std::uint32_t bank0_base;
    std::uint32_t bank1_base;
    std::size_t half_length;
};

[[nodiscard]] PlanarRowSpans planar_row_spans(std::uint32_t row_base, std::size_t length);

// Deterministic, pull-model, frozen-register-snapshot VDP pixel renderer.
// Pure function of V9958Vdp's stored
// VRAM/register state: no elapsed_cycles() dependency, no new clock
// consumer. Carries no
// slot/bus knowledge (matches V9958Vdp, src/CLAUDE.md device-family
// placement). Behavior cites its openmsx-21.0/src/video/*.{hh,cc} grounding
// in vdp_frame_renderer.cpp; never copied verbatim (GPL isolation).
class VdpFrameRenderer {
public:
    explicit VdpFrameRenderer(const V9958Vdp& vdp);

    // Active-display pixel width/height for the CURRENT mode.
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;

    // Render one active-display scanline (0-based within [0, height())) into
    // `out` (must hold at least width() entries).
    void render_line(int line, Field field, std::span<std::uint16_t> out) const;

    // Render a full frame (frozen snapshot) in one call.
    [[nodiscard]] FrameBuffer render_frame(Field field = Field::Progressive) const;

    // Mode-aware backdrop/border color (VDP.hh:211-226 getBackgroundColor;
    // GRAPHIC7 fixed-color decode; the GRAPHIC5 even-pixel border
    // half, SDLRasterizer.cc:376-393 -- FrameBuffer.border_color is a single
    // value, so the even-pixel half is used for GRAPHIC5, a documented
    // simplification since real hardware alternates two border colors).
    [[nodiscard]] std::uint16_t border_color() const;

private:
    void dispatch_content(int line, Field field, std::span<std::uint16_t> out) const;

    // Sprite pixel compositing. Called from
    // render_line() after dispatch_content() fills the background row and
    // BEFORE the border-mask step (mirrors the reference's "background then
    // sprite overdraw" order). Reads
    // vdp_->sprite_engine().visible_sprites(line) -- state V9958Vdp::on_vsync()
    // already computed -- so this stays a PURE, read-only consumer.
    void composite_sprites(int line, Field field, std::span<std::uint16_t> out) const;

    // V9958 table addressing. The real V9958
    // uses the TMS9918-legacy AND-MASK model, NOT an additive base+offset: the
    // pattern/color table effectiveBaseMask forces the "unused" low bits of the
    // base register to 1 (mirror bits, not address bits) and ANDs it with the
    // index (which carries its own high-bit fill). Canonical SCREEN 2 registers
    // (R#4=0x03, R#3=0xFF) therefore address pattern @ 0x0000 / color @ 0x2000 --
    // an additive model wrongly lands them at 0x1800 / 0x3FC0 (blank screen),
    // the defect this mask model fixes. name_table_base() stays a plain base
    // (name addressing is
    // additive-equivalent for valid configs and was already correct); the
    // *_mask() helpers return the openMSX effectiveBaseMask (VDP.cc:1299-1355
    // updateColorBase/updatePatternBase; VDPVRAM.hh:153-180 setMask / :266-269
    // readNP `data[effectiveBaseMask & index]`) -- re-derived, never copied.
    // (DEC-0063)
    [[nodiscard]] std::uint32_t name_table_base() const;
    [[nodiscard]] std::uint32_t pattern_table_mask() const;
    [[nodiscard]] std::uint32_t color_table_mask() const;

    [[nodiscard]] std::uint8_t vram_read(std::uint32_t addr) const;
    [[nodiscard]] std::uint16_t pal16(int index) const;

    // Color-0 backdrop substitution for CONTENT pixels (the Konami-splash
    // fix). Real V99x8 behavior: R#8 TP bit CLEAR -> color 0 is TRANSPARENT,
    // so a color-0 content pixel shows the BACKDROP color (R#7 low nibble),
    // not palette entry 0. Grounded:
    //   * Yamaha V9958 VDP fact sheet ("TP
    //     colour0 transparent");
    //   * openMSX: VDP.hh:189-191 getTransparency() == (R#8 & 0x20) == 0;
    //     SDLRasterizer.cc:346-373 precalcColorIndex0() -- palFg[0] =
    //     palBg[tpIndex], tpIndex = transparency ? bgColor : 0. palFg feeds
    //     EVERY content path (SDLRasterizer.cc:99-100: characterConverter and
    //     bitmapConverter both get palFg); sprites and the border read the
    //     RAW palette (palBg, :101);
    //   * fMSX corroborates (Common.h:76 / Wide.h:44:
    //     `XPal[0]=(!BGColor||SolidColor0)? XPal0:XPal[BGColor]`), but
    //     unconditionally -- it doesn't model TP; openMSX's TP-conditioned
    //     model matches the data book and is followed.
    // GRAPHIC7 never substitutes (transparency forced off,
    // SDLRasterizer.cc:349-352); GRAPHIC5 splits the backdrop nibble into
    // even/odd 2-bit halves (SDLRasterizer.cc:364-372, matching
    // BitmapConverter.cc:145-157's palette16[0..3]/[16..19] split).
    [[nodiscard]] bool color0_transparent() const;
    [[nodiscard]] std::uint16_t content_pal16(int index) const;
    [[nodiscard]] std::uint16_t content_pal16_g5(int two_bit_index, bool odd_pixel) const;

    // Vertical scroll (R#23). Non-text/bitmap modes wrap the ENTIRE line
    // index by 256 (PixelRenderer.cc:44-49's `displayY += getVerticalScroll()`,
    // applied before the per-mode converter runs). TEXT modes instead apply
    // R#23 only to the intra-cell pattern row (`l = (line + getVerticalScroll())
    // & 7`, CharacterConverter.cc:149-150/:183), leaving name-table row
    // selection (`line/8`) unscrolled -- reproduced directly in
    // render_text1/render_text2.
    [[nodiscard]] int vertical_scroll_wrap(int line) const;
    // Horizontal scroll (R#26) for character/tile modes: shifts which
    // name-table column feeds output column `col`, wrapping over the 32
    // columns of one name row (CharacterConverter.cc:255-270's `scroll =
    // getHorizontalScrollHigh(); charCode = namePtr[scroll & 0x1F]`). The
    // name-table-page-crossing nuance (`scroll & 0x20` selecting an alternate
    // 0x8000-offset name-table half) is NOT reproduced -- deliberately
    // unimplemented, distinct from the bitmap-mode multi-page-scroll feature
    // this renderer DOES implement (R#25 bit0 + R#2 bit5).
    [[nodiscard]] int scrolled_name_col(int col) const;
    // Horizontal scroll for bitmap modes (R#26 coarse + R#27 fine) -- a
    // DIFFERENT mechanism from the character-mode one above, and
    // internally TWO independent mechanisms with OPPOSITE effect
    // (re-derived from openMSX SDLRasterizer.cc:464-538 + PixelRenderer.cc:
    // 586-604 + VDP.hh:335-370/629-631 -- never copied):
    //   * COARSE (R#26 & 0x1F): rotates the display LEFT in 8-dot steps
    //     (`8 * (lineWidth/256) * (R#26&0x1F)`, 16-dot for 512-wide G5/G6).
    //   * FINE (R#27 & 0x07): shifts the WHOLE displayed image to the RIGHT by
    //     0..7 dots (0..7*2 for 512-wide) and exposes the vacated left edge as
    //     BORDER/backdrop (PixelRenderer.cc:586-594: "the 0..7 extra horizontal
    //     scroll low pixels should be drawn in border color"; displayL =
    //     getLeftBackground() = getLeftSprites() + R#27*4). NOT a circular wrap,
    //     NOT the same sign as coarse. Getting this wrong was the root cause
    //     of an earlier fine-scroll rendering defect.
    // compose_bitmap_scroll() applies both to the already-decoded page
    // buffer(s): `page_first` is the primary page's decoded line, `page_wrap`
    // the page supplying the coarse wrap tail (== `page_first` unless multi-page
    // scroll is active; see bitmap_scroll_pages()).
    [[nodiscard]] int bitmap_coarse_shift(int width) const;
    [[nodiscard]] int bitmap_fine_shift(int width) const;
    // `content_lead` is a fixed, mode-intrinsic RIGHTWARD registration of the
    // decoded page (0 for every plain bitmap mode; 4 for the V9958 YJK/YAE
    // modes -- see render_yjk()/render_yjk_yae() and kYjkDisplayLead). It is
    // ADDED to the R#27 fine shift: the left `fine + content_lead` output dots
    // show BORDER/backdrop and the content source is displaced by the same
    // amount, so the page is registered `content_lead` dots right of the G7
    // base while the R#26/R#27 scroll model is preserved untouched.
    void compose_bitmap_scroll(std::span<std::uint16_t> out, const std::uint16_t* page_first,
                               const std::uint16_t* page_wrap, int width, int content_lead = 0) const;

    // Bitmap-mode page selection (R#2 bits 5-6; VDP.hh getDisplayPage()),
    // multi-page scroll (R#25 bit0 + R#2 bit5; VDP.hh:362-370
    // isMultiPageScrolling(), "wraps to the lower even page"), and the
    // even/odd field page-alternation hedge. NOTE: deliberately
    // narrower than VDP.hh:443-459 getEvenOddMask() -- that formula's
    // "EO-bit-clear implies alternate" term actually concerns per-SCANLINE
    // page splitting for multi-page-scroll smooth-blit (SDLRasterizer.cc:
    // 490-497), a DIFFERENT feature from interlace-FIELD selection; naively
    // substituting Field for it would flip every bitmap page read by default.
    // This renderer only alternates when R#9's EO bit is explicitly SET and
    // blink doesn't suppress it; Field::Odd then selects the alternate page
    // (full reasoning in vdp_frame_renderer.cpp). `page_mask` is 0x03 for
    // G4/G5 (2-bit page, 4 x 0x8000 = 128 KB) or 0x01 for G6/G7/YJK (1-bit
    // page, 2 x 0x10000 = 128 KB) -- re-derived from VRAM capacity, matching
    // SCREEN5/6's 4 pages vs SCREEN7/8's 2.
    [[nodiscard]] bool multi_page_scrolling() const;
    [[nodiscard]] bool use_alternate_page(Field field) const;
    [[nodiscard]] std::uint32_t resolve_bitmap_page(std::uint32_t page_mask, Field field) const;
    // The {first, wrap} page pair fed to compose_bitmap_scroll(): both equal
    // the resolved display page when multi-page scroll is inactive, else the
    // even/odd page pair ordered by R#26 bit5 (SDLRasterizer.cc:479-538).
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> bitmap_scroll_pages(std::uint32_t page_mask,
                                                                             Field field) const;

    // Per-mode content renderers (CharacterConverter.{hh,cc} /
    // BitmapConverter.{hh,cc} two-family precedent, independently
    // re-expressed -- never copied). Each bitmap renderer decodes one page's
    // scanline into a temp buffer via the matching decode_*_row() helper (so a
    // second, wrap page can be decoded when multi-page scroll is active) and
    // then applies compose_bitmap_scroll().
    void render_text1(int line, std::span<std::uint16_t> out) const;
    void render_text2(int line, std::span<std::uint16_t> out) const;
    void render_graphic1(int line, std::span<std::uint16_t> out) const;
    void render_graphic2_or_3(int line, std::span<std::uint16_t> out) const;
    void render_multicolor(int line, std::span<std::uint16_t> out) const;
    void decode_graphic4_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_graphic4(int line, std::span<std::uint16_t> out) const;
    void decode_graphic5_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_graphic5(int line, std::span<std::uint16_t> out) const;
    void decode_graphic6_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_graphic6(int line, Field field, std::span<std::uint16_t> out) const;
    void decode_graphic7_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_graphic7(int line, Field field, std::span<std::uint16_t> out) const;
    void decode_yjk_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_yjk(int line, Field field, std::span<std::uint16_t> out) const;
    void decode_yjk_yae_row(std::uint32_t row_base, std::span<std::uint16_t> temp) const;
    void render_yjk_yae(int line, Field field, std::span<std::uint16_t> out) const;
    // TEXT1Q, MULTIQ, and any undefined mode byte render flat blank (palette
    // entry 15), NEVER TMS9918-compatible content -- the HB-F1XV's V9958 is
    // never isMSX1VDP() (re-derived from CharacterConverter.cc:
    // 64-84's isMSX1VDP() branch and renderBlank(), :368-373).
    void render_blank(std::span<std::uint16_t> out) const;

    const V9958Vdp* vdp_;
};

}  // namespace sony_msx::devices::video
