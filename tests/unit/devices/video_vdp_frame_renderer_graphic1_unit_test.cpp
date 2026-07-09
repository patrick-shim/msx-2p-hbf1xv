// Suite: Devices_VdpFrameRendererGraphic1_Unit
//
// Deterministic unit coverage for M21-S1's GRAPHIC1 (SCREEN 1) render path --
// the pipeline-proof tile mode. Grounded in
// references/openmsx-21.0/src/video/CharacterConverter.cc:255-270
// (renderGraphic1) -- never copied (GPL isolation).

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
    // GRAPHIC1 is the reset/default mode (base 0x00) -- no R#0/R#1 write needed.
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256, "Graphic1_Width_Is256");
        expect(renderer.height() == 192, "Graphic1_Height_Is192_WhenLnClear");
    }

    // --- Correct content from name/pattern/color-table content, with the
    //     PER-CHARACTER-BLOCK color byte (not global R#7). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 2, 0x00);   // name table base 0x0000
        set_register(vdp, 4, 0x01);   // pattern table base 0x0800 (1<<11)
        set_register(vdp, 3, 0x40);   // color table base 0x1000 ((0x40<<6)|(0<<14))
        set_register(vdp, 10, 0x00);
        set_palette(vdp, 2, 1, 6, 1);
        set_palette(vdp, 14, 5, 5, 5);

        write_vram(vdp, 0, 0x05);                 // row0 col0: char code 5
        write_vram(vdp, 0x0800 + 5 * 8 + 0, 0xC3);  // pattern row0: 1100_0011
        write_vram(vdp, 0x1000 + 5 / 8, 0x2E);      // color byte for char-block 0: fg=2, bg=14 (0xE)

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);

        const std::uint16_t fg = pack_rgb555(expand3to5(1), expand3to5(6), expand3to5(1));
        const std::uint16_t bg = pack_rgb555(expand3to5(5), expand3to5(5), expand3to5(5));

        // Pattern 0xC3 = 1100_0011 -> 8 bits: 1,1,0,0,0,0,1,1.
        const bool bits[8] = {true, true, false, false, false, false, true, true};
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            if (row[i] != (bits[i] ? fg : bg)) ok = false;
        }
        expect(ok, "Graphic1_FirstBlock_MatchesPatternWithPerBlockColor");
    }

    // --- Border color reflects R#7 (used for backdrop even though GRAPHIC1
    //     content itself uses per-block colors). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 7, 0x03);  // backdrop index 3
        set_palette(vdp, 3, 4, 2, 0);
        const VdpFrameRenderer renderer(vdp);
        const std::uint16_t expected = pack_rgb555(expand3to5(4), expand3to5(2), expand3to5(0));
        expect(renderer.border_color() == expected, "Graphic1_BorderColor_ReflectsR7LowNibble");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 4, 0x01);   // pattern table base 0x0800
            set_register(*vdp, 3, 0x40);   // color table base 0x1000
            write_vram(*vdp, 0, 0x07);
            write_vram(*vdp, 0x0800 + 7 * 8, 0x5A);
            write_vram(*vdp, 0x1000 + 7 / 8, 0x3C);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        const auto fb_a = ra.render_frame();
        const auto fb_b = rb.render_frame();
        expect(fb_a.pixels == fb_b.pixels, "Graphic1_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
