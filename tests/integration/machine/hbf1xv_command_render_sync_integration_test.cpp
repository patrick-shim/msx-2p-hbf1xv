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

// Suite: Machine_Hbf1xvCommandRenderSync_Integration
//
// The V9958 command engine's ATOMIC bitmap writes are routed through the
// scanline accumulator at per-destination-row granularity (the openMSX
// cmdWrite -> notify -> renderUntil analog;
// openMSX 21.0: src/video/VDPVRAM.hh).
//
// BEAM-CLAMP RE-DERIVATION (spec-grounded, no assertion weakened -- the
// contract FLIPPED to the hardware-true one and the pre-clamp behavior now
// FAILS this
// test): the original case 1(b)/(c) pinned the sink sealing rows AHEAD of the
// render beam with the command's commit-time content. That early seal is NOT
// hardware behavior -- a command writing VRAM rows the beam has not reached
// has NO early-display effect; each display line is generated when the beam
// scans it, from the VRAM/register/SAT state live at that line (tier 1:
// Yamaha V9958 VDP fact sheet §6 per-scanline model; tier 2:
// openMSX renders strictly the BACKLOG on every command write --
// renderUntil(current time), never ahead: "Subsystem synchronisation should
// happen before the commit, to be able to draw backlog using old state",
// VDPVRAM.hh, PixelRenderer.cc). The early seal was the
// racing-title defect (frame-start R#26/#27 sealed into ~145 rows -> the
// racing-view flash). The sink is now
// clamped to the beam+2 seam boundary, and THIS test pins the clamped
// contract; the per-line-live positive assertions (rows ahead of the beam
// render with their beam-time register/SAT state) live in
// hbf1xv_command_beam_clamp_integration_test.cpp.
// (DEC-0065; beam clamp re-derived under DEC-0091-AMENDMENT-A)
//
// Deterministic isolation trick (unchanged): after a hooked command,
// OVERWRITE the whole bitmap page through the machine's debug_io_write seam
// (which fires NEITHER the render-sync seam NOR the command-row sink -- the
// documented debug-seam exclusion). Rows committed on the beam path BEFORE the overwrite keep the
// pre-overwrite content; rows ahead of the beam materialize at finalize()
// from the overwritten VRAM -- which is exactly what the beam would display
// (the overwrite stands in for "later VRAM state at the row's beam time").
//
// Cases:
//   1. BEAM CLAMP: a command whose unswept destination rows lie AHEAD
//      of the beam leaves NO early-display trace -- above the beam+2
//      watermark the sealed rows show the LATER (overwritten) VRAM, not the
//      command's commit-time content; below/at the watermark the rows keep
//      the pre-command backlog (WRAP guard -- already-swept rows). The
//      sealed frame is byte-identical to the no-command reference. The
//      pre-clamp sink (destination-keyed early seal) FAILS this case -- the
//      adversarial arm is the recorded pre-fix run.
//   2. Active-display gate: the identical command issued while the beam is
//      in VBLANK commits nothing -> whole frame renders from final
//      (overwritten) VRAM, byte-identical to the reference.
//   3. Visible-page gate: a command to a NON-displayed page falls back ->
//      sealed frame byte-identical to the no-sink reference.
//   4. Two-machine determinism.

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

// Deterministic scene geometry.
constexpr std::uint8_t kR23 = 30;      // vertical scroll -> the mapping inverse
constexpr std::uint8_t kBg = 0x11;     // background pixel byte (color 1)
constexpr std::uint8_t kCmd = 0x22;    // command fill pixel byte (color 2)
constexpr std::uint8_t kOverwrite = 0x33;  // post-command debug overwrite (color 3)
constexpr int kBeamLine = 60;          // raster display line at command issue
constexpr int kWatermark = kBeamLine + 2;  // io_write-seam commit boundary (L+2)

// Command destination page-rows (DY..DY+NY) chosen so the display-line inverse
// L = (page_row - R#23) & 0xFF STRADDLES the beam+2 watermark:
//   page-rows [85,102) -> display lines [55,72).
// Below the watermark (lines 62..70) commit the command color per-row; the
// above-watermark rows (lines 55..61) are already swept (wrap guard).
constexpr unsigned kCmdDy = 85;
constexpr unsigned kCmdNy = 17;  // rows 85..101 -> display lines 55..71
constexpr int kBandTopLine = kWatermark;  // 62: first below-beam committed line
constexpr int kBandBotLine = 71;          // last sink-committed line is 70 (exclusive 71)

