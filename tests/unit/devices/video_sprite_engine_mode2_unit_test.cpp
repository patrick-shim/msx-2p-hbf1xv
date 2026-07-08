// Suite: Devices_SpriteEngineMode2_Unit
//
// Deterministic unit coverage for M22-S2: SpriteEngine's Sprite Mode 2
// (MSX2+, max 8/line, per-line color/attribute table, CC/IC bit semantics)
// check/collision/9th-sprite algorithm. Pixel compositing (CC-cascade merge,
// GRAPHIC5 half-pixel case, IC-still-drawn) is covered separately in
// tests/unit/devices/video_vdp_frame_renderer_sprites_unit_test.cpp. Grounded
// in references/openmsx-21.0/src/video/SpriteChecker.cc:262-480 -- never
// copied (GPL isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"

namespace {

using sony_msx::devices::video::V9958Vdp;

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

// Attribute-table layout (A-M22-15): per-line color/attrib sub-table at
// offset 0 (attrib_base + sprite*16 + spriteLine); Y/X/pattern sub-table at
// the FIXED +512-byte offset (attrib_base + 512 + sprite*4 + {0,1,2}).
constexpr std::uint16_t kAttribBase = 0;
constexpr std::uint16_t kYxpBase = 512;
constexpr std::uint16_t kPatternBase = 0x1000;

void write_yxp(V9958Vdp& vdp, const int index, const std::uint8_t y, const std::uint8_t x,
                const std::uint8_t pattern_nr) {
    const std::uint16_t base = static_cast<std::uint16_t>(kYxpBase + index * 4);
    write_vram(vdp, base + 0, y);
    write_vram(vdp, base + 1, x);
    write_vram(vdp, base + 2, pattern_nr);
}

void write_color(V9958Vdp& vdp, const int index, const int line, const std::uint8_t color_attrib) {
    write_vram(vdp, static_cast<std::uint16_t>(kAttribBase + index * 16 + line), color_attrib);
}

void write_sentinel(V9958Vdp& vdp, const int index) {
    write_vram(vdp, static_cast<std::uint16_t>(kYxpBase + index * 4), 216);
}

void write_pattern_row(V9958Vdp& vdp, const std::uint8_t pattern_nr, const int row, const std::uint8_t byte) {
    write_vram(vdp, static_cast<std::uint16_t>(kPatternBase + pattern_nr * 8 + row), byte);
}

void select_graphic4(V9958Vdp& vdp) {
    set_register(vdp, 0, 0x06);  // GRAPHIC4 (base 0x0C) -> sprite mode 2, non-planar
    set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
    // R#5 low 3 bits are AND-mask bits in sprite mode 2, NOT base bits
    // (VDP.cc:1357-1371: addr = ((R11<<15)|(R5<<7)|0x7F) & (~0x3FF|index));
    // real software always sets them (BIOS SCREEN5 R#5=0xEF, Metal Gear
    // R#5=0xE7). 0x07 -> 1KB-aligned table at 0x0000: colors at 0-511,
    // Y/X/pattern at 512-1023 -- this file's exact kAttribBase/kYxpBase
    // layout. Leaving R#5=0 (the pre-fix version of this test) would force
    // address bits A9-A7 to zero and alias every table read (see docs/
    // sprite-invisibility-investigation.md).
    set_register(vdp, 5, 0x07);
    set_register(vdp, 6, static_cast<std::uint8_t>(kPatternBase >> 11));
}

}  // namespace

