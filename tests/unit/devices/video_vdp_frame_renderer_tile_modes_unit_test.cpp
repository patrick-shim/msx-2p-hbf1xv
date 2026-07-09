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

// Suite: Devices_VdpFrameRendererTileModes_Unit
//
// Deterministic unit coverage for M21-S2's remaining CharacterConverter-family
// modes: GRAPHIC2/GRAPHIC3 (quarter-addressed pattern/color tables),
// MULTICOLOR (the REAL 256x192 pixel canvas with 4x4-pixel color-cell
// granularity, A-M21-9 -- NOT a literal 64x48-pixel image), and the
// TEXT1Q/MULTIQ/Unknown flat-blank fallback (A-M21-6). Grounded in
// references/openmsx-21.0/src/video/CharacterConverter.cc:272-350 -- never
// copied (GPL isolation).

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
    // --- GRAPHIC2 (base 0x04): quarter-addressed pattern/color tables
    //     (CharacterConverter.cc:272-297), one 2048-byte quarter per 64
    //     raster lines. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x02);  // M3 -> GRAPHIC2 (base 0x04)
        set_register(vdp, 2, 0x00);  // name table base 0
        set_register(vdp, 4, 0x02);  // pattern table base 0x1000 (2<<11)
        // Color table base 0x2800 (past pattern_base's 3 quarters,
        // 0x1000-0x27FF) -- kept within the 14-bit CPU-port pointer range
        // (< 0x4000) so write_vram's plain address-setup helper (which does
        // NOT touch R#14) lands correctly without needing an R#14 page
        // select.
        set_register(vdp, 10, 0x00);
        set_register(vdp, 3, 0xA0);  // (0xA0<<6) = 0x2800
        set_palette(vdp, 5, 5, 0, 0);
        set_palette(vdp, 9, 0, 0, 9 & 0x7);

        // Same char code (5) at name row-group 0 (line 0-7) and name row-group
        // 8 (line 64-71); DIFFERENT pattern/color bytes per quarter prove the
        // quarter offset is genuinely applied.
        write_vram(vdp, 0, 0x05);                              // line0..7, col0: char 5
        write_vram(vdp, 8 * 32, 0x05);                         // line64..71, col0: char 5
        write_vram(vdp, 0x1000 + 5 * 8 + 0, 0xF0);              // quarter0 pattern
        write_vram(vdp, 0x1000 + 2048 + 5 * 8 + 0, 0x0F);       // quarter1 pattern (DIFFERENT)
        write_vram(vdp, 0x2800 + 5 * 8 + 0, 0x59);              // quarter0 color: fg=5,bg=9
        write_vram(vdp, 0x2800 + 2048 + 5 * 8 + 0, 0x59);       // quarter1 color: same fg/bg

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row0[256];
        std::uint16_t row64[256];
        renderer.render_line(0, Field::Progressive, row0);
        renderer.render_line(64, Field::Progressive, row64);

        const std::uint16_t fg = pack_rgb555(expand3to5(5), expand3to5(0), expand3to5(0));
        const std::uint16_t bg = pack_rgb555(expand3to5(0), expand3to5(0), expand3to5(1));  // 9&7=1

        // Quarter0 pattern 0xF0 = 1111_0000 -> first 4 bits fg, next 4 bg.
        expect(row0[0] == fg && row0[3] == fg && row0[4] == bg && row0[7] == bg,
               "Graphic2_Quarter0_PatternMatches");
        // Quarter1 pattern 0x0F = 0000_1111 -> the OPPOSITE arrangement,
        // proving the 2048-byte-per-64-lines quarter offset is applied.
        expect(row64[0] == bg && row64[3] == bg && row64[4] == fg && row64[7] == fg,
               "Graphic2_Quarter1_UsesDifferentPatternByte");
    }

    // --- GRAPHIC3 (base 0x08) reuses the SAME renderer as GRAPHIC2. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x04);  // M4 -> GRAPHIC3 (base 0x08)
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256, "Graphic3_Width_Is256");
    }

    // --- MULTICOLOR: the real pixel canvas is 256x192 (A-M21-9), NOT
    //     literally 64x48. Each 4x4-pixel color-cell group holds ONE of two
    //     possible colors (cl for the left half, cr for the right half),
    //     matching CharacterConverter.cc:299-325. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 1, 0x48);  // M2 -> MULTICOLOR (base 0x02)
        set_register(vdp, 2, 0x00);  // name table base
        set_register(vdp, 4, 0x02);  // pattern table base 0x1000
        set_palette(vdp, 9, 1, 2, 3);
        set_palette(vdp, 12, 4, 5, 6);

        write_vram(vdp, 0, 0x03);                    // name index0 (row-group0): char 3
        write_vram(vdp, 0x1000 + 3 * 8 + 0, 0x9C);    // pattern_sub=0 (lines 0-3): cl=9,cr=12
        write_vram(vdp, 0x1000 + 3 * 8 + 1, 0xC9);    // pattern_sub=1 (lines 4-7): SWAPPED

        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256, "Multicolor_RealCanvas_Is256Wide_NotSixtyFour");

        std::uint16_t row0[256];
        std::uint16_t row4[256];
        renderer.render_line(0, Field::Progressive, row0);
        renderer.render_line(4, Field::Progressive, row4);

        const std::uint16_t c9 = pack_rgb555(expand3to5(1), expand3to5(2), expand3to5(3));
        const std::uint16_t c12 = pack_rgb555(expand3to5(4), expand3to5(5), expand3to5(6));

        // Lines 0-3 of the 4x4 cell: left 4 pixels = palette 9, right 4 = 12.
        bool group0_ok = true;
        for (int i = 0; i < 4; ++i) {
            if (row0[i] != c9) group0_ok = false;
        }
        for (int i = 4; i < 8; ++i) {
            if (row0[i] != c12) group0_ok = false;
        }
        expect(group0_ok, "Multicolor_Lines0to3_LeftRightColorPair");

        // Lines 4-7 use the OTHER pattern byte (swapped colors), proving the
        // 4-line (not 8-line) sub-block granularity.
        bool group4_ok = true;
        for (int i = 0; i < 4; ++i) {
            if (row4[i] != c12) group4_ok = false;
        }
        for (int i = 4; i < 8; ++i) {
            if (row4[i] != c9) group4_ok = false;
        }
        expect(group4_ok, "Multicolor_Lines4to7_UsesDifferentSubBlock");
    }

    // --- TEXT1Q/MULTIQ/Unknown: flat blank fill (palette 15), never
    //     TMS9918-compatible striped content (A-M21-6). ---
    {
        // TEXT1Q: base 0x05 (M1 + M3).
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x02);  // M3
        set_register(vdp, 1, 0x50);  // M1
        set_palette(vdp, 15, 6, 6, 6);
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256 && renderer.height() == 192, "Text1Q_Dimensions_256x192");
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t blank = pack_rgb555(expand3to5(6), expand3to5(6), expand3to5(6));
        bool all_blank = true;
        for (int i = 0; i < 256; ++i) {
            if (row[i] != blank) all_blank = false;
        }
        expect(all_blank, "Text1Q_RendersFlatBlank_Palette15");
    }
    {
        // MULTIQ: base 0x06 (M2 + M3).
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x02);  // M3
        set_register(vdp, 1, 0x48);  // M2
        set_palette(vdp, 15, 3, 2, 1);
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t blank = pack_rgb555(expand3to5(3), expand3to5(2), expand3to5(1));
        bool all_blank = true;
        for (int i = 0; i < 256; ++i) {
            if (row[i] != blank) all_blank = false;
        }
        expect(all_blank, "MultiQ_RendersFlatBlank_Palette15");
    }
    {
        // Unknown: base 0x03 (M1 + M2, no M3/M4/M5) -- not in the decoded set.
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 1, 0x58);  // M1 + M2
        set_palette(vdp, 15, 1, 1, 1);
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t blank = pack_rgb555(expand3to5(1), expand3to5(1), expand3to5(1));
        bool all_blank = true;
        for (int i = 0; i < 256; ++i) {
            if (row[i] != blank) all_blank = false;
        }
        expect(all_blank, "Unknown_RendersFlatBlank_Palette15");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
