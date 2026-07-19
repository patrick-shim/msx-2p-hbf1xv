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

// Suite: Devices_SpriteEngineCheckUntilEquivalence_Unit  (M49-S1, backlog D9)
//
// AC-S1 byte-identity oracle: with NO mid-frame sprite-relevant write, driving
// the new progressive SpriteEngine::check_until() watermark pass line-by-line
// (or in arbitrary chunks) to `height` MUST produce byte-identical per-line
// visible-sprite buffers + S#0 5S/number + S#3-S#6 collision X/Y + the whole
// collision-event re-latch queue as the single-shot recompute_frame() sweep the
// pre-M49 code used. The single-shot path (recompute_frame -> begin_frame +
// check_until(height-1)) is itself pinned to the OLD algorithm's exact values by
// the existing sprite unit tests (video_sprite_engine_mode1/mode2/
// mode2_attribute_masking/collision_relatch), so proving incremental ==
// single-shot here transitively proves incremental == pre-M49 behavior.
//
// Non-tautology: the SAME frame is checked two ways -- one big call vs many
// small (and irregular-chunk) calls. Any incremental bug (a segment double-
// counting collision events, resetting the 5th-sprite latch per segment,
// mis-clearing the event queue on the first active segment, off-by-one in the
// watermark) makes the two paths DIVERGE and fails this oracle. A raster sweep
// of the S#0 collision re-latch is compared step-by-step, not just the settled
// value, so the event ORDER across segments is also pinned.

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "devices/video/sprite_engine.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace {

using sony_msx::devices::video::decode_vdp_mode;
using sony_msx::devices::video::SpriteEngine;
using sony_msx::devices::video::VdpModeState;
using sony_msx::devices::video::VdpVram;

int g_failures = 0;

