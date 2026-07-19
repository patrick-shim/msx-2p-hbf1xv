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

// Suite: Machine_Hbf1xvCommandSpritePacing_Integration (M51, DEC-0078,
// docs/m51-planner-package.md fix branch (a) shape (i) / AC-F3)
//
// THE M51 DEFECT SCENARIO, machine-level: the M44 command-row sink
// (VdpRenderSyncAdapter::on_commit_up_to) commits background rows PAST the
// beam. Pre-M51 it did NOT pace the render-only sprite pass, so after the
// M49 lazy-open begin_frame() clear every committed row past the sprite
// watermark was sealed from an EMPTY visible-sprite buffer -- the
// scroll-shooter / sprite-scroll / split-screen-title "player sprite
// disappears while scrolling" defect
// (Task 2 trace on the real scroll-shooter gameplay capture: "CMDCOMMIT range=[69,231)
// sprite_wm=69" + plane rows 148-175 all "visible=0"). The M51 fix paces the
// sprite pass to the sink's own destination boundary first (openMSX's
// consumer-side sync rule -- every render advance checks sprites first,
// PixelRenderer.cc:580-584 / SpriteChecker.hh:242-247, effect only).
//
// Scene (mirrors the real trigger, scaled to the M44/M49 in-test oracle
// pattern): GRAPHIC4, sprites ENABLED, one solid 8x8 sprite at Y=100 ->
// display lines 101..108, R#23=0, unique-signature bitmap. At beam line 20 a
// harmless HOOKED register touch opens the render split (sprite watermark ->
// 22); a HOOKED HMMV then fills page-rows [150,160) -> the row sink commits
// display rows [22,151..160) -- sweeping the sprite band 101..108 far past
// the sprite watermark, exactly the failing geometry.
//
// Cases:
//   1. SEALED FRAME == REFERENCE, byte-for-byte: the reference is the same
//      static scene (command band pre-painted via the debug seam) sealed with
//      NO mid-frame activity -- its recompute-populated sprite plane carries
//      the sprite on 101..108. Pre-M51 the command run seals rows 101..108
//      sprite-LESS (this assertion fails: the adversarial-revert proof
//      executes exactly that); post-M51 the frames are identical.
//   2. Sprite rows explicitly present (free-standing sprite-band oracle, so a
//      failure names the band rather than "frames differ").
//   3. DEC-0031: S#0 byte-identical between the command run and the
//      reference run (frame-atomic collision path untouched by the pacing).
//   4. No-command control: the identical run WITHOUT the command also shows
//      the sprite (finalize path) -- the command+sink path was the only
//      broken leg, and the fix changes nothing else.
//   5. Determinism: two identically-driven command runs -> byte-identical
//      sealed frames.

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

constexpr int kBeamLine = 20;        // raster line of the split-opening touch
constexpr std::uint8_t kBg = 0x11;   // background pixel byte
constexpr std::uint8_t kCmd = 0x22;  // HMMV fill byte (colour 2 pixel pair)
constexpr unsigned kCmdDy = 150;     // page-rows [150,160) -> display rows (R#23=0)
constexpr unsigned kCmdNy = 10;
constexpr int kSpriteTop = 101;      // sprite Y=100 -> display lines 101..108
constexpr int kSpriteBot = 108;

