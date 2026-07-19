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

// Suite: Devices_VdpFrameRendererEffects_Unit
//
// Deterministic unit coverage for M21-S6's effects layer (backlog D6):
// vertical scroll (R#23) for a character mode AND a bitmap mode, horizontal
// scroll (R#26 for character modes, R#26/R#27 for bitmap modes -- A-M21-8, a
// DIFFERENT mechanism), border mask (R#25 bit1), multi-page scroll (R#25
// bit0 + R#2 bit5), and the openMSX-parity-hedged Field-based even/odd
// page-alternation (A-M21-7). Grounded in
// references/openmsx-21.0/src/video/PixelRenderer.cc:44-69,
// CharacterConverter.cc:255-270, SDLRasterizer.cc:465-471,
// VDP.hh:353-370/443-459 -- never copied (GPL isolation). Superimpose/
// digitize is explicitly, correctly OUT of scope for this machine (no
// digitizer hardware, fact-sheet §9) -- no test needed, no production code
// beyond the citation already present in vdp_frame_renderer.h/.cpp.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_palette.h"

namespace {

using sony_msx::devices::video::expand3to5;
using sony_msx::devices::video::Field;
using sony_msx::devices::video::pack_rgb555;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpFrameRenderer;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

void set_write_address(V9958Vdp& vdp, const std::uint16_t addr) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
}

void write_vram(V9958Vdp& vdp, const std::uint16_t addr, const std::uint8_t value) {
    set_write_address(vdp, addr);
    vdp.io_write(0x98, value);
}

void set_palette(V9958Vdp& vdp, const int index, const std::uint8_t r3, const std::uint8_t g3,
                  const std::uint8_t b3) {
    set_register(vdp, 16, static_cast<std::uint8_t>(index & 0x0F));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

}  // namespace

