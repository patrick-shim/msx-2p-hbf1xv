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

// Suite: Devices_VdpFrameRendererYjk_Unit
//
// Deterministic unit coverage for M21-S5's YJK (SCREEN12) and YJK+YAE
// (SCREEN10/11) color decode, layered on S4's planar plumbing. Grounded in
// references/openmsx-21.0/src/video/BitmapConverter.cc:217-276 -- never
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

void set_palette(V9958Vdp& vdp, const int index, const std::uint8_t r3, const std::uint8_t g3,
                  const std::uint8_t b3) {
    set_register(vdp, 16, static_cast<std::uint8_t>(index & 0x0F));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

}  // namespace

int main() {
    // --- SCREEN12 (YJK, no YAE). One 4-pixel group: p0=0x08 (y0=1),
    //     p1=0x10 (y1=2, contributes 0 to k), p2=0x20 (y2=4), p3=0x00
    //     (y3=0, contributes 0 to j) -> j=0, k=0 for this group, so
    //     R=G=y, B=clamp((5y+2)/4) (plain int division).
    //
    //     DEF-M41-YJKOFFSET (re-derived M41 production-QA oracle): YJK content
    //     is registered kYjkDisplayLead == 4 dots to the RIGHT of the GRAPHIC7
    //     base (the whole group's chroma J lives in its 3rd/4th bytes, so the
    //     group cannot resolve until all four bytes are fetched -> a four-dot
    //     display latency). Confirmed three ways: openMSX 19.1 A/B (YJK at raw
    //     col 36 vs the IDENTICAL-base G7 control at col 32); fMSX 6.0
    //     references/fmsx-60/source/fMSX/Common.h:778-783 (RefreshLine12 draws
    //     the first four pixels as backdrop before the YJK groups); fact-sheet
    //     references/fact-sheets/Yamaha V9958 VDP.md:104 (group byte packing).
    //     OLD oracle asserted the group at display pixels 0..3; NEW (correct)
    //     oracle: display pixels 0..3 are BACKDROP, VRAM group 0 decodes at
    //     display pixels 4..7 -- strengthened (it now pins the registration
    //     AND the decode, not the decode alone). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0E);   // GRAPHIC7 base
        set_register(vdp, 25, 0x08);  // YJK (no YAE) -> ScreenYjk
        set_register(vdp, 7, 0x05);   // backdrop = palette index 5 (the lead-strip color)
        set_palette(vdp, 5, 7, 0, 0);  // distinctive red backdrop, != any content pixel below
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256, "Yjk_Width_Is256");

        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0x08);  // logical0 -> bank0[0] = p0
        vdp.io_write(0x98, 0x10);  // logical1 -> bank1[0] = p1
        vdp.io_write(0x98, 0x20);  // logical2 -> bank0[1] = p2
        vdp.io_write(0x98, 0x00);  // logical3 -> bank1[1] = p3

        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);

        // Left 4 dots are the YJK display-latency backdrop strip (== border).
        const std::uint16_t backdrop = renderer.border_color();
        expect(backdrop != pack_rgb555(1, 1, 1), "Yjk_Backdrop_DistinctFromContent");
        expect(row[0] == backdrop, "Yjk_LeadPixel0_Backdrop");
        expect(row[1] == backdrop, "Yjk_LeadPixel1_Backdrop");
        expect(row[2] == backdrop, "Yjk_LeadPixel2_Backdrop");
        expect(row[3] == backdrop, "Yjk_LeadPixel3_Backdrop");
        // VRAM group 0 now decodes at display pixels 4..7 (was 0..3).
        // y0=1,j=0,k=0 -> R=1,G=1,B=clamp((5+2)/4=1)=1.
        expect(row[4] == pack_rgb555(1, 1, 1), "Yjk_Pixel4_Y1");
        // y1=2,j=0,k=0 -> R=2,G=2,B=clamp((10+2)/4=3)=3.
        expect(row[5] == pack_rgb555(2, 2, 3), "Yjk_Pixel5_Y2");
        // y2=4,j=0,k=0 -> R=4,G=4,B=clamp((20+2)/4=5)=5.
        expect(row[6] == pack_rgb555(4, 4, 5), "Yjk_Pixel6_Y4");
        // y3=0,j=0,k=0 -> R=0,G=0,B=clamp((0+2)/4=0, plain trunc)=0.
        expect(row[7] == pack_rgb555(0, 0, 0), "Yjk_Pixel7_Y0_TruncatingDivision");
    }

    // --- SCREEN10/11 (YJK+YAE): pixel1's attribute bit (p1 & 0x08) selects
    //     the 16-color palette branch INSTEAD of the computed YJK color;
    //     the OTHER three pixels in the same group still use the YJK path
    //     with the unaffected J/K (bits 0-2 of p1 are unchanged). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0E);
        set_register(vdp, 25, 0x18);  // YJK + YAE -> ScreenYjkYae
        set_register(vdp, 7, 0x05);   // backdrop = palette index 5 (the lead-strip color)
        const VdpFrameRenderer renderer(vdp);

        set_palette(vdp, 1, 6, 5, 4);  // distinguishing palette color for the YAE branch
        set_palette(vdp, 5, 7, 0, 0);  // distinctive red backdrop for the lead strip

        set_write_address(vdp, 0);
        // p0 uses an EVEN y (y0=2 -> p0=0x10) so its bit3 (the YAE attribute
        // bit) is naturally CLEAR -- unlike the plain-YJK test above (y0=1,
        // p0=0x08), which would have bit3 SET and thus wrongly trigger the
        // attribute branch for pixel0 too in THIS (YAE) mode. Only p1 is
        // deliberately given bit3=1 to exercise the attribute branch.
        vdp.io_write(0x98, 0x10);  // p0 = 0x10 (y0=2, bit3 clear -> still YJK)
        vdp.io_write(0x98, 0x18);  // p1 = 0x18: bit3 (YAE attribute) SET, low2=0 (k contribution 0)
        vdp.io_write(0x98, 0x20);  // p2 = 0x20 (y2=4, bit3 clear)
        vdp.io_write(0x98, 0x00);  // p3 = 0x00 (bit3 clear)

        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);

        // DEF-M41-YJKOFFSET: YAE shares the plain-YJK 4-dot display latency
        // (fMSX RefreshLine10, Common.h:732-737 -- first four pixels backdrop).
        // Left 4 dots = backdrop; the group's decode is shifted to 4..7.
        const std::uint16_t backdrop = renderer.border_color();
        expect(row[0] == backdrop && row[3] == backdrop, "YjkYae_LeadStrip_Backdrop");
        // y0=2,j=0,k=0 -> R=2,G=2,B=clamp((10+2)/4=3)=3.
        expect(row[4] == pack_rgb555(2, 2, 3), "YjkYae_Pixel4_StillYjkComputed");
        // Pixel(5) uses the YAE palette branch: p1>>4 = 1 -> palette[1] = (6,5,4).
        expect(row[5] == pack_rgb555(expand3to5(6), expand3to5(5), expand3to5(4)),
               "YjkYae_Pixel5_UsesPaletteBranch_NotYjkComputed");
        expect(row[6] == pack_rgb555(4, 4, 5), "YjkYae_Pixel6_StillYjkComputed");
        expect(row[7] == pack_rgb555(0, 0, 0), "YjkYae_Pixel7_StillYjkComputed");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x0E);
            set_register(*vdp, 25, 0x08);
            set_write_address(*vdp, 0);
            vdp->io_write(0x98, 0x33);
            vdp->io_write(0x98, 0x44);
            vdp->io_write(0x98, 0x55);
            vdp->io_write(0x98, 0x66);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels, "Yjk_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
