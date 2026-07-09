// Suite: Devices_VdpPlanarInterleave_Unit
//
// Deterministic unit coverage for M21-S4's G6/G7 planar VRAM interleave
// (backlog D7, CPU-port piece + display-path piece), GRAPHIC6/GRAPHIC7
// content decode, and the genuine END-TO-END cross-path test (§1.4 of
// docs/m21-planner-package.md): write distinguishing bytes via the CPU port
// at chosen logical addresses while in G6/G7 mode, then render via
// VdpFrameRenderer, confirming the EXPECTED (not mirrored/garbled) pixel
// content -- the strong correctness signal for D7, since a CPU-port-only
// round-trip would pass even with the transform omitted entirely on BOTH
// sides. Grounded in references/openmsx-21.0/src/video/VDP.cc:849-857
// (executeCpuVramAccess) and VDPVRAM.hh:236-261 (getReadAreaPlanar) -- never
// copied (GPL isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_palette.h"

namespace {

using sony_msx::devices::video::expand3to5;
using sony_msx::devices::video::Field;
using sony_msx::devices::video::graphic7_fixed_color_to_rgb555;
using sony_msx::devices::video::pack_rgb555;
using sony_msx::devices::video::planar_row_spans;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::vdp_base_is_planar;
using sony_msx::devices::video::VdpFrameRenderer;

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

void set_palette(V9958Vdp& vdp, const int index, const std::uint8_t r3, const std::uint8_t g3,
                  const std::uint8_t b3) {
    set_register(vdp, 16, static_cast<std::uint8_t>(index & 0x0F));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

}  // namespace

int main() {
    // --- vdp_base_is_planar: correctly identifies G6 (0x14)/G7 (0x1C),
    //     excludes G5 (0x10). ---
    expect(vdp_base_is_planar(0x14), "VdpBaseIsPlanar_Graphic6_True");
    expect(vdp_base_is_planar(0x1C), "VdpBaseIsPlanar_Graphic7_True");
    expect(!vdp_base_is_planar(0x10), "VdpBaseIsPlanar_Graphic5_False");

    // --- D7 CPU-port piece: writing at a logical EVEN address lands in
    //     physical bank0 (0x00000-0x0FFFF); ODD lands in bank1
    //     (0x10000-0x1FFFF) -- verified via the RAW physical vram().read(),
    //     independent of any round-trip read-back (A-M21-10). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0A);  // M3+M5 -> GRAPHIC6 (base 0x14, planar)

        set_write_address(vdp, 0);  // logical 0 (even)
        vdp.io_write(0x98, 0x11);
        expect(vdp.vram().read(0x00000) == 0x11, "PlanarCpuPort_EvenLogicalAddress_LandsBank0");

        set_write_address(vdp, 1);  // logical 1 (odd)
        vdp.io_write(0x98, 0x22);
        expect(vdp.vram().read(0x10000) == 0x22, "PlanarCpuPort_OddLogicalAddress_LandsBank1");

        // With R#14 A16-A14 contributing: logical 0x4000 (even) -> bank0
        // offset 0x2000; logical 0x4001 (odd) -> bank1 offset 0x2000.
        set_register(vdp, 14, 0x01);
        set_write_address(vdp, 0x0000);  // (1<<14)|0 = 0x4000
        vdp.io_write(0x98, 0x33);
        expect(vdp.vram().read(0x2000) == 0x33, "PlanarCpuPort_R14Contributes_EvenLandsBank0Offset");

        set_write_address(vdp, 0x0001);  // (1<<14)|1 = 0x4001
        vdp.io_write(0x98, 0x44);
        expect(vdp.vram().read(0x12000) == 0x44, "PlanarCpuPort_R14Contributes_OddLandsBank1Offset");
    }

