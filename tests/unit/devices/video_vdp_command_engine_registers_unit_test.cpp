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

// Suite: Devices_VdpCommandEngineRegisters_Unit
//
// Deterministic unit coverage for M22-S3: VdpCommandEngine's register file
// (R#32-46, SX/SY/DX/DY/NX/NY/COL/ARG/CMD + bit-width masking, A-M22-2/3),
// the change_register() dispatch extension reaching the command engine via
// BOTH the #99 two-write latch protocol AND the #9B indirect-register path
// (A-M22-1), scrMode determination (A-M22-6), the five
// vdp_command_address.h coordinate functions (the D7-closing piece, §1.5,
// hand-verified against the two G6/G7 cases independently re-derived in the
// planner package), the R#2-bypass nuance (R-M22-7), and the always-inert
// extended-VRAM ARG bits (A-M22-8). Grounded in
// references/openmsx-21.0/src/video/VDPCmdEngine.cc -- never copied (GPL
// isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_command_address.h"

namespace {

using sony_msx::devices::video::graphic4_command_address;
using sony_msx::devices::video::graphic5_command_address;
using sony_msx::devices::video::graphic6_command_address;
using sony_msx::devices::video::graphic7_command_address;
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

// R#32..R#46 write via the #99 two-write latch protocol.
void set_cmd_reg_via_99(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}

// R#32..R#46 write via the #9B indirect-register path (sets R#17 first).
void set_cmd_reg_via_9b(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, 17, static_cast<std::uint8_t>(32 + index));  // R#17 pointer, AII clear -> auto-increment
    vdp.io_write(0x9B, value);
}

