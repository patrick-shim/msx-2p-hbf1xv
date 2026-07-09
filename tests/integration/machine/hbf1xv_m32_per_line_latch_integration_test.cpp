// Suite: Machine_Hbf1xvM32PerLineLatch_Integration (M32-S2, Defect A of
// DEC-0039, docs/m32-planner-package.md §3-S2 / test matrix row 3)
//
// Machine-level L+1 latch rule (§2.3): a hooked VDP write while the raster
// is on display line L takes effect from line L+1 -- lines <= L keep the
// pre-write state (the openMSX sync-before-change protocol at LINE
// granularity, references/openmsx-21.0/src/video/PixelRenderer.cc:253-394,
// 549-571). Cases:
//   1. R#23 rewrite at display line 100 => sealed frame rows [0,101) at the
//      old scroll, rows [101,height) at the new scroll (EXACT split -- no
//      CPU latency is involved when the write is injected directly at a
//      known raster position via V9958Vdp::io_write()).
//   2. The same write during vblank/border => the WHOLE frame renders at
//      the new scroll.
//   3. render_frame() at the boundary == the finalized frame, and repeated
//      calls return identical bytes.
//   4. debug_io_write() NEVER fires the render-sync hook (§2.3 documented
//      exclusion): the identical mid-display write through the debug seam
//      produces NO split -- the whole frame renders from final state
//      (legacy snapshot semantics preserved for debug-built scenes).
//   5. Two-machine determinism (byte-identical sealed frames).
//
// Oracles are LEGACY-RENDERER REFERENCES: full-frame renders of twin
// machines with the same VRAM and a single static R#23 value -- the M32
// design's own untouched in-test reference (§2.2).

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
// GRAPHIC4 with R#9 LN=0: 192 active lines, 70 non-active lines after the
// vsync origin (fact-sheet §7; v9958_vdp.cpp raster_display_line()).
constexpr std::uint64_t kNonActiveLines = 70;

void debug_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Hooked (real) register write through the VDP's own io_write -- the same
// entry point a CPU OUT reaches over the IoBus.
void hooked_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.vdp().io_write(0x99, value);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Per-row-unique GRAPHIC4 content: row r byte c = (r + c) & 0xFF, rows
// 0..255 (the full 32 KB page 0), so every output row identifies its source
// row and scroll offsets are machine-checkable.
void fill_signature_vram(Hbf1xvMachine& m) {
    m.debug_io_write(0x99, 0x00);
    m.debug_io_write(0x99, 0x40);  // VRAM write address 0
    for (int r = 0; r < 256; ++r) {
        for (int c = 0; c < 128; ++c) {
            m.debug_io_write(0x98, static_cast<std::uint8_t>((r + c) & 0xFF));
        }
    }
}

// Static G4 scene shared by the live machine and the references.
void setup_scene(Hbf1xvMachine& m, const std::uint8_t r23) {
    m.cold_boot();
    debug_set_register(m, 0, 0x06);  // GRAPHIC4
    debug_set_register(m, 1, 0x40);  // M34: R#1 BL=1 (display enable) -- the
                                     // render gate blanks BL=0 lines
    debug_set_register(m, 8, 0x02);  // SPD: sprites disabled
    fill_signature_vram(m);
    debug_set_register(m, 23, r23);
}

FrameBuffer reference_frame(const std::uint8_t r23) {
    Hbf1xvMachine ref;
    setup_scene(ref, r23);
    return ref.render_frame();  // never stepped/finalized -> pure live projection
}

bool row_equals(const FrameBuffer& a, const FrameBuffer& b, const int y) {
    for (int x = 0; x < a.width; ++x) {
        if (a.at(x, y) != b.at(x, y)) {
            return false;
        }
    }
    return true;
}

// Runs the mid-display-write scenario; `via_debug_seam` selects the write
// path (hooked VDP io_write vs the machine debug seam).
FrameBuffer run_scenario(const bool via_debug_seam) {
    Hbf1xvMachine m;
    setup_scene(m, 0);
    // Advance the raster to display line 100 (no CPU needed -- the raster
    // derives from scheduler cycles since the vsync origin).
    m.run_cycles((kNonActiveLines + 100) * kCyclesPerLine + 30);
    if (via_debug_seam) {
        debug_set_register(m, 23, 64);
    } else {
        hooked_set_register(m, 23, 64);
    }
    m.run_cycles(kFrameCycles - ((kNonActiveLines + 100) * kCyclesPerLine + 30));
    m.on_vsync_boundary();
    return m.render_frame();
}

}  // namespace

int main() {
    const FrameBuffer ref0 = reference_frame(0);
    const FrameBuffer ref64 = reference_frame(64);
    expect(ref0.width == 256 && ref0.height == 192, "Reference_Graphic4Dimensions");
    expect(ref0.pixels != ref64.pixels, "Reference_ScrollValues_ProduceDistinctFrames");

    // --- 1. The L+1 latch rule: hooked write at display line 100. ---
    {
        const FrameBuffer fb = run_scenario(false);
        expect(fb.width == ref0.width && fb.height == ref0.height, "Split_Dimensions");
        bool top_old = true;
        for (int y = 0; y <= 100 && top_old; ++y) {
            top_old = row_equals(fb, ref0, y);
        }
        expect(top_old, "HookedWriteAtLine100_RowsThrough100_KeepPreWriteScroll");
        bool bottom_new = true;
        for (int y = 101; y < fb.height && bottom_new; ++y) {
            bottom_new = row_equals(fb, ref64, y);
        }
        expect(bottom_new, "HookedWriteAtLine100_RowsFrom101_UseNewScroll");
    }

    // --- 2. Vblank/border write affects the whole frame. ---
    {
        Hbf1xvMachine m;
        setup_scene(m, 0);
        m.run_cycles(30 * kCyclesPerLine);  // raster in the border region
        hooked_set_register(m, 23, 64);
        m.run_cycles(kFrameCycles - 30 * kCyclesPerLine);
        m.on_vsync_boundary();
        const FrameBuffer fb = m.render_frame();
        expect(fb.pixels == ref64.pixels, "VblankWrite_WholeFrame_NewScroll");
    }

    // --- 3. render_frame() at the boundary == finalized frame; repeated
    //     calls identical. ---
    {
        Hbf1xvMachine m;
        setup_scene(m, 0);
        m.run_cycles(kFrameCycles);
        m.on_vsync_boundary();
        const FrameBuffer first = m.render_frame();
        const FrameBuffer second = m.render_frame();
        expect(first.pixels == second.pixels && first.width == second.width &&
                   first.height == second.height && first.border_color == second.border_color,
               "BoundaryRender_RepeatedCalls_ByteIdentical");
        expect(first.pixels == ref0.pixels, "BoundaryRender_EqualsFinalizedFrame");
    }

    // --- 4. debug_io_write never fires the hook: same scenario through the
    //     debug seam => NO split (whole frame from final state). ---
    {
        const FrameBuffer fb = run_scenario(true);
        expect(fb.pixels == ref64.pixels,
               "DebugSeamWriteAtLine100_NoHook_WholeFrameFinalState_NoSplit");
    }

    // --- 5. Two-machine determinism. ---
    {
        const FrameBuffer a = run_scenario(false);
        const FrameBuffer b = run_scenario(false);
        expect(a.pixels == b.pixels, "Determinism_TwoRuns_ByteIdenticalSplitFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM32PerLineLatch_Integration cases passed\n";
    return 0;
}
