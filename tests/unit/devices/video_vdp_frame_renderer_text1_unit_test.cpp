// Suite: Devices_VdpFrameRendererText1_Unit
//
// Deterministic unit coverage for M21-S1's TEXT1 (SCREEN 0 W40) render path --
// the pipeline-proof text mode. Drives VRAM + registers through the EXISTING
// M14 V9958Vdp port contract (#98/#99/#9A), then asserts on VdpFrameRenderer's
// raw RGB555 pixel output. Grounded in
// references/openmsx-21.0/src/video/CharacterConverter.cc:142-160
// (renderText1) -- never copied (GPL isolation).

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

// #9A two-write palette protocol (VDP.cc:709-714): byte1 = 0 R2R1R0 0 B2B1B0,
// byte2 = 0 0 0 0 0 G2G1G0.
void set_palette(V9958Vdp& vdp, const int index, const std::uint8_t r3, const std::uint8_t g3,
                  const std::uint8_t b3) {
    set_register(vdp, 16, static_cast<std::uint8_t>(index & 0x0F));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

}  // namespace

int main() {
    // --- TEXT1 mode selection + dimensions. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 1, 0x50);  // M1 -> TEXT1 (base 0x01)
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 240, "Text1_Width_Is240");
        expect(renderer.height() == 192, "Text1_Height_Is192_IgnoresLn");
    }

    // --- Correct content from name/pattern table content, and border color
    //     reflecting R#7. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 1, 0x50);  // TEXT1
        set_register(vdp, 2, 0x00);  // name table base 0x0000
        set_register(vdp, 4, 0x00);  // pattern table base 0x0000
        set_register(vdp, 7, 0xF1);  // fg=palette[15], bg=palette[1]
        set_palette(vdp, 15, 7, 7, 7);
        set_palette(vdp, 1, 2, 4, 6);

        write_vram(vdp, 0x0000, 0x41);        // row0 col0: char code 'A' (0x41)
        write_vram(vdp, 0x41 * 8 + 0, 0xB4);  // pattern row0: 1011_0100

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[240];
        renderer.render_line(0, Field::Progressive, row);

        const std::uint16_t fg = pack_rgb555(expand3to5(7), expand3to5(7), expand3to5(7));
        const std::uint16_t bg = pack_rgb555(expand3to5(2), expand3to5(4), expand3to5(6));

        // Pattern 0xB4 = 1011_0100 -> bits 7..2: 1,0,1,1,0,1.
        expect(row[0] == fg, "Text1_FirstColumn_Pixel0_Fg");
        expect(row[1] == bg, "Text1_FirstColumn_Pixel1_Bg");
        expect(row[2] == fg, "Text1_FirstColumn_Pixel2_Fg");
        expect(row[3] == fg, "Text1_FirstColumn_Pixel3_Fg");
        expect(row[4] == bg, "Text1_FirstColumn_Pixel4_Bg");
        expect(row[5] == fg, "Text1_FirstColumn_Pixel5_Fg");

        expect(renderer.border_color() == bg, "Text1_BorderColor_ReflectsR7LowNibble");
    }

    // --- Second name row (line 8..15) reads name index (line/8)*40 + col. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 1, 0x50);
        set_register(vdp, 2, 0x00);
        set_register(vdp, 4, 0x00);
        set_register(vdp, 7, 0xF0);
        set_palette(vdp, 15, 7, 7, 7);

        write_vram(vdp, 40, 0x10);            // row1 col0: char code 0x10
        write_vram(vdp, 0x10 * 8 + 0, 0xFF);  // all-fg pattern

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[240];
        renderer.render_line(8, Field::Progressive, row);
        const std::uint16_t fg = pack_rgb555(expand3to5(7), expand3to5(7), expand3to5(7));
        expect(row[0] == fg, "Text1_SecondNameRow_UsesNameIndexPlusFortyTimesRow");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 1, 0x50);
            set_register(*vdp, 7, 0xF2);
            write_vram(*vdp, 0, 0x20);
            write_vram(*vdp, 0x20 * 8, 0xAA);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        const auto fb_a = ra.render_frame();
        const auto fb_b = rb.render_frame();
        expect(fb_a.width == fb_b.width && fb_a.height == fb_b.height, "Text1_Determinism_SameDimensions");
        expect(fb_a.pixels == fb_b.pixels, "Text1_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
