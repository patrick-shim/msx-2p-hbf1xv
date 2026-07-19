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

// Suite: Devices_VdpFrameRendererSpriteEdgeClip_Unit  (boot-logo fix cycle)
//
// Sprite X coordinates live in a 256-wide space even on the 512-wide
// GRAPHIC5/GRAPHIC6 canvases: each sprite pixel there spans TWO canvas
// pixels, so the compositor's per-pixel loop must clip at the sprite-space
// limit (256), NOT the canvas width (512). The reference encodes this
// contract in SpriteConverter.hh:134-143 (mode-2 buffer documented as "256
// pixels" in sprite coordinates, clipped via clipPattern's maxX) with the
// GRAPHIC5/6 pixelPtr[x*2+0/1] writes at :186-198 (behavior reference,
// never copied). The pre-fix compositor used the canvas width as the loop
// bound, letting a right-edge sprite write out[x*2+1] past the 512-entry
// row span -- an out-of-bounds write, hit in practice by the MSX2+ boot
// logo's SCREEN-6 letter sprites (debug-CRT abort during boot).

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"

namespace {

using sony_msx::devices::video::Field;
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

void write_vram(V9958Vdp& vdp, const std::uint16_t addr, const std::uint8_t value) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    vdp.io_write(0x98, value);
}

// Mode-2 sprite tables: R#5=0x07 -> 1KB-aligned table at 0 (colors 0-511,
// Y/X/pattern at 512-1023), pattern table at 0x1800 (the established
// mode-2 harness layout, video_vdp_frame_renderer_sprites_unit_test.cpp).
constexpr std::uint16_t kYxpBase = 512;
constexpr std::uint16_t kColorBase = 0;
constexpr std::uint16_t kPatternBase = 0x1800;

void setup_right_edge_sprite(V9958Vdp& vdp) {
    set_register(vdp, 1, 0x40);  // display enable
    set_register(vdp, 5, 0x07);
    set_register(vdp, 6, kPatternBase >> 11);
    // Sprite 0 at X=250, Y=0 (visible from output line 1), 8x8 all-ones
    // pattern: sprite-space pixels 250..257 -- 250..255 visible, 256..257
    // clipped on real hardware.
    write_vram(vdp, kYxpBase + 0, 0);
    write_vram(vdp, kYxpBase + 1, 250);
    write_vram(vdp, kYxpBase + 2, 0);
    write_vram(vdp, kYxpBase + 4, 216);  // sentinel
    for (int line = 0; line < 8; ++line) {
        write_vram(vdp, static_cast<std::uint16_t>(kColorBase + line), 0x0F);  // color 15
        write_vram(vdp, static_cast<std::uint16_t>(kPatternBase + line), 0xFF);
    }
}

}  // namespace

int main() {
    // --- GRAPHIC5 (SCREEN 6, 512-wide canvas): a sprite at X=250 draws its
    //     half-pixel pairs up to canvas x=511 and is clipped at sprite-space
    //     256 -- no out-of-bounds write past the 512-entry row. Guard
    //     padding around the row detects any overrun in release builds;
    //     debug builds would abort via the checked std::span before the
    //     asserts even run on pre-fix code. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x08);  // GRAPHIC5
        setup_right_edge_sprite(vdp);
        vdp.on_vsync();

        const VdpFrameRenderer renderer(vdp);
        std::vector<std::uint16_t> padded(512 + 64, 0xDEAD);
        renderer.render_line(1, Field::Progressive, {padded.data(), 512});

        // Color 15's half-pixel split: 0x0F >> 2 = 3, 0x0F & 3 = 3 --
        // both canvas half-pixels use palette entry 3.
        bool edge_pixels_drawn = true;
        for (int x = 500; x <= 511; ++x) {
            if (padded[static_cast<std::size_t>(x)] == 0xDEAD) {
                edge_pixels_drawn = false;
                break;
            }
        }
        expect(edge_pixels_drawn, "Graphic5_RightEdgeSprite_DrawsThroughCanvasX511");

        bool no_overrun = true;
        for (std::size_t i = 512; i < padded.size(); ++i) {
            if (padded[i] != 0xDEAD) {
                no_overrun = false;
                break;
            }
        }
        expect(no_overrun, "Graphic5_RightEdgeSprite_NoWritePastRowEnd");
    }

    // --- GRAPHIC6 (SCREEN 7): same doubled-pixel geometry, same clip. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x0A);  // GRAPHIC6 (base 0x14)
        setup_right_edge_sprite(vdp);
        vdp.on_vsync();

        const VdpFrameRenderer renderer(vdp);
        std::vector<std::uint16_t> padded(512 + 64, 0xDEAD);
        renderer.render_line(1, Field::Progressive, {padded.data(), 512});

        expect(padded[511] != 0xDEAD, "Graphic6_RightEdgeSprite_DrawsThroughCanvasX511");
        bool no_overrun = true;
        for (std::size_t i = 512; i < padded.size(); ++i) {
            if (padded[i] != 0xDEAD) {
                no_overrun = false;
                break;
            }
        }
        expect(no_overrun, "Graphic6_RightEdgeSprite_NoWritePastRowEnd");
    }

    // --- Full-frame render survives (the exact boot-logo crash shape) and
    //     is deterministic across two runs. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x08);
            setup_right_edge_sprite(*vdp);
            vdp->on_vsync();
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        const auto fa = ra.render_frame();
        const auto fb = rb.render_frame();
        expect(fa.width == 512, "Graphic5_FullFrame_Width512");
        expect(fa.pixels == fb.pixels, "Graphic5_RightEdgeSprite_FullFrameDeterministic");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpFrameRendererSpriteEdgeClip_Unit cases passed\n";
    return 0;
}
