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

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace sony_msx::devices::video {

// V9958 sprite check / collision / 5th-sprite-detection engine (M22-S1/S2,
// backlog D2). Owned INSIDE V9958Vdp as a private member (mirrors the
// existing blink_countdown_/blink_state_ ownership style -- NOT a new
// machine-level sibling like VdpFrameRenderer). Driven only by the existing
// on_vsync() frame-boundary hook: recompute_frame() runs once per frame,
// over all output lines in one deterministic pass -- no new clock consumer
// (mirrors the M21 blink-countdown precedent exactly).
//
// CPU-visible status side effects (S#0 5S/C, S#3-S#6 collision X/Y) don't
// depend on whether any frame is ever rendered: real software polls these
// via IN (#99) without needing pixel output. VdpFrameRenderer's sprite-pixel
// compositing pass (composite_sprites()) then queries this same
// already-computed per-line visible-sprite buffer via visible_sprites()
// (read-only), avoiding a second, drift-prone implementation of the
// sprite-selection algorithm (planner package §1.4 Resolution 1).
//
// Grounding (behavior reference only, GPL isolation -- never copied):
// references/openmsx-21.0/src/video/SpriteChecker.{hh,cc} (check/collision/
// 5th-sprite algorithm, magnification, EC/CC/IC bit semantics, the +12/+8
// collision-coordinate offsets). Pixel compositing (SpriteConverter.hh) is
// consumed by VdpFrameRenderer, not this class.
class SpriteEngine {
public:
    // Mirrors SpriteChecker::SpriteInfo (SpriteChecker.hh:37-50). `pattern`'s
    // bit31 is the sprite's leftmost pixel; unused bits are zero. `x` is
    // already EC-adjusted (bit7 of colorAttrib, x -= 32, applied at
    // check time -- SpriteChecker.cc:150/320/370). `color_attrib`: bits3-0 =
    // palette index, bit5 = IC (collision-exclusion only, A-M22-11), bit6 =
    // CC (color-cascade, mode 2 only), bit7 = EC (already consumed above).
    struct VisibleSprite {
        std::uint32_t pattern = 0;
        int x = 0;
        std::uint8_t color_attrib = 0;
    };

    // Recomputes per-line visible-sprite lists + S#0/S#3-S#6 status for the
    // WHOLE frame in one deterministic pass. `control_regs` is the full
    // R#0..R#31 file (only R#1/R#5/R#6/R#8/R#11/R#23 are consulted). `height`
    // is the active-display pixel height for the CURRENT mode (matches
    // VdpFrameRenderer::height()'s own formula -- duplicated at the
    // V9958Vdp::on_vsync() call site rather than introducing a cross-layer
    // dependency from the VDP core onto the presentation-layer renderer).
    //
    // M49-S1 (backlog D9): now a thin single-shot convenience WRAPPER over the
    // stateful progressive engine below -- `begin_frame(); check_until(height-1)`.
    // Byte-identical to the pre-M49 single-sweep pass (AC-S1): the whole frame
    // is checked in one segment against one register/VRAM snapshot, so every
    // per-line visible-sprite buffer + S#0/S#3-S#6 status is unchanged. This is
    // the entry `V9958Vdp::on_vsync()` still calls, so absent S2's seam wiring
    // the whole subsystem behaves exactly as today.
    void recompute_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs,
                         const VdpModeState& mode, int height);

    // --- M49-S1 progressive per-line LIVE sprite check (backlog D9) -----------
    //
    // The sprite subsystem's analogue of VdpScanlineAccumulator's watermark
    // idiom (vdp_scanline_accumulator.h:61-145): the background renderer already
    // commits display lines up to the beam from LIVE registers; sprites did not.
    // `begin_frame()` binds the LIVE VRAM + control-register file (read through
    // the stored pointers by every later `check_until()`) and captures the
    // frame-boundary geometry (height, mode -> sprite-mode/planar; A-M49-2:
    // height/mode stay a per-frame decision), resetting the watermark. Each
    // `check_until(line)` advances the watermark so that all display lines up to
    // and INCLUDING `line` (clamped to height) have been checked using the bound
    // state AS IT STANDS AT THAT CALL -- so a caller that mutates a sprite-
    // relevant register (R#1 BL / R#5 / R#6 / R#8 SPD / R#11 / R#23) or sprite-
    // attribute VRAM between two `check_until()` calls makes the lines before the
    // second call reflect the OLD state and the lines after it the NEW state
    // (the actual D9 fix; the per-line R#1 BL / R#8 SPD / sprite-mode-0 gates are
    // evaluated per SEGMENT, i.e. per-line-live). When the watermark reaches
    // height the S#0 5S/number + frame-boundary collision latch are composed
    // exactly as the pre-M49 single sweep did. Grounding (behavior/model only,
    // never copied): tier-1 fact-sheet Yamaha V9958 VDP.md §6 line 120 (sprite
    // data for line N fetched during line N-1 -> per-scanline, not per-frame);
    // tier-2 openMSX SpriteChecker::checkUntil() (the incremental sync-before-
    // change model our background M32 seam already implements).
    //
    // S2 (the seam wiring, a later slice) drives this from
    // Hbf1xvMachine's render-sync seam; S1 is deliberately a NO-OP vs today --
    // the only caller is recompute_frame()'s single full-height sweep.
    void begin_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs,
                     const VdpModeState& mode, int height);
    void check_until(int line);

    // M49-S2 (backlog D9): RENDERING-ONLY progressive pass. Populates the per-line
    // VISIBLE-sprite buffers up to (and including) `line` from the bound LIVE state
    // AT THIS CALL -- exactly like check_until -- but does NOT touch the DEC-0031
    // collision / 5th-sprite / S#0-status machinery (no event collection, no
    // frame-boundary finalize). The seam drives THIS from the render-sync path so a
    // mid-frame sprite-relevant change splits the RENDERED sprite plane per-line-live
    // (the D9 fix) while the CPU-visible collision/status stays frame-atomic
    // (recompute_frame at on_vsync), byte-identical to pre-M49 for EVERY game -- no
    // game-behaviour change (the collision per-line-live migration is Slice 3's
    // separately-verified scope). Advance-only; shares check_until's range logic.
    void check_until_visible_only(int line);

    // First display line NOT yet checked this frame (watermark). Test/observer
    // accessor; monotonic within a frame, reset to 1 by begin_frame() (line 0 is
    // unconditionally sprite-free, A-M22-9, so it is never in the checked range).
    [[nodiscard]] int watermark() const { return watermark_; }

    // Visible sprites for 0-based active-display output line `line`. Empty
    // for line 0 (A-M22-9: the 1-pixel vertical shift means output line 0 is
    // UNCONDITIONALLY sprite-free) and for any out-of-range line.
    [[nodiscard]] std::span<const VisibleSprite> visible_sprites(int line) const;

    // S#0 bits 6-0 (bit6 = 5S/9th-sprite flag, bit5 = C collision, bits4-0 =
    // sprite number). Bit7 (F, VBlank) is owned by V9958Vdp itself and is
    // OR'd in separately by the caller.
    [[nodiscard]] std::uint8_t status_bits() const { return status_; }
    [[nodiscard]] int collision_x() const { return collision_x_; }
    [[nodiscard]] int collision_y() const { return collision_y_; }

    // S#0-read side effect (A-M22-14, SpriteChecker.hh:104-110): clears ONLY
    // bits 6/5 (`& 0x1F`), leaving the sprite-number field (bits 4-0) stale
    // until the next frame's recompute overwrites it.
    void reset_status();
    // S#5-read side effect (VDP.cc:975-977 `readStatusReg` case 5): zeroes
    // BOTH collision X and Y (SpriteChecker::resetCollision()).
    void reset_collision();

    // Per-line collision re-latch against the live raster position (boot-logo
    // fix). Real hardware checks sprites progressively as the raster scans
    // (openMSX models this in SpriteChecker::sync()/checkSprites1/2, which
    // run "up to the current emulation time" -- SpriteChecker.hh:70-100), so
    // the S#0 C flag re-latches on the NEXT colliding line scanned after an
    // S#0 read cleared it -- LINE granularity, not frame granularity. The
    // HB-F1XV BIOS's boot-logo wobble effect depends on this: it paces one
    // R#26/R#27 write per S#0-C poll exit (BIOS f1xvbios.rom loop at
    // 0x7A5D-0x7A74), which on a frame-granular model runs two orders of
    // magnitude too slowly and never visibly completes.
    //
    // recompute_frame() therefore collects the frame's per-line collision
    // events (raster order); the V9958Vdp S#0 read path calls these two
    // hooks with the current raster display line (derived from its existing
    // pull-style VdpClockSource -- this engine still owns no clock):
    //  * sync_collision_to_raster(): if C is clear, latch the next
    //    unconsumed event the raster has already scanned
    //    (event line <= display_line).
    //  * consume_collision_events_up_to(): called after the S#0 read-clear;
    //    events at already-scanned lines are in the raster's past and can
    //    never re-latch (matches progressive hardware checking).
    // With no clock attached the caller passes INT_MIN, making both no-ops
    // (the pre-fix frame-granular behavior, preserved for clockless tests).
    void sync_collision_to_raster(int display_line);
    void consume_collision_events_up_to(int display_line);

    // Deterministic power-on reset.
    void reset();

