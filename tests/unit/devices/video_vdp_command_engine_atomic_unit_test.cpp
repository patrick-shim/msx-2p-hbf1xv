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

// Suite: Devices_VdpCommandEngineAtomic_Unit
//
// Deterministic unit coverage for M22-S4: the atomic (no logical-op)
// commands ABRT/STOP, POINT, SRCH, HMMV, HMMM, YMMM -- CE observably 0
// immediately after the triggering write returns (planner package §1.4
// Resolution 2); BD (S#2 bit4, SRCH's result) persists as REAL state,
// cleared ONLY by an explicit S#9 read; and the low-nibble-ignored property
// (A-M22-4/R-M22-1): these commands ALWAYS perform their fixed, implicit
// operation regardless of the CMD register's low nibble. Grounded in
// references/openmsx-21.0/src/video/VDPCmdEngine.cc -- never copied (GPL
// isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"

namespace {

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

std::uint8_t read_status_destructive(V9958Vdp& vdp, const std::uint8_t reg) {
    set_register(vdp, 15, reg);
    return vdp.io_read(0x99);
}

void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }

}  // namespace

int main() {
    // --- HMMV fill across NX*NY with DIX/DIY direction bits. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 6);  // 3 bytes/row (2 pixels/byte)
        set_ny(vdp, 2);
        set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);  // HMMV, IMP (ignored anyway)

        expect(vdp.cmd_engine().ce() == false, "Hmmv_CeClearsImmediately");
        for (int row = 0; row < 2; ++row) {
            for (int byte = 0; byte < 3; ++byte) {
                expect(vdp.vram().read(static_cast<std::size_t>(row * 128 + byte)) == 0x5A,
                       "Hmmv_FillsEntireRectangle");
            }
        }
    }
    {
        // DIX/DIY reversed: fill grows toward decreasing X/Y.
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_dx(vdp, 4);   // byte index 2
        set_dy(vdp, 1);
        set_nx(vdp, 4);   // 2 bytes/row
        set_ny(vdp, 1);
        set_arg(vdp, 0x0C);  // DIX | DIY
        set_col(vdp, 0x11);
        set_cmd(vdp, 0xC0);
        expect(vdp.vram().read(128 + 2) == 0x11, "Hmmv_Dix_FillsAtDx");
        expect(vdp.vram().read(128 + 1) == 0x11, "Hmmv_Dix_FillsTowardDecreasingX");
    }

    // --- HMMM VRAM->VRAM byte copy. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0xAA);
        write_vram(vdp, 1, 0xBB);
        set_sx(vdp, 0);
        set_sy(vdp, 0);
        set_dx(vdp, 20);  // byte index 10
        set_dy(vdp, 0);
        set_nx(vdp, 4);   // 2 bytes
        set_ny(vdp, 1);
        set_cmd(vdp, 0xD0);  // HMMM
        expect(vdp.vram().read(10) == 0xAA, "Hmmm_CopiesFirstByte");
        expect(vdp.vram().read(11) == 0xBB, "Hmmm_CopiesSecondByte");
    }

    // --- YMMM: Y-direction-only byte copy (same X for src and dest). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x77);       // row 0, byte 0
        set_dx(vdp, 0);
        set_sy(vdp, 0);
        set_dy(vdp, 5);
        set_ny(vdp, 1);
        set_cmd(vdp, 0xE0);  // YMMM
        expect(vdp.vram().read(5 * 128 + 0) == 0x77, "Ymmm_CopiesRowZeroToRowFive");
    }

    // --- SRCH: finds a target color, sets BD; BD persists across an
    //     unrelated status read, clearing ONLY on an explicit S#9 read. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 3, 0x50);  // byte index 3 -> pixel6 (even nibble) = color 5
        set_sx(vdp, 6);
        set_sy(vdp, 0);
        set_col(vdp, 0x05);
        set_arg(vdp, 0x02);  // EQ: search for equality
        set_cmd(vdp, 0x60);  // SRCH

        expect(vdp.cmd_engine().bd() == true, "Srch_FindsTarget_SetsBd");
        // An unrelated status read (S#0) does not clear BD.
        read_status_destructive(vdp, 0);
        expect(vdp.cmd_engine().bd() == true, "Srch_Bd_PersistsAcrossUnrelatedRead");
        // Only an explicit S#9 read clears BD.
        read_status_destructive(vdp, 9);
        expect(vdp.cmd_engine().bd() == false, "Srch_Bd_ClearedByS9Read");
    }

    // --- POINT: reads a pixel into COL/S#7. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x3C);  // pixel0 (high nibble) = 3, pixel1 (low nibble) = C
        set_sx(vdp, 0);
        set_sy(vdp, 0);
        set_cmd(vdp, 0x40);  // POINT
        expect(vdp.cmd_engine().color() == 0x03, "Point_ReadsPixelIntoCol");
        expect(vdp.cmd_engine().ce() == false, "Point_CeClearsImmediately");
    }

    // --- ABRT/STOP immediately clears CE (and is a no-op otherwise). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x12);
        set_cmd(vdp, 0x00);  // ABRT/STOP
        expect(vdp.cmd_engine().ce() == false, "Abrt_CeClears");
        expect(vdp.vram().read(0) == 0x12, "Abrt_NoVramSideEffect");
    }

    // --- A-M22-4/R-M22-1: the low nibble is IGNORED for these 8 commands --
    //     a non-zero, non-IMP low nibble on HMMV still behaves as a plain
    //     overwrite (not AND/OR/XOR/NOT). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0xFF);  // pre-existing content that AND/XOR would change
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 2);
        set_ny(vdp, 1);
        set_col(vdp, 0x03);
        set_cmd(vdp, 0xC3);  // HMMV with low nibble = XOR's code-point (ignored for HMMV)
        expect(vdp.vram().read(0) == 0x03, "Hmmv_LowNibbleIgnored_PlainOverwrite");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            select_graphic4(*vdp);
            set_dx(*vdp, 0);
            set_dy(*vdp, 0);
            set_nx(*vdp, 8);
            set_ny(*vdp, 3);
            set_col(*vdp, 0x42);
            set_cmd(*vdp, 0xC0);
        }
        expect(vdp_a.vram().dump() == vdp_b.vram().dump(), "Atomic_Determinism_VramIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