    // --- A-M21-12 regression guard: advance_vram_pointer()/R#14-carry is
    //     UNCHANGED by the D7 edit. The FULL existing M14 pointer-carry
    //     suite (tests/unit/devices/video_v9958_ports_unit_test.cpp) is
    //     re-run UNMODIFIED as part of this milestone's evidence gate; this
    //     local check re-confirms the carry rule survives even while a
    //     planar mode is selected (a case the ORIGINAL M14 suite -- written
    //     before D7 existed -- never exercised).
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0A);   // GRAPHIC6 (planar)
        set_register(vdp, 14, 0x00);
        set_write_address(vdp, 0x3FFF);
        vdp.io_write(0x98, 0x55);  // ptr 0x3FFF -> 0, R#14 -> 1 (V9938-mode carry)
        expect(vdp.control_register(14) == 0x01, "PlanarMode_R14Carry_StillIncrementsOnWrap");
    }

    // --- D7 display-path piece: planar_row_spans cross-checked against a
    //     byte-by-byte application of the rotate formula for a full
    //     synthetic 256-byte row (A-M21-11). ---
    {
        constexpr std::uint32_t kRowBase = 0x400;  // arbitrary EVEN logical base
        const auto spans = planar_row_spans(kRowBase, 256);
        expect(spans.half_length == 128, "PlanarRowSpans_HalfLength_Is128For256ByteRow");

        bool all_match = true;
        for (std::uint32_t i = 0; i < 256; ++i) {
            const std::uint32_t logical = (kRowBase + i) & 0x1FFFF;
            const std::uint32_t rotated = (logical >> 1) | ((logical & 1) << 16);  // A-M21-10
            const std::uint32_t via_spans =
                (i % 2 == 0) ? (spans.bank0_base + i / 2) : (spans.bank1_base + i / 2);
            if (rotated != via_spans) {
                all_match = false;
            }
        }
        expect(all_match, "PlanarRowSpans_MatchesByteByByteRotate_Full256ByteRow");
    }

    // --- GRAPHIC6 (SCREEN7) content decode: 4bpp planar. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0A);  // GRAPHIC6
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 512, "Graphic6_Width_Is512");

        set_palette(vdp, 0xA, 1, 0, 0);
        set_palette(vdp, 0xB, 0, 1, 0);
        set_palette(vdp, 0xC, 0, 0, 1);
        set_palette(vdp, 0xD, 1, 1, 1);

        // Logical row base for line0/page0 is 0; logical byte 0 (even) ->
        // bank0[0], logical byte 1 (odd) -> bank1[0] -- EXACTLY the CPU
        // port's own transform (the cross-path proof below reuses this).
        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0xAB);  // logical 0 -> bank0[0] = 0xAB
        vdp.io_write(0x98, 0xCD);  // logical 1 -> bank1[0] = 0xCD

        std::uint16_t row[512];
        renderer.render_line(0, Field::Progressive, row);
        const std::uint16_t pa = pack_rgb555(expand3to5(1), expand3to5(0), expand3to5(0));
        const std::uint16_t pb = pack_rgb555(expand3to5(0), expand3to5(1), expand3to5(0));
        const std::uint16_t pc = pack_rgb555(expand3to5(0), expand3to5(0), expand3to5(1));
        const std::uint16_t pd = pack_rgb555(expand3to5(1), expand3to5(1), expand3to5(1));
        expect(row[0] == pa, "Graphic6_CrossPath_Pixel0_MatchesCpuPortWrite_NotGarbled");
        expect(row[1] == pb, "Graphic6_CrossPath_Pixel1_MatchesCpuPortWrite_NotGarbled");
        expect(row[2] == pc, "Graphic6_CrossPath_Pixel2_MatchesCpuPortWrite_NotGarbled");
        expect(row[3] == pd, "Graphic6_CrossPath_Pixel3_MatchesCpuPortWrite_NotGarbled");
    }

    // --- GRAPHIC7 (SCREEN8) fixed-256-color decode, INCLUDING the
    //     GGGRRRBB byte-order boundary (A-M21-4), exercised through the
    //     FULL renderer (not just the palette-only unit test). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(vdp, 0, 0x0E);  // M3+M4+M5 -> GRAPHIC7 (base 0x1C)
        const VdpFrameRenderer renderer(vdp);
        expect(renderer.width() == 256, "Graphic7_Width_Is256");

        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0b111'000'00);  // logical0 -> bank0[0]: green=7,red=0,blue=0
        vdp.io_write(0x98, 0b000'111'00);  // logical1 -> bank1[0]: red=7,green=0,blue=0

        std::uint16_t row[256];
        renderer.render_line(0, Field::Progressive, row);
        expect(row[0] == graphic7_fixed_color_to_rgb555(0b111'000'00),
               "Graphic7_CrossPath_Pixel0_MaxGreen_NotMaxRed");
        expect(row[1] == graphic7_fixed_color_to_rgb555(0b000'111'00), "Graphic7_CrossPath_Pixel1_MaxRed");
        // Independently re-assert the exact boundary claim inline: TOP 3
        // bits are green, not red.
        {
            const std::uint16_t px = row[0];
            expect(((px >> 5) & 0x1F) == 31 /* green */, "Graphic7_CrossPath_TopBitsAreGreen");
            expect(((px >> 10) & 0x1F) == 0 /* red */, "Graphic7_CrossPath_RedIsZero");
        }
    }

    // --- Two-run determinism (planar). ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 0, 0x0A);
            set_write_address(*vdp, 10);
            vdp->io_write(0x98, 0x5A);
            vdp->io_write(0x98, 0xA5);
        }
        const VdpFrameRenderer ra(vdp_a);
        const VdpFrameRenderer rb(vdp_b);
        expect(ra.render_frame().pixels == rb.render_frame().pixels, "Planar_Determinism_ByteIdenticalFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
