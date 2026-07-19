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

// Suite: Devices_V9958CommandRowSpritePacing_Unit (M51, DEC-0078,
// docs/m51-planner-package.md fix branch (a) shape (i) / AC-F3)
//
// V9958Vdp::commit_sprite_rows() -- the M51 consumer-side sprite-pacing
// primitive the M44 command-row sink and the render_frame() memoization
// paths call BEFORE any background row is committed. Contract under test
// (grounded in the openMSX consumer-side sync rule, PixelRenderer.cc:580-584 /
// SpriteChecker.hh:242-247, effect only; fMSX's fused per-scanline selection,
// Common.h:99-155, triangulates the same invariant):
//
//   1. THE M51 MECHANISM, reproduced: after commit_sprite_split()'s lazy-open
//      begin_frame() CLEAR, a display line past the render-only sprite
//      watermark reads an EMPTY visible-sprite buffer -- the exact state the
//      pre-M51 command sink sealed into the frame (the scroll-shooter title plane rows
//      committed with visible=0, Task 2 trace).
//   2. THE FIX: commit_sprite_rows(target) repopulates the per-line buffers up
//      to the exclusive `target` from the LIVE state, so the same line then
//      reads the hardware-expected sprite set.
//   3. ADVANCE-ONLY-WHEN-ACTIVE: with the split NOT open the buffers still
//      hold the previous on_vsync() frame-atomic recompute (populated;
//      pre-M49 1-frame-stale fallback contract, package section 2.1) and
//      commit_sprite_rows must NOT clear or advance anything.
//   4. Advance-only watermark (M44 discipline): a lower target never rewinds.
//   5. Wrap-space clamping: a scroll-inverse destination line above the frame
//      height (e.g. 231 on a 192/212-line frame -- the scroll-shooter terrain-strip
//      geometry) clamps to the frame height, pacing every remaining row.
//   6. RENDER-ONLY (DEC-0031): the frame-atomic S#0 collision/5S status the
//      CPU reads is byte-identical across split-open + pacing.
//
// Scene: GRAPHIC4 (sprite mode 2), SCREEN5 sprite-table layout (R#5=0xEF,
// R#11=0 -> attributes 0x7600, colours 0x7400; R#6=0x0F -> patterns 0x7800),
// solid 8x8 sprite 0 at Y=100/X=100 -> display lines 101..108 (fact-sheet
// section 6 line 120: sprite data for line N is fetched during line N-1).

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

// Control register write via the real #99 two-write latch protocol.
void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Single VRAM byte via the real #98 path (14-bit pointer + R#14 page bits).
void write_vram(V9958Vdp& vdp, const std::uint32_t addr, const std::uint8_t value) {
    set_register(vdp, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    vdp.io_write(0x98, value);
}

// GRAPHIC4 + sprite mode 2 scene with `count` solid 8x8 sprites stacked at
// Y=100 (X=100, 110, ...). count=2 makes them overlap -> S#0 C collision in
// the frame-atomic recompute (case 6).
void setup_scene(V9958Vdp& vdp, const int count) {
    set_register(vdp, 0, 0x06);   // GRAPHIC4 (sprite mode 2)
    set_register(vdp, 1, 0x40);   // BL=1, 8x8, mag off
    set_register(vdp, 8, 0x00);   // SPD=0 (sprites enabled)
    set_register(vdp, 5, 0xEF);   // sprite attribute base (SCREEN5 layout)
    set_register(vdp, 11, 0x00);
    set_register(vdp, 6, 0x0F);   // sprite pattern base -> 0x7800
    set_register(vdp, 23, 0x00);  // no vertical scroll
    for (int i = 0; i < count; ++i) {
        const std::uint32_t attr = 0x7600u + static_cast<std::uint32_t>(i) * 4u;
        write_vram(vdp, attr + 0, 100);                                    // Y
        write_vram(vdp, attr + 1, static_cast<std::uint8_t>(100 + i * 4)); // X (overlap)
        write_vram(vdp, attr + 2, 0);                                      // pattern 0
        for (int line = 0; line < 8; ++line) {                             // colour 15
            write_vram(vdp, 0x7400u + static_cast<std::uint32_t>(i) * 16u +
                                static_cast<std::uint32_t>(line), 15);
        }
    }
    write_vram(vdp, 0x7600u + static_cast<std::uint32_t>(count) * 4u, 216);  // sentinel
    for (int row = 0; row < 8; ++row) {
        write_vram(vdp, 0x7800u + static_cast<std::uint32_t>(row), 0xFF);  // solid pattern
    }
    set_register(vdp, 14, 0x00);
}

}  // namespace

