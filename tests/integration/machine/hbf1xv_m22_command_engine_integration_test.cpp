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

// Suite: Machine_Hbf1xvM22CommandEngine_Integration
//
// Cross-boundary deterministic coverage for M22-S3..S7 (backlog D3, closes
// D7's remaining command-engine-path piece): drives R#32-46 through the
// machine's existing debug_io_write seam, confirms VRAM writes land at the
// expected physical addresses (including a G6 planar destination,
// cross-validating D7's closure), confirms the S#2 TR/CE status handshake
// for an event-driven transfer command, and two-machine determinism.

#include <cstdint>
#include <iostream>

#include "devices/video/vdp_command_address.h"
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

void set_cmd_reg(sony_msx::machine::Hbf1xvMachine& m, const int index, const std::uint8_t value) {
    set_register(m, static_cast<std::uint8_t>(32 + index), value);
}

void set_dx(sony_msx::machine::Hbf1xvMachine& m, const unsigned v) {
    set_cmd_reg(m, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(m, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(sony_msx::machine::Hbf1xvMachine& m, const unsigned v) {
    set_cmd_reg(m, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(m, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(sony_msx::machine::Hbf1xvMachine& m, const unsigned v) {
    set_cmd_reg(m, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(m, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(sony_msx::machine::Hbf1xvMachine& m, const unsigned v) {
    set_cmd_reg(m, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(m, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(sony_msx::machine::Hbf1xvMachine& m, const std::uint8_t v) { set_cmd_reg(m, 12, v); }
void set_cmd(sony_msx::machine::Hbf1xvMachine& m, const std::uint8_t v) { set_cmd_reg(m, 14, v); }

std::uint8_t peek_s2(sony_msx::machine::Hbf1xvMachine& m) { return m.vdp().peek_status_register(2); }

}  // namespace

int main() {
    using sony_msx::devices::video::graphic6_command_address;
    using sony_msx::machine::Hbf1xvMachine;

    // --- HMMV fill through the machine's port seam. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 0, 0x06);  // GRAPHIC4
        set_dx(machine, 0);
        set_dy(machine, 0);
        set_nx(machine, 4);
        set_ny(machine, 1);
        set_col(machine, 0x5A);
        set_cmd(machine, 0xC0);
        expect(machine.vdp().vram().read(0) == 0x5A, "CommandEngine_Hmmv_ViaMachinePorts");
    }

    // --- A G6-planar destination cross-validates D7's closure: an HMMV
    //     issued in GRAPHIC6 lands at the SAME physical bank/offset the
    //     dedicated vdp_command_address.h function computes. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 0, 0x0A);  // GRAPHIC6 (base 0x14, planar)
        set_dx(machine, 2);
        set_dy(machine, 0);
        set_nx(machine, 2);
        set_ny(machine, 1);
        set_col(machine, 0x0D);
        set_cmd(machine, 0xC0);  // HMMV
        const std::uint32_t addr = graphic6_command_address(2, 0);
        expect(addr == 0x10000u, "CommandEngine_Graphic6_AddressIsBank1");
        expect(machine.vdp().vram().read(addr) == 0x0D, "CommandEngine_Graphic6_WriteLandsAtDedicatedAddress");
    }

    // --- Command engine status handshake: an LMMC transfer's TR/CE sequence
    //     across each discrete R#44 write. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        set_register(machine, 0, 0x06);  // GRAPHIC4
        set_dx(machine, 0);
        set_dy(machine, 0);
        set_nx(machine, 2);
        set_ny(machine, 1);
        set_cmd(machine, 0xB0);  // LMMC, IMP

        expect((peek_s2(machine) & 0x80) != 0, "CommandEngine_Lmmc_TrSetAtStart");
        expect((peek_s2(machine) & 0x01) != 0, "CommandEngine_Lmmc_CeSetAtStart");

        set_col(machine, 0x03);
        expect((peek_s2(machine) & 0x01) != 0, "CommandEngine_Lmmc_CeStillSetAfterFirstByte");

        set_col(machine, 0x0A);
        expect((peek_s2(machine) & 0x01) == 0, "CommandEngine_Lmmc_CeClearsAfterSecondByte");
        expect(machine.vdp().vram().read(0) == 0x3A, "CommandEngine_Lmmc_BothPixelsWritten");
    }

    // --- Two-machine determinism. ---
    {
        Hbf1xvMachine machine_a;
        Hbf1xvMachine machine_b;
        machine_a.cold_boot();
        machine_b.cold_boot();
        for (auto* m : {&machine_a, &machine_b}) {
            set_register(*m, 0, 0x06);
            set_dx(*m, 0);
            set_dy(*m, 0);
            set_nx(*m, 8);
            set_ny(*m, 2);
            set_col(*m, 0x37);
            set_cmd(*m, 0xC0);
        }
        expect(machine_a.vdp().vram().dump() == machine_b.vdp().vram().dump(),
               "CommandEngine_Determinism_TwoMachines_VramIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
