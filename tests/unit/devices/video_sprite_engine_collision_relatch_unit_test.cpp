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

// Suite: Devices_SpriteEngineCollisionRelatch_Unit  (boot-logo fix)
//
// Line-granular S#0 collision (C, bit5) re-latching against the live raster
// position. Real hardware checks sprites PROGRESSIVELY as the raster scans
// (references/openmsx-21.0/src/video/SpriteChecker.hh:70-100 sync()/
// checkSprites -- behavior reference, never copied): after an S#0 read
// clears C, the NEXT colliding line the raster scans re-latches it. The
// HB-F1XV MSX2+ boot-logo wobble effect (SUB-ROM loop polling S#0 bit5, one
// R#26/R#27 scroll write per poll exit) paces on exactly this; a
// frame-granular C model runs it two orders of magnitude too slowly and the
// logo never completes.
//
// Deterministic oracle: engine-level event/latch sequences and V9958-level
// S#0 read sequences under a settable fake VdpClockSource are exact-value
// asserted; no wall clock anywhere.

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

#include "devices/video/sprite_engine.h"
#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_access_timing.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace {

using sony_msx::devices::video::decode_vdp_mode;
using sony_msx::devices::video::SpriteEngine;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;
using sony_msx::devices::video::VdpVram;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Deterministic settable raster clock (X-pattern of the RTC/FDC stub clocks).
class FakeVdpClock final : public VdpClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_tstates_since_vsync() const override { return tstates; }
    std::uint64_t tstates = 0;
};

// --- engine-level harness (sprite mode 1, GRAPHIC1) ------------------------
// Two 8x8 sprites fully overlapping at (x=50, y=9): both visible on output
// lines 10..17 (the A-M22-9 1-pixel vertical shift), so the frame carries
// one collision event per line 10..17 at x=50 (S#3/S#5 report +12/+8:
// x=62, y=line+8).

constexpr std::uint16_t kPatternBase = 0x0800;

void setup_overlapping_sprites(VdpVram& vram) {
    // Attribute table at 0: sprite0 (9,50,pat0,color1), sprite1
    // (9,50,pat1,color2), sentinel Y=208 at index 2.
    const std::uint8_t sprite0[4] = {9, 50, 0, 1};
    const std::uint8_t sprite1[4] = {9, 50, 1, 2};
    for (int i = 0; i < 4; ++i) {
        vram.write(static_cast<std::uint32_t>(0 + i), sprite0[i]);
        vram.write(static_cast<std::uint32_t>(4 + i), sprite1[i]);
    }
    vram.write(8, 208);
    for (int row = 0; row < 8; ++row) {
        vram.write(static_cast<std::uint32_t>(kPatternBase + 0 * 8 + row), 0xFF);
        vram.write(static_cast<std::uint32_t>(kPatternBase + 1 * 8 + row), 0xFF);
    }
}

std::array<std::uint8_t, 32> make_mode1_regs() {
    std::array<std::uint8_t, 32> regs{};
    regs[1] = 0x40;                                       // display enable
    regs[6] = static_cast<std::uint8_t>(kPatternBase >> 11);  // sprite pattern base
    return regs;
}

}  // namespace

