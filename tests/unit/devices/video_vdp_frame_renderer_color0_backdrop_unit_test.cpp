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

// Suite: Devices_VdpFrameRendererColor0Backdrop_Unit
//
// Regression guard for the Konami-splash defect (targeted defect cycle,
// docs/konami-splash-regression-investigation.md): content pixels with color
// index 0 must display the BACKDROP color (R#7 low nibble through the
// palette) when R#8's TP bit is CLEAR (color-0 transparency ON -- the
// power-on default), and palette entry 0 itself when TP is SET. Before the
// fix the renderer always resolved content color 0 as palette entry 0, so
// the stealth-action title's Konami splash (SCREEN 5, background = color 0, R#7 = 0x0F,
// R#8 = 0x08) rendered a BLACK background where real hardware -- verified
// against live openMSX 19.1 Sony_HB-F1XV screenshots -- shows WHITE.
//
// Grounding (read, never copied -- GPL isolation):
//   * references/fact-sheets/Yamaha V9958 VDP.md:72 "TP colour0 transparent"
//   * references/openmsx-21.0/src/video/VDP.hh:189-191 getTransparency()
//     == (R#8 & 0x20) == 0
//   * references/openmsx-21.0/src/video/SDLRasterizer.cc:346-373
//     precalcColorIndex0(): palFg[0] = palBg[tpIndex], tpIndex =
//     transparency ? bgColor : 0; GRAPHIC7 forces transparency off
//     (:349-352); GRAPHIC5 splits the backdrop nibble into even/odd 2-bit
//     halves (:364-372)
//   * fMSX cross-reference references/fmsx-60/source/fMSX/Common.h:76:
//     XPal[0]=(!BGColor||SolidColor0)? XPal0:XPal[BGColor]
//     (same substitution, unconditional -- fMSX does not model TP; the
//     TP-conditioned openMSX model matches the data book and is followed).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_palette.h"

namespace {

using sony_msx::devices::video::expand3to5;
using sony_msx::devices::video::Field;
using sony_msx::devices::video::graphic7_fixed_color_to_rgb555;
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

std::uint16_t rgb(const int r3, const int g3, const int b3) {
    return pack_rgb555(expand3to5(static_cast<std::uint8_t>(r3)), expand3to5(static_cast<std::uint8_t>(g3)),
                       expand3to5(static_cast<std::uint8_t>(b3)));
}

}  // namespace

