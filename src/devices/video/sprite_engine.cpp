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

#include "devices/video/sprite_engine.h"

#include <algorithm>
#include <bit>

namespace sony_msx::devices::video {

namespace {

// D7's rotate-right-by-1 planar interleave (A-M21-10), re-applied here to an
// arbitrary logical VRAM address (not just a display row): even logical
// addresses land in bank0 (physical = addr>>1), odd in bank1 (physical =
// 0x10000 + (addr>>1)). Same physical-bank placement rule as
// V9958Vdp::effective_address()/VdpFrameRenderer's planar_row_spans(),
// re-expressed here as a tiny self-contained helper (never calling into
// v9958_vdp.cpp) because the sprite attribute/pattern tables in mode 2
// planar screens (G6/G7) are spread over the same two VRAM ICs as bitmap
// data (SpriteChecker.cc:283-327's getReadAreaPlanar/readPlanar calls).
std::uint32_t planar_address(const std::uint32_t logical) {
    const std::uint32_t masked = logical & 0x1FFFFu;
    return (masked >> 1) | ((masked & 1u) << 16);
}

std::uint8_t read_table_byte(const VdpVram& vram, const std::uint32_t logical_addr, const bool planar) {
    return vram.read(planar ? planar_address(logical_addr) : (logical_addr & 0x1FFFFu));
}

// Bit-doubling for MAG=1 (SpriteChecker.hh:52-63 doublePattern(), "abcd...."
// -> "aabbccdd"), independently re-expressed as an explicit bit loop rather
// than the reference's specific shift-and-mask sequence. Only the top 16
// bits of `pattern` are ever non-zero (max sprite width is 16 pixels), so
// only those are examined.
std::uint32_t double_pattern(const std::uint32_t pattern) {
    std::uint32_t result = 0;
    for (int i = 0; i < 16; ++i) {
        if ((pattern >> (31 - i)) & 1u) {
            result |= (0b11u << (30 - 2 * i));
        }
    }
    return result;
}

// calculatePatternNP/calculatePatternPlanar (SpriteChecker.cc:44-67),
// unified into one function keyed by `planar`: reads the 8x8 (or, for size16,
// two 8x8 quadrants spread +16 bytes apart) pattern rows and packs them into
// a 32-bit pattern (bit31 = leftmost pixel), then doubles for MAG=1.
std::uint32_t calculate_pattern(const VdpVram& vram, const std::uint32_t pattern_base,
                                const std::uint8_t pattern_nr, const int line_in_pattern,
                                const bool size16, const bool mag, const bool planar) {
    const std::uint32_t index = pattern_base + static_cast<std::uint32_t>(pattern_nr) * 8u +
                                 static_cast<std::uint32_t>(line_in_pattern);
    std::uint32_t pattern = static_cast<std::uint32_t>(read_table_byte(vram, index, planar)) << 24;
    if (size16) {
        pattern |= static_cast<std::uint32_t>(read_table_byte(vram, index + 16u, planar)) << 16;
    }
    return mag ? double_pattern(pattern) : pattern;
}

}  // namespace

void SpriteEngine::recompute_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs,
                                   const VdpModeState& mode, const int height) {
    // M49-S1 (backlog D9): the pre-M49 single all-lines sweep, re-expressed as
    // "bind the frame + check every line in ONE segment". begin_frame() takes
    // the frameStart() per-line clear + the frame-boundary geometry; the single
    // check_until(height-1) below runs the identical sprite loop + collision
    // pass over [1, height) against the identical register/VRAM snapshot and
    // finalizes S#0/collision -- byte-identical to the old code (AC-S1). This is
    // the only caller in S1, so on_vsync() behavior is unchanged.
    begin_frame(vram, control_regs, mode, height);
    if (height > 0) {
        check_until(height - 1);
    }
}