void set_reg(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Single VRAM byte via the debug seam (no render-sync side effects).
void write_vram(Hbf1xvMachine& m, const std::uint32_t addr, const std::uint8_t value) {
    set_reg(m, 14, static_cast<std::uint8_t>((addr >> 14) & 0x07));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    m.debug_io_write(0x98, value);
}

// Fill GRAPHIC4 page-rows [row0,row1) with `byte` via the debug seam. The
// two-write #99 pointer carries only A13..A0; rows >= 128 (byte offset >=
// 0x4000) need the A16..A14 page bits in R#14 (the auto-increment then carries
// across the 16K boundary for sequential streaming).
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

// GRAPHIC4 + sprite mode 2, SCREEN5 sprite tables (attr 0x7600 / colour 0x7400
// / patterns 0x7800 -- all above the 192-line bitmap, no overlap), solid 8x8
// sprite 0 at Y=100/X=100, colour 15; unique bitmap background.
void setup_scene(Hbf1xvMachine& m) {
    m.cold_boot();
    set_reg(m, 0, 0x06);   // GRAPHIC4 (sprite mode 2)
    set_reg(m, 1, 0x40);   // BL=1, 8x8, mag off
    set_reg(m, 8, 0x00);   // SPD=0: sprites ENABLED
    set_reg(m, 5, 0xEF);   // sprite attribute base (SCREEN5 layout)
    set_reg(m, 11, 0x00);
    set_reg(m, 6, 0x0F);   // sprite pattern base -> 0x7800
    set_reg(m, 23, 0x00);  // no vertical scroll (display row == page row)
    fill_rows(m, 0, 256, kBg);
    write_vram(m, 0x7600, 100);  // sprite 0 Y
    write_vram(m, 0x7601, 100);  // sprite 0 X
    write_vram(m, 0x7602, 0);    // pattern 0
    write_vram(m, 0x7604, 216);  // sentinel stops the scan
    for (int line = 0; line < 8; ++line) {
        write_vram(m, 0x7400u + static_cast<std::uint32_t>(line), 15);  // colour 15
    }
    for (int row = 0; row < 8; ++row) {
        write_vram(m, 0x7800u + static_cast<std::uint32_t>(row), 0xFF);  // solid
    }
    set_reg(m, 14, 0x00);
}

// HMMV atomic fill (DX=0, NX=256) via the HOOKED path: parameters through the
// debug seam, the R#46 CMD write through vdp().io_write -> fires the seam and
// the M44 per-destination-row sink (the M44 test's exact recipe).
void issue_hmmv_hooked(Hbf1xvMachine& m, const unsigned dy, const unsigned ny,
                       const std::uint8_t col) {
    set_reg(m, 32 + 4, 0x00);
    set_reg(m, 32 + 5, 0x00);
    set_reg(m, 32 + 6, static_cast<std::uint8_t>(dy & 0xFF));
    set_reg(m, 32 + 7, static_cast<std::uint8_t>((dy >> 8) & 0x03));
    set_reg(m, 32 + 8, 0x00);
    set_reg(m, 32 + 9, 0x01);
    set_reg(m, 32 + 10, static_cast<std::uint8_t>(ny & 0xFF));
    set_reg(m, 32 + 11, static_cast<std::uint8_t>((ny >> 8) & 0x03));
    set_reg(m, 32 + 12, col);
    set_reg(m, 32 + 13, 0x00);
    m.vdp().io_write(0x99, 0xC0);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | 46));
}

// Reference: the same scene with the command band PRE-painted statically and
// no mid-frame hooked activity; a full frame + boundary seals it with the
// recompute-populated sprite plane (sprite on 101..108) -- the frame real
// hardware displays for this end-of-frame state.
FrameBuffer reference_frame() {
    Hbf1xvMachine m;
    setup_scene(m);
    fill_rows(m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy), kCmd);
    m.run_cycles(kFrameCycles);
    m.on_vsync_boundary();
    return m.render_frame();
}

// The M51 scenario run. `with_command` selects the defect leg (hooked HMMV
// past the sprite band) vs the control leg (split opens, no command).
// Returns {sealed frame, end-of-run S#0}.
struct RunResult {
    FrameBuffer frame;
    std::uint8_t s0 = 0;
};