int main() {
    // ------------------------------------------------------------------------
    // Cases 1-2: the M51 mechanism and the fix, back to back.
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        setup_scene(vdp, 1);
        vdp.on_vsync();  // frame-atomic recompute populates lines 101..108
        expect(vdp.sprite_engine().visible_sprites(105).size() == 1,
               "Recompute_SpriteAtY100_VisibleOnLine105");

        // Lazy-open the render-only split at an early boundary (target 20):
        // begin_frame() clears ALL per-line buffers, then re-checks only
        // [1,20). Line 105 is now past the watermark -> EMPTY. This is the
        // exact buffer state the pre-M51 command sink composited and sealed.
        vdp.commit_sprite_split(20, false);
        expect(vdp.sprite_engine().watermark() == 20,
               "SplitOpen_Target20_WatermarkAt20");
        expect(vdp.sprite_engine().visible_sprites(105).empty(),
               "LazyOpenClear_Line105PastWatermark_Empty");

        // THE FIX: the command sink paces the sprite pass to its destination
        // boundary before committing -> line 105 reads the hardware-expected
        // set again (same sprite, same X).
        vdp.commit_sprite_rows(150);
        expect(vdp.sprite_engine().watermark() == 150,
               "CommitSpriteRows_Target150_WatermarkAt150");
        expect(vdp.sprite_engine().visible_sprites(105).size() == 1,
               "CommitSpriteRows_Line105Repopulated");
        expect(!vdp.sprite_engine().visible_sprites(105).empty() &&
                   vdp.sprite_engine().visible_sprites(105)[0].x == 100,
               "RepopulatedSprite_CorrectX");

        // Case 4: advance-only -- a lower target never rewinds the watermark.
        vdp.commit_sprite_rows(50);
        expect(vdp.sprite_engine().watermark() == 150,
               "CommitSpriteRows_LowerTarget_AdvanceOnlyNoRewind");

        // Case 5: wrap-space destination (the scroll-shooter geometry: display-line
        // inverse 231 on a 192-line frame) clamps to the frame height.
        vdp.commit_sprite_rows(231);
        expect(vdp.sprite_engine().watermark() == 192,
               "CommitSpriteRows_WrapSpaceTarget231_ClampsToHeight192");
        expect(vdp.sprite_engine().visible_sprites(108).size() == 1,
               "CommitSpriteRows_AfterClamp_LastSpriteLinePopulated");
    }

    // ------------------------------------------------------------------------
    // Case 3: advance-only-WHEN-ACTIVE -- split not open => strict no-op (the
    // buffers keep the previous frame-atomic recompute; stale-populated
    // fallback, never absence).
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        setup_scene(vdp, 1);
        vdp.on_vsync();  // populated + finalized; split NOT active
        const int wm_before = vdp.sprite_engine().watermark();
        vdp.commit_sprite_rows(150);
        expect(vdp.sprite_engine().watermark() == wm_before,
               "SplitInactive_CommitSpriteRows_WatermarkUntouched");
        expect(vdp.sprite_engine().visible_sprites(105).size() == 1,
               "SplitInactive_CommitSpriteRows_RecomputeBuffersPreserved");
    }

    // ------------------------------------------------------------------------
    // Case 6: RENDER-ONLY -- the CPU-visible frame-atomic S#0 (collision C set
    // by two overlapping sprites) is byte-identical across split-open +
    // pacing (DEC-0031; peek_status_register is non-destructive).
    // ------------------------------------------------------------------------
    {
        V9958Vdp vdp;
        setup_scene(vdp, 2);  // overlapping pair -> collision
        vdp.on_vsync();
        const std::uint8_t s0_atomic = vdp.peek_status_register(0);
        expect((s0_atomic & 0x20) != 0, "TwoOverlappingSprites_Recompute_CollisionSet");
        vdp.commit_sprite_split(20, false);
        vdp.commit_sprite_rows(150);
        expect(vdp.peek_status_register(0) == s0_atomic,
               "CommitSpriteRows_RenderOnly_S0ByteIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_V9958CommandRowSpritePacing_Unit cases passed\n";
    return 0;
}