int main() {
    const auto mode = decode_vdp_mode(0x00, 0x00, 0x00);  // GRAPHIC1 -> sprite mode 1

    // --- Frame-boundary latch preserved: recompute latches the FIRST event
    //     (line 10), later events stay queued. ---
    {
        VdpVram vram;
        setup_overlapping_sprites(vram);
        SpriteEngine engine;
        const auto regs = make_mode1_regs();
        engine.recompute_frame(vram, regs, mode, 192);

        expect((engine.status_bits() & 0x20) != 0, "Recompute_LatchesFirstEvent_CSet");
        expect(engine.collision_x() == 62, "Recompute_FirstEvent_X_PlusTwelve");
        expect(engine.collision_y() == 18, "Recompute_FirstEvent_Y_Line10PlusEight");
    }

    // --- Read-clear then raster advance: C re-latches per colliding LINE,
    //     one latch per read/line; past events never re-latch. ---
    {
        VdpVram vram;
        setup_overlapping_sprites(vram);
        SpriteEngine engine;
        const auto regs = make_mode1_regs();
        engine.recompute_frame(vram, regs, mode, 192);

        // Simulate the S#0 read-clear at raster display line 10.
        engine.reset_status();
        engine.consume_collision_events_up_to(10);
        expect((engine.status_bits() & 0x20) == 0, "AfterReadClear_CClear");

        // Raster still on line 10: the next event (line 11) is not due yet.
        engine.sync_collision_to_raster(10);
        expect((engine.status_bits() & 0x20) == 0, "SameLine_NoRelatchYet");

        // Raster reaches line 11: the line-11 event latches.
        engine.sync_collision_to_raster(11);
        expect((engine.status_bits() & 0x20) != 0, "NextLineScanned_CRelatches");
        expect(engine.collision_y() == 19, "NextLineScanned_CoordsAreLine11Event");

        // While latched-unread, nothing further updates.
        engine.sync_collision_to_raster(17);
        expect(engine.collision_y() == 19, "LatchedUnread_CoordsFrozen");

        // Clear at line 17: every remaining event is in the raster's past.
        engine.reset_status();
        engine.consume_collision_events_up_to(17);
        engine.sync_collision_to_raster(500);
        expect((engine.status_bits() & 0x20) == 0, "AllEventsConsumed_NoRelatch");
    }

    // --- Clockless sentinel (INT_MIN): both hooks are no-ops, preserving
    //     the pre-fix frame-granular behavior for clockless machines. ---
    {
        VdpVram vram;
        setup_overlapping_sprites(vram);
        SpriteEngine engine;
        const auto regs = make_mode1_regs();
        engine.recompute_frame(vram, regs, mode, 192);
        engine.reset_status();
        engine.consume_collision_events_up_to(std::numeric_limits<int>::min());
        engine.sync_collision_to_raster(std::numeric_limits<int>::min());
        expect((engine.status_bits() & 0x20) == 0, "ClocklessSentinel_NoRelatch");
    }

    // --- V9958-level: S#0 reads through the real #99 port path with a fake
    //     raster clock. The pre-fix model kept C clear for the whole rest of
    //     the frame after one read; the fixed model re-latches per colliding
    //     line as the clock advances. ---
    {
        using sony_msx::devices::video::vdp_access_timing::kCpuTstatesPerLine;
        constexpr int kNonActiveLinesLn0 = 70;  // 262-192, matches S#2 VR arithmetic

        V9958Vdp vdp;
        FakeVdpClock clock;
        vdp.attach_clock_source(&clock);

        auto set_register = [&](const std::uint8_t reg, const std::uint8_t value) {
            vdp.io_write(0x99, value);
            vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
        };
        auto write_vram = [&](const std::uint16_t addr, const std::uint8_t value) {
            vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
            vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
            vdp.io_write(0x98, value);
        };
        auto at_display_line = [&](const int line) {
            clock.tstates = static_cast<std::uint64_t>(kNonActiveLinesLn0 + line) *
                                static_cast<std::uint64_t>(kCpuTstatesPerLine) +
                            5;
        };

        set_register(1, 0x40);  // display enable (GRAPHIC1 reset mode, sprite mode 1)
        set_register(6, kPatternBase >> 11);
        const std::uint8_t sprite0[4] = {9, 50, 0, 1};
        const std::uint8_t sprite1[4] = {9, 50, 1, 2};
        for (std::uint16_t i = 0; i < 4; ++i) {
            write_vram(static_cast<std::uint16_t>(0 + i), sprite0[i]);
            write_vram(static_cast<std::uint16_t>(4 + i), sprite1[i]);
        }
        write_vram(8, 208);
        for (std::uint16_t row = 0; row < 8; ++row) {
            write_vram(static_cast<std::uint16_t>(kPatternBase + row), 0xFF);
            write_vram(static_cast<std::uint16_t>(kPatternBase + 8 + row), 0xFF);
        }

        vdp.on_vsync();
        set_register(15, 0);  // S#0 selected (the BIOS poll leaves it here too)

        at_display_line(10);
        const std::uint8_t first = vdp.io_read(0x99);
        expect((first & 0x20) != 0, "V9958_FirstRead_CSet");

        const std::uint8_t second = vdp.io_read(0x99);
        expect((second & 0x20) == 0, "V9958_SecondReadSameLine_CCleared");

        at_display_line(11);
        const std::uint8_t third = vdp.io_read(0x99);
        expect((third & 0x20) != 0, "V9958_NextLine_CRelatched_TheBootLogoPacer");

        const std::uint8_t fourth = vdp.io_read(0x99);
        expect((fourth & 0x20) == 0, "V9958_FourthReadSameLine_CCleared");

        at_display_line(12);
        expect((vdp.io_read(0x99) & 0x20) != 0, "V9958_Line12_CRelatchedAgain");

        // Determinism oracle: a second identically-driven machine produces
        // the identical read sequence.
        // (Covered implicitly by exact-value asserts above on fixed inputs.)
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_SpriteEngineCollisionRelatch_Unit cases passed\n";
    return 0;
}
