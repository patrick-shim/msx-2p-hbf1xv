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

// Suite: Devices_SpriteEnginePerLineState_Unit  (M49-S1, backlog D9)
//
// Proves the progressive SpriteEngine::check_until() ENGINE itself resolves
// sprite state PER-LINE-LIVE -- the actual D9 fix -- independently of the S2
// seam wiring (which is a later slice and NOT exercised here). The harness
// binds a mutable control-register file via begin_frame(), advances the
// watermark to a split line K, MUTATES the bound registers (R#23 vertical
// scroll / R#1 BL display-enable / R#8 SPD sprite-enable), then advances the
// watermark to the frame bottom. The lines BEFORE K must reflect the OLD
// register state and the lines AT/AFTER K the NEW state -- exactly the split
// behavior split-screen HUD titles need for a fixed HUD over a scrolling
// playfield. Grounding: tier-1 fact-sheet Yamaha V9958 VDP.md §6 line 120
// (per-scanline sprite fetch), §7/§10 (R#23 live per-line display offset).
//
// Non-tautology: a CONTROL engine driven with the register left UNCHANGED for
// the whole frame shows the sprite exactly where the "old-state" lines expect
// AND on the after-K lines the split engine suppressed -- so the difference is
// provably caused by the mid-frame register mutation, not by geometry.

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

#include "devices/video/sprite_engine.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace {

using sony_msx::devices::video::decode_vdp_mode;
using sony_msx::devices::video::SpriteEngine;
using sony_msx::devices::video::VdpVram;

int g_failures = 0;

void expect(const bool condition, const std::string& case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Mode 1 sprite table at 0, pattern at 0x0800.
void m1_sprite(VdpVram& v, int i, std::uint8_t y, std::uint8_t x, std::uint8_t pat, std::uint8_t col) {
    const std::uint32_t base = static_cast<std::uint32_t>(i) * 4u;
    v.write(base + 0, y);
    v.write(base + 1, x);
    v.write(base + 2, pat);
    v.write(base + 3, col);
}
void m1_sentinel(VdpVram& v, int i) { v.write(static_cast<std::uint32_t>(i) * 4u, 208); }
void m1_pattern_all(VdpVram& v, std::uint8_t pat, std::uint8_t byte) {
    for (int r = 0; r < 16; ++r)
        v.write(0x0800u + static_cast<std::uint32_t>(pat) * 8u + static_cast<std::uint32_t>(r), byte);
}

std::array<std::uint8_t, 32> mode1_regs() {
    std::array<std::uint8_t, 32> r{};
    r[1] = 0x40;          // display enable, size8, mag off
    r[6] = 0x0800 >> 11;  // sprite pattern base -> 0x0800
    return r;
}

constexpr int kHeight = 212;

bool line_has_sprite(const SpriteEngine& e, int line) { return !e.visible_sprites(line).empty(); }

}  // namespace

