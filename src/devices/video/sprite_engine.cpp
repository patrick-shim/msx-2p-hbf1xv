#include "devices/video/sprite_engine.h"

#include <algorithm>
#include <bit>

namespace sony_msx::devices::video {

namespace {

// D7's rotate-right-by-1 planar interleave (A-M21-10), independently
// re-applied here to an arbitrary LOGICAL VRAM address (not just a display
// row): even logical addresses land in bank0 (physical = addr>>1), odd land
// in bank1 (physical = 0x10000 + (addr>>1)). This is the SAME physical-bank
// placement rule V9958Vdp::effective_address()/VdpFrameRenderer's
// planar_row_spans() already establish -- re-expressed here as a tiny, self-
// contained helper (never calling into v9958_vdp.cpp) because the sprite
// attribute/pattern tables in mode 2 planar screens (G6/G7) are spread over
// the SAME two VRAM ICs as bitmap data (SpriteChecker.cc:283-327's
// getReadAreaPlanar/readPlanar calls).
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
    // frameStart() unconditionally clears the per-line visible-sprite counts
    // every frame, regardless of sprite mode (SpriteChecker.hh:228-233).
    lines_.assign(height > 0 ? static_cast<std::size_t>(height) : 0, {});
    if (height <= 0) {
        return;
    }

    const int sprite_mode = vdp_sprite_mode(mode.base);
    if (sprite_mode == 0) {
        // SpriteChecker::sync() no-ops entirely in sprite mode 0
        // (SpriteChecker.hh:82-87, "skip vram sync and sprite checks in
        // sprite mode 0"): status stays frozen at whatever it last was.
        return;
    }

    // spritesEnabledFast() gate (VDP.hh:313-319): `displayEnabled` (R#1
    // bit6, the display-enable/BLANK bit -- false at reset, VDP.cc:284/437)
    // AND `spriteEnabled` (R#8 bit1 clear, SPD). updateSprites1/2 do NOTHING
    // at all (not even the per-line visible-sprite population) when this is
    // false -- only frameStart()'s unconditional spriteCount clear (already
    // applied above via lines_.assign()) still happens. Without this gate, a
    // freshly reset/unconfigured VDP (VRAM all-zero, so all 32 "phantom"
    // sprites read Y=0) would spuriously populate S#0 on every on_vsync(),
    // which is both wrong (real hardware/openMSX gate this identically) and
    // was independently confirmed by a regression against the M14 VBlank
    // status tests, which call on_vsync() without configuring any sprites.
    const bool display_enabled = (control_regs[1] & 0x40) != 0;
    const bool sprite_enabled = (control_regs[8] & 0x02) == 0;
    if (!display_enabled || !sprite_enabled) {
        return;
    }

    const std::uint8_t r1 = control_regs[1];
    const bool size16 = (r1 & 0x02) != 0;
    const bool mag = (r1 & 0x01) != 0;
    const int size = size16 ? 16 : 8;
    const int mag_size = (mag ? 2 : 1) * size;
    const std::uint8_t pattern_index_mask = size16 ? std::uint8_t{0xFC} : std::uint8_t{0xFF};

    // Table base formulas (VDP.hh:262-268, A-M22-16).
    const std::uint32_t attrib_base = (static_cast<std::uint32_t>(control_regs[11]) << 15) |
                                       (static_cast<std::uint32_t>(control_regs[5]) << 7);
    const std::uint32_t pattern_base = static_cast<std::uint32_t>(control_regs[6]) << 11;
    const bool planar = vdp_base_is_planar(mode.base);
    const int r23 = control_regs[23];
    const int y_sentinel = (sprite_mode == 1) ? 208 : 216;
    const int max_per_line = (sprite_mode == 1) ? 4 : 8;

