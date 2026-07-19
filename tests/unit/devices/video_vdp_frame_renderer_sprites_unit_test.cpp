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

// Suite: Devices_VdpFrameRendererSprites_Unit
//
// Deterministic unit coverage for M22-S2's sprite pixel compositing, folded
// into VdpFrameRenderer::render_frame()'s existing pipeline (planner package
// §1.4 Resolution 1): mode 1 overdraw priority, mode 2 CC-cascade merge, the
// GRAPHIC5 half-pixel split, IC=1-still-drawn (A-M22-11), and the color-0/TP
// transparency rule (A-M22-12) in both sprite modes. Grounded in
// references/openmsx-21.0/src/video/SpriteConverter.hh -- never copied (GPL
// isolation).

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

// Mode 1 (GRAPHIC1) sprite tables, deliberately placed away from address 0
// (GRAPHIC1's own name/pattern/color tables default there but are left
// unwritten/irrelevant in these tests).
constexpr std::uint16_t kM1AttribBase = 0x0800;
constexpr std::uint16_t kM1PatternBase = 0x1800;

void select_graphic1_sprite_tables(V9958Vdp& vdp) {
    set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
    set_register(vdp, 5, 0x10);  // attrib_base = (R11<<15)|(R5<<7) = 0x0800
    set_register(vdp, 6, 0x03);  // pattern_base = R6<<11 = 0x1800
}

void write_sprite1(V9958Vdp& vdp, const int index, const std::uint8_t y, const std::uint8_t x,
                    const std::uint8_t pattern_nr, const std::uint8_t color_attrib) {
    const std::uint16_t base = static_cast<std::uint16_t>(kM1AttribBase + index * 4);
    write_vram(vdp, base + 0, y);
    write_vram(vdp, base + 1, x);
    write_vram(vdp, base + 2, pattern_nr);
    write_vram(vdp, base + 3, color_attrib);
}

void write_sentinel1(V9958Vdp& vdp, const int index) {
    write_vram(vdp, static_cast<std::uint16_t>(kM1AttribBase + index * 4), 208);
}

void write_pattern1(V9958Vdp& vdp, const std::uint8_t pattern_nr, const int row, const std::uint8_t byte) {
    write_vram(vdp, static_cast<std::uint16_t>(kM1PatternBase + pattern_nr * 8 + row), byte);
}

// Mode 2 (GRAPHIC4/GRAPHIC5) sprite tables.
constexpr std::uint16_t kM2AttribBase = 0;      // per-line color/attrib sub-table
constexpr std::uint16_t kM2YxpBase = 512;       // Y/X/pattern sub-table (+512)
constexpr std::uint16_t kM2PatternBase = 0x1000;

void select_m2_sprite_tables(V9958Vdp& vdp) {
    set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
    // R#5 low 3 bits are AND-mask bits in sprite mode 2 (VDP.cc:1357-1371:
    // addr = ((R11<<15)|(R5<<7)|0x7F) & (~0x3FF|index)); 0x07 -> 1KB-aligned
    // table at 0x0000 with colors at 0-511 and Y/X/pattern at 512-1023 --
    // this file's exact kM2AttribBase/kM2YxpBase layout (see the sprite-
    // invisibility investigation, docs/sprite-invisibility-investigation.md).
    set_register(vdp, 5, 0x07);
    set_register(vdp, 6, static_cast<std::uint8_t>(kM2PatternBase >> 11));
}

void write_yxp2(V9958Vdp& vdp, const int index, const std::uint8_t y, const std::uint8_t x,
                 const std::uint8_t pattern_nr) {
    const std::uint16_t base = static_cast<std::uint16_t>(kM2YxpBase + index * 4);
    write_vram(vdp, base + 0, y);
    write_vram(vdp, base + 1, x);
    write_vram(vdp, base + 2, pattern_nr);
}

