// Suite: Devices_SpriteEngineMode2AttributeMasking_Unit
//
// Regression coverage for the sprite-invisibility bug found via live Metal
// Gear (Konami, MSX2) gameplay (docs/sprite-invisibility-investigation.md):
// in sprite mode 2 the attribute-table registers R#5/R#11 form an AND-mask,
// not a plain base address --
//     addr = ((R#11<<15) | (R#5<<7) | 0x7F) & (~0x3FF | index)
// (references/openmsx-21.0/src/video/VDP.cc:1357-1371
// updateSpriteAttributeBase, VDPVRAM.hh:263-279 readNP/getReadArea; behavior
// reference only, never copied -- GPL isolation). R#5's low 3 bits (address
// bits A9-A7) are mask bits that real software always sets to 1 (BIOS
// SCREEN5 R#5=0xEF, Metal Gear R#5=0xE7): the resulting 1KB region holds the
// per-line color table at offsets 0-511 and the Y/X/pattern sub-table at
// offsets 512-1023. The pre-fix SpriteEngine treated R#5's full value as a
// plain base and added +512, reading Y/X/pattern 0x200 bytes too high
// (inside the sprite pattern table -- garbage/zeros), which made every
// sprite in every mode-2 game invisible.
//
// Case 1 reproduces the exact Metal Gear register shape (R#5=0xE7, R#11=0x01,
// R#6=0x1F, 16x16 sprites, sprite 0 at Y=0xB0/X=0x2A) end-to-end: engine
// visible-sprite selection AND renderer FrameBuffer pixels.
// Case 2 uses the BIOS SCREEN5 default shape (R#5=0xEF, R#11=0) with a DECOY
// sprite planted at the pre-fix (naive base+512) read location, asserting the
// engine reads the masked location and NOT the naive one.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_palette.h"