int main() {
    // ------------------------------------------------------------------------
    // 1) R#23 per-line-LIVE vertical scroll. Sprite Y=50, 8x8. With R#23=0 the
    //    sprite shows on output lines 51..58; with R#23=100 the wrapped Y offset
    //    puts it on lines 207..214. Split at K=55: lines [1,55) checked with
    //    R#23=0, lines [55,H) with R#23=100.
    // ------------------------------------------------------------------------
    {
        VdpVram vram;
        m1_sprite(vram, 0, /*y=*/50, /*x=*/40, /*pat=*/0, /*col=*/1);
        m1_sentinel(vram, 1);
        m1_pattern_all(vram, 0, 0xFF);

        auto regs = mode1_regs();
        const auto mode = decode_vdp_mode(0x00, regs[1], 0x00);

        SpriteEngine e;
        e.begin_frame(vram, std::span<const std::uint8_t, 32>(regs), mode, kHeight);
        regs[23] = 0;
        e.check_until(54);            // commit lines [1,55) with R#23=0
        expect(e.watermark() == 55, "R23: watermark at split");
        regs[23] = 100;              // mid-frame vertical-scroll rewrite
        e.check_until(kHeight - 1);   // commit lines [55,H) with R#23=100

        // Old-state region (R#23=0): sprite on 51..54, but NOT on 55..58 (those
        // lines were committed under R#23=100 where the sprite is elsewhere).
        expect(line_has_sprite(e, 51), "R23: line 51 present (old scroll)");
        expect(line_has_sprite(e, 54), "R23: line 54 present (old scroll)");
        expect(!line_has_sprite(e, 55), "R23: line 55 absent (new scroll took effect)");
        expect(!line_has_sprite(e, 58), "R23: line 58 absent (new scroll took effect)");
        // New-state region (R#23=100): sprite on 207..214.
        expect(line_has_sprite(e, 207), "R23: line 207 present (new scroll)");
        expect(line_has_sprite(e, 211), "R23: line 211 present (new scroll)");

        // Control engine: SAME frame, R#23 left at 0 the whole time -> the
        // after-split lines 55..58 DO carry the sprite (proving the split
        // engine's absence there was caused by the R#23 mutation).
        auto ctrl_regs = mode1_regs();
        ctrl_regs[23] = 0;
        SpriteEngine ctrl;
        ctrl.recompute_frame(vram, std::span<const std::uint8_t, 32>(ctrl_regs), mode, kHeight);
        expect(line_has_sprite(ctrl, 55), "R23-control: line 55 present when scroll never changed");
        expect(line_has_sprite(ctrl, 58), "R23-control: line 58 present when scroll never changed");
        expect(!line_has_sprite(ctrl, 207), "R23-control: line 207 absent when scroll never changed");
    }

    // ------------------------------------------------------------------------
    // 2) R#1 BL (display-enable) per-line-LIVE gate. Sprite Y=100 -> visible on
    //    lines 101..108. Enable for lines [1,105), then CLEAR R#1 bit6 (blank).
    //    Lines 101..104 keep the sprite; lines 105..108 are blank (gated off),
    //    even though the sprite geometrically covers them.
    // ------------------------------------------------------------------------
    {
        VdpVram vram;
        m1_sprite(vram, 0, /*y=*/100, /*x=*/40, /*pat=*/0, /*col=*/1);
        m1_sentinel(vram, 1);
        m1_pattern_all(vram, 0, 0xFF);

        auto regs = mode1_regs();       // R#1 bit6 set (display on)
        const auto mode = decode_vdp_mode(0x00, regs[1], 0x00);

        SpriteEngine e;
        e.begin_frame(vram, std::span<const std::uint8_t, 32>(regs), mode, kHeight);
        e.check_until(104);             // lines [1,105) with display ON
        regs[1] = 0x00;                 // BL cleared -> display blank
        e.check_until(kHeight - 1);     // lines [105,H) with display OFF

        expect(line_has_sprite(e, 101), "BL: line 101 present (display on)");
        expect(line_has_sprite(e, 104), "BL: line 104 present (display on)");
        expect(!line_has_sprite(e, 105), "BL: line 105 blank (display gated off)");
        expect(!line_has_sprite(e, 108), "BL: line 108 blank (display gated off)");

        // Control: display ON the whole frame -> lines 105..108 DO carry it.
        SpriteEngine ctrl;
        auto ctrl_regs = mode1_regs();
        ctrl.recompute_frame(vram, std::span<const std::uint8_t, 32>(ctrl_regs), mode, kHeight);
        expect(line_has_sprite(ctrl, 105), "BL-control: line 105 present when display stays on");
        expect(line_has_sprite(ctrl, 108), "BL-control: line 108 present when display stays on");
    }

    // ------------------------------------------------------------------------
    // 3) R#8 SPD (sprite-disable) per-line-LIVE gate. Same sprite; enable
    //    sprites for lines [1,105), then SET R#8 bit1 (SPD) -> sprites off.
    // ------------------------------------------------------------------------
    {
        VdpVram vram;
        m1_sprite(vram, 0, /*y=*/100, /*x=*/40, /*pat=*/0, /*col=*/1);
        m1_sentinel(vram, 1);
        m1_pattern_all(vram, 0, 0xFF);

        auto regs = mode1_regs();       // R#8 bit1 clear (sprites enabled)
        const auto mode = decode_vdp_mode(0x00, regs[1], 0x00);

        SpriteEngine e;
        e.begin_frame(vram, std::span<const std::uint8_t, 32>(regs), mode, kHeight);
        e.check_until(104);             // lines [1,105) with sprites ENABLED
        regs[8] = 0x02;                 // SPD set -> sprites disabled
        e.check_until(kHeight - 1);     // lines [105,H) with sprites DISABLED

        expect(line_has_sprite(e, 101), "SPD: line 101 present (sprites on)");
        expect(line_has_sprite(e, 104), "SPD: line 104 present (sprites on)");
        expect(!line_has_sprite(e, 105), "SPD: line 105 empty (SPD gated off)");
        expect(!line_has_sprite(e, 108), "SPD: line 108 empty (SPD gated off)");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