void SpriteEngine::begin_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs,
                               const VdpModeState& mode, const int height) {
    // frameStart() unconditionally clears the per-line visible-sprite counts
    // every frame, regardless of sprite mode (SpriteChecker.hh:228-233).
    lines_.assign(height > 0 ? static_cast<std::size_t>(height) : 0, {});

    // Bind the LIVE state read by check_until()/process_segment(). The pointers
    // stay valid for the frame's duration (the caller owns control_regs + vram);
    // reading them LIVE at each check_until() is what makes a mid-frame write
    // affect only the lines after its commit boundary (the S2 seam drives this).
    vram_ptr_ = &vram;
    regs_ptr_ = control_regs.data();
    mode_captured_ = mode;  // A-M49-2: sprite-mode/planar are per-frame decisions
    height_ = height;
    watermark_ = 1;  // line 0 is unconditionally sprite-free (A-M22-9)
    finalized_ = false;
    frame_active_ = false;
    fifth_num_ = -1;         // no 5th/9th sprite detected yet
    fifth_line_ = height;    // larger than any valid line index
    sprite_end_ = 32;        // sentinel-Y loop-stop index (32 = loop never broke)

    // NOTE: collision_events_, next_collision_event_ and status_ are DELIBERATELY
    // not reset here. The pre-M49 code only cleared them inside the enabled path
    // (right before its collision loop) and left them frozen on the disabled /
    // sprite-mode-0 / height<=0 early return; the first ACTIVE segment clears
    // them (check_until()), preserving that exact behavior.
}

void SpriteEngine::check_until(const int line) {
    if (finalized_ || height_ <= 0) {
        return;  // frame already sealed, or the degenerate height<=0 no-op
    }
    int end = line + 1;  // exclusive
    if (end > height_) {
        end = height_;
    }
    if (end <= watermark_) {
        return;  // idempotent: nothing new to check (advance-only watermark)
    }
    const int lo = watermark_ < 1 ? 1 : watermark_;

    // Per-SEGMENT (per-line-live) gates, read from the LIVE bound state:
    //  * sprite-mode-0 (SpriteChecker.hh:82-87 no-ops entirely; status frozen);
    //  * spritesEnabledFast() (VDP.hh:313-319): R#1 bit6 displayEnabled AND
    //    R#8 bit1 clear (SPD). A freshly reset VDP (VRAM all-zero, phantom
    //    sprites at Y=0) must not populate S#0 -- the M14 VBlank status tests
    //    call on_vsync() with no sprites configured and rely on this gate.
    // This is the actual D9 fix: a mid-frame R#1 BL / R#8 SPD / mode toggle now
    // affects only the lines checked after its commit boundary, not the frame.
    const int sprite_mode = vdp_sprite_mode(mode_captured_.base);
    const bool display_enabled = (regs_ptr_[1] & 0x40) != 0;
    const bool sprite_enabled = (regs_ptr_[8] & 0x02) == 0;
    if (sprite_mode != 0 && display_enabled && sprite_enabled) {
        if (!frame_active_) {
            // First active segment: clear the collision-event queue exactly
            // where the pre-M49 enabled path did (before its collision loop).
            frame_active_ = true;
            collision_events_.clear();
            next_collision_event_ = 0;
        }
        process_segment(lo, end);
    }

    watermark_ = end;
    if (watermark_ >= height_) {
        finalize_frame();
    }
}

void SpriteEngine::check_until_visible_only(const int line) {
    // M49-S2 (backlog D9): the RENDERING-ONLY analogue of check_until -- populate
    // the per-line visible-sprite buffers up to `line`, but NEVER touch the
    // collision-event queue or the S#0 status (no clear, no collection, no
    // finalize). The seam drives THIS during the frame so the RENDERED sprite plane
    // splits per-line-live, while the CPU-visible collision/status is left frozen
    // (the previous frame's frame-atomic result) until on_vsync's recompute_frame
    // recomputes it -- byte-identical to pre-M49 for every game (DEC-0031 + no
    // game-behaviour change).
    if (finalized_ || height_ <= 0) {
        return;
    }
    int end = line + 1;  // exclusive
    if (end > height_) {
        end = height_;
    }
    if (end <= watermark_) {
        return;  // advance-only (never move the watermark backwards)
    }
    const int lo = watermark_ < 1 ? 1 : watermark_;
    // Per-SEGMENT (per-line-live) gates, identical to check_until's, read LIVE.
    const int sprite_mode = vdp_sprite_mode(mode_captured_.base);
    const bool display_enabled = (regs_ptr_[1] & 0x40) != 0;
    const bool sprite_enabled = (regs_ptr_[8] & 0x02) == 0;
    if (sprite_mode != 0 && display_enabled && sprite_enabled) {
        process_segment(lo, end, /*collect_collision=*/false);
    }
    watermark_ = end;
    // DELIBERATELY no finalize_frame(): status/collision stay frozen for the CPU.
}