void set_sx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 0, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_sy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 2, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 3, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg_via_99(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg_via_99(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg_via_99(vdp, 12, v); }
void set_arg(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg_via_99(vdp, 13, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg_via_99(vdp, 14, v); }

}  // namespace

int main() {
    // --- A-M22-3: bit-width masking on the high-byte registers. ---
    {
        V9958Vdp vdp;
        set_sx(vdp, 0x3FF);  // 9-bit: only bit0 of the high byte sticks
        expect(vdp.cmd_engine().read_register(0) == 0xFF, "BitWidth_Sx_LowByte");
        expect(vdp.cmd_engine().read_register(1) == 0x01, "BitWidth_Sx_HighByteOnlyBit0");

        set_sy(vdp, 0x3FF);  // 10-bit: bits1-0 of the high byte stick
        expect(vdp.cmd_engine().read_register(2) == 0xFF, "BitWidth_Sy_LowByte");
        expect(vdp.cmd_engine().read_register(3) == 0x03, "BitWidth_Sy_HighByteBits10");

        set_dx(vdp, 0x3FF);
        expect(vdp.cmd_engine().read_register(5) == 0x01, "BitWidth_Dx_HighByteOnlyBit0");

        set_dy(vdp, 0x3FF);
        expect(vdp.cmd_engine().read_register(7) == 0x03, "BitWidth_Dy_HighByteBits10");

        set_nx(vdp, 0x3FF);
        expect(vdp.cmd_engine().read_register(9) == 0x03, "BitWidth_Nx_HighByteBits10");

        set_ny(vdp, 0x3FF);
        expect(vdp.cmd_engine().read_register(11) == 0x03, "BitWidth_Ny_HighByteBits10");
    }

    // --- A-M22-1: register writes reach the command engine identically via
    //     BOTH the #99 two-write path and the #9B indirect path. ---
    {
        V9958Vdp vdp;
        set_cmd_reg_via_99(vdp, 13, 0x77);  // R#45 = ARG
        expect(vdp.cmd_engine().read_register(13) == 0x77, "Dispatch_Via99_ArgWritten");

        set_cmd_reg_via_9b(vdp, 13, 0x55);
        expect(vdp.cmd_engine().read_register(13) == 0x55, "Dispatch_Via9B_ArgWritten");

        // POINT via BOTH paths in GRAPHIC4 (scrMode 0): confirm both reach
        // execute_command() identically (COL updates from a real VRAM read).
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        write_vram(vdp, 0, 0xAB);
        set_sx(vdp, 0);
        set_sy(vdp, 0);
        set_cmd_reg_via_99(vdp, 14, 0x40);  // CMD>>4 = 4 -> POINT
        // SX=0 (even) reads pixel0 = the HIGH nibble (Graphic4Mode::point's
        // shift = ((~x)&1)<<2 = 4 for even x).
        expect(vdp.cmd_engine().read_register(12) == (0xAB >> 4), "Dispatch_Via99_PointReadsPixel");

        write_vram(vdp, 0, 0xCD);
        set_cmd_reg_via_9b(vdp, 14, 0x40);  // same POINT command, via #9B
        expect(vdp.cmd_engine().read_register(12) == (0xCD >> 4), "Dispatch_Via9B_PointReadsPixel");
    }

    // --- A-M22-6: scrMode table across mode x R#25-CMD-bit combinations. ---
    {
        // TEXT1 (base 0x01), CMD bit clear: scrMode = -1, PSET is a silent
        // no-op (writing CMD immediately behaves as ABRT).
        V9958Vdp vdp;
        set_register(vdp, 0, 0x10);  // R#0 bit4 -> TEXT1 base 0x01
        write_vram(vdp, 0, 0x00);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_col(vdp, 0x0F);
        set_cmd(vdp, 0x50);  // PSET, IMP
        expect(vdp.vram().read(0) == 0x00, "ScrMode_TextNoCmdBit_PsetIsNoOp");
    }
    {
        // TEXT1, CMD bit SET (R#25 bit6): scrMode = 4 (NonBitmap), PSET now
        // legal and uses non_bitmap_command_address.
        V9958Vdp vdp;
        set_register(vdp, 0, 0x10);
        set_register(vdp, 25, 0x40);
        set_dx(vdp, 3);
        set_dy(vdp, 0);
        set_col(vdp, 0x77);
        set_cmd(vdp, 0x50);  // PSET, IMP
        expect(vdp.vram().read(non_bitmap_command_address(3, 0)) == 0x77, "ScrMode_TextCmdBit_PsetWritesNonBitmap");
    }
    {
        // GRAPHIC4: scrMode = 0 regardless of the CMD bit.
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_dx(vdp, 3);
        set_dy(vdp, 0);
        set_col(vdp, 0x05);
        set_cmd(vdp, 0x50);
        expect(vdp.vram().read(graphic4_command_address(3, 0)) != 0, "ScrMode_Graphic4_PsetWritesGraphic4Address");
    }

    // --- The five address functions, hand-computed (independently
    //     re-derived in the planner package §1.5, including the two G6/G7
    //     cross-checked cases). ---
    {
        expect(graphic4_command_address(0, 0) == 0, "Address_Graphic4_Origin");
        expect(graphic4_command_address(2, 0) == 1, "Address_Graphic4_TwoPixelsPerByte");
        expect(graphic5_command_address(0, 0) == 0, "Address_Graphic5_Origin");
        expect(graphic5_command_address(4, 0) == 1, "Address_Graphic5_FourPixelsPerByte");
        expect(non_bitmap_command_address(0, 0) == 0, "Address_NonBitmap_Origin");
        expect(non_bitmap_command_address(1, 0) == 1, "Address_NonBitmap_FlatByteAdvance");

        // G7 hand-verified cases (planner package §1.5).
        expect(graphic7_command_address(0, 0) == 0, "Address_Graphic7_Origin");
        expect(graphic7_command_address(1, 256) == 0x18000u, "Address_Graphic7_Page1OddColumn");

        // G6 hand-verified cases.
        expect(graphic6_command_address(0, 0) == 0, "Address_Graphic6_Origin");
        expect(graphic6_command_address(2, 0) == 0x10000u, "Address_Graphic6_ByteIndexOneBank1");
    }

    // --- R-M22-7: commands address BOTH pages DIRECTLY through Y (the
    //     Y-coordinate's own 0-511 range spans both G6/G7 pages, DY=300 =
    //     page 1, line 44 since 300 = 256+44), completely bypassing R#2's
    //     display-page-select bits. Issuing the IDENTICAL command with R#2
    //     set to page 0 and then to page 1 must land the write at the
    //     EXACT SAME address in both cases -- R#2 has NO effect at all on
    //     command addressing (a naive implementation might incorrectly gate
    //     addressing on R#2, copying the DISPLAY path's own page-resolution
    //     logic -- a genuine, easy-to-introduce defect this test rules out). ---
    {
        const std::uint32_t addr = graphic6_command_address(0, 300);
        for (const std::uint8_t r2_value : {std::uint8_t{0x00}, std::uint8_t{0x20}}) {
            V9958Vdp vdp;
            set_register(vdp, 0, 0x0A);  // GRAPHIC6 (base 0x14)
            set_register(vdp, 2, r2_value);  // R#2 page-select bits (irrelevant to commands)
            set_dx(vdp, 0);
            set_dy(vdp, 300);
            set_nx(vdp, 2);  // GRAPHIC6 is 2 pixels/byte -- NX=2 -> exactly 1 byte
            set_ny(vdp, 1);
            set_col(vdp, 0x0A);
            set_cmd(vdp, 0xC0);  // HMMV (fixed fill, no logical op)
            expect(vdp.vram().read(addr) == 0x0A, "R2Bypass_Graphic6_Dy300_WriteLandsAtSameAddressRegardlessOfR2");
        }
    }

    // --- A-M22-8: extended-VRAM ARG bits (MXS/MXD) are stored but always
    //     treated as false -- addressing never reaches for 0x20000+. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_arg(vdp, 0x30);          // MXS | MXD set
        set_dx(vdp, 5);
        set_dy(vdp, 0);
        set_nx(vdp, 2);  // GRAPHIC4 is 2 pixels/byte -- NX=2 -> exactly 1 byte
        set_ny(vdp, 1);
        set_col(vdp, 0x09);
        set_cmd(vdp, 0xC0);  // HMMV
        expect(vdp.cmd_engine().read_register(13) == 0x30, "ExtVram_ArgBitsStored");
        expect(vdp.vram().read(graphic4_command_address(5, 0)) == 0x09, "ExtVram_AddressingUnaffected");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
