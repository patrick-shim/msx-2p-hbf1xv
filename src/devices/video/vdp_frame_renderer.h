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

#include "devices/video/frame_buffer.h"
#include "devices/video/v9958_vdp.h"

namespace sony_msx::devices::video {

// Display-path row-pointer resolution for the G6/G7 planar VRAM interleave
// (M21-S4, backlog D7 display-path piece; A-M21-11). For a display row's
// LOGICAL byte range [row_base, row_base+length) -- row_base is always even
// for every G6/G7-family display row (256 logical bytes/row, row_base =
// page*0x10000 + line*256) -- the 17-bit rotate-right-by-1 (A-M21-10) used by
// the CPU port degenerates to two CONTIGUOUS half-length spans in the flat
// physical VRAM store: even logical bytes land at `bank0_base ..
// bank0_base+half_length`, odd logical bytes at `bank1_base ..
// bank1_base+half_length` (bank1_base == bank0_base + 0x10000).
struct PlanarRowSpans {
    std::uint32_t bank0_base;
    std::uint32_t bank1_base;
    std::size_t half_length;
};

[[nodiscard]] PlanarRowSpans planar_row_spans(std::uint32_t row_base, std::size_t length);

// Deterministic, pull-model, frozen-register-snapshot VDP pixel renderer
// (M21, backlog D1/D5/D6, D7 display-path piece). A pure function of a
// V9958Vdp's stored VRAM/register state at call time: no elapsed_cycles()
// dependency, no new clock consumer (mirrors the M17/M19/M20 "no new clock
// consumer" precedent for combinational devices). Carries no slot/bus
// knowledge (matches V9958Vdp itself, src/CLAUDE.md device-family
// placement). Every behavior cites its concrete
// references/openmsx-21.0/src/video/*.{hh,cc} grounding in
// vdp_frame_renderer.cpp; never copied verbatim (GPL isolation).
class VdpFrameRenderer {
public:
    explicit VdpFrameRenderer(const V9958Vdp& vdp);

    // Active-display pixel width/height for the CURRENT mode (planner
    // package §2.2's independently-derived dimension table).
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;

    // Render one active-display scanline (0-based within [0, height())) into
    // `out` (must hold at least width() entries).
    void render_line(int line, Field field, std::span<std::uint16_t> out) const;

    // Render a full frame (frozen snapshot) in one call.
    [[nodiscard]] FrameBuffer render_frame(Field field = Field::Progressive) const;

    // Mode-aware backdrop/border color (VDP.hh:211-226 getBackgroundColor;
    // A-M21-4 GRAPHIC7 fixed-color decode; the GRAPHIC5 even-pixel border
    // half, SDLRasterizer.cc:376-393 -- FrameBuffer.border_color is a single
    // value, so the even-pixel half is used for GRAPHIC5, a documented
    // simplification since real hardware alternates two border colors).
    [[nodiscard]] std::uint16_t border_color() const;

private:
    void dispatch_content(int line, Field field, std::span<std::uint16_t> out) const;

    // Sprite pixel compositing (M22-S2, backlog D2), folded directly into
    // the existing per-line pipeline: called from render_line() immediately
    // after dispatch_content() populates the background row and BEFORE the
    // existing border-mask step (mirrors the reference's own "background
    // then sprite overdraw" order, planner package §1.4 Resolution 1).
    // Queries vdp_->sprite_engine().visible_sprites(line) -- a read-only
    // accessor onto state V9958Vdp::on_vsync() already computed -- so this
    // remains a PURE, read-only consumer (no mutation of V9958Vdp state).
    void composite_sprites(int line, Field field, std::span<std::uint16_t> out) const;

    // Table-base register formulas (VDP.hh:246-262, independently
    // re-derived): name/pattern/color table bases. Note: the "forced-1 low
    // bits" hardware mirroring-mask nuance openMSX's VRAMWindow machinery
    // models is NOT reproduced (an out-of-scope depth item, §ok per
    // docs/m21-implementation-report.md); tests use canonical/valid base
    // register values, matching this project's existing simplification
    // level for VRAM addressing.
    [[nodiscard]] std::uint32_t name_table_base() const;
    [[nodiscard]] std::uint32_t pattern_table_base() const;
    [[nodiscard]] std::uint32_t color_table_base() const;