void SpriteEngine::process_segment(const int lo, const int hi, const bool collect_collision) {
    const VdpVram& vram = *vram_ptr_;
    const std::uint8_t* const regs = regs_ptr_;
    const int sprite_mode = vdp_sprite_mode(mode_captured_.base);

    const std::uint8_t r1 = regs[1];
    const bool size16 = (r1 & 0x02) != 0;
    const bool mag = (r1 & 0x01) != 0;
    const int size = size16 ? 16 : 8;
    const int mag_size = (mag ? 2 : 1) * size;
    const std::uint8_t pattern_index_mask = size16 ? std::uint8_t{0xFC} : std::uint8_t{0xFF};

    // Table base formulas (VDP.hh:262-268, A-M22-16).
    const std::uint32_t attrib_base = (static_cast<std::uint32_t>(regs[11]) << 15) |
                                       (static_cast<std::uint32_t>(regs[5]) << 7);
    // Sprite mode 2 attribute-table addressing (the Metal Gear sprite-
    // invisibility fix): the effective address of a mode-2 table read is
    //   baseMask & (indexMask | index)
    // with baseMask = (R#11<<15) | (R#5<<7) | 0x7F and indexMask = ~0x3FF
    // (VDP.cc:1357-1371 updateSpriteAttributeBase; VDPVRAM.hh:263-279
    // readNP/getReadArea apply `effectiveBaseMask & index` with unused index
    // bits set to one). R#5's low 3 bits are therefore AND-mask bits in mode
    // 2, not base-address bits: with the universal software convention of
    // setting them to 1 (BIOS SCREEN5 R#5=0xEF, Metal Gear R#5=0xE7), the
    // table is a 1KB-aligned region -- per-line colors at offsets 0-511, the
    // Y/X/pattern sub-table at offsets 512-1023. Treating R#5's full value as
    // a plain base (the pre-fix code) landed every mode-2 Y/X/pattern read
    // 0x200 bytes too high (inside the sprite PATTERN table), so every
    // sprite read garbage/zero attributes and vanished.
    const std::uint32_t attrib_base_mask = attrib_base | 0x7Fu;
    const auto mode2_attr_addr = [attrib_base_mask](const std::uint32_t index) {
        return attrib_base_mask & (~0x3FFu | index);
    };
    const std::uint32_t pattern_base = static_cast<std::uint32_t>(regs[6]) << 11;
    const bool planar = vdp_base_is_planar(mode_captured_.base);
    const int r23 = regs[23];
    const int y_sentinel = (sprite_mode == 1) ? 208 : 216;
    const int max_per_line = (sprite_mode == 1) ? 4 : 8;

    int local_sprite_end = 32;  // sentinel-Y loop-stop index (32 = never broke)

    for (int sprite = 0; sprite < 32; ++sprite) {
        int y;
        std::uint32_t x_addr;
        std::uint32_t pattern_index_addr;
        std::uint32_t color_addr_base = 0;  // mode 1 only (constant per sprite)
        const bool sprite_planar = (sprite_mode == 2) && planar;

        if (sprite_mode == 1) {
            // Flat 4-byte-per-sprite table: Y, X, pattern, color (A-M22-16,
            // SpriteChecker.cc:114-153).
            y = read_table_byte(vram, attrib_base + static_cast<std::uint32_t>(sprite) * 4u + 0u, false);
            x_addr = attrib_base + static_cast<std::uint32_t>(sprite) * 4u + 1u;
            pattern_index_addr = attrib_base + static_cast<std::uint32_t>(sprite) * 4u + 2u;
            color_addr_base = attrib_base + static_cast<std::uint32_t>(sprite) * 4u + 3u;
        } else {
            // Mode 2's Y/X/pattern sub-table sits at index offset +512
            // (A-M22-15, SpriteChecker.cc:284-285/329-330), same 4-byte
            // stride as mode 1 -- addressed through the baseMask/indexMask
            // combine above, NOT a plain base+offset add.
            y = read_table_byte(vram, mode2_attr_addr(512u + static_cast<std::uint32_t>(sprite) * 4u + 0u),
                                sprite_planar);
            x_addr = mode2_attr_addr(512u + static_cast<std::uint32_t>(sprite) * 4u + 1u);
            pattern_index_addr = mode2_attr_addr(512u + static_cast<std::uint32_t>(sprite) * 4u + 2u);
        }
        if (y == y_sentinel) {
            local_sprite_end = sprite;
            break;
        }

        const std::uint8_t pattern_nr = static_cast<std::uint8_t>(
            read_table_byte(vram, pattern_index_addr, sprite_planar) & pattern_index_mask);
        const int x_base = read_table_byte(vram, x_addr, sprite_planar);

        for (int line = lo; line < hi; ++line) {
            // 1-pixel vertical shift (A-M22-9): sprites are checked one line
            // earlier than displayed; output line 0 is unconditionally
            // sprite-free (the checked range starts at line 1).
            const int sprite_line = ((line - 1) + r23 - y) & 0xFF;
            if (sprite_line >= mag_size) {
                continue;
            }

            auto& vis = lines_[static_cast<std::size_t>(line)];
            if (vis.size() == static_cast<std::size_t>(max_per_line)) {
                if (line < fifth_line_) {
                    fifth_line_ = line;
                    fifth_num_ = sprite;
                }
                continue;
            }

            const int pattern_line = mag ? (sprite_line / 2) : sprite_line;
            const std::uint32_t pattern =
                calculate_pattern(vram, pattern_base, pattern_nr, pattern_line, size16, mag, sprite_planar);

            std::uint8_t color_attrib;
            if (sprite_mode == 1) {
                color_attrib = read_table_byte(vram, color_addr_base, false);
            } else {
                // Per-LINE color/attribute byte (A-M22-15): table index
                // sprite*16 + spriteLine (offsets 0-511, NOT the +512
                // sub-table), through the same baseMask/indexMask combine
                // (SpriteChecker.cc:312-314/357-359 `(~0u << 10) | (sprite *
                // 16 + spriteLine)`).
                color_attrib = read_table_byte(
                    vram,
                    mode2_attr_addr(static_cast<std::uint32_t>(sprite) * 16u +
                                    static_cast<std::uint32_t>(pattern_line)),
                    sprite_planar);
            }

            int x = x_base;
            if (color_attrib & 0x80) {
                x -= 32;  // EC: early clock (A-M22-10, applied at check time).
            }
            vis.push_back(VisibleSprite{pattern, x, color_attrib});
        }
    }
    sprite_end_ = local_sprite_end;

    // M49-S2 (backlog D9): the RENDERING-ONLY pass (check_until_visible_only) skips
    // this entire collision block, so the incremental per-line split NEVER perturbs
    // the CPU-visible collision -- that stays frame-atomic (recompute_frame at
    // on_vsync), byte-identical to pre-M49 for every game.
    if (!collect_collision) {
        return;
    }

    // Collision detection (SpriteChecker.cc:196-241/410-479), collected in
    // raster order for THIS segment's lines and APPENDED to the frame's event
    // queue (segments are checked in increasing line order, so the queue stays
    // raster-ordered across segments -- what the boot-logo S#0 re-latch path
    // consumes). The per-line collision uses this segment's LIVE mag_size /
    // sprite_mode / can0collide, so it too is per-line-live.
    const bool can0collide = (regs[8] & 0x20) != 0;  // TP bit SET (VDP.hh:195-201)

    for (int line = lo; line < hi; ++line) {
        const auto& vis = lines_[static_cast<std::size_t>(line)];
        int min_x_collision = 999;
        for (int i = static_cast<int>(vis.size()) - 1; i >= 1; --i) {
            const VisibleSprite& si = vis[static_cast<std::size_t>(i)];
            const std::uint8_t color1 = si.color_attrib & 0x0F;
            if (!can0collide && color1 == 0) continue;
            if (sprite_mode == 2 && (si.color_attrib & 0x60) != 0) continue;  // CC or IC excludes collision
            const int x_i = si.x;
            const std::uint32_t pattern_i = si.pattern;

            for (int j = i - 1; j >= 0; --j) {
                const VisibleSprite& sj = vis[static_cast<std::size_t>(j)];
                const std::uint8_t color2 = sj.color_attrib & 0x0F;
                if (!can0collide && color2 == 0) continue;
                if (sprite_mode == 2 && (sj.color_attrib & 0x60) != 0) continue;

                const int x_j = sj.x;
                const int dist = x_j - x_i;
                if (dist > -mag_size && dist < mag_size) {
                    std::uint32_t pattern_j = sj.pattern;
                    if (dist < 0) {
                        pattern_j <<= static_cast<unsigned>(-dist);
                    } else {
                        pattern_j >>= static_cast<unsigned>(dist);
                    }
                    std::uint32_t col_pat = pattern_i & pattern_j;
                    if (x_i < 0) {
                        col_pat &= (1u << static_cast<unsigned>(32 + x_i)) - 1u;
                    }
                    if (col_pat != 0) {
                        const int x_collision = x_i + std::countl_zero(col_pat);
                        min_x_collision = std::min(min_x_collision, x_collision);
                    }
                }
            }
        }
        if (min_x_collision < 256) {
            // Fixed +12 (X) / +8 (Y) offsets, baked into the status-register
            // contract (SpriteChecker.cc:236-238/474-476).
            collision_events_.push_back(CollisionEvent{line, min_x_collision + 12, line + 8});
        }
    }
}

