// Suite: Devices_VdpFrameRendererText2_Unit
//
// Deterministic unit coverage for M21-S2's TEXT2 (SCREEN 0 W80) render path,
// including the blink color-flip driven by the new R#12/R#13 countdown state
// on V9958Vdp (decremented by the EXISTING on_vsync() hook -- no new clock
// consumer). Grounded in
// references/openmsx-21.0/src/video/CharacterConverter.cc:183-231
// (renderText2) and VDP.cc:600-608/1040-1057 (blink timing) -- never copied
// (GPL isolation).

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

void select_text2(V9958Vdp& vdp) {
    set_register(vdp, 0, 0x04);  // M4
    set_register(vdp, 1, 0x10);  // M1 -> base 0x09 (TEXT2)
}

}  // namespace

int main() {
    // --- Dimensions. ---
    {
        V9958Vdp vdp;
        select_text2(vdp);
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 480, "Text2_Width_Is480");
        expect(renderer.height() == 192, "Text2_Height_Is192");
    }

    // --- Plain (non-blinking) content: name/pattern/color triple lookup. ---
    {
        V9958Vdp vdp;
        select_text2(vdp);
        set_register(vdp, 2, 0x00);   // name table base 0x0000
        set_register(vdp, 4, 0x01);   // pattern table base 0x0800
        set_register(vdp, 3, 0x40);   // color table base 0x1000
        set_register(vdp, 10, 0x00);
        set_register(vdp, 7, 0xF1);   // plain fg=15, bg=1
        set_palette(vdp, 15, 7, 7, 7);
        set_palette(vdp, 1, 1, 1, 1);

        write_vram(vdp, 0, 0x41);                  // col0 char code
        write_vram(vdp, 0x0800 + 0x41 * 8, 0x80);  // pattern: bit7 set only
        write_vram(vdp, 0x1000, 0x00);             // color byte block0: no blink bits set

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[480];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t fg = pack_rgb555(expand3to5(7), expand3to5(7), expand3to5(7));
        const std::uint16_t bg = pack_rgb555(expand3to5(1), expand3to5(1), expand3to5(1));
        expect(row[0] == fg, "Text2_Plain_Pixel0_Fg");
        expect(row[1] == bg, "Text2_Plain_Pixel1_Bg");
    }

    // --- Blink: writing R#13 (ON=1,OFF=5 -> 0x15) forces blink_state()==true
    //     IMMEDIATELY (VDP.cc:1040-1057), and blinking column0 shows the R#12
    //     blink colors instead of the plain R#7 colors. ---
    {
        V9958Vdp vdp;
        select_text2(vdp);
        set_register(vdp, 2, 0x00);
        set_register(vdp, 4, 0x01);   // pattern table base 0x0800
        set_register(vdp, 3, 0x40);   // color table base 0x1000
        set_register(vdp, 10, 0x00);
        set_register(vdp, 7, 0xF1);    // plain fg=15,bg=1
        set_register(vdp, 12, 0x23);   // blink fg=2, blink bg=3
        set_palette(vdp, 15, 7, 7, 7);
        set_palette(vdp, 1, 1, 1, 1);
        set_palette(vdp, 2, 2, 2, 2);
        set_palette(vdp, 3, 3, 3, 3);

        write_vram(vdp, 0, 0x41);
        write_vram(vdp, 0x0800 + 0x41 * 8, 0x80);
        write_vram(vdp, 0x1000, 0x80);  // color byte block0: bit7 set -> col0 blinks

        set_register(vdp, 13, 0x15);  // ON=1,OFF=5 -> forces blink_state_=true, countdown=10
        expect(vdp.blink_state(), "Text2_WriteR13_ForcesBlinkStateTrueImmediately");

        const VdpFrameRenderer renderer(vdp);
        std::uint16_t row[480];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t blink_fg = pack_rgb555(expand3to5(2), expand3to5(2), expand3to5(2));
        const std::uint16_t blink_bg = pack_rgb555(expand3to5(3), expand3to5(3), expand3to5(3));
        expect(row[0] == blink_fg, "Text2_Blinking_Pixel0_UsesBlinkFg");
        expect(row[1] == blink_bg, "Text2_Blinking_Pixel1_UsesBlinkBg");

        // Countdown re-armed to (ON=1)*10=10 frames; after 9 on_vsync() calls
        // the state has NOT yet flipped.
        for (int i = 0; i < 9; ++i) {
            vdp.on_vsync();
        }
        expect(vdp.blink_state(), "Text2_NinthVsync_StillOnPhase");

        // The 10th on_vsync() call reaches the countdown and flips the phase.
        vdp.on_vsync();
        expect(!vdp.blink_state(), "Text2_TenthVsync_FlipsToOffPhase");

        std::uint16_t row_after[480];
        renderer.render_line(0, Field::Progressive, row_after);
        const std::uint16_t plain_fg = pack_rgb555(expand3to5(7), expand3to5(7), expand3to5(7));
        const std::uint16_t plain_bg = pack_rgb555(expand3to5(1), expand3to5(1), expand3to5(1));
        expect(row_after[0] == plain_fg, "Text2_AfterFlip_UsesPlainFg");
        expect(row_after[1] == plain_bg, "Text2_AfterFlip_UsesPlainBg");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            select_text2(*vdp);
            write_vram(*vdp, 0, 0x10);
            write_vram(*vdp, 0x10 * 8, 0x55);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels, "Text2_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
