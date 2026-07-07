// Suite: Devices_VdpCommandEngineLogicOps_Unit
//
// Deterministic unit coverage for M22-S5: the 5 logical operations
// (IMP/AND/OR/XOR/NOT, each with a "T" transparent variant) applied to the
// 5 commands that actually consult the CMD register's low nibble
// (PSET/LINE/LMMV/LMMM/LMMC), including mode-specific sub-byte pixel
// masking for a packed mode (GRAPHIC4, 4-bit nibble) and a byte mode
// (GRAPHIC7, no sub-byte mask), the undefined-low-nibble no-op (DummyOp),
// and LINE's Bresenham major-axis behavior. Grounded in
// references/openmsx-21.0/src/video/VDPCmdEngine.cc:655-720/2126-2185 --
// never copied (GPL isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_command_address.h"

namespace {

using sony_msx::devices::video::non_bitmap_command_address;
using sony_msx::devices::video::V9958Vdp;

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

void set_cmd_reg(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}

void set_sx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 0, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_sy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 2, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 3, static_cast<std::uint8_t>((v >> 8) & 0xFF));
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
void set_arg(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 13, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }

void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }
void select_graphic7(V9958Vdp& vdp) { set_register(vdp, 0, 0x0E); }

// One PSET, in GRAPHIC4, at pixel 0 (high nibble of byte 0), with pre-loaded
// byte 0xA5 (high nibble 0xA, low nibble 0x5 -- the low nibble must survive
// untouched, proving the sub-byte mask). Returns the resulting byte.
std::uint8_t pset_g4(const std::uint8_t col, const std::uint8_t low_nibble) {
    V9958Vdp vdp;
    select_graphic4(vdp);
    write_vram(vdp, 0, 0xA5);
    set_dx(vdp, 0);
    set_dy(vdp, 0);
    set_col(vdp, col);
    set_cmd(vdp, static_cast<std::uint8_t>(0x50 | low_nibble));
    return vdp.vram().read(0);
}

}  // namespace