void SpriteEngine::finalize_frame() {
    if (finalized_) {
        return;
    }
    finalized_ = true;
    if (!frame_active_) {
        // Disabled / sprite-mode-0 / height<=0 frame: status + collision stay
        // frozen (the pre-M49 early-return behavior).
        return;
    }

    // S#0 composition (SpriteChecker.cc:157-171/387-402). Unlike the
    // reference's combined byte (which also gates on bit7/F), this project
    // keeps F inside V9958Vdp, so the gate here is simply "5S/9S bit (bit6)
    // not already latched".
    const int last_sprite_num = std::min(sprite_end_, 31);
    if (fifth_num_ != -1 && (status_ & 0x40) == 0) {
        status_ = static_cast<std::uint8_t>(0x40 | (status_ & 0x20) | fifth_num_);
    }
    if ((status_ & 0x40) == 0) {
        status_ = static_cast<std::uint8_t>((status_ & 0x20) | last_sprite_num);
    }

    // Frame-boundary latch: preserves the pre-fix contract (C set right after
    // on_vsync() when this frame has any collision and none is pending),
    // consuming the earliest event. Later events stay queued for the raster-
    // granular S#0 re-latch path.
    if ((status_ & 0x20) == 0 && !collision_events_.empty()) {
        latch_collision(collision_events_[0]);
        next_collision_event_ = 1;
    }
}

