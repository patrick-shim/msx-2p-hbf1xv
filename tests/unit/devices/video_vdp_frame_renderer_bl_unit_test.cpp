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

// Suite: Devices_VdpFrameRendererBl_Unit (M34-S5, DEC-0043 Defect B,
// docs/m34-planner-package.md §3.4.3 / test matrix row 12)
//
// THE BL (R#1 bit6, display enable) RENDER-GATE ORACLE: a BL=0 line renders
// PURE BACKDROP -- every pixel == border_color(), content skipped, sprites
// skipped (they are off when the display is off) -- across a representative
// mode set (TEXT2, GRAPHIC4, GRAPHIC5, GRAPHIC7, YJK). BL=1 arms assert the
// same scene renders genuine (non-backdrop) content, and the sprite arm
// asserts sprite pixels present under BL=1 and ABSENT under BL=0.
//
// Grounding (§3.1, both references AGREE):
//   * openMSX references/openmsx-21.0/src/video/PixelRenderer.cc:608-611
//     (!displayEnabled => whole line DRAW_BORDER), :580-584 (sprites only
//     under displayEnabled); VDP.cc:435-442 (displayEnabled = R#1 & 0x40).
//   * fMSX references/fmsx-60/source/MSX.h:216 (ScreenON = VDP[1]&0x40) +
//     Common.h:463 etc. (every mode's refresh starts
//     `if(!ScreenON) ClearLine(background)`).
// Fill color is the mode-aware border_color() (VDP.hh:211-226
// getBackgroundColor), NOT render_blank()'s undefined-mode palette-15
// fallback -- asserted here by comparing against renderer.border_color().

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"

namespace {

using sony_msx::devices::video::Field;
using sony_msx::devices::video::FrameBuffer;
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

// Deterministic pseudo-random VRAM content (seeded LCG, tests/CLAUDE.md).
struct Lcg {
    std::uint32_t state;
    explicit Lcg(const std::uint32_t seed) : state(seed) {}
    std::uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state >> 16;
    }
};

void fill_vram(V9958Vdp& vdp, const std::uint16_t base, const int count, const std::uint32_t seed) {
    Lcg lcg(seed);
    set_write_address(vdp, base);
    for (int i = 0; i < count; ++i) {
        vdp.io_write(0x98, static_cast<std::uint8_t>(lcg.next() & 0xFF));
    }
}

struct ModeRow {
    const char* name;
    std::uint8_t r0;
    std::uint8_t r1_mode_bits;  // mode bits only; BL added per arm
    std::uint8_t r25;
};

// Renders a full frame with the given BL state and returns it.
FrameBuffer render_with_bl(const ModeRow& row, const bool bl, const std::uint32_t seed) {
    V9958Vdp vdp;
    set_register(vdp, 0, row.r0);
    set_register(vdp, 1, static_cast<std::uint8_t>(row.r1_mode_bits | (bl ? 0x40 : 0x00)));
    set_register(vdp, 25, row.r25);
    set_register(vdp, 7, 0xF5);  // fg 15 (TEXT2 legibility), border/backdrop = palette 5
    fill_vram(vdp, 0x0000, 0x2000, seed);
    const VdpFrameRenderer renderer(vdp);
    return renderer.render_frame(Field::Progressive);
}