private:
    // One per-line collision event (coordinates already carry the +12/+8
    // S#3-S#6 offsets, SpriteChecker.cc:236-238/474-476).
    struct CollisionEvent {
        int line = 0;
        int x = 0;
        int y = 0;
    };

    void latch_collision(const CollisionEvent& event);

    // M49-S1: check + collect collision for display lines [lo, hi) (lo>=1,
    // hi<=height_) using the bound LIVE state, advancing fifth_num_/fifth_line_/
    // sprite_end_ and appending collision events in raster order. Preserves the
    // pre-M49 per-line Y/X/pattern/mode-2-base-mask/planar formulas VERBATIM --
    // only the line RANGE is bounded and the register/VRAM reads are taken from
    // the bound pointers (LIVE at the call), so a single [1, height) segment is
    // byte-identical to the old sprite loop.
    // M49-S2: `collect_collision` (default true) gates ONLY the collision-event
    // loop -- the per-line visible-sprite population + 5th-sprite/sentinel tracking
    // always run. check_until_visible_only() passes false so the rendering split
    // never perturbs the frame-atomic collision the CPU reads.
    void process_segment(int lo, int hi, bool collect_collision = true);
    // M49-S1: compose S#0 (5S/number) + the frame-boundary collision latch once
    // the whole frame has been checked -- byte-identical to the tail of the
    // pre-M49 recompute_frame(). No-op if the frame never became active
    // (disabled / sprite-mode-0 / height<=0), leaving status + collision frozen
    // exactly as the old early-return did.
    void finalize_frame();

    std::vector<std::vector<VisibleSprite>> lines_;
    std::vector<CollisionEvent> collision_events_;
    std::size_t next_collision_event_ = 0;
    std::uint8_t status_ = 0;
    int collision_x_ = 0;
    int collision_y_ = 0;

    // --- M49-S1 progressive-frame state (backlog D9) --------------------------
    // Bound LIVE state (read by check_until()/process_segment()); valid only
    // between begin_frame() and the frame's finalize. Never owns the data.
    const VdpVram* vram_ptr_ = nullptr;
    const std::uint8_t* regs_ptr_ = nullptr;
    VdpModeState mode_captured_{};  // A-M49-2: sprite-mode/planar are per-frame
    int height_ = 0;
    int watermark_ = 1;             // first line NOT yet checked (line 0 skipped)
    bool finalized_ = false;        // frame's S#0/collision latch already composed
    bool frame_active_ = false;     // any checked segment had sprites enabled
    int fifth_num_ = -1;            // 5th/9th-sprite number (S#0 bits 4-0) or -1
    int fifth_line_ = 0;            // earliest overflow line (== height sentinel)
    int sprite_end_ = 32;           // sentinel-Y loop-stop index (S#0 number src)
};

}  // namespace sony_msx::devices::video