int main() {
    const std::uint16_t white = rgb(7, 7, 7);

    // --- 1. The exact Konami-splash shape: GRAPHIC4 (SCREEN 5), background
    //     pixels color 0, R#7 = 0x0F (backdrop = palette 15 = white),
    //     R#8 = 0x08 (TP bit CLEAR -> transparency ON). Color-0 content
    //     must render as the WHITE backdrop, and match border_color()
    //     exactly (the human-visible symptom was white border around black
    //     content). FAILS pre-fix (content came out palette-0 black). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_register(vdp, 7, 0x0F);  // backdrop color 15
        set_register(vdp, 8, 0x08);  // TP clear (bit 5 = 0): transparency ON
        set_palette(vdp, 15, 7, 7, 7);
        set_palette(vdp, 9, 5, 1, 1);
        write_vram(vdp, 0x0000, 0x09);  // pixel0 index 0, pixel1 index 9

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == white, "Graphic4_Color0Content_TransparencyOn_ShowsWhiteBackdrop_KonamiSplashShape");
        expect(row[1] == rgb(5, 1, 1), "Graphic4_NonZeroContent_Unaffected");
        expect(renderer.border_color() == white && row[0] == renderer.border_color(),
               "Graphic4_Color0Content_MatchesBorderColor_NoBlackBoxInsideWhiteBorder");
    }

    // --- 2. TP bit SET (R#8 bit 5 = 1): color 0 is a normal, opaque color
    //     -- palette entry 0 itself is shown (no substitution). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_register(vdp, 7, 0x0F);
        set_register(vdp, 8, 0x28);  // TP SET (bit 5 = 1): color 0 opaque
        set_palette(vdp, 15, 7, 7, 7);
        set_palette(vdp, 0, 1, 2, 3);
        write_vram(vdp, 0x0000, 0x00);

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == rgb(1, 2, 3), "Graphic4_Color0Content_TransparencyOff_ShowsPalette0Raw");
    }

    // --- 3. Character mode (GRAPHIC1): a color-table background nibble of 0
    //     shows the backdrop too (the classic MSX "border color shows
    //     through color-0 game backgrounds" behavior;
    //     CharacterConverter.cc:268-269 reads both nibbles through palFg).
    //     FAILS pre-fix. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        // GRAPHIC1 is the reset/default mode; R#8 reset value 0 -> TP clear.
        set_register(vdp, 4, 0x01);  // pattern table base 0x0800 (avoid table aliasing)
        set_register(vdp, 3, 0x40);  // color table base 0x1000
        set_register(vdp, 7, 0x04);  // backdrop color 4
        set_palette(vdp, 4, 0, 0, 7);
        set_palette(vdp, 5, 5, 1, 1);
        write_vram(vdp, 0, 0x07);                    // name col0: char 7
        write_vram(vdp, 0x0800 + 7 * 8 + 0, 0xF0);   // pattern: 4 fg pixels then 4 bg pixels
        write_vram(vdp, 0x1000 + 7 / 8, 0x50);       // color byte: fg=5, bg=0

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == rgb(5, 1, 1), "Graphic1_ForegroundNibble_Unaffected");
        expect(row[4] == rgb(0, 0, 7), "Graphic1_ColorTableZeroNibble_TransparencyOn_ShowsBackdrop");
    }

    // --- 4. GRAPHIC5 (SCREEN 6) split-backdrop: R#7 = 0x0E -> even pixels'
    //     color 0 shows palette[3] (bits 3-2), odd pixels' color 0 shows
    //     palette[2] (bits 1-0) -- SDLRasterizer.cc:364-372's
    //     palFg[0]/palFg[16] split. FAILS pre-fix. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x08);  // GRAPHIC5 (base 0x10)
        set_register(vdp, 7, 0x0E);  // even half = 3, odd half = 2
        set_register(vdp, 8, 0x08);  // TP clear
        set_palette(vdp, 2, 0, 7, 0);
        set_palette(vdp, 3, 7, 0, 7);
        write_vram(vdp, 0x0000, 0x00);  // 4 pixels, all color 0

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[512];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == rgb(7, 0, 7) && row[2] == rgb(7, 0, 7),
               "Graphic5_Color0EvenPixels_ShowBackdropBits32");
        expect(row[1] == rgb(0, 7, 0) && row[3] == rgb(0, 7, 0),
               "Graphic5_Color0OddPixels_ShowBackdropBits10");
    }

    // --- 5. GRAPHIC7 (SCREEN 8) never substitutes: transparency is forced
    //     off in this mode (SDLRasterizer.cc:349-352); byte 0x00 stays the
    //     fixed-decode black regardless of R#7/R#8. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0E);  // GRAPHIC7 (base 0x1C)
        set_register(vdp, 7, 0xFF);
        set_register(vdp, 8, 0x08);  // TP clear -- must still NOT substitute
        write_vram(vdp, 0x0000, 0x00);

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == graphic7_fixed_color_to_rgb555(0),
               "Graphic7_ByteZero_NeverSubstituted_TransparencyForcedOff");
    }

    // --- 6. Determinism: the substitution is a pure function of stored
    //     register/palette state -- two renders are bit-identical. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x06);
        set_register(vdp, 7, 0x0F);
        set_palette(vdp, 15, 7, 7, 7);
        write_vram(vdp, 0x0000, 0x0A);

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t a[256];
        std::uint16_t b[256];
        renderer.render_line(0, Field::Progressive, a);
        renderer.render_line(0, Field::Progressive, b);
        bool identical = true;
        for (int i = 0; i < 256; ++i) {
            identical = identical && a[i] == b[i];
        }
        expect(identical, "Determinism_TwoRenders_BitIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpFrameRendererColor0Backdrop_Unit cases passed\n";
    return 0;
}
