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

// Suite: Machine_Hbf1xvM22Sprites_Integration
//
// Cross-boundary deterministic coverage for M22-S1/S2 (backlog D2): drives
// sprite VRAM/registers through the machine's existing debug_io_write seam
// (the SAME M14 port contract #98/#99 a real CPU program would use),
// triggers the sprite-check recompute via the machine's vdp() accessor's
// on_vsync() (the existing frame-boundary hook -- not something software
// drives via an I/O port), and asserts on the machine-level status registers
// and render_frame() output, including two-machine determinism.

#include <cstdint>
#include <iostream>

#include "devices/video/frame_buffer.h"
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

void set_register(sony_msx::machine::Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

void set_write_address(sony_msx::machine::Hbf1xvMachine& m, const std::uint16_t addr) {
    m.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
}

void write_vram(sony_msx::machine::Hbf1xvMachine& m, const std::uint16_t addr, const std::uint8_t value) {
    set_write_address(m, addr);
    m.debug_io_write(0x98, value);
}

void set_palette(sony_msx::machine::Hbf1xvMachine& m, const int index, const std::uint8_t r3,
                  const std::uint8_t g3, const std::uint8_t b3) {
    set_register(m, 16, static_cast<std::uint8_t>(index & 0x0F));
    m.debug_io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    m.debug_io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

std::uint8_t read_status_destructive(sony_msx::machine::Hbf1xvMachine& m, const std::uint8_t reg) {
    set_register(m, 15, reg);
    return m.debug_io_read(0x99);
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Sprite Mode 1 collision, driven end-to-end through the machine's
    //     port seam, with status read back via the SAME seam. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // GRAPHIC1 is the reset/default mode -> sprite mode 1. Attribute
        // table defaults to address 0 (R#11/R#5 = 0); pattern table moved to
        // 0x800 (R#6 = 1) to avoid aliasing the attribute table's 128 bytes.
        set_register(machine, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(machine, 6, 0x01);
        // Two fully-overlapping solid sprites at (0,0).
        write_vram(machine, 0, 0);   // sprite0 Y
        write_vram(machine, 1, 0);   // sprite0 X
        write_vram(machine, 2, 0);   // sprite0 pattern index
        write_vram(machine, 3, 1);   // sprite0 color
        write_vram(machine, 4, 0);   // sprite1 Y
        write_vram(machine, 5, 0);   // sprite1 X
        write_vram(machine, 6, 0);   // sprite1 pattern index
        write_vram(machine, 7, 2);   // sprite1 color
        write_vram(machine, 8, 208);  // sentinel
        write_vram(machine, 0x0800, 0xFF);  // pattern row0: all set

        machine.vdp().on_vsync();

        const std::uint8_t s0 = static_cast<std::uint8_t>(read_status_destructive(machine, 0) & 0x7F);
        expect((s0 & 0x20) != 0, "Sprites_Collision_CFlagSetViaMachinePorts");
        expect(read_status_destructive(machine, 3) == 12, "Sprites_Collision_XOffsetViaMachinePorts");
        expect(read_status_destructive(machine, 5) == (1 + 8), "Sprites_Collision_YOffsetViaMachinePorts");
    }

    // --- render_frame() at machine level composites sprite pixels into the
    //     SAME FrameBuffer contract M21 established (no new output type). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 1, 0x40);  // R#1 bit6: display enable (spritesEnabledFast() gate)
        set_register(machine, 6, 0x01);
        set_palette(machine, 3, 6, 0, 0);
        write_vram(machine, 0, 0);
        write_vram(machine, 1, 10);
        write_vram(machine, 2, 0);
        write_vram(machine, 3, 3);
        write_vram(machine, 4, 208);
        write_vram(machine, 0x0800, 0xFF);

        machine.vdp().on_vsync();
        const auto fb = machine.render_frame();
        const std::uint16_t expected =
            sony_msx::devices::video::pack_rgb555(sony_msx::devices::video::expand3to5(6), 0, 0);
        expect(fb.at(10, 1) == expected, "Sprites_RenderFrame_ShowsSpritePixel");
    }

    // --- Two-machine determinism. ---
    {
        Hbf1xvMachine machine_a;
        Hbf1xvMachine machine_b;
        machine_a.cold_boot();
        machine_b.cold_boot();
        for (auto* m : {&machine_a, &machine_b}) {
            set_register(*m, 1, 0x40);
            set_register(*m, 6, 0x01);
            write_vram(*m, 0, 0);
            write_vram(*m, 1, 0);
            write_vram(*m, 2, 0);
            write_vram(*m, 3, 1);
            write_vram(*m, 4, 208);
            write_vram(*m, 0x0800, 0xFF);
            m->vdp().on_vsync();
        }
        const auto fb_a = machine_a.render_frame();
        const auto fb_b = machine_b.render_frame();
        expect(fb_a.pixels == fb_b.pixels, "Sprites_Determinism_TwoMachines_ByteIdentical");
        expect(read_status_destructive(machine_a, 0) == read_status_destructive(machine_b, 0),
               "Sprites_Determinism_StatusIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
