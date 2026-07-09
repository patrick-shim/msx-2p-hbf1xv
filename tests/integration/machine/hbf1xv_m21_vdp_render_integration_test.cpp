// Suite: Machine_Hbf1xvM21VdpRender_Integration
//
// Cross-boundary deterministic coverage for the M21 additive
// `Hbf1xvMachine::render_frame()` accessor (mirrors the existing vdp()
// accessor pattern -- no change to wire_bus()/cold_boot() was needed, since
// the renderer is a pure, on-demand consumer of vdp_'s stored state). Drives
// VRAM + registers through the machine's existing debug_io_write seam (the
// SAME M14 port contract #98/#99/#9A a real CPU program would use), then
// asserts on the machine-level FrameBuffer, including the S1985 #9C-#9F
// mirror and two-machine determinism.

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

void set_palette(sony_msx::machine::Hbf1xvMachine& m, const int index, const std::uint8_t r3,
                  const std::uint8_t g3, const std::uint8_t b3) {
    set_register(m, 16, static_cast<std::uint8_t>(index & 0x0F));
    m.debug_io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    m.debug_io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

}  // namespace

int main() {
    using sony_msx::devices::video::Field;
    using sony_msx::machine::Hbf1xvMachine;

    // --- render_frame() reflects VRAM/register content driven through the
    //     machine's own #98/#99/#9A ports (GRAPHIC1, the reset/default mode). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines

        set_palette(machine, 3, 5, 1, 2);
        set_palette(machine, 7, 0, 0, 0);
        // Separate pattern/color table bases from the name table (address 0)
        // so the three writes below cannot alias each other.
        set_register(machine, 4, 0x01);  // pattern table base 0x0800
        set_register(machine, 3, 0x40);  // color table base 0x1000
        set_register(machine, 10, 0x00);

        set_write_address(machine, 0);
        machine.debug_io_write(0x98, 0x03);  // name index0: char code 3
        set_write_address(machine, 0x0800 + 3 * 8);
        machine.debug_io_write(0x98, 0xFF);  // pattern: all set
        set_write_address(machine, 0x1000 + 3 / 8);
        machine.debug_io_write(0x98, 0x37);  // color: fg=3, bg=7

        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "RenderFrame_Graphic1_Dimensions");
        const std::uint16_t expected_fg = sony_msx::devices::video::pack_rgb555(
            sony_msx::devices::video::expand3to5(5), sony_msx::devices::video::expand3to5(1),
            sony_msx::devices::video::expand3to5(2));
        expect(fb.at(0, 0) == expected_fg, "RenderFrame_Graphic1_AllSetPattern_MatchesForegroundPalette");
        expect(fb.at(0, 0) == fb.at(1, 0), "RenderFrame_Graphic1_AllSetPatternUniform");
        expect(fb.pixels.size() == static_cast<std::size_t>(fb.width) * static_cast<std::size_t>(fb.height),
               "RenderFrame_PixelBufferSizeMatchesDimensions");
    }

    // --- The S1985 #9C-#9F mirror routes to the SAME VDP the renderer reads
    //     (M14-precedent mirror equivalence, extended to the render path). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_palette(machine, 5, 4, 4, 4);
        // Mirror-write the pattern/color table base registers too, so the
        // three content writes below (name/pattern/color) cannot alias.
        machine.debug_io_write(0x9D, 0x01);
        machine.debug_io_write(0x9D, 0x84);  // R#4 = 0x01 (pattern base 0x0800), via #9D=#99
        machine.debug_io_write(0x9D, 0x40);
        machine.debug_io_write(0x9D, 0x83);  // R#3 = 0x40 (color base low half)

        // Write via the MIRROR ports instead of the base ports.
        machine.debug_io_write(0x9D, 0x00);
        machine.debug_io_write(0x9D, 0x41);  // address 0, write mode (mirror of #99)
        machine.debug_io_write(0x9C, 0x05);  // name index0 = char5 (mirror of #98)
        machine.debug_io_write(0x9D, static_cast<std::uint8_t>((0x0800 + 5 * 8) & 0xFF));
        machine.debug_io_write(0x9D, static_cast<std::uint8_t>(0x40 | (((0x0800 + 5 * 8) >> 8) & 0x3F)));
        machine.debug_io_write(0x9C, 0xFF);
        machine.debug_io_write(0x9D, static_cast<std::uint8_t>((0x1000 + 5 / 8) & 0xFF));
        machine.debug_io_write(0x9D, static_cast<std::uint8_t>(0x40 | (((0x1000 + 5 / 8) >> 8) & 0x3F)));
        machine.debug_io_write(0x9C, 0x55);  // fg=bg=5

        const auto fb = machine.render_frame();
        const std::uint16_t expected = sony_msx::devices::video::pack_rgb555(
            sony_msx::devices::video::expand3to5(4), sony_msx::devices::video::expand3to5(4),
            sony_msx::devices::video::expand3to5(4));
        expect(fb.at(0, 0) == expected, "RenderFrame_MirrorPorts_SameVdp_ContentVisible");
    }

    // --- Two-machine determinism: identical write sequences on two
    //     independent machines produce byte-identical FrameBuffer content. ---
    {
        Hbf1xvMachine machine_a;
        Hbf1xvMachine machine_b;
        machine_a.cold_boot();
        machine_b.cold_boot();
        set_register(machine_a, 1, 0x40);  // M34: R#1 bit6 BL=1 (display enable) -- the render gate blanks BL=0 lines
        set_register(machine_b, 1, 0x40);
        for (auto* m : {&machine_a, &machine_b}) {
            set_palette(*m, 2, 3, 3, 3);
            set_write_address(*m, 10);
            m->debug_io_write(0x98, 0x22);
        }
        const auto fb_a = machine_a.render_frame(Field::Progressive);
        const auto fb_b = machine_b.render_frame(Field::Progressive);
        expect(fb_a.pixels == fb_b.pixels, "RenderFrame_Determinism_TwoMachines_ByteIdentical");
        expect(fb_a.border_color == fb_b.border_color, "RenderFrame_Determinism_BorderColorIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
