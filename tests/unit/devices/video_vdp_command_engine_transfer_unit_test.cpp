// Suite: Devices_VdpCommandEngineTransfer_Unit
//
// Deterministic unit coverage for M22-S6: the 3 event-driven, stateful
// transfer commands (LMCM, LMMC, HMMC), planner package §1.4 Resolution 2.
// CE stays 1 across the WHOLE transfer; the command completes only after
// NX*NY individual, SEPARATE CPU-port interactions have each been serviced
// (writes to R#44/COL for LMMC/HMMC; reads of S#7 for LMCM). This is a
// genuine correctness requirement (R-M22-9): collapsing this to the atomic
// model would silently drop every pixel/byte after the first. Grounded in
// references/openmsx-21.0/src/video/VDPCmdEngine.cc:1240-1350/1720-1771 --
// never copied (GPL isolation).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_command_address.h"

namespace {

using sony_msx::devices::video::graphic7_command_address;
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
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }

std::uint8_t read_status_destructive(V9958Vdp& vdp, const std::uint8_t reg) {
    set_register(vdp, 15, reg);
    return vdp.io_read(0x99);
}

void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }
void select_graphic7(V9958Vdp& vdp) { set_register(vdp, 0, 0x0E); }

}  // namespace

int main() {
    // --- LMMC: write CMD to start, confirm TR=1/CE=1; write R#44 repeatedly
    //     and confirm EACH write completes exactly one pixel's transfer
    //     (VRAM updated immediately, not just the first); CE clears exactly
    //     after the NX*NY-th write. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);  // 4-bit nibble packing
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 4);  // 4 pixels (2 bytes)
        set_ny(vdp, 1);
        set_cmd(vdp, 0xB0);  // LMMC, IMP

        expect(vdp.cmd_engine().tr() == true, "Lmmc_Start_TrSet");
        expect(vdp.cmd_engine().ce() == true, "Lmmc_Start_CeSet");

        set_cmd_reg(vdp, 12, 1);  // pixel 0
        expect(vdp.vram().read(0) == 0x10, "Lmmc_FirstWrite_PixelZeroWritten");
        expect(vdp.cmd_engine().ce() == true, "Lmmc_AfterFirstWrite_StillExecuting");

        set_cmd_reg(vdp, 12, 2);  // pixel 1
        expect(vdp.vram().read(0) == 0x12, "Lmmc_SecondWrite_PixelOneWritten_FirstNotDropped");

        set_cmd_reg(vdp, 12, 3);  // pixel 2
        expect(vdp.vram().read(1) == 0x30, "Lmmc_ThirdWrite_PixelTwoWritten");
        expect(vdp.cmd_engine().ce() == true, "Lmmc_AfterThirdWrite_StillExecuting");

        set_cmd_reg(vdp, 12, 4);  // pixel 3 (last)
        expect(vdp.vram().read(1) == 0x34, "Lmmc_FourthWrite_PixelThreeWritten");
        expect(vdp.cmd_engine().ce() == false, "Lmmc_AfterFourthWrite_CeClears");
    }

    // --- LMCM: read S#7 repeatedly, confirm COL advances through the
    //     source pixels in order and CE clears at the end. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        write_vram(vdp, 0, 0x12);  // pixel0=1, pixel1=2
        write_vram(vdp, 1, 0x34);  // pixel2=3, pixel3=4
        set_sx(vdp, 0);
        set_sy(vdp, 0);
        set_nx(vdp, 4);
        set_ny(vdp, 1);
        set_cmd(vdp, 0xA0);  // LMCM

        expect(vdp.cmd_engine().ce() == true, "Lmcm_Start_CeSet");
        expect(read_status_destructive(vdp, 7) == 1, "Lmcm_FirstRead_PixelZero");
        expect(vdp.cmd_engine().ce() == true, "Lmcm_AfterFirstRead_StillExecuting");
        expect(read_status_destructive(vdp, 7) == 2, "Lmcm_SecondRead_PixelOne");
        expect(read_status_destructive(vdp, 7) == 3, "Lmcm_ThirdRead_PixelTwo");
        expect(read_status_destructive(vdp, 7) == 4, "Lmcm_FourthRead_PixelThree");
        expect(vdp.cmd_engine().ce() == false, "Lmcm_AfterFourthRead_CeClears");
    }

    // --- HMMC: byte-granularity analog (GRAPHIC7, 1 pixel/byte, so NX
    //     directly counts bytes). ---
    {
        V9958Vdp vdp;
        select_graphic7(vdp);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 2);
        set_ny(vdp, 1);
        set_cmd(vdp, 0xF0);  // HMMC

        expect(vdp.cmd_engine().tr() == true, "Hmmc_Start_TrSet");
        expect(vdp.cmd_engine().ce() == true, "Hmmc_Start_CeSet");

        set_cmd_reg(vdp, 12, 0xAA);
        expect(vdp.vram().read(graphic7_command_address(0, 0)) == 0xAA, "Hmmc_FirstWrite_ByteZero");
        expect(vdp.cmd_engine().ce() == true, "Hmmc_AfterFirstWrite_StillExecuting");

        // GRAPHIC7 is planar even at byte granularity: x=1 (odd) lands in
        // the SECOND bank (graphic7_command_address(1,0) -- bit16 set), not
        // simply "address 1".
        set_cmd_reg(vdp, 12, 0xBB);
        expect(vdp.vram().read(graphic7_command_address(1, 0)) == 0xBB, "Hmmc_SecondWrite_ByteOne_FirstNotDropped");
        expect(vdp.cmd_engine().ce() == false, "Hmmc_AfterSecondWrite_CeClears");
    }

    // --- No wall-clock/cycle dependency: advancing zero simulated cycles
    //     (no on_vsync()/clock call at all) between transfer steps still
    //     works -- purely event- (I/O-access-) driven. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_dx(vdp, 0);
        set_dy(vdp, 0);
        set_nx(vdp, 2);
        set_ny(vdp, 1);
        set_cmd(vdp, 0xB0);  // LMMC
        // Two immediate, back-to-back writes with NO intervening on_vsync()
        // or any other clock-like call.
        set_cmd_reg(vdp, 12, 5);
        set_cmd_reg(vdp, 12, 6);
        expect(vdp.vram().read(0) == 0x56, "Transfer_NoClockDependency_CompletesOnPureIoAccess");
        expect(vdp.cmd_engine().ce() == false, "Transfer_NoClockDependency_CeClears");
    }

    // --- Two-run determinism. ---
    {
        V9958Vdp vdp_a;
        V9958Vdp vdp_b;
        for (auto* vdp : {&vdp_a, &vdp_b}) {
            select_graphic4(*vdp);
            set_dx(*vdp, 0);
            set_dy(*vdp, 0);
            set_nx(*vdp, 4);
            set_ny(*vdp, 1);
            set_cmd(*vdp, 0xB0);
            set_cmd_reg(*vdp, 12, 1);
            set_cmd_reg(*vdp, 12, 2);
            set_cmd_reg(*vdp, 12, 3);
            set_cmd_reg(*vdp, 12, 4);
        }
        expect(vdp_a.vram().dump() == vdp_b.vram().dump(), "Transfer_Determinism_VramIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