void write_color2(V9958Vdp& vdp, const int index, const int line, const std::uint8_t color_attrib) {
    write_vram(vdp, static_cast<std::uint16_t>(kM2AttribBase + index * 16 + line), color_attrib);
}

void write_sentinel2(V9958Vdp& vdp, const int index) {
    write_vram(vdp, static_cast<std::uint16_t>(kM2YxpBase + index * 4), 216);
}

void write_pattern2(V9958Vdp& vdp, const std::uint8_t pattern_nr, const int row, const std::uint8_t byte) {
    write_vram(vdp, static_cast<std::uint16_t>(kM2PatternBase + pattern_nr * 8 + row), byte);
}

}  // namespace

int main() {
    // --- Mode 1 overdraw priority: sprite 0 visually wins overlaps (drawn
    //     LAST, in reverse index order). ---
    {
        V9958Vdp vdp;
        select_graphic1_sprite_tables(vdp);
        set_palette(vdp, 3, 7, 0, 0);
        set_palette(vdp, 5, 0, 7, 0);
        write_sprite1(vdp, 0, 0, 0, 0, 3);  // sprite 0: color 3
        write_sprite1(vdp, 1, 0, 0, 0, 5);  // sprite 1: color 5, same position
        write_sentinel1(vdp, 2);
        write_pattern1(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);  // Y=0 sprite first visible on line 1
        const std::uint16_t color3 = pack_rgb555(expand3to5(7), expand3to5(0), expand3to5(0));
        expect(row[0] == color3, "Mode1_Overdraw_Sprite0Wins");
    }

    // --- Mode 1 color-0/TP transparency (A-M22-12): TP enabled (R#8 bit5
    //     clear, the reset default) skips color-0 sprite pixels; TP disabled
    //     (bit5 set) draws them using palette entry 0. ---
    {
        V9958Vdp vdp;
        select_graphic1_sprite_tables(vdp);
        // GRAPHIC1's own name/pattern tables stay at their default (all
        // zero, unwritten): char_code 0, pattern 0 (all background). Move
        // the color table to 0x1000 (color_base = R3<<6) and write ONE byte
        // there (fg=0, bg=5) so the ENTIRE background line renders as a
        // KNOWN, non-zero palette entry -- distinguishing "sprite drawn"
        // (palette 0) from "sprite skipped" (background, palette 5).
        set_register(vdp, 3, 0x40);     // color_base = 0x40<<6 = 0x1000
        write_vram(vdp, 0x1000, 0x05);  // fg=0, bg=5
        set_palette(vdp, 5, 7, 0, 0);   // background color
        write_sprite1(vdp, 0, 0, 0, 0, 0);  // color index 0
        write_sentinel1(vdp, 1);
        write_pattern1(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint16_t background = pack_rgb555(expand3to5(7), expand3to5(0), expand3to5(0));

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[0] == background, "Mode1_Color0_TPEnabledDefault_BackgroundShows");
    }
    {
        V9958Vdp vdp;
        select_graphic1_sprite_tables(vdp);
        set_register(vdp, 3, 0x40);
        set_register(vdp, 8, 0x20);      // TP disabled
        set_palette(vdp, 5, 7, 0, 0);
        write_vram(vdp, 0x1000, 0x05);
        write_sprite1(vdp, 0, 0, 0, 0, 0);
        write_sentinel1(vdp, 1);
        write_pattern1(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[0] == 0, "Mode1_Color0_TPDisabled_PaletteZeroDrawn");
    }

    // --- Mode 2 CC-cascade merge: sprite0 (CC=0, color=1), sprite1 (CC=1,
    //     color=2), sprite2 (CC=1, color=4), all fully overlapping ->
    //     merged color = 1|2|4 = 7. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4 -> sprite mode 2
        select_m2_sprite_tables(vdp);
        set_palette(vdp, 7, 3, 5, 1);
        write_yxp2(vdp, 0, 0, 0, 0);
        write_color2(vdp, 0, 0, 1);
        write_yxp2(vdp, 1, 0, 0, 0);
        write_color2(vdp, 1, 0, static_cast<std::uint8_t>(0x40 | 2));
        write_yxp2(vdp, 2, 0, 0, 0);
        write_color2(vdp, 2, 0, static_cast<std::uint8_t>(0x40 | 4));
        write_sentinel2(vdp, 3);
        write_pattern2(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint16_t merged = pack_rgb555(expand3to5(3), expand3to5(5), expand3to5(1));
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[0] == merged, "Mode2_CcCascade_MergedColorSeven");
    }

    // --- IC=1 sprite is still DRAWN (A-M22-11) despite being excluded from
    //     collision detection. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        select_m2_sprite_tables(vdp);
        set_palette(vdp, 6, 2, 4, 6);
        write_yxp2(vdp, 0, 0, 0, 0);
        write_color2(vdp, 0, 0, static_cast<std::uint8_t>(0x20 | 6));  // IC set, color 6
        write_sentinel2(vdp, 1);
        write_pattern2(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint16_t color6 = pack_rgb555(expand3to5(2), expand3to5(4), expand3to5(6));
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[0] == color6, "Mode2_IC_StillDrawn");
    }

    // --- Mode 2 color-0/TP transparency, both states. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        select_m2_sprite_tables(vdp);
        set_palette(vdp, 5, 1, 2, 3);
        write_vram(vdp, 128 + 4, 0x55);  // row 1 (sprite visible from line 1): pixels 8,9 -> palette 5
        write_yxp2(vdp, 0, 0, 8, 0);
        write_color2(vdp, 0, 0, 0);  // color index 0
        write_sentinel2(vdp, 1);
        write_pattern2(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint16_t background = pack_rgb555(expand3to5(1), expand3to5(2), expand3to5(3));
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[8] == background, "Mode2_Color0_TPEnabledDefault_BackgroundShows");
    }
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);
        select_m2_sprite_tables(vdp);
        set_register(vdp, 8, 0x20);  // TP disabled
        write_vram(vdp, 128 + 4, 0x55);
        write_yxp2(vdp, 0, 0, 8, 0);
        write_color2(vdp, 0, 0, 0);
        write_sentinel2(vdp, 1);
        write_pattern2(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[8] == 0, "Mode2_Color0_TPDisabled_PaletteZeroDrawn");
    }

    // --- GRAPHIC5 (SCREEN6) half-pixel split: a merged 4-bit color splits
    //     into palette[color>>2] (even half-pixel) / palette[color&3] (odd
    //     half-pixel). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x08);  // GRAPHIC5 (base 0x10) -> sprite mode 2, 512-wide
        select_m2_sprite_tables(vdp);
        set_palette(vdp, 2, 4, 4, 4);
        set_palette(vdp, 3, 6, 1, 1);
        write_yxp2(vdp, 0, 0, 0, 0);
        write_color2(vdp, 0, 0, 0x0B);  // color nibble 1011 -> >>2=2, &3=3
        write_sentinel2(vdp, 1);
        write_pattern2(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint16_t left = pack_rgb555(expand3to5(4), expand3to5(4), expand3to5(4));
        const std::uint16_t right = pack_rgb555(expand3to5(6), expand3to5(1), expand3to5(1));
        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[512];
        renderer.render_line(1, Field::Progressive, row);
        expect(row[0] == left, "Graphic5_HalfPixel_LeftIsColorShiftedRight2");
        expect(row[1] == right, "Graphic5_HalfPixel_RightIsColorAndThree");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x06);
            select_m2_sprite_tables(*vdp);
            set_palette(*vdp, 4, 3, 3, 3);
            write_yxp2(*vdp, 0, 0, 0, 0);
            write_color2(*vdp, 0, 0, 4);
            write_sentinel2(*vdp, 1);
            write_pattern2(*vdp, 0, 0, 0xFF);
            vdp->on_vsync();
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels, "Sprites_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