int main() {
    // --- GRAPHIC4 (packed, 4-bit nibble): all 5 ops. Pre-existing byte
    //     0xA5 -> high nibble (pixel 0) = 0xA, low nibble MUST stay 0x5. ---
    expect(pset_g4(0x03, 0x00) == 0x35, "Pset_G4_Imp");                 // new = color
    expect(pset_g4(0x06, 0x01) == 0x25, "Pset_G4_And");                 // 0xA & 0x6 = 0x2
    expect(pset_g4(0x06, 0x02) == 0xE5, "Pset_G4_Or");                  // 0xA | 0x6 = 0xE
    expect(pset_g4(0x06, 0x03) == 0xC5, "Pset_G4_Xor");                 // 0xA ^ 0x6 = 0xC
    expect(pset_g4(0x06, 0x04) == 0x95, "Pset_G4_Not");                 // ~0x6 & 0xF = 0x9

    // --- T-variants: skip entirely when the source color is 0. ---
    expect(pset_g4(0x00, 0x08) == 0xA5, "Pset_G4_TImp_ZeroColor_Unchanged");
    expect(pset_g4(0x03, 0x08) == 0x35, "Pset_G4_TImp_NonZeroColor_AppliesImp");
    expect(pset_g4(0x00, 0x09) == 0xA5, "Pset_G4_TAnd_ZeroColor_Unchanged");
    expect(pset_g4(0x06, 0x09) == 0x25, "Pset_G4_TAnd_NonZeroColor_AppliesAnd");
    expect(pset_g4(0x00, 0x0A) == 0xA5, "Pset_G4_TOr_ZeroColor_Unchanged");
    expect(pset_g4(0x00, 0x0B) == 0xA5, "Pset_G4_TXor_ZeroColor_Unchanged");
    expect(pset_g4(0x00, 0x0C) == 0xA5, "Pset_G4_TNot_ZeroColor_Unchanged");
    expect(pset_g4(0x06, 0x0C) == 0x95, "Pset_G4_TNot_NonZeroColor_AppliesNot");

    // --- Undefined low nibbles (5,6,7,D,E,F) -> DummyOp: no VRAM access
    //     at all, byte stays completely unchanged. ---
    expect(pset_g4(0xFF, 0x05) == 0xA5, "Pset_G4_UndefinedNibbleFive_NoOp");
    expect(pset_g4(0xFF, 0x0F) == 0xA5, "Pset_G4_UndefinedNibbleF_NoOp");

    // --- GRAPHIC7 (byte mode, no sub-byte mask): IMP and NOT. ---
    {
        V9958Vdp vdp;
        select_graphic7(vdp);
        write_vram(vdp, 0, 0x77);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_col(vdp, 0x0F);
        set_cmd(vdp, 0x50);  // IMP
        expect(vdp.vram().read(0) == 0x0F, "Pset_G7_Imp_FullByteOverwrite");
    }
    {
        V9958Vdp vdp;
        select_graphic7(vdp);
        write_vram(vdp, 0, 0x77);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_col(vdp, 0x0F);
        set_cmd(vdp, 0x54);  // NOT
        expect(vdp.vram().read(0) == 0xF0, "Pset_G7_Not_ComplementOfSourceIgnoresOldDest");
    }

    // --- LMMV: fill a small rectangle with OR, and a T-variant demonstrating
    //     the masked-blit/animation idiom (skip when source color is 0). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x11);
        write_vram(vdp, 1, 0x22);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 4);  // 2 bytes
        set_ny(vdp, 1);
        set_col(vdp, 0x04);  // OR nibble 0x4 into both pixels of both bytes
        set_cmd(vdp, 0x82);  // LMMV, OR
        expect(vdp.vram().read(0) == 0x55, "Lmmv_Or_ByteZero");  // (1|4)(1|4)
        expect(vdp.vram().read(1) == 0x66, "Lmmv_Or_ByteOne");   // (2|4)(2|4)
    }
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x99);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 2);
        set_ny(vdp, 1);
        set_col(vdp, 0x00);   // transparent color -> TImp skips entirely
        set_cmd(vdp, 0x88);   // LMMV, TImp
        expect(vdp.vram().read(0) == 0x99, "Lmmv_TImp_ZeroColor_LeavesDestUnchanged");
    }

    // --- LMMM: VRAM->VRAM copy with XOR. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x0F);   // src byte
        write_vram(vdp, 10, 0xFF);  // dst byte (pre-existing)
        set_sx(vdp, 0);
        set_sy(vdp, 0);
        set_dx(vdp, 20);  // byte index 10
        set_dy(vdp, 0);
        set_nx(vdp, 2);
        set_ny(vdp, 1);
        set_cmd(vdp, 0x93);  // LMMM, XOR
        expect(vdp.vram().read(10) == (0xFF ^ 0x0F), "Lmmm_Xor_CopiesWithXor");
    }

    // --- LINE: Bresenham, X-major (MAJ=0), a 45-degree octant (NX==NY).
    //     Uses NonBitmap mode (R#25 CMD bit, TEXT1) -- one pixel/byte, flat
    //     addressing -- so the expected address is directly computable via
    //     non_bitmap_command_address() (the SAME D7-closing function under
    //     test elsewhere), not hand-derived arithmetic. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x10);  // TEXT1 (base 0x01)
        set_register(vdp, 25, 0x40);  // R#25 CMD bit -> scrMode 4 (NonBitmap)
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 4);
        set_ny(vdp, 4);
        set_col(vdp, 0x0A);
        set_cmd(vdp, 0x70);  // LINE, IMP, MAJ=0 (X major), DIX=DIY=0
        bool all_set = true;
        for (int i = 0; i < 4; ++i) {
            if (vdp.vram().read(non_bitmap_command_address(static_cast<unsigned>(i), static_cast<unsigned>(i))) !=
                0x0A) {
                all_set = false;
            }
        }
        expect(all_set, "Line_XMajor_DiagonalOctant_AllFourPixelsSet");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            select_graphic4(*vdp);
            write_vram(*vdp, 0, 0x11);
            set_dx(*vdp, 0);
            set_dy(*vdp, 0);
            set_nx(*vdp, 2);
            set_ny(*vdp, 1);
            set_col(*vdp, 0x0A);
            set_cmd(*vdp, 0x82);  // LMMV, OR
        }
        expect(vdp_a.vram().dump() == vdp_b.vram().dump(), "LogicOps_Determinism_VramIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
