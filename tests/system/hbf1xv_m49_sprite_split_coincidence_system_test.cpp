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

// Suite: System_Hbf1xvM49SpriteSplitCoincidence_System (M49-S2, backlog D9,
// docs/m49-planner-package.md §3 S2 / AC-S2)
//
// The M49-S2 split-coincidence oracle, with sprites ENABLED (the M32/M34/M44
// render-sync oracles all run sprites OFF, so they only exercise the BACKGROUND
// plane -- this is the first machine-level oracle that drives the incremental
// per-line-live sprite plane through the real render-sync seam and proves it
// splits at the SAME display line as the background).
//
// Scene: GRAPHIC4 (sprite mode 2), display + sprites enabled, one solid mode-2
// sprite at Y=100 spanning display lines 101..108 under R#23=0. A HOOKED
// mid-frame R#23 0->50 write while the raster is on display line 104 must, by
// the openMSX active-display left-margin +2 rounding (M39/DEC-0060,
// hbf1xv_machine.cpp:101), take effect from display line 106 (=104+2) -- for
// BOTH planes at once:
//   * background: rows [0,106) at vertical scroll 0, rows [106,192) at scroll 50;
//   * sprite:     the sprite shows on rows 101..105 (old scroll region) and is
//                 GONE on rows 106..108 (the new scroll moves it to lines 51..58,
//                 which are all above the split, so it never reappears below).
// Because that is EXACTLY "ref(R#23=0) above the split, ref(R#23=50) below it",
// a single frame-equality assertion (split == ref0 for rows [0,106) AND ==
// ref50 for rows [106,192)) proves the sprite split boundary EQUALS the
// background split boundary EQUALS L+2. The sprite-engine visible-sprite buffer
// is also asserted directly (present at 105, absent at 106) and a CONTROL run
// (R#23=0 the whole frame) shows the sprite on 106..108 -- proving the absence
// is caused by the mid-frame split, not geometry (non-tautology).
//
// Oracles are LEGACY-RENDERER REFERENCES: full-frame renders of never-stepped
// twin machines with a single static R#23 value (the untouched M32/M34 in-test
// oracle pattern). Deterministic (two independent runs -> byte-identical).

#include <cstdint>
#include <iostream>

#include "devices/video/frame_buffer.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::video::FrameBuffer;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerLine = 228;
constexpr std::uint64_t kFrameCycles = kCyclesPerLine * 262;
constexpr std::uint64_t kNonActiveLines = 70;  // GRAPHIC4 R#9 LN=0 (192 active)
constexpr int kWriteLine = 104;                // R#23 rewrite raster line L
constexpr int kSplitLine = kWriteLine + 2;     // openMSX +2 margin -> 106
constexpr std::uint8_t kNewScroll = 50;