void expect(const bool condition, const std::string& case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Planar (G6/G7) VRAM write: mirror the sprite engine's own rotate-right-by-1
// interleave (sprite_engine.cpp planar_address) so a planar config's sprite
// tables land where the engine reads them. even logical -> bank0 (addr>>1),
// odd -> bank1 (0x10000 + addr>>1).
void write_planar(VdpVram& vram, const std::uint32_t logical, const std::uint8_t value) {
    const std::uint32_t masked = logical & 0x1FFFFu;
    vram.write((masked >> 1) | ((masked & 1u) << 16), value);
}

struct Config {
    std::string name;
    std::array<std::uint8_t, 32> regs{};
    VdpModeState mode{};
    int height = 192;
    std::function<void(VdpVram&)> setup_vram;
};

// Compare two engines that were driven over the same frame(s): every per-line
// visible-sprite buffer, the settled S#0/S#3-S#6, and a full raster sweep of
// the boot-logo S#0 collision re-latch queue.
void compare_engines(SpriteEngine& single, SpriteEngine& incr, const int height, const std::string& name) {
    for (int line = 0; line < height; ++line) {
        const auto a = single.visible_sprites(line);
        const auto b = incr.visible_sprites(line);
        if (a.size() != b.size()) {
            expect(false, name + ": line " + std::to_string(line) + " visible-count mismatch");
            continue;
        }
        for (std::size_t k = 0; k < a.size(); ++k) {
            const bool same = a[k].pattern == b[k].pattern && a[k].x == b[k].x &&
                              a[k].color_attrib == b[k].color_attrib;
            expect(same, name + ": line " + std::to_string(line) + " sprite " + std::to_string(k) +
                             " field mismatch");
        }
    }
    expect(single.status_bits() == incr.status_bits(), name + ": S#0 status_bits mismatch");
    expect(single.collision_x() == incr.collision_x(), name + ": collision_x mismatch");
    expect(single.collision_y() == incr.collision_y(), name + ": collision_y mismatch");

    // Raster sweep of the S#0 C re-latch: clear 5S/C on both, then advance the
    // raster line-by-line asserting the C bit + coords stay lock-step. This
    // exercises collision_events_ ORDER, not just the settled latch.
    single.reset_status();
    incr.reset_status();
    for (int line = 0; line < height; ++line) {
        single.sync_collision_to_raster(line);
        incr.sync_collision_to_raster(line);
        const bool same = single.status_bits() == incr.status_bits() &&
                          single.collision_x() == incr.collision_x() &&
                          single.collision_y() == incr.collision_y();
        expect(same, name + ": collision re-latch diverged at raster line " + std::to_string(line));
        // Simulate an S#0 read-clear every 16 lines so later queued events get
        // a chance to re-latch (drives more of the queue than a single latch).
        if ((line % 16) == 15) {
            single.reset_status();
            incr.reset_status();
            single.consume_collision_events_up_to(line);
            incr.consume_collision_events_up_to(line);
        }
    }
}

// Drive `single` with the single-shot recompute_frame() and `incr` with an
// incremental check_until() sequence, over the same VRAM/regs, then compare.
// `chunks` selects the incremental stride pattern.
enum class Drive { PerLine, IrregularChunks };

void run_case(const Config& cfg, const Drive drive) {
    VdpVram vram;
    if (cfg.setup_vram) {
        cfg.setup_vram(vram);
    }
    const std::span<const std::uint8_t, 32> regs_span(cfg.regs);

    SpriteEngine single;
    single.recompute_frame(vram, regs_span, cfg.mode, cfg.height);

    SpriteEngine incr;
    incr.begin_frame(vram, regs_span, cfg.mode, cfg.height);
    if (drive == Drive::PerLine) {
        for (int line = 0; line < cfg.height; ++line) {
            incr.check_until(line);
        }
    } else {
        // Irregular, out-of-uniform-stride chunks; the final call reaches the
        // last line. A repeated/backward target must be a safe no-op.
        const int h = cfg.height;
        for (int t : {0, 3, 3, 30, 29, 31, 100, 99, h / 2, h - 20, h - 1, h - 1, h + 5}) {
            incr.check_until(t);
        }
    }

    const std::string tag = cfg.name + (drive == Drive::PerLine ? " [per-line]" : " [chunks]");
    compare_engines(single, incr, cfg.height, tag);
    // The incremental frame must be fully sealed at the same watermark.
    expect(incr.watermark() == cfg.height, tag + ": watermark did not reach height");
}

// --- config builders -------------------------------------------------------

std::array<std::uint8_t, 32> mode1_regs() {
    std::array<std::uint8_t, 32> r{};
    r[1] = 0x40;                 // display enable
    r[6] = 0x0800 >> 11;         // sprite pattern base -> 0x0800
    return r;
}

// Mode 1 attrib table at 0 (4 bytes/sprite: Y,X,pattern,color), pattern at
// 0x0800. Sentinel Y=208.
void m1_sprite(VdpVram& v, int i, std::uint8_t y, std::uint8_t x, std::uint8_t pat, std::uint8_t col) {
    const std::uint32_t base = static_cast<std::uint32_t>(i) * 4u;
    v.write(base + 0, y);
    v.write(base + 1, x);
    v.write(base + 2, pat);
    v.write(base + 3, col);
}
void m1_sentinel(VdpVram& v, int i) { v.write(static_cast<std::uint32_t>(i) * 4u, 208); }
void m1_pattern_row(VdpVram& v, std::uint8_t pat, int row, std::uint8_t byte) {
    v.write(0x0800u + static_cast<std::uint32_t>(pat) * 8u + static_cast<std::uint32_t>(row), byte);
}

std::array<std::uint8_t, 32> mode2_regs() {
    std::array<std::uint8_t, 32> r{};
    r[0] = 0x06;                 // GRAPHIC4 base 0x0C -> sprite mode 2
    r[1] = 0x40;                 // display enable
    r[5] = 0x07;                 // R#5 low bits = AND-mask set -> attr table at 0
    r[6] = 0x1000 >> 11;         // sprite pattern base -> 0x1000
    return r;
}

// Mode 2: color/attrib table at 0 (sprite*16 + line), Y/X/pattern at +512.
void m2_yxp(VdpVram& v, int i, std::uint8_t y, std::uint8_t x, std::uint8_t pat) {
    const std::uint32_t base = 512u + static_cast<std::uint32_t>(i) * 4u;
    v.write(base + 0, y);
    v.write(base + 1, x);
    v.write(base + 2, pat);
}
void m2_color(VdpVram& v, int i, int line, std::uint8_t col) {
    v.write(static_cast<std::uint32_t>(i) * 16u + static_cast<std::uint32_t>(line), col);
}
void m2_sentinel(VdpVram& v, int i) { v.write(512u + static_cast<std::uint32_t>(i) * 4u, 216); }
void m2_pattern_row(VdpVram& v, std::uint8_t pat, int row, std::uint8_t byte) {
    v.write(0x1000u + static_cast<std::uint32_t>(pat) * 8u + static_cast<std::uint32_t>(row), byte);
}

std::vector<Config> build_configs() {
    std::vector<Config> cfgs;

    // 1) Mode 1: one sprite at Y=0 (1-px shift), spread across many lines.
    {
        Config c;
        c.name = "M1_single_Y0";
        c.regs = mode1_regs();
        c.mode = decode_vdp_mode(0x00, c.regs[1], 0x00);
        c.setup_vram = [](VdpVram& v) {
            m1_sprite(v, 0, 0, 20, 0, 1);
            m1_sentinel(v, 1);
            for (int r = 0; r < 8; ++r) m1_pattern_row(v, 0, r, 0xFF);
        };
        cfgs.push_back(c);
    }
    // 2) Mode 1: 5 sprites same line (5th-sprite + earliest-line latch).
    {
        Config c;
        c.name = "M1_fifth_sprite";
        c.regs = mode1_regs();
        c.mode = decode_vdp_mode(0x00, c.regs[1], 0x00);
        c.setup_vram = [](VdpVram& v) {
            for (int i = 0; i < 5; ++i)
                m1_sprite(v, i, 40, static_cast<std::uint8_t>(i * 20), 0, static_cast<std::uint8_t>(i + 1));
            m1_sentinel(v, 5);
            for (int r = 0; r < 8; ++r) m1_pattern_row(v, 0, r, 0xFF);
        };
        cfgs.push_back(c);
    }
    // 3) Mode 1: collisions on several different lines (event-queue order).
    {
        Config c;
        c.name = "M1_multi_line_collisions";
        c.regs = mode1_regs();
        c.mode = decode_vdp_mode(0x00, c.regs[1], 0x00);
        c.setup_vram = [](VdpVram& v) {
            // Pair A overlapping around Y=20, pair B around Y=120.
            m1_sprite(v, 0, 20, 30, 0, 1);
            m1_sprite(v, 1, 20, 30, 1, 2);
            m1_sprite(v, 2, 120, 80, 0, 3);
            m1_sprite(v, 3, 120, 80, 1, 4);
            m1_sentinel(v, 4);
            for (int r = 0; r < 8; ++r) {
                m1_pattern_row(v, 0, r, 0xFF);
                m1_pattern_row(v, 1, r, 0xFF);
            }
        };
        cfgs.push_back(c);
    }
    // 4) Mode 1: EC + MAG + size16 (attribute/geometry paths).
    {
        Config c;
        c.name = "M1_EC_MAG_size16";
        c.regs = mode1_regs();
        c.regs[1] = 0x40 | 0x03;  // SI (size16) + MAG
        c.mode = decode_vdp_mode(0x00, c.regs[1], 0x00);
        c.setup_vram = [](VdpVram& v) {
            m1_sprite(v, 0, 10, 60, 0, static_cast<std::uint8_t>(0x80 | 1));  // EC set
            m1_sprite(v, 1, 90, 130, 0, 2);
            m1_sentinel(v, 2);
            for (int r = 0; r < 16; ++r) m1_pattern_row(v, 0, r, 0xAA);
        };
        cfgs.push_back(c);
    }
    // 5) Mode 2 (GRAPHIC4): per-line color, one plain sprite.
    {
        Config c;
        c.name = "M2_g4_perline_color";
        c.regs = mode2_regs();
        c.mode = decode_vdp_mode(c.regs[0], c.regs[1], 0x00);
        c.height = 212;
        c.setup_vram = [](VdpVram& v) {
            m2_yxp(v, 0, 30, 50, 0);
            m2_sentinel(v, 1);
            for (int line = 0; line < 16; ++line)
                m2_color(v, 0, line, static_cast<std::uint8_t>((line & 0x0F)));
            for (int r = 0; r < 8; ++r) m2_pattern_row(v, 0, r, 0xF0);
        };
        cfgs.push_back(c);
    }
    // 6) Mode 2: 9 sprites same line (9th-sprite) + a CC/IC collision-exclusion.
    {
        Config c;
        c.name = "M2_g4_ninth_and_CCIC";
        c.regs = mode2_regs();
        c.mode = decode_vdp_mode(c.regs[0], c.regs[1], 0x00);
        c.height = 212;
        c.setup_vram = [](VdpVram& v) {
            for (int i = 0; i < 9; ++i)
                m2_yxp(v, i, 50, static_cast<std::uint8_t>(i * 12), 0);
            m2_sentinel(v, 9);
            for (int i = 0; i < 9; ++i)
                for (int line = 0; line < 8; ++line)
                    // Give sprite 1 the IC bit (0x20) to exercise collision-exclusion.
                    m2_color(v, i, line, static_cast<std::uint8_t>((i == 1 ? 0x20 : 0x00) | 0x0A));
            for (int r = 0; r < 8; ++r) m2_pattern_row(v, 0, r, 0xFF);
        };
        cfgs.push_back(c);
    }
    // 7) Mode 2 planar (GRAPHIC7): sprite tables via the planar interleave.
    {
        Config c;
        c.name = "M2_g7_planar";
        c.regs = mode2_regs();
        c.regs[0] = 0x0E;         // GRAPHIC7 base 0x1C -> sprite mode 2 planar
        c.mode = decode_vdp_mode(c.regs[0], c.regs[1], 0x00);
        c.height = 212;
        c.setup_vram = [](VdpVram& v) {
            // Y/X/pattern at +512, color at sprite*16+line, pattern at 0x1000 --
            // all through the planar transform so the engine finds them.
            auto yxp = [&](int i, std::uint8_t y, std::uint8_t x, std::uint8_t pat) {
                const std::uint32_t b = 512u + static_cast<std::uint32_t>(i) * 4u;
                write_planar(v, b + 0, y);
                write_planar(v, b + 1, x);
                write_planar(v, b + 2, pat);
            };
            yxp(0, 40, 70, 0);
            yxp(1, 40, 74, 0);  // overlaps sprite 0 -> collision
            write_planar(v, 512u + 2u * 4u, 216);  // sentinel at sprite 2
            for (int line = 0; line < 8; ++line) {
                write_planar(v, 0u * 16u + static_cast<std::uint32_t>(line), 0x0C);
                write_planar(v, 1u * 16u + static_cast<std::uint32_t>(line), 0x0D);
            }
            for (int r = 0; r < 8; ++r) write_planar(v, 0x1000u + static_cast<std::uint32_t>(r), 0xFF);
        };
        cfgs.push_back(c);
    }
    // 8) TP-set (R#8 bit5) collision eligibility for color-0 sprites.
    {
        Config c;
        c.name = "M1_TP_color0_collision";
        c.regs = mode1_regs();
        c.regs[8] = 0x20;  // TP -> color-0 sprites can collide
        c.mode = decode_vdp_mode(0x00, c.regs[1], 0x00);
        c.setup_vram = [](VdpVram& v) {
            m1_sprite(v, 0, 15, 40, 0, 0);
            m1_sprite(v, 1, 15, 40, 1, 0);
            m1_sentinel(v, 2);
            for (int r = 0; r < 8; ++r) {
                m1_pattern_row(v, 0, r, 0xFF);
                m1_pattern_row(v, 1, r, 0xFF);
            }
        };
        cfgs.push_back(c);
    }
    return cfgs;
}

}  // namespace