    [[nodiscard]] std::uint8_t vram_read(std::uint32_t addr) const;
    [[nodiscard]] std::uint16_t pal16(int index) const;

    // Color-0 backdrop substitution for CONTENT pixels (the Konami-splash
    // fix). Real V99x8 behavior: when R#8's TP bit is CLEAR, color 0 is
    // TRANSPARENT -- a color-0 content pixel displays the BACKDROP color
    // (R#7 low nibble through the palette), not palette entry 0. Grounded:
    //   * fact-sheet: references/fact-sheets/Yamaha V9958 VDP.md:72 ("TP
    //     colour0 transparent");
    //   * openMSX: VDP.hh:189-191 getTransparency() == (R#8 & 0x20) == 0;
    //     SDLRasterizer.cc:346-373 precalcColorIndex0() -- palFg[0] =
    //     palBg[tpIndex] with tpIndex = transparency ? bgColor : 0; the
    //     palFg table feeds EVERY content path (SDLRasterizer.cc:99-100:
    //     characterConverter gets subspan<16>(palFg), bitmapConverter gets
    //     palFg), while sprites (palBg, :101) and the border (palBg) read
    //     the RAW palette;
    //   * fMSX independently corroborates (Common.h:76 / Wide.h:44:
    //     `XPal[0]=(!BGColor||SolidColor0)? XPal0:XPal[BGColor]`), though
    //     unconditionally (it does not model the TP bit; openMSX's
    //     TP-conditioned model matches the data book and is followed).
    // GRAPHIC7 never substitutes (transparency forced off,
    // SDLRasterizer.cc:349-352); GRAPHIC5 splits the backdrop nibble into
    // even/odd 2-bit halves (SDLRasterizer.cc:364-372, palFg[0]=
    // palBg[tpIndex>>2] even / palFg[16]=palBg[tpIndex&3] odd, matching
    // BitmapConverter.cc:145-157's palette16[0..3]/palette16[16..19] split).
    [[nodiscard]] bool color0_transparent() const;
    [[nodiscard]] std::uint16_t content_pal16(int index) const;
    [[nodiscard]] std::uint16_t content_pal16_g5(int two_bit_index, bool odd_pixel) const;

    // Vertical scroll (R#23). Non-text/bitmap modes wrap the ENTIRE line
    // index by 256 (PixelRenderer.cc:44-49's `displayY += getVerticalScroll()`
    // applied before the per-mode converter is invoked); TEXT modes instead
    // apply R#23 only to the intra-cell pattern row (CharacterConverter's own
    // `l = (line + getVerticalScroll()) & 7`, CharacterConverter.cc:149-150 /
    // :183), leaving the name-table row selection (`line/8`) unscrolled --
    // reproduced faithfully inside render_text1/render_text2 directly.
    [[nodiscard]] int vertical_scroll_wrap(int line) const;
    // Horizontal scroll (R#26) for character/tile modes: shifts which
    // name-table column feeds output column `col`, wrapping over the 32
    // columns of one name row (CharacterConverter.cc:255-270's `scroll =
    // getHorizontalScrollHigh(); charCode = namePtr[scroll & 0x1F]`). The
    // multi-page name-table-page-crossing nuance (`scroll & 0x20` selecting
    // an alternate 0x8000-offset name-table half) is NOT reproduced -- an
    // out-of-scope depth item distinct from the bitmap-mode multi-page-
    // scroll feature this milestone DOES implement (R#25 bit0 + R#2 bit5).
    [[nodiscard]] int scrolled_name_col(int col) const;
    // Horizontal scroll for bitmap modes (R#26 coarse + R#27 fine), a
    // DIFFERENT mechanism from the character-mode one above (A-M21-8),
    // independently grounded at SDLRasterizer.cc:465-471 (coarse: `8 *
    // (lineWidth/256) * (R26&0x1F)`, doubled to 16-dot units for 512-wide
    // modes per the fact-sheet) and PixelRenderer.cc:60-69 (fine: R#27,
    // similarly doubled for 512-wide modes). Returns a pixel shift in
    // [0, width); the full split-page-blit windowing mechanics
    // (SDLRasterizer.cc:477-538) are NOT reproduced -- only the documented
    // register-driven shift formula is (an honest, disclosed depth limit).
    [[nodiscard]] int bitmap_horizontal_shift(int width) const;
    void apply_bitmap_scroll(std::span<std::uint16_t> out, const std::uint16_t* unshifted, int width) const;

