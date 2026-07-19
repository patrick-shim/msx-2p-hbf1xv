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

// Suite: Devices_SpriteEngineMode1_Unit
//
// Deterministic unit coverage for SpriteEngine's Sprite Mode 1
// (TMS9918-compatible, max 4/line) check/collision/5th-sprite algorithm,
// wired into V9958Vdp::on_vsync() and the S#0/S#3-S#6 status registers.
// Grounded in openMSX 21.0: src/video/SpriteChecker.{hh,cc} --
// never copied (GPL isolation).

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

std::uint8_t read_status_destructive(V9958Vdp& vdp, const std::uint8_t reg) {
    set_register(vdp, 15, reg);
    return vdp.io_read(0x99);
}

// Attribute table at 0 (mode1: 32*4 = 128 bytes), pattern table at 0x800.
// Sentinel Y=208 written just past the last real sprite used in each test
// avoids uninitialized (VRAM=0) "phantom" sprites (also Y=0) contaminating
// the per-line visible-sprite list.
constexpr std::uint16_t kAttribBase = 0;
constexpr std::uint16_t kPatternBase = 0x0800;

void write_sprite(V9958Vdp& vdp, const int index, const std::uint8_t y, const std::uint8_t x,
                   const std::uint8_t pattern_nr, const std::uint8_t color_attrib) {
    const std::uint16_t base = static_cast<std::uint16_t>(kAttribBase + index * 4);
    write_vram(vdp, base + 0, y);
    write_vram(vdp, base + 1, x);
    write_vram(vdp, base + 2, pattern_nr);
    write_vram(vdp, base + 3, color_attrib);
}

void write_sentinel(V9958Vdp& vdp, const int index) {
    write_vram(vdp, static_cast<std::uint16_t>(kAttribBase + index * 4), 208);
}

void write_pattern_row(V9958Vdp& vdp, const std::uint8_t pattern_nr, const int row, const std::uint8_t byte) {
    write_vram(vdp, static_cast<std::uint16_t>(kPatternBase + pattern_nr * 8 + row), byte);
}

}  // namespace

