// Suite: Devices_VdpScanlineAccumulator_Unit (M32-S1, Defect A of DEC-0039,
// docs/m32-planner-package.md §3-S1 / test matrix row 1)
//
// THE STATIC-FRAME EQUIVALENCE HARD ORACLE (AC-4): for any VDP state with NO
// writes interleaved between sync points, the scanline accumulator's output
// -- synced in randomized (explicitly seeded) line-chunk patterns, then
// finalized -- must be BYTE-IDENTICAL to the legacy frozen-snapshot
// VdpFrameRenderer::render_frame() across the full mode matrix
// (TEXT1/TEXT2/G1/G2/G3/MC/G4/G5/G6/G7/YJK/YJK+YAE/blank), sprites on/off,
// R#23 != 0. The legacy renderer is deliberately UNTOUCHED by M32 -- it is
// the in-test reference oracle (§2.2).
//
// Plus: compose() partial-accumulation equivalence, wrap-safety reset
// (backwards sync = the DEC-0034 step-only loop-shape class), deterministic
// 256<->512 geometry adaptation (§2.4 policy, named remainder D10), and
// two-run byte identity.

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_scanline_accumulator.h"

namespace {

using sony_msx::devices::video::Field;
using sony_msx::devices::video::FrameBuffer;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpFrameRenderer;
using sony_msx::devices::video::VdpScanlineAccumulator;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

void set_write_address(V9958Vdp& vdp, const std::uint16_t addr) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
}

// Deterministic pseudo-random VRAM content: one address setup, then
// sequential auto-incrementing #98 writes (explicitly seeded LCG,
// tests/CLAUDE.md randomness rule).
struct Lcg {
    std::uint32_t state;
    explicit Lcg(const std::uint32_t seed) : state(seed) {}
    std::uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state >> 16;
    }
};

void fill_vram(V9958Vdp& vdp, const std::uint16_t base, const int count, const std::uint32_t seed) {
    Lcg lcg(seed);
    set_write_address(vdp, base);
    for (int i = 0; i < count; ++i) {
        vdp.io_write(0x98, static_cast<std::uint8_t>(lcg.next() & 0xFF));
    }
}

// One mode-matrix configuration applied to a fresh VDP.
struct ModeConfig {
    const char* name;
    std::uint8_t r0;
    std::uint8_t r1;
    std::uint8_t r25;
    std::uint8_t r23;      // vertical scroll
    bool sprites;          // program mode-2 sprites + on_vsync() recompute
};

void apply_config(V9958Vdp& vdp, const ModeConfig& cfg, const std::uint32_t seed) {
    set_register(vdp, 0, cfg.r0);
    set_register(vdp, 1, cfg.r1);
    set_register(vdp, 25, cfg.r25);
    set_register(vdp, 23, cfg.r23);
    fill_vram(vdp, 0x0000, 0x2000, seed);
    if (cfg.sprites) {
        // Mode-2 sprite tables at 0x2800 (colors) / 0x2A00 (Y/X/pattern),
        // pattern base 0x3000 (the sprites unit test's table idiom).
        set_register(vdp, 5, static_cast<std::uint8_t>((0x2800 >> 7) | 0x07));
        set_register(vdp, 6, 0x3000 >> 11);
        set_write_address(vdp, 0x2A00);
        vdp.io_write(0x98, 40);   // sprite0 y
        vdp.io_write(0x98, 32);   // x
        vdp.io_write(0x98, 4);    // pattern
        vdp.io_write(0x98, 0);
        set_write_address(vdp, 0x2A04);
        vdp.io_write(0x98, 208);  // sentinel for sprite1 (mode-2 uses 216; 208
                                  // also terminates in mode 1 -- harmless here)
        set_write_address(vdp, 0x2A04);
        vdp.io_write(0x98, 216);  // mode-2 end marker
        Lcg lcg(seed ^ 0x5A5Au);
        set_write_address(vdp, 0x2800);
        for (int i = 0; i < 16; ++i) {
            vdp.io_write(0x98, static_cast<std::uint8_t>(0x08 | (lcg.next() & 0x07)));
        }
        set_write_address(vdp, 0x3000 + 4 * 8);
        for (int i = 0; i < 8; ++i) {
            vdp.io_write(0x98, 0xF0);
        }
        // Frame-hook recompute (sprite visibility tables are computed once
        // per frame by on_vsync(), M22).
        vdp.on_vsync();
    }
}

bool frames_equal(const FrameBuffer& a, const FrameBuffer& b) {
    return a.width == b.width && a.height == b.height && a.border_color == b.border_color &&
           a.pixels == b.pixels;
}