int main() {
    // --- 8-sprites-then-9th-sprite: nine sprites share Y=0 (all overlap on
    //     the same lines); sprite index 8 (the 9th) triggers the overflow,
    //     recorded in the SAME S#0 bits6/4-0 field as mode 1's 5th-sprite. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        for (int i = 0; i < 9; ++i) {
            write_yxp(vdp, i, /*y=*/0, static_cast<std::uint8_t>(i * 20), /*pattern_nr=*/0);
            write_color(vdp, i, /*line=*/0, static_cast<std::uint8_t>(i + 1));
        }
        write_sentinel(vdp, 9);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        const std::uint8_t s0 = static_cast<std::uint8_t>(vdp.peek_status_register(0) & 0x7F);
        expect((s0 & 0x40) != 0, "Mode2_NinthSprite_5SFlagSet");
        expect((s0 & 0x1F) == 8, "Mode2_NinthSprite_NumberIsEight");
        expect(vdp.sprite_engine().visible_sprites(1).size() == 8, "Mode2_NinthSprite_OnlyEightVisible");
    }

    // --- Per-line (not per-sprite) color: the SAME sprite shows a DIFFERENT
    //     color_attrib on two different output lines. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_yxp(vdp, 0, /*y=*/0, /*x=*/5, /*pattern_nr=*/0);
        write_color(vdp, 0, /*line=*/0, /*color=*/1);  // used by output line 1 (spriteLine 0)
        write_color(vdp, 0, /*line=*/1, /*color=*/2);  // used by output line 2 (spriteLine 1)
        write_sentinel(vdp, 1);

        vdp.on_vsync();
        const auto line1 = vdp.sprite_engine().visible_sprites(1);
        const auto line2 = vdp.sprite_engine().visible_sprites(2);
        expect(line1.size() == 1 && line2.size() == 1, "Mode2_PerLineColor_BothLinesVisible");
        if (line1.size() == 1 && line2.size() == 1) {
            expect((line1[0].color_attrib & 0x0F) == 1, "Mode2_PerLineColor_LineOneColorOne");
            expect((line2[0].color_attrib & 0x0F) == 2, "Mode2_PerLineColor_LineTwoColorTwo");
        }
    }

    // --- A-M22-11: IC (bit5) excludes a sprite from COLLISION detection
    //     only. Two fully-overlapping sprites, one with IC=1, must NOT
    //     register a collision. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_yxp(vdp, 0, /*y=*/0, /*x=*/0, /*pattern_nr=*/0);
        write_color(vdp, 0, 0, /*color=*/1);
        write_yxp(vdp, 1, /*y=*/0, /*x=*/0, /*pattern_nr=*/0);
        write_color(vdp, 1, 0, /*color=*/(0x20 | 2));  // IC set
        write_sentinel(vdp, 2);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        expect((vdp.peek_status_register(0) & 0x20) == 0, "Mode2_IC_ExcludesCollision");
    }

    // --- CC-cascade fields survive into the visible-sprite buffer (the
    //     actual pixel-merge behavior is tested at the compositing level). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_yxp(vdp, 0, /*y=*/0, /*x=*/0, /*pattern_nr=*/0);
        write_color(vdp, 0, 0, /*color=*/1);  // CC=0
        write_yxp(vdp, 1, /*y=*/0, /*x=*/0, /*pattern_nr=*/0);
        write_color(vdp, 1, 0, /*color=*/(0x40 | 3));  // CC=1
        write_sentinel(vdp, 2);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        const auto line1 = vdp.sprite_engine().visible_sprites(1);
        expect(line1.size() == 2, "Mode2_CC_BothSpritesCounted");
        if (line1.size() == 2) {
            expect((line1[0].color_attrib & 0x40) == 0, "Mode2_CC_FirstSpriteCcClear");
            expect((line1[1].color_attrib & 0x40) != 0, "Mode2_CC_SecondSpriteCcSet");
        }
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            select_graphic4(*vdp);
            write_yxp(*vdp, 0, 0, 0, 0);
            write_color(*vdp, 0, 0, 1);
            write_yxp(*vdp, 1, 0, 0, 0);
            write_color(*vdp, 1, 0, 2);
            write_sentinel(*vdp, 2);
            write_pattern_row(*vdp, 0, 0, 0xFF);
            vdp->on_vsync();
        }
        expect(vdp_a.peek_status_register(0) == vdp_b.peek_status_register(0), "Mode2_Determinism_S0Identical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
