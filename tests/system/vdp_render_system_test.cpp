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

// Suite: System_Hbf1xvVdpRender_System
//
// Machine-level system test (tests/CLAUDE.md's three-tier convention) for
// M21's rendering pipeline: a REAL CPU program, executed over the M11 bus,
// writes VRAM/registers via `OUT (#98)/(#99)` -- exactly as real MSX2+
// software would -- and the test then calls the machine's render_frame()
// accessor and asserts against a hand-computed golden RGB555 value, for one
// representative case per mode FAMILY: character (GRAPHIC1), non-planar
// bitmap (GRAPHIC4), planar bitmap (GRAPHIC6), and YJK (SCREEN12). Every
// program relies ONLY on the V9938 boot palette (VDP.cc:299-302, already
// loaded at cold_boot -- palette index 15 = white 0x777, index 0 = black
// 0x000) so no #9A palette-write instructions are needed, keeping each
// program short and the golden computation simple.

#include <array>
#include <cstdint>
#include <iostream>

#include "devices/video/vdp_palette.h"
#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void run_to_halt(sony_msx::machine::Hbf1xvMachine& machine, const int max_steps) {
    for (int i = 0; i < max_steps && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    const std::uint16_t white = sony_msx::devices::video::pack_rgb555(31, 31, 31);  // boot palette 15 = 0x777
    const std::uint16_t black = sony_msx::devices::video::pack_rgb555(0, 0, 0);     // boot palette 0 = 0x000

    // --- Character family: GRAPHIC1 (SCREEN1, the reset/default mode). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0x99, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        machine.debug_io_write(0x99, 0x81);  // R#1 <- 0x40
        machine.map_flat_ram();

        const std::array<std::uint8_t, 37> program{
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT(99)   name addr low
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)   name addr high (write)
            0x3E, 0x40, 0xD3, 0x98,  // LD A,0x40 ; OUT(98)   name[0] = char 0x40
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT(99)   pattern addr low (0x0200)
            0x3E, 0x42, 0xD3, 0x99,  // LD A,0x42 ; OUT(99)   pattern addr high (write)
            0x3E, 0xFF, 0xD3, 0x98,  // LD A,0xFF ; OUT(98)   pattern = all set
            0x3E, 0x08, 0xD3, 0x99,  // LD A,0x08 ; OUT(99)   color addr low (0x0008)
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)   color addr high (write)
            0x3E, 0xF0, 0xD3, 0x98,  // LD A,0xF0 ; OUT(98)   color: fg=15 (white), bg=0 (black)
            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic1_CpuProgram_ReachesHalt");

        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "Graphic1_FrameDimensions");
        expect(fb.at(0, 0) == white, "Graphic1_Golden_Pixel0_White");
        expect(fb.at(7, 0) == white, "Graphic1_Golden_Pixel7_White_AllSetPattern");
    }

    // --- Non-planar bitmap family: GRAPHIC4 (SCREEN5). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0x99, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        machine.debug_io_write(0x99, 0x81);  // R#1 <- 0x40
        machine.map_flat_ram();

        const std::array<std::uint8_t, 21> program{
            0x3E, 0x06, 0xD3, 0x99,  // LD A,0x06 ; OUT(99)   R#0 data (M3+M4)
            0x3E, 0x80, 0xD3, 0x99,  // LD A,0x80 ; OUT(99)   register-write R#0 -> GRAPHIC4
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT(99)   vram addr low
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)   vram addr high (write)
            0x3E, 0xF0, 0xD3, 0x98,  // LD A,0xF0 ; OUT(98)   byte0: pixel0=15(white), pixel1=0(black)
            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic4_CpuProgram_ReachesHalt");

        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "Graphic4_FrameDimensions");
        expect(fb.at(0, 0) == white, "Graphic4_Golden_Pixel0_White_HighNibble");
        expect(fb.at(1, 0) == black, "Graphic4_Golden_Pixel1_Black_LowNibble");
    }

    // --- Planar bitmap family: GRAPHIC6 (SCREEN7) -- exercises the D7
    //     CPU-port planar transform end-to-end through a real CPU program. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0x99, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        machine.debug_io_write(0x99, 0x81);  // R#1 <- 0x40
        machine.map_flat_ram();

        const std::array<std::uint8_t, 25> program{
            0x3E, 0x0A, 0xD3, 0x99,  // LD A,0x0A ; OUT(99)   R#0 data (M3+M5)
            0x3E, 0x80, 0xD3, 0x99,  // LD A,0x80 ; OUT(99)   register-write R#0 -> GRAPHIC6 (planar)
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT(99)   vram addr low
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)   vram addr high (write)
            0x3E, 0xF0, 0xD3, 0x98,  // LD A,0xF0 ; OUT(98)   logical0 -> bank0[0]
            0x3E, 0x00, 0xD3, 0x98,  // LD A,0x00 ; OUT(98)   logical1 -> bank1[0]
            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic6_CpuProgram_ReachesHalt");

        const auto fb = machine.render_frame();
        expect(fb.width == 512 && fb.height == 192, "Graphic6_FrameDimensions");
        // Cross-path proof (D7): logical0/1 written via the CPU port land at
        // the EXACT physical bytes the display path's planar_row_spans reads
        // for row0 -- pixels 0/1 from bank0[0]=0xF0, pixels 2/3 from
        // bank1[0]=0x00.
        expect(fb.at(0, 0) == white, "Graphic6_Golden_Pixel0_White_Bank0HighNibble");
        expect(fb.at(1, 0) == black, "Graphic6_Golden_Pixel1_Black_Bank0LowNibble");
        expect(fb.at(2, 0) == black, "Graphic6_Golden_Pixel2_Black_Bank1HighNibble");
        expect(fb.at(3, 0) == black, "Graphic6_Golden_Pixel3_Black_Bank1LowNibble");
    }

    // --- YJK family: SCREEN12 (GRAPHIC7 base + R#25 YJK). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0x99, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        machine.debug_io_write(0x99, 0x81);  // R#1 <- 0x40
        machine.map_flat_ram();

        const std::array<std::uint8_t, 41> program{
            0x3E, 0x0E, 0xD3, 0x99,  // LD A,0x0E ; OUT(99)   R#0 data (M3+M4+M5)
            0x3E, 0x80, 0xD3, 0x99,  // LD A,0x80 ; OUT(99)   register-write R#0 -> GRAPHIC7
            0x3E, 0x08, 0xD3, 0x99,  // LD A,0x08 ; OUT(99)   R#25 data (YJK)
            0x3E, 0x99, 0xD3, 0x99,  // LD A,0x99 ; OUT(99)   register-write R#25 -> ScreenYjk
            0x3E, 0x00, 0xD3, 0x99,  // LD A,0x00 ; OUT(99)   vram addr low
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)   vram addr high (write)
            0x3E, 0x08, 0xD3, 0x98,  // LD A,0x08 ; OUT(98)   p0 (y0=1)
            0x3E, 0x10, 0xD3, 0x98,  // LD A,0x10 ; OUT(98)   p1 (y1=2)
            0x3E, 0x20, 0xD3, 0x98,  // LD A,0x20 ; OUT(98)   p2 (y2=4)
            0x3E, 0x00, 0xD3, 0x98,  // LD A,0x00 ; OUT(98)   p3 (y3=0)
            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 64);
        expect(machine.cpu().state().halted(), "Yjk_CpuProgram_ReachesHalt");

        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "Yjk_FrameDimensions");
        // j=0,k=0 for this group (derived in docs/m21-implementation-report.md
        // and tests/unit/devices/video_vdp_frame_renderer_yjk_unit_test.cpp):
        // group pixel0 y=1->(1,1,1); pixel1 y=2->(2,2,3); pixel2 y=4->(4,4,5);
        // pixel3 y=0->(0,0,0) via PLAIN truncating division (A-M21-5).
        //
        // DEF-M41-YJKOFFSET (re-derived M41 production-QA oracle): the V9958
        // registers YJK content kYjkDisplayLead == 4 dots RIGHT of the G7 base
        // (openMSX 19.1 A/B + fMSX Common.h:778-783 + fact-sheet:104). OLD
        // oracle pinned the group at display columns 0..3; NEW (correct):
        // columns 0..3 are the BACKDROP lead strip and VRAM group 0 decodes at
        // display columns 4..7 -- strengthened (registration + decode).
        expect(fb.at(0, 0) == fb.border_color, "Yjk_Golden_LeadCol0_Backdrop");
        expect(fb.at(1, 0) == fb.border_color, "Yjk_Golden_LeadCol1_Backdrop");
        expect(fb.at(2, 0) == fb.border_color, "Yjk_Golden_LeadCol2_Backdrop");
        expect(fb.at(3, 0) == fb.border_color, "Yjk_Golden_LeadCol3_Backdrop");
        expect(fb.at(4, 0) == sony_msx::devices::video::pack_rgb555(1, 1, 1), "Yjk_Golden_Pixel4");
        expect(fb.at(5, 0) == sony_msx::devices::video::pack_rgb555(2, 2, 3), "Yjk_Golden_Pixel5");
        expect(fb.at(6, 0) == sony_msx::devices::video::pack_rgb555(4, 4, 5), "Yjk_Golden_Pixel6");
        expect(fb.at(7, 0) == sony_msx::devices::video::pack_rgb555(0, 0, 0), "Yjk_Golden_Pixel7_TruncatingDivision");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
