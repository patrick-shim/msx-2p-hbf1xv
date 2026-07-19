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

// Suite: Machine_Hbf1xvCommandBeamClamp_Integration
//
// THE BEAM-CLAMP DEFECT SCENARIO, machine-level -- the racing-title mechanism
// in miniature. The command-row sink (VdpRenderSyncAdapter::on_commit_up_to)
// used to commit [watermark, display_line) even when the command's
// destination rows lay far AHEAD of the render beam, sealing every swept row
// with the COMMIT-TIME state: the frame-start R#26/R#27 horizontal-scroll
// pair (F1's straight-road flash) and the instantaneous sprite buffers (F1's
// vanishing car). On real hardware a command writing VRAM rows ahead of the
// beam has NO early-display effect -- each display line is generated when
// the beam scans it, from the register file and sprite state live AT THAT
// LINE:
//   - tier 1: Yamaha V9958 VDP fact sheet §6 (per-scanline
//     display model; sprite data for line N fetched during line N-1);
//   - tier 2: openMSX renders strictly the BACKLOG on every command write --
//     renderUntil(current time), never ahead ("Subsystem synchronisation
//     should happen before the commit, to be able to draw backlog using old
//     state", openMSX 21.0: src/video/VDPVRAM.hh,
//     PixelRenderer.cc) -- EFFECT re-expressed, never copied.
// The fix clamps the sink's sweep to the io-write-seam beam boundary
// (raster + 2, the same calibrated margin on_before_state_change uses): rows
// ahead of the beam are left for the beam path and render per-line-live.
// (DEC-0091-AMENDMENT-A)
//
// Scene: GRAPHIC4, R#23=0, a horizontally-striped page-0 canvas (colour
// changes every 8 px) so R#26 coarse horizontal scroll is observable. At
// beam line 20 a hooked HMMV fills page-rows [150,160) -- destination rows
// far AHEAD of the beam (pre-fix: the sink sealed display rows [22,150)+
// with the then-current registers). At beam line 100 a hooked R#26=4 write
// scrolls the screen. Hardware/openMSX contract for the sealed frame:
//   rows [0, 102)  = UNSCROLLED (committed on the beam path before the
//                    R#26 write applied -- backlog, pre-change state);
//   rows [102,150) = SCROLLED   (the beam had not reached them when R#26
//                    changed -- per-line-live; PRE-FIX these were sealed
//                    UNSCROLLED at the blit instant = the F1 flash);
//   rows [150,160) = SCROLLED command fill (the blit destination itself
//                    renders with the THEN-CURRENT scroll -- F1's dashboard);
//   rows [160,192) = SCROLLED canvas.
//
// Cases:
//   A1. Backlog rows [0,102) keep the pre-change (unscrolled) state.
//   A2. Ahead-of-beam rows [102,150) render with the LATER per-line-live
//       R#26 (FAILS pre-fix: sealed with the blit-time scroll).
//   A3. The command's own destination rows [150,160) render with the later
//       scroll too (FAILS pre-fix).
//   A4. Below-band rows [160,192) scrolled (sanity, passes pre+post).
//   B.  Sprite half (F1's car): a hooked SAT write AFTER an ahead-of-beam
//       blit must still show the sprite on its band -- the band renders from
//       the beam-time SAT, not the blit-time SAT (FAILS pre-fix: the sprite-
//       pacing sink sealed the band from the not-yet-written SAT). Whole sealed
//       frame byte-identical to the static reference.
//   C.  Determinism: two identically-driven runs, byte-identical frames.

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

constexpr int kBlitBeamLine = 20;     // raster line at the HMMV issue
constexpr int kScrollBeamLine = 100;  // raster line at the R#26 write
constexpr int kScrollSplitLine = kScrollBeamLine + 2;  // seam boundary (raster + 2 margin)
constexpr unsigned kCmdDy = 150;      // page-rows [150,160) -> display rows (R#23=0)
constexpr unsigned kCmdNy = 10;
// Case A fills only the LEFT half of each destination row (NX=128): a
// full-row solid fill would be horizontally scroll-INVARIANT and make the
// destination-row assertion (A3) vacuous. Case B fills the whole row.
constexpr unsigned kCmdNxHalf = 128;
constexpr unsigned kCmdNxFull = 256;
constexpr std::uint8_t kCmdCol = 0xDD;   // colour-13 pixel pair (outside canvas colours)
constexpr std::uint8_t kScrollR26 = 4;   // 4 * 8 = 32-dot coarse scroll