namespace {

using sony_msx::devices::video::expand3to5;
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

// 17-bit VRAM write: R#14 carries A16-A14, the port-1 address setup carries
// A13-A0 (the SAME protocol real software uses for tables above 0x3FFF --
// Metal Gear's sprite tables live at 0xF000-0xF27F).
void write_vram17(V9958Vdp& vdp, const std::uint32_t addr, const std::uint8_t value) {
    set_register(vdp, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
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
    // --- Case 1: the exact Metal Gear register shape. R#5=0xE7/R#11=0x01 ->
    //     baseMask 0xF3FF -> per-line colors at 0xF000, Y/X/pattern at
    //     0xF200, pattern generator at 0xF800 (R#6=0x1F). Sprite 0: 16x16 at
    //     Y=0xB0 (first visible output line 0xB1), X=0x2A, pattern 0, color
    //     14 on every line. Pre-fix, Y/X/pattern were read from 0xF580
    //     (zeros) and the sprite never appeared anywhere in the frame. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4 (base 0x0C) -> sprite mode 2
        set_register(vdp, 1, 0x42);  // display enable + 16x16 sprites (as Metal Gear: R#1=0x62)
        set_register(vdp, 5, 0xE7);
        set_register(vdp, 11, 0x01);
        set_register(vdp, 6, 0x1F);  // pattern_base = 0xF800
        set_register(vdp, 9, 0x80);  // 212-line display (Metal Gear's R#9 LN=1)
        set_palette(vdp, 14, 7, 7, 7);

        // Y/X/pattern sub-table at masked base + 512 = 0xF200.
        write_vram17(vdp, 0xF200 + 0, 0xB0);  // Y = 176
        write_vram17(vdp, 0xF200 + 1, 0x2A);  // X = 42
        write_vram17(vdp, 0xF200 + 2, 0x00);  // pattern 0
        write_vram17(vdp, 0xF200 + 4, 216);   // sentinel (sprite 1)
        // Per-line colors at masked base = 0xF000: color 14, all 16 lines.
        for (std::uint32_t line = 0; line < 16; ++line) {
            write_vram17(vdp, 0xF000 + line, 0x0E);
        }
        // Pattern 0, row 0, both 16x16 quadrant columns solid.
        write_vram17(vdp, 0xF800 + 0, 0xFF);
        write_vram17(vdp, 0xF800 + 16, 0xFF);

        vdp.on_vsync();

        // Y=0xB0 with the 1-pixel vertical shift -> first visible on output
        // line 0xB1 = 177.
        const auto visible = vdp.sprite_engine().visible_sprites(177);
        expect(visible.size() == 1, "MetalGearShape_SpriteVisibleOnLine177");
        if (visible.size() == 1) {
            expect(visible[0].x == 0x2A, "MetalGearShape_SpriteXFromMaskedTable");
            expect((visible[0].color_attrib & 0x0F) == 14, "MetalGearShape_ColorFromMaskedTable");
            expect(visible[0].pattern == 0xFFFF0000u, "MetalGearShape_PatternRowSolid16");
        }

        const VdpFrameRenderer renderer(vdp);
        const auto fb = renderer.render_frame();
        const std::uint16_t color14 = pack_rgb555(expand3to5(7), expand3to5(7), expand3to5(7));
        expect(fb.at(0x2A, 177) == color14, "MetalGearShape_FrameBufferShowsSpritePixel");
        expect(fb.at(0x2A + 15, 177) == color14, "MetalGearShape_FrameBufferShows16thColumn");
    }

    // --- Case 2: BIOS SCREEN5 default shape (R#5=0xEF, R#11=0) -> colors at
    //     0x7400, Y/X/pattern at 0x7600. A DECOY sprite is planted at the
    //     PRE-FIX read location (naive (R#5<<7)+512 = 0x7980): if the
    //     regression ever returns, the decoy (Y=10 -> line 11) becomes
    //     visible and the real sprite (Y=100 -> line 101) vanishes. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4 -> sprite mode 2
        set_register(vdp, 1, 0x40);  // display enable, 8x8 sprites
        set_register(vdp, 5, 0xEF);
        set_register(vdp, 11, 0x00);
        set_register(vdp, 6, 0x07);  // pattern_base = 0x3800

        // Real table (masked semantics): Y/X/pattern at 0x7600.
        write_vram17(vdp, 0x7600 + 0, 100);  // Y
        write_vram17(vdp, 0x7600 + 1, 50);   // X
        write_vram17(vdp, 0x7600 + 2, 0);    // pattern
        write_vram17(vdp, 0x7600 + 4, 216);  // sentinel
        write_vram17(vdp, 0x7400 + 0, 0x0F);  // sprite 0 line 0 color
        // Decoy at the naive/pre-fix location 0x7980.
        write_vram17(vdp, 0x7980 + 0, 10);   // decoy Y
        write_vram17(vdp, 0x7980 + 1, 0);    // decoy X
        write_vram17(vdp, 0x7980 + 2, 0);    // decoy pattern
        write_vram17(vdp, 0x7980 + 4, 216);  // decoy sentinel
        // Pattern 0 row 0 solid.
        write_vram17(vdp, 0x3800, 0xFF);

        vdp.on_vsync();

        expect(vdp.sprite_engine().visible_sprites(101).size() == 1,
               "BiosScreen5Shape_RealSpriteVisibleOnLine101");
        expect(vdp.sprite_engine().visible_sprites(11).empty(),
               "BiosScreen5Shape_DecoyAtNaiveAddressNotRead");
        const auto visible = vdp.sprite_engine().visible_sprites(101);
        if (visible.size() == 1) {
            expect(visible[0].x == 50, "BiosScreen5Shape_XFromMaskedTable");
        }
    }

    // --- Two-run determinism (required oracle, tests/CLAUDE.md). ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x06);
            set_register(*vdp, 1, 0x42);
            set_register(*vdp, 5, 0xE7);
            set_register(*vdp, 11, 0x01);
            set_register(*vdp, 6, 0x1F);
            write_vram17(*vdp, 0xF200 + 0, 0xB0);
            write_vram17(*vdp, 0xF200 + 1, 0x2A);
            write_vram17(*vdp, 0xF200 + 2, 0x00);
            write_vram17(*vdp, 0xF200 + 4, 216);
            write_vram17(*vdp, 0xF000 + 0, 0x0E);
            write_vram17(*vdp, 0xF800 + 0, 0xFF);
            write_vram17(*vdp, 0xF800 + 16, 0xFF);
            vdp->on_vsync();
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels,
               "MaskedAttributeTable_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