RunResult run_scenario(const bool with_command) {
    Hbf1xvMachine m;
    setup_scene(m);
    const std::uint64_t advance =
        (kNonActiveLines + static_cast<std::uint64_t>(kBeamLine)) * kCyclesPerLine + 30;
    m.run_cycles(advance);
    // Harmless hooked touch (R#23 rewritten to its current value): opens the
    // render-only split at beam+2 = 22 -- the state every scrolling game is in
    // when its main loop starts blitting (the scroll-shooter title opens the split at line 13
    // every gameplay frame via its HUD R#1 choreography).
    m.vdp().io_write(0x99, 0x00);
    m.vdp().io_write(0x99, static_cast<std::uint8_t>(0x80 | 23));
    if (with_command) {
        issue_hmmv_hooked(m, kCmdDy, kCmdNy, kCmd);
        // Pre-command band content equals kCmd too (reference paints it
        // statically), so the sealed frame must equal the reference
        // EVERYWHERE -- unless the sink seals sprite-less rows (the defect).
    } else {
        fill_rows(m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy), kCmd);
    }
    m.run_cycles(kFrameCycles - advance);
    m.on_vsync_boundary();
    RunResult r{m.render_frame(), m.vdp().peek_status_register(0)};
    return r;
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
    const FrameBuffer ref = reference_frame();
    expect(ref.width == 256 && ref.height == 192, "Reference_Graphic4Dimensions");

    // The reference must actually carry the sprite, or every later equality
    // would be vacuous (non-tautology guard): row 105 (in the sprite band)
    // must differ from row 90 (same uniform background, no sprite).
    {
        bool differs = false;
        for (int x = 0; x < ref.width; ++x) {
            if (ref.at(x, 105) != ref.at(x, 90)) {
                differs = true;
                break;
            }
        }
        expect(differs, "Reference_SpriteBandRow_DiffersFromBackgroundRow");
    }

    // --- Case 1+2: the defect scenario seals the sprite into the frame. ---
    const RunResult cmd_run = run_scenario(/*with_command=*/true);
    bool whole_frame_equal = cmd_run.frame.pixels == ref.pixels;
    expect(whole_frame_equal, "CommandRun_SealedFrame_ByteIdentical_ToReference");
    if (!whole_frame_equal) {
        int shown = 0;
        for (int y = 0; y < cmd_run.frame.height && shown < 12; ++y) {
            for (int x = 0; x < cmd_run.frame.width; ++x) {
                if (cmd_run.frame.at(x, y) != ref.at(x, y)) {
                    std::cerr << "  diff row " << y << " first x=" << x << " cmd=0x" << std::hex
                              << cmd_run.frame.at(x, y) << " ref=0x" << ref.at(x, y) << std::dec
                              << "\n";
                    ++shown;
                    break;
                }
            }
        }
    }
    bool sprite_band_present = true;
    for (int y = kSpriteTop; y <= kSpriteBot && sprite_band_present; ++y) {
        sprite_band_present = row_equals(cmd_run.frame, ref, y);
    }
    expect(sprite_band_present,
           "CommandRun_SpriteRows101to108_PresentDespiteCommandSinkSweep");

    // --- Case 3: DEC-0031 -- CPU-visible S#0 identical to the reference. ---
    {
        Hbf1xvMachine ref_m;
        setup_scene(ref_m);
        fill_rows(ref_m, static_cast<int>(kCmdDy), static_cast<int>(kCmdDy + kCmdNy), kCmd);
        ref_m.run_cycles(kFrameCycles);
        ref_m.on_vsync_boundary();
        expect(cmd_run.s0 == ref_m.vdp().peek_status_register(0),
               "CommandRun_S0Status_ByteIdentical_FrameAtomicCollisionUntouched");
    }

    // --- Case 4: no-command control (split opens, finalize path). ---
    const RunResult control_run = run_scenario(/*with_command=*/false);
    bool control_sprite_present = true;
    for (int y = kSpriteTop; y <= kSpriteBot && control_sprite_present; ++y) {
        control_sprite_present = row_equals(control_run.frame, ref, y);
    }
    expect(control_sprite_present, "ControlRun_NoCommand_SpriteRowsPresent");

    // --- Case 5: determinism. ---
    const RunResult cmd_run2 = run_scenario(/*with_command=*/true);
    expect(cmd_run.frame.pixels == cmd_run2.frame.pixels,
           "Determinism_TwoCommandRuns_ByteIdenticalSealedFrames");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvCommandSpritePacing_Integration cases passed\n";
    return 0;
}