void set_reg(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

void set_cmd_reg(Hbf1xvMachine& m, const int index, const std::uint8_t value) {
    set_reg(m, static_cast<std::uint8_t>(32 + index), value);
}

// Fill VRAM page-rows [row0,row1) (GRAPHIC4: 128 bytes/row, page 0) with `byte`,
// through the debug seam (no render-sync side effects). R#14 (the A16..A14
// page bits) is reset to 0 first -- a prior 32 KB fill leaves R#14 advanced, so
// without this a fill starting at the 14-bit pointer would land in a later page.
void fill_rows(Hbf1xvMachine& m, const int row0, const int row1, const std::uint8_t byte) {
    set_reg(m, 14, 0x00);
    const std::uint16_t addr = static_cast<std::uint16_t>(row0 * 128);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    for (int i = 0; i < (row1 - row0) * 128; ++i) {
        m.debug_io_write(0x98, byte);
    }
}

void setup_scene(Hbf1xvMachine& m) {
    m.cold_boot();
    set_reg(m, 0, 0x06);   // GRAPHIC4
    set_reg(m, 1, 0x40);   // BL=1 (display enable)
    set_reg(m, 8, 0x02);   // SPD: sprites disabled
    set_reg(m, 23, kR23);  // vertical scroll
    fill_rows(m, 0, 256, kBg);  // whole page 0 = background
}

// A static-VRAM reference frame (never stepped -> pure live projection, the
// M32 in-test legacy-renderer oracle). `band_color`, when non-zero, paints the
// command's destination page-rows [kCmdDy, kCmdDy+kCmdNy).
FrameBuffer reference_frame(const std::uint8_t page_byte, const bool paint_band,
                            const std::uint8_t band_byte) {
    Hbf1xvMachine m;
    setup_scene(m);
    fill_rows(m, 0, 256, page_byte);
    if (paint_band) {
        fill_rows(m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy), band_byte);
    }
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

// Issue an HMMV atomic fill (DX=0, NX=256, COL=`col`) to DY=`dy`, NY=`ny`
// through the HOOKED VDP io_write path (fires the seam + the M44 row sink).
void issue_hmmv_hooked(Hbf1xvMachine& m, const unsigned dy, const unsigned ny,
                       const std::uint8_t col) {
    // Command registers via the debug seam (no commit); only the R#46 write is
    // hooked, so exactly one io_write-seam commit precedes the command.
    set_cmd_reg(m, 4, 0x00);  // DX low
    set_cmd_reg(m, 5, 0x00);  // DX high
    set_cmd_reg(m, 6, static_cast<std::uint8_t>(dy & 0xFF));
    set_cmd_reg(m, 7, static_cast<std::uint8_t>((dy >> 8) & 0x03));
    set_cmd_reg(m, 8, 0x00);  // NX low (256 -> 0x100)
    set_cmd_reg(m, 9, 0x01);  // NX high
    set_cmd_reg(m, 10, static_cast<std::uint8_t>(ny & 0xFF));
    set_cmd_reg(m, 11, static_cast<std::uint8_t>((ny >> 8) & 0x03));
    set_cmd_reg(m, 12, col);   // COL
    set_cmd_reg(m, 13, 0x00);  // ARG: DIX=0/DIY=0 (top-to-bottom, left-to-right)
    // R#46 CMD = 0xC0 (HMMV) -- HOOKED so the seam + per-row sink fire.
    m.vdp().io_write(0x99, 0xC0);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | 46));
}

// Run the headline scenario. `beam_line` places the raster; `dy`/`ny` pick the
// command band; returns the sealed frame after a full-page overwrite + finalize.
FrameBuffer run_scenario(const int beam_line, const unsigned dy, const unsigned ny,
                         const std::uint8_t cmd_col, const std::uint8_t overwrite_col) {
    Hbf1xvMachine m;
    setup_scene(m);
    const std::uint64_t issue_cycle =
        (kNonActiveLines + static_cast<std::uint64_t>(beam_line >= 0 ? beam_line : 0)) *
            kCyclesPerLine + 30;
    // For a vblank beam (beam_line < 0) advance only into the border region.
    const std::uint64_t advance = (beam_line < 0) ? (20 * kCyclesPerLine + 30) : issue_cycle;
    m.run_cycles(advance);
    issue_hmmv_hooked(m, dy, ny, cmd_col);
    // Overwrite the WHOLE displayed page through the non-hooked debug seam:
    // rows the sink already committed are immune; the rest finalize from this.
    fill_rows(m, 0, 256, overwrite_col);
    m.run_cycles(kFrameCycles - advance);
    m.on_vsync_boundary();
    return m.render_frame();
}

// No-command / no-sink reference: identical to run_scenario but the command is
// replaced by a HARMLESS hooked register touch, so the io_write render-sync SEAM
// fires identically (commit [0, beam+2) from pre-overwrite VRAM) while the M44
// per-row sink NEVER runs. This is the exact pre-fix sealed frame for a beam-60
// scene: rows [0,62)=background, rows [62,192)=overwrite.
FrameBuffer run_reference_no_command(const int beam_line, const std::uint8_t overwrite_col) {
    Hbf1xvMachine m;
    setup_scene(m);
    const std::uint64_t advance =
        (kNonActiveLines + static_cast<std::uint64_t>(beam_line)) * kCyclesPerLine + 30;
    m.run_cycles(advance);
    // Harmless HOOKED touch: re-write R#8 (SPD) to its current value -> the seam
    // commits [0, beam+2) with no command and no per-row sink.
    m.vdp().io_write(0x99, 0x02);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | 8));
    fill_rows(m, 0, 256, overwrite_col);
    m.run_cycles(kFrameCycles - advance);
    m.on_vsync_boundary();
    return m.render_frame();
}

}  // namespace

