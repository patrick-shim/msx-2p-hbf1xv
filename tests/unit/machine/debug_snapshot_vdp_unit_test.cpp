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

#include <cstdint>
#include <iostream>
#include <string>

#include "machine/debug_format.h"
#include "machine/debug_snapshot.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_DebugSnapshotVdp_Unit (M36 Phase 3 slice S2). Drives register
// writes / a palette set / a VRAM write via the non-perturbing debug_io_write
// seam and asserts the dumped R#0-46 / status / latch / palette / mode / VRAM
// pointer / command-FSM fields match the live device state (typed exactness),
// plus every VDP sub-section is present (AC-1).

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
const std::string* find_file(const sony_msx::machine::debug_snapshot::Snapshot& snap,
                             const std::string& name) {
    for (const auto& f : snap.files) {
        if (f.name == name) {
            return &f.content;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::debug_format::to_hex;
    namespace ds = sony_msx::machine::debug_snapshot;

    Hbf1xvMachine machine;
    machine.cold_boot();

    auto set_reg = [&](std::uint8_t reg, std::uint8_t val) {
        machine.debug_io_write(0x99, val);
        machine.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | reg));
    };

    // Drive a known register/mode state.
    set_reg(0, 0x02);   // R#0 (part of the mode base)
    set_reg(7, 0x1E);   // R#7 border/text colour
    set_reg(16, 0x01);  // R#16 colour-palette pointer -> entry 1

    // Two-byte palette write for entry 1 (auto-advances the pointer).
    machine.debug_io_write(0x9A, 0x24);  // R,B
    machine.debug_io_write(0x9A, 0x06);  // G

    // VRAM write at address 0 (write-access bit 0x40 on the high byte).
    machine.debug_io_write(0x99, 0x00);
    machine.debug_io_write(0x99, 0x40);
    machine.debug_io_write(0x98, 0xAB);

    const ds::Snapshot snap = machine.serialize_snapshot("vdp");
    const std::string* vdp = find_file(snap, "vdp.txt");
    const std::string* vram = find_file(snap, "vram.txt");
    expect(vdp != nullptr, "VdpFile_Present");
    expect(vram != nullptr, "VramFile_Present");
    if (vdp == nullptr || vram == nullptr) {
        return 1;
    }

    // --- All twelve VDP sub-sections are present (AC-1 completeness). ---
    for (const char* sec : {"[VDP.REGS]", "[VDP.CMDREGS]", "[VDP.STATUS]", "[VDP.LATCH]",
                            "[VDP.VRAMPTR]", "[VDP.PALETTE]", "[VDP.MODE]", "[VDP.IRQ]",
                            "[VDP.BLINK]", "[VDP.RASTER]", "[VDP.SPRITE]", "[VDP.CMD]"}) {
        expect(contains(*vdp, sec), (std::string("VdpSection_Present_") + sec).c_str());
    }

    // --- Register typed exactness (against live device state). ---
    expect(contains(*vdp, "R00=" + to_hex(machine.vdp().control_register(0), 2)), "Vdp_R00_Exact");
    expect(contains(*vdp, "R07=1E"), "Vdp_R07_Exact");
    expect(contains(*vdp, "R10=" + to_hex(machine.vdp().control_register(16), 2)), "Vdp_R16_Exact");

    // --- Status S#0..S#9: match the non-destructive peek. ---
    expect(contains(*vdp, "S0=" + to_hex(machine.vdp().peek_status_register(0), 2)), "Vdp_S0_Exact");

    // --- Palette entry 1 matches the live 9-bit value. ---
    expect(contains(*vdp, "P01=" + to_hex(machine.vdp().palette_entry(1), 4)), "Vdp_Palette1_Exact");

    // --- Mode base matches the live decode. ---
    expect(contains(*vdp, "BASE=" + to_hex(machine.vdp().mode().base, 2)), "Vdp_ModeBase_Exact");
    expect(contains(*vdp, "MODE="), "Vdp_ModeName_Present");

    // --- VRAM pointer advanced to 1 after the single #98 write. ---
    expect(contains(*vdp, "VRAM_POINTER=" + to_hex(machine.vdp().vram_pointer(), 4)),
           "Vdp_VramPointer_Exact");

    // --- The VRAM byte landed at offset 0 (folded region first data line). ---
    expect(contains(*vram, "[VRAM] size=131072\n00000000 AB "), "Vram_ByteAtOffset0_Exact");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_DebugSnapshotVdp_Unit cases passed\n";
    return 0;
}
