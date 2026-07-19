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

// Suite: Machine_Hbf1xvBlLatch_Integration
//
// Machine-level L+2 latch for the R#1 BL (display enable) bit (a re-derived
// rule: openMSX's LINE renderer rounds the render-sync point with the
// active-display left margin, PixelRenderer.cc, so a mid-frame write
// lands one display line later than the raw scanline boundary -- calibrated
// against openMSX 19.1 Sony_HB-F1XV raw scroll-shooter gameplay captures, where the top-HUD
// BL-band raster split lands exactly on openMSX's scanline; DEC-0043,
// DEC-0060). Driven through the
// REAL seam (V9958Vdp::io_write on #99 -- the exact entry point a CPU OUT
// reaches over the IoBus; the hbf1xv_per_line_latch precedent):
//   1. BL 1->0 written while the raster is on display line L: the sealed
//      frame shows CONTENT rows [0, L+1] and BACKDROP rows [L+2, height).
//   2. BL 0->1 written at display line M: BACKDROP rows [0, M+1], CONTENT
//      rows [M+2, height).
//   3. debug_io_write arm: the identical mid-display BL write through the
//      machine's non-perturbing debug seam fires NO render-sync hook -- the
//      whole frame renders from final state (the documented debug-seam
//      exclusion).
//   4. Two-run determinism (byte-identical sealed split frames).
//
// Grounding: openMSX 21.0: src/video/VDP.cc (an R#1 bit6 change schedules
// syncAtNextLine, and syncSetBlank applies the change at the NEXT line --
// corroborating the per-line L+1 latch at line granularity) and
// src/video/PixelRenderer.cc (a BL=0 line renders DRAW_BORDER); fMSX 6.0:
// source/MSX.h + Common.h (per-line
// `if(!ScreenON) ClearLine(background)`). Oracles are LEGACY-RENDERER
// references (full-frame renders of never-stepped twin machines with a
// static BL value -- the same in-test oracle pattern as the per-line-latch
// test).

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
// GRAPHIC4, R#9 LN=0: 192 active lines, 70 non-active lines after the vsync
// origin (the per-line-latch test's own constant).
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

// Per-row-unique GRAPHIC4 signature content (row r byte c = (r+c) & 0xFF).
void fill_signature_vram(Hbf1xvMachine& m) {
    m.debug_io_write(0x99, 0x00);
    m.debug_io_write(0x99, 0x40);
    for (int r = 0; r < 256; ++r) {
        for (int c = 0; c < 128; ++c) {
            m.debug_io_write(0x98, static_cast<std::uint8_t>((r + c) & 0xFF));
        }
    }
}

// Static G4 scene; `bl` selects the initial display-enable state.
void setup_scene(Hbf1xvMachine& m, const bool bl) {
    m.cold_boot();
    debug_set_register(m, 0, 0x06);  // GRAPHIC4
    debug_set_register(m, 1, bl ? 0x40 : 0x00);
    debug_set_register(m, 8, 0x02);  // SPD: sprites disabled
    debug_set_register(m, 7, 0x05);  // backdrop = palette 5
    fill_signature_vram(m);
}

FrameBuffer reference_frame(const bool bl) {
    Hbf1xvMachine ref;
    setup_scene(ref, bl);
    return ref.render_frame();  // never stepped -> pure live projection
}

bool row_equals(const FrameBuffer& a, const FrameBuffer& b, const int y) {
    for (int x = 0; x < a.width; ++x) {
        if (a.at(x, y) != b.at(x, y)) {
            return false;
        }
    }
    return true;
}

// Mid-display BL write scenario: start at `initial_bl`, write `new_r1` at
// display line `line`, seal the frame. `via_debug_seam` selects the path.
FrameBuffer run_scenario(const bool initial_bl, const std::uint8_t new_r1, const int line,
                         const bool via_debug_seam) {
    Hbf1xvMachine m;
    setup_scene(m, initial_bl);
    m.run_cycles((kNonActiveLines + static_cast<std::uint64_t>(line)) * kCyclesPerLine + 30);
    if (via_debug_seam) {
        debug_set_register(m, 1, new_r1);
    } else {
        hooked_set_register(m, 1, new_r1);
    }
    m.run_cycles(kFrameCycles -
                 ((kNonActiveLines + static_cast<std::uint64_t>(line)) * kCyclesPerLine + 30));
    m.on_vsync_boundary();
    return m.render_frame();
}

}  // namespace

int main() {
    const FrameBuffer ref_on = reference_frame(true);    // content everywhere
    const FrameBuffer ref_off = reference_frame(false);  // backdrop everywhere
    expect(ref_on.width == 256 && ref_on.height == 192, "Reference_Graphic4Dimensions");
    expect(ref_on.pixels != ref_off.pixels, "Reference_BlStates_ProduceDistinctFrames");

    // --- 1. Disable direction: BL 1->0 at display line 100 (in-line tstate
    //     30) => content through row 101, backdrop from row 102 (L+2). ---
    {
        const FrameBuffer fb = run_scenario(true, 0x00, 100, false);
        bool top_content = true;
        for (int y = 0; y <= 101 && top_content; ++y) {
            top_content = row_equals(fb, ref_on, y);
        }
        expect(top_content, "BlDisableAtLine100_RowsThrough101_KeepContent");
        bool bottom_backdrop = true;
        for (int y = 102; y < fb.height && bottom_backdrop; ++y) {
            bottom_backdrop = row_equals(fb, ref_off, y);
        }
        expect(bottom_backdrop, "BlDisableAtLine100_RowsFrom102_PureBackdrop");
    }

    // --- 2. Re-enable direction: BL 0->1 at display line 64 (in-line tstate
    //     30) => backdrop through row 65, content from row 66 (L+2). ---
    {
        const FrameBuffer fb = run_scenario(false, 0x40, 64, false);
        bool top_backdrop = true;
        for (int y = 0; y <= 65 && top_backdrop; ++y) {
            top_backdrop = row_equals(fb, ref_off, y);
        }
        expect(top_backdrop, "BlEnableAtLine64_RowsThrough65_PureBackdrop");
        bool bottom_content = true;
        for (int y = 66; y < fb.height && bottom_content; ++y) {
            bottom_content = row_equals(fb, ref_on, y);
        }
        expect(bottom_content, "BlEnableAtLine64_RowsFrom66_Content");
    }

    // --- 3. debug_io_write seam exclusion: the identical BL 1->0 write via
    //     the debug seam produces NO split -- whole frame at final (BL=0)
    //     state. ---
    {
        const FrameBuffer fb = run_scenario(true, 0x00, 100, true);
        expect(fb.pixels == ref_off.pixels,
               "DebugSeamBlWrite_NoHook_WholeFrameFinalState_NoSplit");
    }

    // --- 4. Two-run determinism. ---
    {
        const FrameBuffer a = run_scenario(true, 0x00, 100, false);
        const FrameBuffer b = run_scenario(true, 0x00, 100, false);
        expect(a.pixels == b.pixels, "Determinism_TwoRuns_ByteIdenticalSplitFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvBlLatch_Integration cases passed\n";
    return 0;
}