int main() {
    // --- Vertical scroll, character mode (GRAPHIC1): R#23=8 shifts the WHOLE
    //     line index by one full name-row (PixelRenderer.cc:44-49), so
    //     render_line(0) with scroll=8 shows what render_line(8) would show
    //     unscrolled. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        // GRAPHIC1 is the reset/default mode.
        set_palette(vdp, 5, 5, 1, 1);
        set_palette(vdp, 9, 1, 1, 5);
        write_vram(vdp, 32, 0x07);      // name row1 (addr 32 = 1*32), col0: char 7
        write_vram(vdp, 7 * 8 + 0, 0xFF);  // all-set pattern
        write_vram(vdp, 7 / 8, 0x59);       // color: fg=5,bg=9
        set_register(vdp, 23, 8);           // vertical scroll by 8 lines

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t fg = pack_rgb555(expand3to5(5), expand3to5(1), expand3to5(1));
        expect(row[0] == fg, "VerticalScroll_CharacterMode_Graphic1_ShiftsWholeLineIndex");
    }

    // --- Horizontal scroll, character mode (GRAPHIC1): R#26=1 shifts the
    //     name-table COLUMN read for output column 0 to column 1
    //     (CharacterConverter.cc:255-270). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 4, 0x01);   // pattern table base 0x0800 (avoid aliasing name/color)
        set_register(vdp, 3, 0x40);   // color table base 0x1000
        set_palette(vdp, 6, 2, 2, 2);
        write_vram(vdp, 1, 0x0A);                     // name col1: char 10
        write_vram(vdp, 0x0800 + 10 * 8 + 0, 0xFF);
        write_vram(vdp, 0x1000 + 10 / 8, 0x66);        // fg=bg=6
        set_register(vdp, 26, 1);                      // horizontal scroll high = 1

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t c6 = pack_rgb555(expand3to5(2), expand3to5(2), expand3to5(2));
        expect(row[0] == c6, "HorizontalScroll_CharacterMode_ShiftsNameColumnByR26");
    }

    // --- Vertical scroll, bitmap mode (GRAPHIC4): R#23=1 shifts the VRAM row
    //     read by one full 128-byte row. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_palette(vdp, 0xC, 3, 3, 3);
        set_palette(vdp, 0xD, 4, 4, 4);
        write_vram(vdp, 128, 0xCD);  // row1 byte0
        set_register(vdp, 23, 1);

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t pc = pack_rgb555(expand3to5(3), expand3to5(3), expand3to5(3));
        expect(row[0] == pc, "VerticalScroll_BitmapMode_Graphic4_ShiftsVramRow");
    }

    // --- Horizontal scroll, bitmap mode (GRAPHIC4, 256-wide): R#26 coarse
    //     (8px/step) -- a DIFFERENT mechanism from the character-mode one
    //     above (A-M21-8), independently grounded at
    //     SDLRasterizer.cc:465-471. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_palette(vdp, 0xC, 6, 0, 0);
        write_vram(vdp, 4, 0xC0);    // byte4 -> pixel8 = palette 0xC
        set_register(vdp, 26, 1);   // coarse scroll: 1 step * 8px = 8px shift

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t pc = pack_rgb555(expand3to5(6), expand3to5(0), expand3to5(0));
        expect(row[0] == pc, "HorizontalScroll_BitmapMode_CoarseR26_ShiftsByEightPixels");
    }
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);
        set_palette(vdp, 0xF, 0, 6, 0);
        write_vram(vdp, 1, 0x0F);   // byte1 low nibble -> pixel3 = palette 0xF
        set_register(vdp, 27, 3);  // fine scroll: 3px to the RIGHT

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t pf = pack_rgb555(expand3to5(0), expand3to5(6), expand3to5(0));
        // M38 Phase B RE-DERIVATION. The OLD oracle asserted row[0]==pf, i.e. a
        // circular LEFT wrap by 3 -- exactly the Phase-A-confirmed bug. Correct
        // V9958 behavior: R#27 shifts the displayed image to the RIGHT by 3 dots
        // and draws the vacated left edge in BORDER/backdrop color; it is NOT a
        // wrap and NOT the same sign as coarse R#26. So the green content pixel
        // originally at x=3 now appears at x=6, and x=0..2 show the border.
        // Re-derived from openMSX SDLRasterizer.cc:464-465 (leftBackground =
        // translateX(getLeftBackground())), VDP.hh:629-631 (getLeftBackground =
        // getLeftSprites + R#27*4), PixelRenderer.cc:587-594 ("the 0..7 extra
        // horizontal scroll low pixels should be drawn in border color"). A/B
        // confirmed: tools/capture/scroll-scenarios-run.ps1 s03_g4_fine flips MISMATCH -> MATCH.
        expect(row[6] == pf, "HorizontalScroll_BitmapMode_FineR27_ShiftsContentRightByThree");
        expect(row[0] == renderer.border_color(),
               "HorizontalScroll_BitmapMode_FineR27_ExposesLeftBorderStrip");
    }

    // --- Border mask (R#25 bit1): leftmost 8 active-display pixels show the
    //     border color; pixel 8 onward is unaffected content (VDP.hh:353-360). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_register(vdp, 7, 0x05);  // backdrop index 5
        set_palette(vdp, 5, 7, 0, 0);
        set_palette(vdp, 9, 0, 7, 0);
        write_vram(vdp, 4, 0x99);    // byte4 -> pixels 8,9 = palette 9 (content)
        set_register(vdp, 25, 0x02);  // MSK

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t border = pack_rgb555(expand3to5(7), expand3to5(0), expand3to5(0));
        const std::uint16_t content = pack_rgb555(expand3to5(0), expand3to5(7), expand3to5(0));
        bool border_ok = true;
        for (int x = 0; x < 8; ++x) {
            if (row[x] != border) border_ok = false;
        }
        expect(border_ok, "BorderMask_LeftmostEightPixels_ShowBorderColor");
        expect(row[8] == content, "BorderMask_PixelEight_UnaffectedContent");
    }

    // --- Multi-page scroll (R#25 bit0 + R#2 bit5): wraps DOWN to the lower
    //     even page, even though R#2 selects the odd page (VDP.hh:362-370). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_palette(vdp, 1, 1, 0, 0);
        set_palette(vdp, 2, 0, 1, 0);
        write_vram(vdp, 0, 0x11);        // page0 content
        set_register(vdp, 2, 0x20);      // select page1 (bit5 set -> odd page)
        // Address 0x8000 needs R#14=2 (A16..A14) -- exceeds the CPU port's
        // 14-bit pointer alone (write_vram only sets the 14-bit pointer).
        set_register(vdp, 14, 0x02);
        write_vram(vdp, 0x0000, 0x22);   // (R#14<<14)|0 = 0x8000; page1 content

        // Baseline (multi-page-scroll OFF): page1 (as configured) is shown.
        {
            const VdpFrameRenderer renderer(vdp);
            std::uint16_t row[256];
            renderer.render_line(0, Field::Progressive, row);
            const std::uint16_t pal2 = pack_rgb555(expand3to5(0), expand3to5(1), expand3to5(0));
            expect(row[0] == pal2, "MultiPageScroll_Off_ShowsConfiguredOddPage");
        }

        // Enable multi-page scroll: wraps to the LOWER EVEN page (page0),
        // even though R#2 still selects page1.
        set_register(vdp, 25, 0x01);
        {
            const VdpFrameRenderer renderer(vdp);
            std::uint16_t row[256];
            renderer.render_line(0, Field::Progressive, row);
            const std::uint16_t pal1 = pack_rgb555(expand3to5(1), expand3to5(0), expand3to5(0));
            expect(row[0] == pal1, "MultiPageScroll_On_WrapsToLowerEvenPage");
        }

        // M38 Phase B (new coverage): with a coarse scroll active, the wrapped-in
        // RIGHT region reads the ADJACENT odd page (page1), NOT a self-wrap of
        // the even page0. This is the exact behavior the Phase-A harness caught
        // as the 25% s06_g4_multipage divergence (SDLRasterizer.cc:530-537: the
        // tail is copied from vramLine[scrollPage2], the p1^1 page). R#26=1 ->
        // 8px coarse, so the rightmost 8 output columns (x>=248) come from page1
        // (byte0=0x22 -> pixel0 = palette2). The pre-M38 renderer forced a single
        // (even) page and self-wrapped, giving palette1 here instead.
        set_register(vdp, 26, 0x01);
        {
            const VdpFrameRenderer renderer(vdp);
            std::uint16_t row[256];
            renderer.render_line(0, Field::Progressive, row);
            const std::uint16_t pal2 = pack_rgb555(expand3to5(0), expand3to5(1), expand3to5(0));
            expect(row[248] == pal2, "MultiPageScroll_On_CoarseWrapReadsAdjacentOddPage");
        }
    }

    // --- Field-based even/odd page-alternation (A-M21-7, hedged): with R#9
    //     EO bit set, Field::Odd selects the alternate (page^1) content;
    //     Field::Even/Progressive select the configured page. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0A);  // GRAPHIC6 (planar; 1-bit page selector)
        set_palette(vdp, 1, 1, 0, 0);
        set_palette(vdp, 2, 0, 1, 0);

        // page0 content: logical row base 0 (page0*0x10000+line0*256=0) ->
        // CPU-port logical addresses 0/1 land at the SAME physical bytes the
        // display path's planar_row_spans(0, ...) reads.
        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0x10);  // bank0[0]=0x10 -> pixel0=palette1
        vdp.io_write(0x98, 0x00);

        // page1 content: the display path's row base for page1/line0 is
        // page(1)*0x10000 + 0 = 0x10000 (a LOGICAL address, per
        // bitmap_row_base_planar/planar_row_spans -- it is right-shifted by
        // ONE MORE bit to get the physical bank offset, exactly mirroring
        // the CPU port's own planar transform). Reach the SAME logical
        // address 0x10000 via the CPU port by setting R#14=4 (bits16-14 of
        // the 17-bit logical address, `(R#14<<14)|pointer` = (4<<14)|0 =
        // 0x10000), so both paths apply the IDENTICAL rotate-right-by-1
        // transform to the IDENTICAL logical address (A-M21-10/A-M21-11's
        // own cross-path consistency argument).
        set_register(vdp, 14, 0x04);
        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0x20);  // logical 0x10000 -> pixel0=palette2
        vdp.io_write(0x98, 0x00);  // logical 0x10001

        set_register(vdp, 9, 0x04);  // EO bit set

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row_even[512];
        std::uint16_t row_odd[512];
        renderer.render_line(0, Field::Even, row_even);
        renderer.render_line(0, Field::Odd, row_odd);
        const std::uint16_t pal1 = pack_rgb555(expand3to5(1), expand3to5(0), expand3to5(0));
        const std::uint16_t pal2 = pack_rgb555(expand3to5(0), expand3to5(1), expand3to5(0));
        expect(row_even[0] == pal1, "FieldEo_Even_ShowsConfiguredPage");
        expect(row_odd[0] == pal2, "FieldEo_Odd_ShowsAlternatePage");

        // Blink state ON suppresses alternation entirely (A-M21-7): forcing
        // blink_state_ true via R#13 makes Field::Odd behave like Even.
        set_register(vdp, 13, 0x11);  // ON=1,OFF=1 -> blink_state_ = true
        expect(vdp.blink_state(), "FieldEo_BlinkForcedOn");
        std::uint16_t row_odd_suppressed[512];
        renderer.render_line(0, Field::Odd, row_odd_suppressed);
        expect(row_odd_suppressed[0] == pal1, "FieldEo_BlinkOn_SuppressesAlternation_OddShowsConfiguredPage");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x06);
            set_register(*vdp, 23, 3);
            set_register(*vdp, 26, 2);
            write_vram(*vdp, 5, 0x21);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels, "Effects_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