bool all_pixels_equal(const FrameBuffer& fb, const std::uint16_t value) {
    for (const std::uint16_t p : fb.pixels) {
        if (p != value) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    const ModeRow rows[] = {
        // name       R#0   R#1-mode  R#25
        {"Text2", 0x04, 0x10, 0x00},
        {"Graphic4", 0x06, 0x00, 0x00},
        {"Graphic5", 0x08, 0x00, 0x00},
        {"Graphic7", 0x0E, 0x00, 0x00},
        {"ScreenYjk", 0x0E, 0x00, 0x08},
    };

    // --- 1. Per-mode: BL=0 => every pixel == border_color() exactly; BL=1
    //     => genuine content (the same scene is NOT all-backdrop). ---
    {
        std::uint32_t seed = 0xB1B1u;
        for (const ModeRow& row : rows) {
            // Border color read from an identically-configured BL=0 VDP.
            V9958Vdp probe;
            set_register(probe, 0, row.r0);
            set_register(probe, 1, row.r1_mode_bits);
            set_register(probe, 25, row.r25);
            set_register(probe, 7, 0xF5);
            const VdpFrameRenderer probe_renderer(probe);
            const std::uint16_t backdrop = probe_renderer.border_color();

            const FrameBuffer off = render_with_bl(row, false, seed);
            if (!all_pixels_equal(off, backdrop)) {
                std::cerr << "  (mode " << row.name << ")\n";
                expect(false, "Bl0_EveryPixelIsBorderColor_PureBackdrop");
            }
            const FrameBuffer on = render_with_bl(row, true, seed);
            if (all_pixels_equal(on, backdrop)) {
                std::cerr << "  (mode " << row.name << ")\n";
                expect(false, "Bl1_SameScene_RendersGenuineContent");
            }
            seed += 31;
        }
        std::cout << "BL gate: " << (sizeof(rows) / sizeof(rows[0])) << " modes checked\n";
    }

    // --- 2. Sprite arm (GRAPHIC4, mode-2 sprites): visible under BL=1,
    //     ABSENT under BL=0 (pure backdrop, no sprite pixels). ---
    {
        const auto build = [](const bool bl) {
            V9958Vdp vdp;
            set_register(vdp, 0, 0x06);  // GRAPHIC4
            set_register(vdp, 1, bl ? 0x40 : 0x00);
            set_register(vdp, 7, 0x05);
            // All-zero content (backdrop-colored via color-0 transparency is
            // NOT in play: use palette color 0 content = transparent ->
            // backdrop; simplest: leave VRAM zero-filled).
            // Mode-2 sprite tables: colors 0x2800, attributes 0x2A00,
            // patterns 0x3000 (the accumulator test's idiom).
            set_register(vdp, 5, static_cast<std::uint8_t>((0x2800 >> 7) | 0x07));
            set_register(vdp, 6, 0x3000 >> 11);
            set_write_address(vdp, 0x2A00);
            vdp.io_write(0x98, 40);   // sprite0 y (visible from line 41)
            vdp.io_write(0x98, 32);   // x
            vdp.io_write(0x98, 4);    // pattern index 4
            vdp.io_write(0x98, 0);
            set_write_address(vdp, 0x2A04);
            vdp.io_write(0x98, 216);  // mode-2 end marker
            set_write_address(vdp, 0x2800);  // sprite0 per-row color entries
            for (int i = 0; i < 16; ++i) {
                vdp.io_write(0x98, 0x0F);  // color 15, no CC/IC/EC
            }
            set_write_address(vdp, 0x3000 + 4 * 8);
            for (int i = 0; i < 8; ++i) {
                vdp.io_write(0x98, 0xFF);  // solid 8x8 pattern rows
            }
            vdp.on_vsync();  // sprite visibility recompute (M22 frame hook)
            return vdp;
        };

        {
            V9958Vdp vdp = build(true);
            const VdpFrameRenderer renderer(vdp);
            const FrameBuffer frame = renderer.render_frame(Field::Progressive);
            const std::uint16_t backdrop = renderer.border_color();
            bool sprite_pixels = false;
            for (int y = 41; y < 49; ++y) {
                for (int x = 32; x < 40; ++x) {
                    if (frame.at(x, y) != backdrop) {
                        sprite_pixels = true;
                    }
                }
            }
            expect(sprite_pixels, "Bl1_SpritesConfigured_SpritePixelsPresent");
        }
        {
            V9958Vdp vdp = build(false);
            const VdpFrameRenderer renderer(vdp);
            const FrameBuffer frame = renderer.render_frame(Field::Progressive);
            expect(all_pixels_equal(frame, renderer.border_color()),
                   "Bl0_SpritesConfigured_PureBackdrop_NoSpritePixels");
        }
    }

    // --- 3. Determinism: two identical BL=0 renders byte-identical. ---
    {
        const FrameBuffer a = render_with_bl(rows[1], false, 0x1234u);
        const FrameBuffer b = render_with_bl(rows[1], false, 0x1234u);
        expect(a.pixels == b.pixels && a.width == b.width && a.height == b.height,
               "TwoRuns_Bl0_ByteIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpFrameRendererBl_Unit cases passed\n";
    return 0;
}
