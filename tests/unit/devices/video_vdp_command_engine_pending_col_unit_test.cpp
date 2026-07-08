// Suite: Devices_VdpCommandEnginePendingCol_Unit  (boot-logo fix)
//
// The reference's `transfer` flag semantics for CPU->VRAM commands
// (references/openmsx-21.0/src/video/VDPCmdEngine.cc:1856-1863 setCmdReg
// case 0x0C: ANY R#44/COL write arms exactly ONE pending transfer unit,
// even BEFORE the command is issued; startLmmc/startHmmc deliberately do
// NOT arm it themselves -- VDPCmdEngine.cc:1303-1305/1732-1733, their
// bug#1014 -- but a pre-armed unit is consumed as the FIRST transferred
// unit when the command starts). Behavior reference only, never copied.
//
// This is exactly the HB-F1XV MSX2+ boot-logo LMMC protocol: the SUB-ROM
// pre-loads the first pixel color into R#44, issues LMMC (NX=16, NY=8),
// then sends only NX*NY-1 further COL writes and finally spins on S#2 CE.
// Without the pre-armed unit the engine stays one write short forever and
// the boot never reaches BASIC.

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

void set_cmd_reg(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}

void set_pair(V9958Vdp& vdp, const int low_index, const unsigned v) {
    set_cmd_reg(vdp, low_index, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, low_index + 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void select_graphic4(V9958Vdp& vdp) { set_register(vdp, 0, 0x06); }

}  // namespace

int main() {
    // --- LMMC, boot-logo protocol: COL pre-loaded BEFORE CMD counts as the
    //     first pixel; N-1 further writes complete the command. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_pair(vdp, 4, 0);  // DX
        set_pair(vdp, 6, 0);  // DY
        set_pair(vdp, 8, 4);  // NX = 4 pixels
        set_pair(vdp, 10, 1); // NY = 1
        set_cmd_reg(vdp, 12, 1);   // pre-load pixel 0 (arms one pending unit)
        set_cmd_reg(vdp, 14, 0xB0);  // LMMC, IMP -- consumes the armed unit

        expect(vdp.cmd_engine().ce() == true, "Lmmc_PreloadedStart_CeSet");
        expect(vdp.vram().read(0) == 0x10, "Lmmc_PreloadedStart_FirstPixelAlreadyWritten");

        set_cmd_reg(vdp, 12, 2);  // pixel 1
        set_cmd_reg(vdp, 12, 3);  // pixel 2
        expect(vdp.cmd_engine().ce() == true, "Lmmc_AfterThreeOfFour_StillExecuting");
        set_cmd_reg(vdp, 12, 4);  // pixel 3 -- the (NX*NY)th unit overall
        expect(vdp.cmd_engine().ce() == false, "Lmmc_PreloadPlusNMinus1Writes_Completes_BootLogoProtocol");
        expect(vdp.vram().read(0) == 0x12 && vdp.vram().read(1) == 0x34,
               "Lmmc_AllFourPixelValuesInOrder");
    }

    // --- LMMC without a pre-load: start does NOT self-arm (bug#1014); the
    //     full NX*NY writes are still required (the pre-fix contract for
    //     programs that do not pre-load, preserved). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_pair(vdp, 4, 0);
        set_pair(vdp, 6, 0);
        set_pair(vdp, 8, 2);
        set_pair(vdp, 10, 1);
        set_cmd_reg(vdp, 14, 0xB0);  // LMMC with NO preceding COL write

        expect(vdp.cmd_engine().ce() == true, "Lmmc_NoPreload_CeSet");
        set_cmd_reg(vdp, 12, 5);
        expect(vdp.cmd_engine().ce() == true, "Lmmc_NoPreload_OneOfTwo_StillExecuting");
        set_cmd_reg(vdp, 12, 6);
        expect(vdp.cmd_engine().ce() == false, "Lmmc_NoPreload_TwoWritesComplete");
        expect(vdp.vram().read(0) == 0x56, "Lmmc_NoPreload_PixelValues");
    }

    // --- The armed unit is consumed exactly once: after a pre-loaded
    //     command completes, a NEW command with no fresh COL write is back
    //     to requiring all NX*NY writes. ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_pair(vdp, 4, 0);
        set_pair(vdp, 6, 0);
        set_pair(vdp, 8, 2);
        set_pair(vdp, 10, 1);
        set_cmd_reg(vdp, 12, 1);
        set_cmd_reg(vdp, 14, 0xB0);  // consumes the armed unit (pixel 0)
        set_cmd_reg(vdp, 12, 2);     // pixel 1 -> completes
        expect(vdp.cmd_engine().ce() == false, "ArmedOnce_FirstCommandCompletes");

        set_pair(vdp, 6, 8);  // fresh DY
        set_cmd_reg(vdp, 14, 0xB0);
        expect(vdp.cmd_engine().ce() == true, "ArmedOnce_SecondCommandStartsUnarmed");
        set_cmd_reg(vdp, 12, 3);
        expect(vdp.cmd_engine().ce() == true, "ArmedOnce_SecondCommandStillNeedsBothWrites");
        set_cmd_reg(vdp, 12, 4);
        expect(vdp.cmd_engine().ce() == false, "ArmedOnce_SecondCommandCompletesAfterFullCount");
    }

    // --- HMMC honors the same pre-armed-COL semantics
    //     (VDPCmdEngine.cc:1732-1733 "see startLmmc()"). ---
    {
        V9958Vdp vdp;
        select_graphic4(vdp);
        set_pair(vdp, 4, 0);
        set_pair(vdp, 6, 0);
        set_pair(vdp, 8, 4);  // 4 pixels = 2 bytes in GRAPHIC4
        set_pair(vdp, 10, 1);
        set_cmd_reg(vdp, 12, 0xAB);  // pre-load byte 0
        set_cmd_reg(vdp, 14, 0xF0);  // HMMC

        expect(vdp.vram().read(0) == 0xAB, "Hmmc_PreloadedStart_FirstByteAlreadyWritten");
        expect(vdp.cmd_engine().ce() == true, "Hmmc_PreloadedStart_CeSet");
        set_cmd_reg(vdp, 12, 0xCD);  // byte 1 -- completes the 2-byte row
        expect(vdp.cmd_engine().ce() == false, "Hmmc_PreloadPlusOneWrite_Completes");
        expect(vdp.vram().read(1) == 0xCD, "Hmmc_SecondByteValue");
    }

    // --- Determinism oracle: two identically-driven runs leave identical
    //     VRAM bytes and status. ---
    {
        auto run = [](std::uint8_t out[2]) {
            V9958Vdp vdp;
            set_register(vdp, 0, 0x06);
            V9958Vdp& v = vdp;
            set_pair(v, 4, 0);
            set_pair(v, 6, 0);
            set_pair(v, 8, 4);
            set_pair(v, 10, 1);
            set_cmd_reg(v, 12, 9);
            set_cmd_reg(v, 14, 0xB0);
            set_cmd_reg(v, 12, 8);
            set_cmd_reg(v, 12, 7);
            set_cmd_reg(v, 12, 6);
            out[0] = v.vram().read(0);
            out[1] = v.vram().read(1);
        };
        std::uint8_t a[2];
        std::uint8_t b[2];
        run(a);
        run(b);
        expect(a[0] == b[0] && a[1] == b[1], "Determinism_TwoRunsByteIdentical");
        expect(a[0] == 0x98 && a[1] == 0x76, "Determinism_ExpectedPixelBytes");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpCommandEnginePendingCol_Unit cases passed\n";
    return 0;
}