void set_reg(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Hooked control-register write (fires the render-sync seam, as a real OUT).
void set_reg_hooked(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.vdp().io_write(0x99, value);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Single VRAM byte via the debug seam (no render-sync side effects).
void write_vram(Hbf1xvMachine& m, const std::uint32_t addr, const std::uint8_t value) {
    set_reg(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    m.debug_io_write(0x98, value);
}

// Single VRAM byte via the HOOKED path (fires the seam -- a real CPU write).
void write_vram_hooked(Hbf1xvMachine& m, const std::uint32_t addr, const std::uint8_t value) {
    set_reg_hooked(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    m.vdp().io_write(0x98, value);
}

// Fill GRAPHIC4 page-rows [row0,row1) with the horizontally-striped canvas
// (colour = 1 + (8-px block % 12) in both nibbles) via the debug seam.
void fill_striped_rows(Hbf1xvMachine& m, const int row0, const int row1) {
    const std::uint32_t addr = static_cast<std::uint32_t>(row0) * 128u;
    set_reg(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    for (int row = row0; row < row1; ++row) {
        for (int b = 0; b < 128; ++b) {
            const std::uint8_t colour = static_cast<std::uint8_t>(1 + ((b / 4) % 12));
            m.debug_io_write(0x98, static_cast<std::uint8_t>(colour * 0x11));
        }
    }
    set_reg(m, 14, 0x00);
}

// Fill page-rows [row0,row1) with a solid byte via the debug seam.
void fill_rows(Hbf1xvMachine& m, const int row0, const int row1, const std::uint8_t byte) {
    const std::uint32_t addr = static_cast<std::uint32_t>(row0) * 128u;
    set_reg(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    for (int i = 0; i < (row1 - row0) * 128; ++i) {
        m.debug_io_write(0x98, byte);
    }
    set_reg(m, 14, 0x00);
}

void setup_scene_a(Hbf1xvMachine& m) {
    m.cold_boot();
    set_reg(m, 0, 0x06);   // GRAPHIC4
    set_reg(m, 1, 0x40);   // BL=1
    set_reg(m, 8, 0x02);   // SPD=1: sprites disabled (case A isolates registers)
    set_reg(m, 23, 0x00);  // no vertical scroll
    set_reg(m, 26, 0x00);  // horizontal scroll home
    set_reg(m, 27, 0x00);
    fill_striped_rows(m, 0, 256);
}

// HMMV atomic fill (DX=0) via the HOOKED path: parameters through the
// debug seam, the R#46 CMD write through vdp().io_write -> fires the seam and
// the per-destination-row sink (the command-render-sync and sprite-pacing
// tests' exact recipe).
void issue_hmmv_hooked(Hbf1xvMachine& m, const unsigned dy, const unsigned ny,
                       const unsigned nx, const std::uint8_t col) {
    set_reg(m, 32 + 4, 0x00);
    set_reg(m, 32 + 5, 0x00);
    set_reg(m, 32 + 6, static_cast<std::uint8_t>(dy & 0xFF));
    set_reg(m, 32 + 7, static_cast<std::uint8_t>((dy >> 8) & 0x03));
    set_reg(m, 32 + 8, static_cast<std::uint8_t>(nx & 0xFF));
    set_reg(m, 32 + 9, static_cast<std::uint8_t>((nx >> 8) & 0x01));
    set_reg(m, 32 + 10, static_cast<std::uint8_t>(ny & 0xFF));
    set_reg(m, 32 + 11, static_cast<std::uint8_t>((ny >> 8) & 0x03));
    set_reg(m, 32 + 12, col);
    set_reg(m, 32 + 13, 0x00);
    m.vdp().io_write(0x99, 0xC0);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | 46));
}

// Static-VRAM reference frames (never stepped -> pure live projection, the
// established in-test legacy-renderer oracle pattern).
// Paint the case-A command band statically: the LEFT half (bytes 0..63 = px
// 0..127) of page-rows [row0,row1) -- exactly the HMMV NX=128 footprint.
void fill_left_half_rows(Hbf1xvMachine& m, const int row0, const int row1,
                         const std::uint8_t byte) {
    for (int row = row0; row < row1; ++row) {
        const std::uint32_t addr = static_cast<std::uint32_t>(row) * 128u;
        set_reg(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
        m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
        m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
        for (int b = 0; b < 64; ++b) {
            m.debug_io_write(0x98, byte);
        }
    }
    set_reg(m, 14, 0x00);
}

FrameBuffer reference_frame_a(const bool scrolled, const bool paint_band) {
    Hbf1xvMachine m;
    setup_scene_a(m);
    if (paint_band) {
        fill_left_half_rows(m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy),
                            kCmdCol);
    }
    if (scrolled) {
        set_reg(m, 26, kScrollR26);
    }
    return m.render_frame();
}

// The case-A scenario: blit ahead of the beam at line 20, scroll write at
// line 100, seal the frame.
FrameBuffer run_scenario_a() {
    Hbf1xvMachine m;
    setup_scene_a(m);
    const std::uint64_t blit_at =
        (kNonActiveLines + static_cast<std::uint64_t>(kBlitBeamLine)) * kCyclesPerLine + 30;
    const std::uint64_t scroll_at =
        (kNonActiveLines + static_cast<std::uint64_t>(kScrollBeamLine)) * kCyclesPerLine + 30;
    m.run_cycles(blit_at);
    // Destination rows AHEAD of beam; NX=128 (left half) keeps the band
    // horizontally asymmetric so the scroll assertions are load-bearing.
    issue_hmmv_hooked(m, kCmdDy, kCmdNy, kCmdNxHalf, kCmdCol);
    m.run_cycles(scroll_at - blit_at);
    set_reg_hooked(m, 26, kScrollR26);               // the F1 per-line-scroll analog
    m.run_cycles(kFrameCycles - scroll_at);
    m.on_vsync_boundary();
    return m.render_frame();
}

bool row_equals(const FrameBuffer& a, const FrameBuffer& b, const int y) {
    for (int x = 0; x < a.width; ++x) {
        if (a.at(x, y) != b.at(x, y)) {
            return false;
        }
    }
    return true;
}

bool rows_equal(const FrameBuffer& a, const FrameBuffer& b, const int y0, const int y1,
                int* first_bad = nullptr) {
    for (int y = y0; y < y1; ++y) {
        if (!row_equals(a, b, y)) {
            if (first_bad != nullptr) {
                *first_bad = y;
            }
            return false;
        }
    }
    return true;
}

// --- Case B (sprite half) scene: GRAPHIC4 + sprite mode 2, SCREEN5 sprite
// tables, uniform background; the SAT starts EMPTY (sentinel at entry 0). ---

constexpr std::uint8_t kBg = 0x11;
constexpr int kSpriteTop = 100;  // SAT Y=99 -> display lines 100..107
constexpr int kSpriteBot = 107;

void setup_scene_b(Hbf1xvMachine& m, const bool with_sprite) {
    m.cold_boot();
    set_reg(m, 0, 0x06);   // GRAPHIC4 (sprite mode 2)
    set_reg(m, 1, 0x40);   // BL=1, 8x8, mag off
    set_reg(m, 8, 0x00);   // SPD=0: sprites ENABLED
    set_reg(m, 5, 0xEF);   // sprite attribute base (SCREEN5 layout) -> 0x7600
    set_reg(m, 11, 0x00);
    set_reg(m, 6, 0x0F);   // sprite pattern base -> 0x7800
    set_reg(m, 23, 0x00);
    fill_rows(m, 0, 256, kBg);
    for (int line = 0; line < 8; ++line) {
        write_vram(m, 0x7400u + static_cast<std::uint32_t>(line), 15);  // colour 15
    }
    for (int row = 0; row < 8; ++row) {
        write_vram(m, 0x7800u + static_cast<std::uint32_t>(row), 0xFF);  // solid pattern
    }
    if (with_sprite) {
        write_vram(m, 0x7600, 99);   // Y -> display lines 100..107
        write_vram(m, 0x7601, 100);  // X
        write_vram(m, 0x7602, 0);    // pattern 0
        write_vram(m, 0x7604, 216);  // sentinel stops the scan
    } else {
        write_vram(m, 0x7600, 216);  // empty SAT (sentinel first)
    }
    set_reg(m, 14, 0x00);
}

// Reference: sprite present statically, command band pre-painted, sealed with
// no mid-frame hooked activity (the sprite-pacing test's reference pattern --
// frame-atomic recompute carries the sprite on 100..107).
FrameBuffer reference_frame_b() {
    Hbf1xvMachine m;
    setup_scene_b(m, /*with_sprite=*/true);
    fill_rows(m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy), kCmdCol);
    m.run_cycles(kFrameCycles);
    m.on_vsync_boundary();
    return m.render_frame();
}

// The case-B scenario: SAT empty; split opened + ahead-of-beam blit at line
// 20 (pre-fix: seals rows [22,150) -- including the sprite band -- from the
// STILL-EMPTY SAT); the game then writes the sprite into the SAT at line 40,
// well before the beam reaches the band (F1 writes its car sprites after its
// dashboard blit, before the road band is scanned).
FrameBuffer run_scenario_b() {
    Hbf1xvMachine m;
    setup_scene_b(m, /*with_sprite=*/false);
    const std::uint64_t blit_at =
        (kNonActiveLines + static_cast<std::uint64_t>(kBlitBeamLine)) * kCyclesPerLine + 30;
    const std::uint64_t sat_at =
        (kNonActiveLines + static_cast<std::uint64_t>(40)) * kCyclesPerLine + 30;
    m.run_cycles(blit_at);
    set_reg_hooked(m, 23, 0x00);  // harmless hooked touch: opens the render split
    issue_hmmv_hooked(m, kCmdDy, kCmdNy, kCmdNxFull, kCmdCol);
    m.run_cycles(sat_at - blit_at);
    write_vram_hooked(m, 0x7600, 99);   // Y
    write_vram_hooked(m, 0x7601, 100);  // X
    write_vram_hooked(m, 0x7602, 0);    // pattern 0
    write_vram_hooked(m, 0x7604, 216);  // sentinel
    m.run_cycles(kFrameCycles - sat_at);
    m.on_vsync_boundary();
    return m.render_frame();
}

}  // namespace

int main() {
    // --- Case A references + non-triviality guards. ---
    const FrameBuffer ref_plain = reference_frame_a(/*scrolled=*/false, /*paint_band=*/false);
    const FrameBuffer ref_scrolled = reference_frame_a(/*scrolled=*/true, /*paint_band=*/false);
    const FrameBuffer ref_scrolled_band =
        reference_frame_a(/*scrolled=*/true, /*paint_band=*/true);
    expect(ref_plain.width == 256 && ref_plain.height == 192, "ReferenceA_Graphic4Dimensions");
    expect(!row_equals(ref_plain, ref_scrolled, 50),
           "ReferenceA_R26Scroll_ChangesRowContent_NonTriviality");
    expect(!row_equals(ref_scrolled, ref_scrolled_band, 155),
           "ReferenceA_CommandBand_DiffersFromCanvas_NonTriviality");

    // --- Cases A1-A4: the register half (F1's straight-road flash). ---
    {
        const FrameBuffer fb = run_scenario_a();
        expect(fb.width == 256 && fb.height == 192, "ScenarioA_Dimensions");
        int bad = -1;

        // A1: backlog rows committed on the beam path BEFORE the R#26 write
        // applied keep the pre-change (unscrolled) state.
        expect(rows_equal(fb, ref_plain, 0, kScrollSplitLine, &bad),
               "A1_BacklogRows0to101_PreChangeUnscrolled");
        if (bad >= 0) {
            std::cerr << "  A1 first differing row: " << bad << "\n";
        }

        // A2: rows AHEAD of the beam at blit time -- inside the pre-fix
        // sealed sweep [22,150) -- must render with the LATER per-line-live
        // R#26 (the fix assertion; FAILS pre-fix).
        bad = -1;
        expect(rows_equal(fb, ref_scrolled, kScrollSplitLine, static_cast<int>(kCmdDy), &bad),
               "A2_AheadOfBeamRows102to149_RenderPerLineLive_ScrolledR26");
        if (bad >= 0) {
            std::cerr << "  A2 first differing row: " << bad << "\n";
        }

        // A3: the blit's own destination rows render with the then-current
        // scroll -- F1's dashboard (FAILS pre-fix).
        bad = -1;
        expect(rows_equal(fb, ref_scrolled_band, static_cast<int>(kCmdDy),
                          static_cast<int>(kCmdDy + kCmdNy), &bad),
               "A3_CommandDestinationRows150to159_ScrolledWithBeamTimeRegisters");
        if (bad >= 0) {
            std::cerr << "  A3 first differing row: " << bad << "\n";
        }

        // A4: below the band -- scrolled canvas (sanity; passes pre+post).
        bad = -1;
        expect(rows_equal(fb, ref_scrolled, static_cast<int>(kCmdDy + kCmdNy), fb.height, &bad),
               "A4_BelowBandRows_ScrolledCanvas_Sanity");
        if (bad >= 0) {
            std::cerr << "  A4 first differing row: " << bad << "\n";
        }
    }

    // --- Case B: the sprite half (F1's vanishing car). ---
    {
        const FrameBuffer ref_b = reference_frame_b();
        // Non-triviality: the reference really carries the sprite band.
        {
            bool differs = false;
            for (int x = 0; x < ref_b.width; ++x) {
                if (ref_b.at(x, 103) != ref_b.at(x, 90)) {
                    differs = true;
                    break;
                }
            }
            expect(differs, "ReferenceB_SpriteBandRow_DiffersFromBackgroundRow");
        }
        const FrameBuffer fb = run_scenario_b();
        int bad = -1;
        expect(rows_equal(fb, ref_b, kSpriteTop, kSpriteBot + 1, &bad),
               "B_SpriteBand100to107_PresentFromBeamTimeSat_DespiteAheadOfBeamBlit");
        if (bad >= 0) {
            std::cerr << "  B first differing row: " << bad << "\n";
        }
        expect(fb.pixels == ref_b.pixels, "B_SealedFrame_ByteIdentical_ToStaticReference");
    }

    // --- Case C: determinism. ---
    {
        const FrameBuffer a = run_scenario_a();
        const FrameBuffer b = run_scenario_a();
        expect(a.pixels == b.pixels, "Determinism_TwoRuns_ByteIdenticalSealedFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvCommandBeamClamp_Integration cases passed\n";
    return 0;
}