    // Bitmap-mode page selection (R#2 bits 5-6; VDP.hh getDisplayPage()),
    // multi-page scroll (R#25 bit0 + R#2 bit5; VDP.hh:362-370
    // isMultiPageScrolling(), "wraps to the lower even page"), and the
    // even/odd field page-alternation hedge (A-M21-7). NOTE: this is a
    // deliberately narrower, independently-CHOSEN model, not a bit-for-bit
    // reproduction of VDP.hh:443-459 getEvenOddMask() -- that formula's
    // "EO-bit-clear implies default-alternate" term turns out (per its only
    // call site, SDLRasterizer.cc:490-497) to concern per-SCANLINE-parity
    // page splitting for the multi-page-scroll smooth-blit effect, a
    // DIFFERENT feature from interlace-FIELD selection; naively substituting
    // Field for it would make the common EO-disabled default silently flip
    // every bitmap page read regardless of which Field was requested. This
    // renderer instead only alternates when R#9's EO bit is explicitly SET
    // and blink does not suppress it; Field::Odd then selects the alternate
    // page (see vdp_frame_renderer.cpp for the full reasoning). `page_mask`
    // is 0x03 for G4/G5 (2-bit page, 4 x 0x8000 = 128 KB) or 0x01 for
    // G6/G7/YJK (1-bit page, 2 x 0x10000 = 128 KB) -- independently
    // re-derived from VRAM capacity arithmetic, matching the well-known MSX2
    // fact that SCREEN5/6 support 4 pages and SCREEN7/8 support only 2.
    [[nodiscard]] bool multi_page_scrolling() const;
    [[nodiscard]] bool use_alternate_page(Field field) const;
    [[nodiscard]] std::uint32_t resolve_bitmap_page(std::uint32_t page_mask, Field field) const;
    [[nodiscard]] std::uint32_t bitmap_row_base_nonplanar(int line, Field field) const;
    [[nodiscard]] std::uint32_t bitmap_row_base_planar(int line, Field field) const;

    // Per-mode content renderers (CharacterConverter.{hh,cc} /
    // BitmapConverter.{hh,cc} two-family precedent, independently
    // re-expressed -- never copied).
    void render_text1(int line, std::span<std::uint16_t> out) const;
    void render_text2(int line, std::span<std::uint16_t> out) const;
    void render_graphic1(int line, std::span<std::uint16_t> out) const;
    void render_graphic2_or_3(int line, std::span<std::uint16_t> out) const;
    void render_multicolor(int line, std::span<std::uint16_t> out) const;
    void render_graphic4(int line, std::span<std::uint16_t> out) const;
    void render_graphic5(int line, std::span<std::uint16_t> out) const;
    void render_graphic6(int line, Field field, std::span<std::uint16_t> out) const;
    void render_graphic7(int line, Field field, std::span<std::uint16_t> out) const;
    void render_yjk(int line, Field field, std::span<std::uint16_t> out) const;
    void render_yjk_yae(int line, Field field, std::span<std::uint16_t> out) const;
    // TEXT1Q, MULTIQ, and any undefined mode-byte combination render flat
    // blank (palette entry 15), NEVER TMS9918-compatible content -- the
    // HB-F1XV's V9958 is never isMSX1VDP() (A-M21-6, independently re-
    // derived from CharacterConverter.cc:64-84's isMSX1VDP() branch and
    // renderBlank(), CharacterConverter.cc:368-373).
    void render_blank(std::span<std::uint16_t> out) const;

    const V9958Vdp* vdp_;
};

}  // namespace sony_msx::devices::video