int main() {
    // Reference frames (pure live projection of static VRAM).
    const FrameBuffer frame_bg = reference_frame(kBg, false, 0);
    const FrameBuffer frame_over = reference_frame(kOverwrite, false, 0);
    // The command's own content at its destination display lines (page-rows
    // [85,102) = command color over the overwrite background, so rows below the
    // band still read the overwrite color -- matching the sealed frame's
    // below-band region).
    const FrameBuffer frame_cmd_band = reference_frame(kOverwrite, true, kCmd);

    expect(frame_bg.width == 256 && frame_bg.height == 192, "Reference_Graphic4Dimensions");
    // Sanity: the three colors render distinctly at the band.
    expect(!row_equals(frame_cmd_band, frame_over, kBandTopLine),
           "Reference_CommandColor_DiffersFromOverwrite");
    expect(!row_equals(frame_bg, frame_over, kBandTopLine),
           "Reference_Background_DiffersFromOverwrite");

    // --- 1. M62 BEAM CLAMP: no early-display trace ahead of the beam. ---
    {
        const FrameBuffer fb = run_scenario(kBeamLine, kCmdDy, kCmdNy, kCmd, kOverwrite);
        expect(fb.width == 256 && fb.height == 192, "PerRow_Dimensions");

        // (a) At/below the beam+2 watermark: rows keep the PRE-COMMAND
        //     background (the beam-path backlog committed at the R#46 seam
        //     before the command wrote anything; the WRAP guard skips the
        //     already-swept destination rows 55..61).
        bool above_pre_command = true;
        for (int y = 0; y < kBandTopLine; ++y) {
            above_pre_command = above_pre_command && row_equals(fb, frame_bg, y);
        }
        expect(above_pre_command, "BeamBacklog_Rows0to61_KeepPreCommandBackground");

        // (b) AHEAD of the beam (display lines 62..191, including the
        //     command's own destination lines 62..70): NO commit-time seal --
        //     the rows materialize at their beam/finalize time from the LATER
        //     (overwritten) VRAM, exactly as the beam would display them on
        //     real hardware. PRE-M62 the sink sealed 62..70 with the command
        //     color at the blit instant; that behavior FAILS here.
        bool ahead_shows_later_state = true;
        for (int y = kBandTopLine; y < fb.height; ++y) {
            ahead_shows_later_state = ahead_shows_later_state && row_equals(fb, frame_over, y);
        }
        expect(ahead_shows_later_state,
               "BeamClamp_RowsAheadOfBeam_RenderLaterVram_NoCommitTimeSeal");

        // (c) The full sealed frame is byte-identical to the no-command /
        //     no-sink reference: the command sink leaves NO trace beyond the
        //     beam-path semantics every non-command write already has.
        const FrameBuffer ref = run_reference_no_command(kBeamLine, kOverwrite);
        expect(fb.pixels == ref.pixels,
               "BeamClamp_SealedFrame_ByteIdentical_ToNoCommandReference");

        // (d) Non-vacuity: the command DID change VRAM (its content would
        //     differ from the overwrite if it had been sealed early).
        expect(!row_equals(frame_cmd_band, frame_over, kBandTopLine),
               "NonVacuity_CommandContentDiffersFromOverwrite");
    }

    // --- 2. Active-display gate: the identical command in VBLANK commits
    //     nothing -> the whole frame renders from the final (overwritten) VRAM,
    //     byte-identical to the reference (also an AC-7 inertness proof). ---
    {
        const FrameBuffer fb = run_scenario(-1, kCmdDy, kCmdNy, kCmd, kOverwrite);
        expect(fb.pixels == frame_over.pixels,
               "VblankCommand_NoPerRowCommit_WholeFrameFinalVram_ActiveDisplayGate");
    }

    // --- 3. Visible-page gate: a command to a NON-displayed page (page 3,
    //     page-rows >= 768) falls back -> sealed frame byte-identical to the
    //     no-sink reference (same io_write seam commit, no per-row commit). ---
    {
        const FrameBuffer ref = run_reference_no_command(kBeamLine, kOverwrite);
        const FrameBuffer off_page = run_scenario(kBeamLine, 768u, kCmdNy, kCmd, kOverwrite);
        expect(off_page.pixels == ref.pixels,
               "OffScreenPageCommand_VisiblePageGate_FallsBack_ByteIdentical");
    }

    // --- 4. Two-machine determinism. ---
    {
        const FrameBuffer a = run_scenario(kBeamLine, kCmdDy, kCmdNy, kCmd, kOverwrite);
        const FrameBuffer b = run_scenario(kBeamLine, kCmdDy, kCmdNy, kCmd, kOverwrite);
        expect(a.pixels == b.pixels, "Determinism_TwoRuns_ByteIdenticalSealedFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvCommandRenderSync_Integration cases passed\n";
    return 0;
}