void SpriteEngine::latch_collision(const CollisionEvent& event) {
    status_ = static_cast<std::uint8_t>(status_ | 0x20);
    collision_x_ = event.x;
    collision_y_ = event.y;
}

void SpriteEngine::sync_collision_to_raster(const int display_line) {
    if ((status_ & 0x20) != 0) {
        return;  // latched & unread: hardware does not re-latch/update coords
    }
    if (next_collision_event_ < collision_events_.size() &&
        collision_events_[next_collision_event_].line <= display_line) {
        latch_collision(collision_events_[next_collision_event_]);
        ++next_collision_event_;
    }
}

void SpriteEngine::consume_collision_events_up_to(const int display_line) {
    while (next_collision_event_ < collision_events_.size() &&
           collision_events_[next_collision_event_].line <= display_line) {
        ++next_collision_event_;
    }
}

std::span<const SpriteEngine::VisibleSprite> SpriteEngine::visible_sprites(const int line) const {
    if (line < 0 || static_cast<std::size_t>(line) >= lines_.size()) {
        return {};
    }
    return lines_[static_cast<std::size_t>(line)];
}

void SpriteEngine::reset_status() {
    status_ = static_cast<std::uint8_t>(status_ & 0x1F);
}

void SpriteEngine::reset_collision() {
    collision_x_ = 0;
    collision_y_ = 0;
}

void SpriteEngine::reset() {
    lines_.clear();
    collision_events_.clear();
    next_collision_event_ = 0;
    status_ = 0;
    collision_x_ = 0;
    collision_y_ = 0;
    // M49-S1: deterministic power-on reset of the progressive-frame state too.
    vram_ptr_ = nullptr;
    regs_ptr_ = nullptr;
    mode_captured_ = {};
    height_ = 0;
    watermark_ = 1;
    finalized_ = false;
    frame_active_ = false;
    fifth_num_ = -1;
    fifth_line_ = 0;
    sprite_end_ = 32;
}

}  // namespace sony_msx::devices::video
