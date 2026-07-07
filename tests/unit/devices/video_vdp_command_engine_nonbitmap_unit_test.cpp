// Suite: Devices_VdpCommandEngineNonBitmap_Unit
//
// Deterministic unit coverage for M22-S7: R#25 CMD-bit-gated command
// execution in non-bitmap (text/tile) display modes, using NonBitmapMode's
// flat, non-planar addressing over the SAME shared VdpVram the M21 renderer
// already reads. A genuine cross-path test confirms a command issued while
// in GRAPHIC1 mode writes bytes the EXISTING (unmodified) render_graphic1
// path subsequently displays correctly -- mirroring M21's own D7
// cross-path-test precedent. Grounded in
// references/openmsx-21.0/src/video/VDPCmdEngine.cc:1902-1938 (scrMode
// determination) -- never copied (GPL isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_palette.h"

namespace {

using sony_msx::devices::video::expand3to5;
using sony_msx::devices::video::Field;
using sony_msx::devices::video::pack_rgb555;
using sony_msx::devices::video::V9958Vdp;
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

void write_vram(V9958Vdp& vdp, const std::uint16_t addr, const std::uint8_t value) {
    set_write_address(vdp, addr);
    vdp.io_write(0x98, value);
}

void set_palette(V9958Vdp& vdp, const int index, const std::uint8_t r3, const std::uint8_t g3,
                  const std::uint8_t b3) {
    set_register(vdp, 16, static_cast<std::uint8_t>(index & 0x0F));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(((r3 & 0x07) << 4) | (b3 & 0x07)));
    vdp.io_write(0x9A, static_cast<std::uint8_t>(g3 & 0x07));
}

void set_cmd_reg(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}

void set_dx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 12, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }

}  // namespace

int main() {
    // --- Cross-path test: a command issued in GRAPHIC1 mode (R#25 CMD bit
    //     set) writes a byte the EXISTING, unmodified render_graphic1 path
    //     subsequently displays correctly. GRAPHIC1 is the reset/default
    //     mode; pattern_table_base = R#4<<11 = 0x800, so (x=0, y=8) is the
    //     flat NonBitmap address ((8&511)<<8)|(0&255) = 0x800 = exactly
    //     char0/row0's pattern byte. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 4, 1);     // pattern_base = 0x800
        set_register(vdp, 3, 0x10);  // color_base = 0x10<<6 = 0x400
        write_vram(vdp, 0x400, 0x79);  // fg=7, bg=9 (char code 0's color byte)
        set_palette(vdp, 7, 7, 0, 0);
        set_palette(vdp, 9, 0, 0, 7);
        set_register(vdp, 25, 0x40);  // R#25 CMD bit -> scrMode 4 (NonBitmap)

        const VdpFrameRenderer renderer(vdp);
        const std::uint16_t bg = pack_rgb555(expand3to5(0), expand3to5(0), expand3to5(7));
        const std::uint16_t fg = pack_rgb555(expand3to5(7), expand3to5(0), expand3to5(0));

        std::uint16_t before[256];
        renderer.render_line(0, Field::Progressive, before);
        expect(before[0] == bg, "NonBitmap_CrossPath_BeforeCommand_ShowsBackground");

        set_dx(vdp, 0);
        set_dy(vdp, 8);
        set_nx(vdp, 1);
        set_ny(vdp, 1);
        set_col(vdp, 0xFF);
        set_cmd(vdp, 0xC0);  // HMMV: fill 1 byte at the flat NonBitmap address

        std::uint16_t after[256];
        renderer.render_line(0, Field::Progressive, after);
        expect(after[0] == fg, "NonBitmap_CrossPath_AfterCommand_Graphic1DisplaysNewPattern");
    }

    // --- CMD bit CLEAR in a non-G4-G7 mode: command silently no-ops
    //     (scrMode = -1). ---
    {
        V9958Vdp vdp;
        write_vram(vdp, 0x800, 0x00);
        // R#25 CMD bit stays clear (reset default).
        set_dx(vdp, 0);
        set_dy(vdp, 8);
        set_nx(vdp, 1);
        set_ny(vdp, 1);
        set_col(vdp, 0xFF);
        set_cmd(vdp, 0xC0);
        expect(vdp.vram().read(0x800) == 0x00, "NonBitmap_CmdBitClear_SilentNoOp");
        expect(vdp.cmd_engine().ce() == false, "NonBitmap_CmdBitClear_CeStaysClear");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            set_register(*vdp, 25, 0x40);
            set_dx(*vdp, 0);
            set_dy(*vdp, 8);
            set_nx(*vdp, 1);
            set_ny(*vdp, 1);
            set_col(*vdp, 0xAB);
            set_cmd(*vdp, 0xC0);
        }
        expect(vdp_a.vram().dump() == vdp_b.vram().dump(), "NonBitmap_Determinism_VramIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