// Randomized-chunk accumulation, then finalize; returns the sealed frame.
FrameBuffer accumulate_chunked(VdpScanlineAccumulator& acc, const VdpFrameRenderer& renderer,
                               const std::uint32_t seed) {
    Lcg lcg(seed);
    const int height = renderer.height();
    int pos = 0;
    while (pos < height) {
        pos += 1 + static_cast<int>(lcg.next() % 37);
        acc.sync_to_line(pos < height ? pos : height);
    }
    acc.finalize(Field::Progressive);
    return acc.completed_frame();
}

}  // namespace

int main() {
    // M34 re-grounding (docs/m34-planner-package.md §3.4.2): every content
    // row now sets R#1 bit6 (BL, display enable) -- the M34 render gate
    // blanks BL=0 lines, and pre-M34 the background renderer simply ignored
    // the bit, so adding it preserves each row's rendered bytes while
    // keeping the row a genuine CONTENT-equivalence case. The matrix
    // additionally gains explicit BL=0 configurations (pure-backdrop lines):
    // the accumulator-vs-legacy equivalence must hold for them by
    // construction (the gate lives inside the shared render_line()).
    const ModeConfig configs[] = {
        // name        R#0   R#1   R#25  R#23 sprites
        {"Text1", 0x00, 0x50, 0x00, 0, false},
        {"Text2", 0x04, 0x50, 0x00, 0, false},
        {"Graphic1", 0x00, 0x40, 0x00, 0, false},
        {"Graphic2", 0x02, 0x40, 0x00, 0, false},
        {"Graphic3", 0x04, 0x40, 0x00, 0, false},
        {"Multicolor", 0x00, 0x48, 0x00, 0, false},
        {"Graphic4", 0x06, 0x40, 0x00, 0, false},
        {"Graphic4_Scrolled", 0x06, 0x40, 0x00, 100, false},
        {"Graphic4_Sprites", 0x06, 0x40, 0x00, 0, true},
        {"Graphic5", 0x08, 0x40, 0x00, 0, false},
        {"Graphic6", 0x0A, 0x40, 0x00, 0, false},
        {"Graphic7", 0x0E, 0x40, 0x00, 0, false},
        {"ScreenYjk", 0x0E, 0x40, 0x08, 0, false},
        {"ScreenYjkYae", 0x0E, 0x40, 0x18, 0, false},
        {"Blank_UndefinedModeBits", 0x02, 0x50, 0x00, 0, false},
        // M34 §3.4.2 BL=0 rows: the whole frame is backdrop on both sides.
        {"Graphic4_BL0_Backdrop", 0x06, 0x00, 0x00, 0, false},
        {"Text2_BL0_Backdrop", 0x04, 0x10, 0x00, 0, false},
        {"Graphic7_BL0_Backdrop", 0x0E, 0x00, 0x00, 0, false},
        {"Graphic4_BL0_SpritesConfigured", 0x06, 0x00, 0x00, 0, true},
    };

    // --- 1. AC-4 HARD ORACLE: randomized chunked accumulation ==
    //     legacy frozen snapshot, across the whole mode matrix. ---
    {
        std::uint32_t seed = 0xC0FFEEu;
        for (const ModeConfig& cfg : configs) {
            V9958Vdp vdp;
            apply_config(vdp, cfg, seed);
            const VdpFrameRenderer renderer(vdp);
            VdpScanlineAccumulator acc(vdp, renderer);

            const FrameBuffer legacy = renderer.render_frame(Field::Progressive);
            const FrameBuffer sealed = accumulate_chunked(acc, renderer, seed ^ 0x1234u);
            if (!frames_equal(sealed, legacy)) {
                std::cerr << "  (mode config: " << cfg.name << ")\n";
                expect(false, "StaticFrame_ChunkedAccumulation_ByteIdenticalToLegacySnapshot");
            }
            seed += 17;
        }
        std::cout << "Mode matrix: " << (sizeof(configs) / sizeof(configs[0]))
                  << " configs checked against the legacy snapshot oracle\n";
    }

    // --- 2. compose(): partial accumulation + live projection ==
    //     legacy snapshot for static state, at several watermarks. ---
    {
        V9958Vdp vdp;
        apply_config(vdp, configs[6], 0xBEEFu);  // Graphic4
        const VdpFrameRenderer renderer(vdp);
        VdpScanlineAccumulator acc(vdp, renderer);
        const FrameBuffer legacy = renderer.render_frame(Field::Progressive);

        bool all_equal = true;
        for (const int watermark : {0, 1, 50, 191, 192}) {
            acc.sync_to_line(watermark);
            if (!frames_equal(acc.compose(Field::Progressive), legacy)) {
                all_equal = false;
            }
        }
        expect(all_equal, "Compose_PartialAccumulation_StaticState_EqualsLegacySnapshot");
    }

    // --- 3. Idempotence: repeated/backward-equal syncs are no-ops. ---
    {
        V9958Vdp vdp;
        apply_config(vdp, configs[6], 0x7777u);
        const VdpFrameRenderer renderer(vdp);
        VdpScanlineAccumulator acc(vdp, renderer);
        acc.sync_to_line(80);
        expect(acc.watermark() == 80, "SyncToLine_AdvancesWatermark");
        acc.sync_to_line(80);
        acc.sync_to_line(0);   // clamped border write position -- must not reset
        expect(acc.watermark() == 80, "SyncToLine_EqualOrZero_NoOp");
        acc.sync_to_line(-5);  // negative raster clamp
        expect(acc.watermark() == 80, "SyncToLine_NegativeClamp_NoOp");
    }

    // --- 4. Wrap safety (§2.2): a BACKWARDS sync (raster wrapped without a
    //     finalize -- the DEC-0034 step-only loop-shape class) seals the old
    //     frame deterministically and starts over; no crash, and the sealed
    //     result for static state still equals the legacy snapshot. ---
    {
        V9958Vdp vdp;
        apply_config(vdp, configs[6], 0x2468u);
        const VdpFrameRenderer renderer(vdp);
        VdpScanlineAccumulator acc(vdp, renderer);
        const FrameBuffer legacy = renderer.render_frame(Field::Progressive);

        acc.sync_to_line(150);
        acc.sync_to_line(10);  // wrapped: 10 < 150
        expect(acc.has_completed_frame(), "WrapSafety_BackwardSync_SealsPreviousFrame");
        expect(frames_equal(acc.completed_frame(), legacy),
               "WrapSafety_SealedFrame_StaticState_EqualsLegacy");
        expect(acc.watermark() == 10, "WrapSafety_RestartsAccumulationAtNewPosition");
        acc.finalize(Field::Progressive);
        expect(frames_equal(acc.completed_frame(), legacy),
               "WrapSafety_NextFinalize_StillEqualsLegacy");
    }

    // --- 5. Geometry adaptation (§2.4 policy, remainder D10): lines
    //     accumulated at 256-wide (GRAPHIC4) adapt by pixel-doubling when
    //     the frame finalizes at 512-wide (GRAPHIC5), deterministically. ---
    {
        const auto run = [] {
            V9958Vdp vdp;
            apply_config(vdp, ModeConfig{"G4", 0x06, 0x00, 0x00, 0, false}, 0x1357u);
            const VdpFrameRenderer renderer(vdp);
            VdpScanlineAccumulator acc(vdp, renderer);

            std::vector<std::uint16_t> row0_g4(256);
            renderer.render_line(0, Field::Progressive, std::span<std::uint16_t>(row0_g4));

            acc.sync_to_line(50);                 // 50 rows committed at width 256
            set_register(vdp, 0, 0x08);           // switch to GRAPHIC5 (512-wide)
            acc.finalize(Field::Progressive);
            return std::make_pair(acc.completed_frame(), row0_g4);
        };
        const auto [frame, row0_g4] = run();
        expect(frame.width == 512 && frame.height == 192, "GeometryAdapt_FinalFrame_EndOfFrameGeometry");
        bool doubled = true;
        for (int x = 0; x < 256; ++x) {
            if (frame.at(2 * x, 0) != row0_g4[static_cast<std::size_t>(x)] ||
                frame.at(2 * x + 1, 0) != row0_g4[static_cast<std::size_t>(x)]) {
                doubled = false;
            }
        }
        expect(doubled, "GeometryAdapt_256To512_PixelDoubled");
        const auto [frame_b, row0_b] = run();
        expect(frame.pixels == frame_b.pixels && row0_g4 == row0_b,
               "GeometryAdapt_TwoRuns_ByteIdentical");
    }

    // --- 6. Two-run determinism of the full chunked pipeline. ---
    {
        const auto run = [] {
            V9958Vdp vdp;
            apply_config(vdp, ModeConfig{"G4s", 0x06, 0x40, 0x00, 37, true}, 0xACE1u);
            const VdpFrameRenderer renderer(vdp);
            VdpScanlineAccumulator acc(vdp, renderer);
            return accumulate_chunked(acc, renderer, 0x99u);
        };
        const FrameBuffer a = run();
        const FrameBuffer b = run();
        expect(frames_equal(a, b), "ChunkedPipeline_TwoRuns_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpScanlineAccumulator_Unit cases passed\n";
    return 0;
}