int main() {
    // GRAPHIC1 (base 0x00) is the reset/default mode -> sprite mode 1.
    // Pattern/attrib table base registers default to 0 (matches kAttribBase);
    // R#6 (pattern table) is set explicitly to point at kPatternBase.

    // --- 1-pixel vertical shift: a Y=0 sprite is invisible on output
    //     line 0, first visible on output line 1. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        write_sprite(vdp, 0, /*y=*/0, /*x=*/10, /*pattern_nr=*/0, /*color=*/1);
        write_sentinel(vdp, 1);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        expect(vdp.sprite_engine().visible_sprites(0).empty(), "Mode1_YZero_InvisibleOnLineZero");
        const auto line1 = vdp.sprite_engine().visible_sprites(1);
        expect(line1.size() == 1, "Mode1_YZero_VisibleFromLineOne_Count");
        if (line1.size() == 1) {
            expect(line1[0].x == 10, "Mode1_YZero_VisibleFromLineOne_XUnshifted");
            expect((line1[0].pattern & 0x8000'0000u) != 0, "Mode1_YZero_VisibleFromLineOne_PatternBitSet");
        }
    }

    // --- EC (bit7) applies x -= 32 at check time. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        write_sprite(vdp, 0, /*y=*/0, /*x=*/40, /*pattern_nr=*/0, /*color=*/(0x80 | 1));  // EC set
        write_sentinel(vdp, 1);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        const auto line1 = vdp.sprite_engine().visible_sprites(1);
        expect(line1.size() == 1, "Mode1_EC_Count");
        if (line1.size() == 1) {
            expect(line1[0].x == 40 - 32, "Mode1_EC_XShiftedByMinus32");
        }
    }

    // --- 4-sprites-then-5th-sprite detection, earliest line. Five sprites
    //     share the SAME Y so all overlap on the same set of lines; sprite
    //     index 4 (the 5th) triggers the overflow. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        for (int i = 0; i < 5; ++i) {
            write_sprite(vdp, i, /*y=*/0, static_cast<std::uint8_t>(i * 20), /*pattern_nr=*/0,
                         /*color=*/static_cast<std::uint8_t>(i + 1));
        }
        write_sentinel(vdp, 5);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        const std::uint8_t s0 = static_cast<std::uint8_t>(vdp.peek_status_register(0) & 0x7F);
        expect((s0 & 0x40) != 0, "Mode1_FifthSprite_5SFlagSet");
        expect((s0 & 0x1F) == 4, "Mode1_FifthSprite_NumberIsFour");
        expect(vdp.sprite_engine().visible_sprites(1).size() == 4, "Mode1_FifthSprite_OnlyFourVisible");
    }

    // --- S#0 read clears ONLY bits 7/6/5, leaving the stale
    //     sprite-number field (bits 4-0) intact until the next recompute. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        for (int i = 0; i < 5; ++i) {
            write_sprite(vdp, i, /*y=*/0, static_cast<std::uint8_t>(i * 20), /*pattern_nr=*/0,
                         /*color=*/static_cast<std::uint8_t>(i + 1));
        }
        write_sentinel(vdp, 5);
        write_pattern_row(vdp, 0, 0, 0xFF);
        vdp.on_vsync();

        const std::uint8_t first_read = read_status_destructive(vdp, 0);
        expect((first_read & 0x40) != 0, "Mode1_S0Read_FirstReadShows5S");
        const std::uint8_t after = static_cast<std::uint8_t>(vdp.peek_status_register(0) & 0x7F);
        expect(after == 0x04, "Mode1_S0Read_ClearsOnlyBits765_KeepsStaleNumber");
    }

    // --- Collision detection + the fixed +12/+8 coordinate offsets. Two
    //     fully-overlapping solid 8x8 sprites at (0,0). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        write_sprite(vdp, 0, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/1);
        write_sprite(vdp, 1, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/2);
        write_sentinel(vdp, 2);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        const std::uint8_t s0 = static_cast<std::uint8_t>(vdp.peek_status_register(0) & 0x7F);
        expect((s0 & 0x20) != 0, "Mode1_Collision_CFlagSet");
        expect(vdp.peek_status_register(3) == 12, "Mode1_Collision_XOffsetPlus12");
        expect(vdp.peek_status_register(5) == (1 + 8), "Mode1_Collision_YOffsetPlus8");
    }

    // --- Color-0/TP collision-eligibility interaction: two overlapping
    //     color-0 sprites do NOT collide when TP is disabled (R#8 bit5
    //     clear, the default/reset value -> canSpriteColor0Collide==false). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        write_sprite(vdp, 0, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/0);
        write_sprite(vdp, 1, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/0);
        write_sentinel(vdp, 2);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        expect((vdp.peek_status_register(0) & 0x20) == 0, "Mode1_Color0_TPDefault_NoCollision");
    }
    {
        // TP bit SET (R#8 bit5): color-0 sprites now CAN collide.
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(vdp, 6, kPatternBase >> 11);
        set_register(vdp, 8, 0x20);
        write_sprite(vdp, 0, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/0);
        write_sprite(vdp, 1, /*y=*/0, /*x=*/0, /*pattern_nr=*/0, /*color=*/0);
        write_sentinel(vdp, 2);
        write_pattern_row(vdp, 0, 0, 0xFF);

        vdp.on_vsync();
        expect((vdp.peek_status_register(0) & 0x20) != 0, "Mode1_Color0_TPSet_CollisionOccurs");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 1, 0x40);
            set_register(*vdp, 6, kPatternBase >> 11);
            write_sprite(*vdp, 0, 0, 0, 0, 1);
            write_sprite(*vdp, 1, 0, 0, 0, 2);
            write_sentinel(*vdp, 2);
            write_pattern_row(*vdp, 0, 0, 0xFF);
            vdp->on_vsync();
        }
        expect(vdp_a.peek_status_register(0) == vdp_b.peek_status_register(0), "Mode1_Determinism_S0Identical");
        expect(vdp_a.peek_status_register(3) == vdp_b.peek_status_register(3), "Mode1_Determinism_S3Identical");
        expect(vdp_a.peek_status_register(5) == vdp_b.peek_status_register(5), "Mode1_Determinism_S5Identical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