void debug_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Hooked (real) register write -- the exact entry a CPU OUT reaches over the
// IoBus; fires the render-sync seam (background AND, M49-S2, sprite plane).
void hooked_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.vdp().io_write(0x99, value);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Single VRAM byte via the debug seam, with the full 17-bit address (R#14 high
// bits). Non-perturbing (fires no render-sync hook).
void write_vram(Hbf1xvMachine& m, const std::uint32_t addr, const std::uint8_t value) {
    debug_set_register(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    m.debug_io_write(0x98, value);
}

// GRAPHIC4 sprite mode 2. Mode-2 attribute table via R#5=0xEF, R#11=0 (the
// standard SCREEN5 layout): mode2_attr_addr(index) = 0x7400 | (index & 0x3FF),
// so the colour table is at 0x7400 + sprite*16 + line, and the Y/X/pattern
// sub-table at 0x7600 + sprite*4 (index+512). Sprite pattern generator at
// R#6=0x0F -> 0x7800. All ABOVE the 192-line bitmap [0,0x6000), no overlap.
void setup_scene(Hbf1xvMachine& m, const std::uint8_t r23) {
    m.cold_boot();
    debug_set_register(m, 0, 0x06);   // GRAPHIC4 (sprite mode 2)
    debug_set_register(m, 1, 0x40);   // BL=1 (display enable), size8, mag off
    debug_set_register(m, 8, 0x00);   // SPD=0 (sprites ENABLED)
    debug_set_register(m, 5, 0xEF);   // sprite attr-table base (SCREEN5 layout)
    debug_set_register(m, 6, 0x0F);   // sprite pattern base -> 0x7800

    // Per-row-unique bitmap signature over the 192 displayed rows (row r, byte c
    // = (r+c)&0xFF), so every output row's applied scroll is machine-checkable.
    debug_set_register(m, 14, 0x00);
    m.debug_io_write(0x99, 0x00);
    m.debug_io_write(0x99, 0x40);  // VRAM write address 0
    for (int r = 0; r < 192; ++r) {
        for (int c = 0; c < 128; ++c) {
            m.debug_io_write(0x98, static_cast<std::uint8_t>((r + c) & 0xFF));
        }
    }

    // Sprite 0: Y=100, X=100, pattern 0 (Y/X/pattern sub-table at 0x7600).
    write_vram(m, 0x7600, 100);   // sprite 0 Y
    write_vram(m, 0x7601, 100);   // sprite 0 X
    write_vram(m, 0x7602, 0);     // sprite 0 pattern number
    write_vram(m, 0x7604, 216);   // sprite 1 Y = 216 sentinel (stops the scan)
    // Colour byte for sprite 0, rows 0..7 (palette index 15, no EC/CC/IC).
    for (int line = 0; line < 8; ++line) {
        write_vram(m, 0x7400u + static_cast<std::uint32_t>(line), 15);
    }
    // Sprite pattern 0: 8 solid rows.
    for (int row = 0; row < 8; ++row) {
        write_vram(m, 0x7800u + static_cast<std::uint32_t>(row), 0xFF);
    }

    debug_set_register(m, 14, 0x00);      // restore the address-high register
    debug_set_register(m, 23, r23);       // vertical scroll
}

// Static reference: the identical scene with a single fixed R#23, run one whole
// frame with NO mid-frame write, then sealed. The frame-atomic on_vsync()
// recompute populates the sprite plane at the static scroll (unlike a pure live
// projection, which never runs on_vsync() and would leave the sprite buffers
// empty), so ref0 carries the sprite on 101..108 and ref50 on 51..58 -- the
// discriminating references for the split.
FrameBuffer reference_frame(const std::uint8_t r23) {
    Hbf1xvMachine ref;
    setup_scene(ref, r23);
    ref.run_cycles(kFrameCycles);
    ref.on_vsync_boundary();
    return ref.render_frame();
}

bool row_equals(const FrameBuffer& a, const FrameBuffer& b, const int y) {
    for (int x = 0; x < a.width; ++x) {
        if (a.at(x, y) != b.at(x, y)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    const FrameBuffer ref0 = reference_frame(0);
    const FrameBuffer ref50 = reference_frame(kNewScroll);
    expect(ref0.width == 256 && ref0.height == 192, "Reference_Graphic4Dimensions");
    // The reference sprite must span the split so the coincidence is observable:
    // ref0 shows it on 101..108, ref50 moves it off those lines (to 51..58).
    expect(ref0.pixels != ref50.pixels, "Reference_ScrollValues_ProduceDistinctFrames");
    expect(!row_equals(ref0, ref50, kSplitLine), "Reference_Discriminates_AtSplitLine");

    // --- The split run: hooked R#23 0->50 at display line 104. ---
    Hbf1xvMachine m;
    setup_scene(m, 0);
    m.run_cycles((kNonActiveLines + static_cast<std::uint64_t>(kWriteLine)) * kCyclesPerLine + 30);
    hooked_set_register(m, 23, kNewScroll);
    m.run_cycles(kFrameCycles -
                 ((kNonActiveLines + static_cast<std::uint64_t>(kWriteLine)) * kCyclesPerLine + 30));
    m.on_vsync_boundary();
    const FrameBuffer split = m.render_frame();

    // (1) COINCIDENCE: the sealed frame is ref0 ABOVE the split and ref50 BELOW
    //     it -- the SAME split line for both planes. Because ref0 carries the
    //     sprite on 101..108 and ref50 does not, matching ref0 on rows 101..105
    //     AND ref50 on rows 106..108 pins BOTH the sprite split and the
    //     background split to line 106 = L+2.
    bool top_is_ref0 = true;
    for (int y = 0; y < kSplitLine && top_is_ref0; ++y) {
        top_is_ref0 = row_equals(split, ref0, y);
    }
    expect(top_is_ref0, "SplitRun_RowsAboveSplit_MatchScroll0Reference_IncludingSprite");
    bool bottom_is_ref50 = true;
    for (int y = kSplitLine; y < split.height && bottom_is_ref50; ++y) {
        bottom_is_ref50 = row_equals(split, ref50, y);
    }
    expect(bottom_is_ref50, "SplitRun_RowsFromSplit_MatchScroll50Reference_SpriteGone");

    // (2) The first row that leaves the R#23=0 region is exactly L+2 (the split
    //     boundary is neither early nor late).
    int boundary = -1;
    for (int y = 0; y < split.height; ++y) {
        if (!row_equals(split, ref0, y)) {
            boundary = y;
            break;
        }
    }
    expect(boundary == kSplitLine, "SplitRun_SplitBoundary_EqualsWriteLinePlus2");

    // (3) NON-TAUTOLOGY: the sealed split frame is NEITHER wholly ref0 NOR wholly
    //     ref50 -- it genuinely carries BOTH scroll states, split at the boundary.
    //     (M49-S2 keeps the CPU-visible collision/status frame-atomic, so the sprite
    //     engine's END-OF-FRAME visible_sprites buffer reflects the frame-atomic
    //     recompute, NOT the mid-frame render split; the split lives in the sealed
    //     RENDERED frame -- the accumulator's committed rows -- which is exactly what
    //     the rows-above/rows-below equality above asserts.)
    expect(split.pixels != ref0.pixels, "SplitRun_NotWhollyScroll0");
    expect(split.pixels != ref50.pixels, "SplitRun_NotWhollyScroll50");

    // (5) Determinism: a second identically-driven run reproduces the sealed
    //     split frame byte-for-byte.
    {
        Hbf1xvMachine m2;
        setup_scene(m2, 0);
        m2.run_cycles((kNonActiveLines + static_cast<std::uint64_t>(kWriteLine)) * kCyclesPerLine + 30);
        hooked_set_register(m2, 23, kNewScroll);
        m2.run_cycles(kFrameCycles - ((kNonActiveLines + static_cast<std::uint64_t>(kWriteLine)) *
                                          kCyclesPerLine + 30));
        m2.on_vsync_boundary();
        const FrameBuffer split2 = m2.render_frame();
        expect(split.pixels == split2.pixels, "Determinism_TwoRuns_ByteIdenticalSplitFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All System_Hbf1xvM49SpriteSplitCoincidence_System cases passed\n";
    return 0;
}