    int fifth_num = -1;      // no 5th/9th sprite detected yet
    int fifth_line = height;  // larger than any valid line index
    int sprite_end = 32;      // sprite index where the loop stopped (32 = ran through all 32)

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
            // Mode 2's Y/X/pattern sub-table sits at a FIXED +512-byte offset
            // (A-M22-15, SpriteChecker.cc:284-285/329-330), same 4-byte
            // stride as mode 1.
            y = read_table_byte(vram, attrib_base + 512u + static_cast<std::uint32_t>(sprite) * 4u + 0u,
                                sprite_planar);
            x_addr = attrib_base + 512u + static_cast<std::uint32_t>(sprite) * 4u + 1u;
            pattern_index_addr = attrib_base + 512u + static_cast<std::uint32_t>(sprite) * 4u + 2u;
        }
        if (y == y_sentinel) {
            sprite_end = sprite;
            break;
        }

        const std::uint8_t pattern_nr = static_cast<std::uint8_t>(
            read_table_byte(vram, pattern_index_addr, sprite_planar) & pattern_index_mask);
        const int x_base = read_table_byte(vram, x_addr, sprite_planar);

        for (int line = 1; line < height; ++line) {
            // 1-pixel vertical shift (A-M22-9): sprites are checked one line
            // earlier than displayed; output line 0 is unconditionally
            // sprite-free (the loop starts at line 1).
            const int sprite_line = ((line - 1) + r23 - y) & 0xFF;
            if (sprite_line >= mag_size) {
                continue;
            }

            auto& vis = lines_[static_cast<std::size_t>(line)];
            if (vis.size() == static_cast<std::size_t>(max_per_line)) {
                if (line < fifth_line) {
                    fifth_line = line;
                    fifth_num = sprite;
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
                // Per-LINE color/attribute byte (A-M22-15): attrib_base + 0 +
                // (sprite*16 + spriteLine), NOT the +512 sub-table.
                color_attrib = read_table_byte(
                    vram,
                    attrib_base + static_cast<std::uint32_t>(sprite) * 16u +
                        static_cast<std::uint32_t>(pattern_line),
                    sprite_planar);
            }

            int x = x_base;
            if (color_attrib & 0x80) {
                x -= 32;  // EC: early clock (A-M22-10, applied at check time).
            }
            vis.push_back(VisibleSprite{pattern, x, color_attrib});
        }
    }

    // S#0 composition (SpriteChecker.cc:157-171/387-402). Unlike the
    // reference's combined byte (which also gates on bit7/F), this project
    // keeps F entirely inside V9958Vdp, so the gate here is simply "5S/9S
    // bit (bit6) not already latched" -- behaviorally equivalent for this
    // project's frame-wide, non-progressive recompute model.
    const int last_sprite_num = std::min(sprite_end, 31);
    if (fifth_num != -1 && (status_ & 0x40) == 0) {
        status_ = static_cast<std::uint8_t>(0x40 | (status_ & 0x20) | fifth_num);
    }
    if ((status_ & 0x40) == 0) {
        status_ = static_cast<std::uint8_t>((status_ & 0x20) | last_sprite_num);
    }

    // Collision detection (SpriteChecker.cc:196-241/410-479). Skipped
    // entirely if a collision is already latched and unread (C bit set) --
    // the stored collision X/Y persist until an explicit S#5 read.
    if ((status_ & 0x20) != 0) {
        return;
    }
    const bool can0collide = (control_regs[8] & 0x20) != 0;  // TP bit SET (VDP.hh:195-201)

    for (int line = 0; line < height; ++line) {
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
            status_ = static_cast<std::uint8_t>(status_ | 0x20);
            // Fixed +12 (X) / +8 (Y) offsets, baked into the status-register
            // contract (SpriteChecker.cc:236-238/474-476).
            collision_x_ = min_x_collision + 12;
            collision_y_ = line + 8;
            return;  // don't check lines with a higher Y-coordinate
        }
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
    status_ = 0;
    collision_x_ = 0;
    collision_y_ = 0;
}

}  // namespace sony_msx::devices::video
