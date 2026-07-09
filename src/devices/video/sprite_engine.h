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
// machine-level sibling like VdpFrameRenderer). Driven ONLY by the EXISTING
// on_vsync() frame-boundary hook: recompute_frame() is called once per
// frame, over ALL output lines in one deterministic pass -- no new clock
// consumer (mirrors the M21 blink-countdown precedent exactly).
//
// CPU-visible status side effects (S#0 5S/C, S#3-S#6 collision X/Y) are
// independent of whether any frame is ever rendered: real software polls
// these via IN (#99) without needing a pixel output. VdpFrameRenderer's
// sprite-pixel compositing pass (composite_sprites()) then QUERIES this SAME
// already-computed per-line visible-sprite buffer via visible_sprites()
// (a read-only accessor), avoiding a second, drift-prone implementation of
// the sprite-selection algorithm (planner package §1.4 Resolution 1).
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
    void recompute_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs,
                         const VdpModeState& mode, int height);

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
    // fix). Real hardware checks sprites PROGRESSIVELY as the raster scans
    // (openMSX models this in SpriteChecker::sync()/checkSprites1/2, which
    // run "up to the current emulation time" -- SpriteChecker.hh:70-100): the
    // S#0 C flag therefore re-latches on the NEXT colliding line scanned
    // after an S#0 read cleared it, i.e. with LINE granularity, not frame
    // granularity. The HB-F1XV BIOS's boot-logo wobble effect depends on
    // this: it paces one R#26/R#27 write per S#0-C poll exit (BIOS
    // f1xvbios.rom loop at 0x7A5D-0x7A74), which on a frame-granular model
    // runs two orders of magnitude too slowly and never visibly completes.
    //
    // recompute_frame() therefore collects the frame's per-line collision
    // EVENTS (raster order); the V9958Vdp S#0 read path calls these two
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

    std::vector<std::vector<VisibleSprite>> lines_;
    std::vector<CollisionEvent> collision_events_;
    std::size_t next_collision_event_ = 0;
    std::uint8_t status_ = 0;
    int collision_x_ = 0;
    int collision_y_ = 0;
};

}  // namespace sony_msx::devices::video