int main() {
    for (const Config& cfg : build_configs()) {
        run_case(cfg, Drive::PerLine);
        run_case(cfg, Drive::IrregularChunks);
    }

    // --- Disabled / sprite-mode-0 frames leave status FROZEN identically. -----
    // Both engines run an ENABLED collision frame first (sets S#0), then a
    // disabled/mode-0 frame; the single-shot and incremental paths must agree
    // AND the frozen status must survive the second frame.
    {
        VdpVram vram;
        m1_sprite(vram, 0, 20, 30, 0, 1);
        m1_sprite(vram, 1, 20, 30, 1, 2);
        m1_sentinel(vram, 2);
        for (int r = 0; r < 8; ++r) {
            m1_pattern_row(vram, 0, r, 0xFF);
            m1_pattern_row(vram, 1, r, 0xFF);
        }
        auto regs_on = mode1_regs();
        const auto mode1 = decode_vdp_mode(0x00, regs_on[1], 0x00);

        // R#1 bit6 cleared -> display disabled (frame inactive).
        auto regs_off = regs_on;
        regs_off[1] = 0x00;

        SpriteEngine a;
        SpriteEngine b;
        // Frame 1 (enabled) on both.
        a.recompute_frame(vram, std::span<const std::uint8_t, 32>(regs_on), mode1, 192);
        b.begin_frame(vram, std::span<const std::uint8_t, 32>(regs_on), mode1, 192);
        for (int l = 0; l < 192; ++l) b.check_until(l);
        const std::uint8_t frozen_a = a.status_bits();
        expect((frozen_a & 0x20) != 0, "Disabled: precondition -- enabled frame set C");

        // Frame 2 (disabled) on both.
        a.recompute_frame(vram, std::span<const std::uint8_t, 32>(regs_off), mode1, 192);
        b.begin_frame(vram, std::span<const std::uint8_t, 32>(regs_off), mode1, 192);
        for (int l = 0; l < 192; ++l) b.check_until(l);

        expect(a.status_bits() == frozen_a, "Disabled: single-shot froze status across disabled frame");
        expect(b.status_bits() == frozen_a, "Disabled: incremental froze status across disabled frame");
        for (int l = 0; l < 192; ++l) {
            expect(a.visible_sprites(l).empty(), "Disabled: single-shot line cleared");
            expect(b.visible_sprites(l).empty(), "Disabled: incremental line cleared");
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
