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

// Suite: Machine_Hbf1xvFmPacCartridge_Integration (M36-S-d, DEC-0050)
//
// Real-software FM-PAC peripheral cartridge integration test using the actual
// roms/fmpac.rom asset (a signature-valid, NON-canonical FM-PAC BIOS variant --
// SHA1 2dc4517e..., matching NEITHER openMSX fmpac.xml dump; validation is
// FUNCTIONAL, never hash-matching, AC-d7). Covers:
//  - auto-identification by the "PAC2OPLL"@0x18 signature (AC-d1/d7),
//  - cartridge load into slot 1 + slot-fabric routing (page 1 via #A8),
//  - PAC2OPLL detect marker readable through the bus (AC-d2),
//  - magic unlock + SRAM read/write through the bus (AC-d2),
//  - .sram persistence across two machine instances (AC-d5),
//  - state-dump SRAM section reflects the inserted cart (S-e relocation),
//  - coexistence: the built-in SRAM-less MSX-MUSIC (#7C/#7D) is unaffected,
//    and the FM-PAC's own OPLL is a distinct chip (AC-d4).

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_fmpac_rom.h"
#include "machine/cartridge_cli.h"
#include "machine/cartridge_identifier.h"
#include "machine/hbf1xv_machine.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

using sony_msx::machine::Hbf1xvMachine;
using CMT = sony_msx::devices::cartridge::CartridgeMapperType;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// Select primary slot 1 in CPU page 1 (0x4000-0x7FFF) so the bus routes there.
// #A8 (PPI port A) packs 2 bits per page: [1:0]=p0 [3:2]=p1 [5:4]=p2 [7:6]=p3.
void select_slot1_page1(Hbf1xvMachine& m) {
    m.debug_io_write(0xA8, 0x04);  // page1 -> primary slot 1, others slot 0
}

}  // namespace

int main() {
    const std::filesystem::path fmpac_path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "fmpac.rom";
    const std::vector<std::uint8_t> fmpac_rom = read_file(fmpac_path);

    // Sanity: the asset is present and FM-PAC-shaped (functional gate, not hash).
    // Size gate re-derived for the DEC-0063 canonical 64 KB FM-PAC swap (sha1
    // 9d789166): accept any valid FM-PAC image (16/32/48/64 KB = 1..4 banks), the
    // exact set CartridgeFmPacRom maps, rather than the retired 16 KB-only assert.
    // This reflects the genuine asset change -- not a weakening; every other
    // functional assertion (AB header, PAC2OPLL marker, bus round-trip) stands.
    expect(sony_msx::devices::cartridge::CartridgeFmPacRom::is_valid_image_size(fmpac_rom.size()),
           "Asset_FmPacRomValidSize");
    expect(fmpac_rom.size() >= 0x20 && fmpac_rom[0] == 'A' && fmpac_rom[1] == 'B',
           "Asset_AbHeader");
    expect(std::string(fmpac_rom.begin() + 0x18, fmpac_rom.begin() + 0x20) == "PAC2OPLL",
           "Asset_Pac2OpllMarker");

    // --- AC-d1/d7: auto-identify by signature (no explicit type, no DB). ---
    {
        sony_msx::machine::ParsedCartridgeSlotCli spec;
        spec.type_was_explicit = false;
        const auto ident = sony_msx::machine::resolve_cartridge_type(
            spec, std::span<const std::uint8_t>(fmpac_rom.data(), fmpac_rom.size()), nullptr);
        expect(ident.type == CMT::FmPac, "AutoIdent_TypeIsFmPac");
        expect(ident.method == sony_msx::machine::CartridgeIdentificationMethod::SignatureFmPac,
               "AutoIdent_MethodIsSignature");
        expect(!ident.unsupported, "AutoIdent_Supported");
    }

    // --- Bare machine: no FM-PAC, empty SRAM dump section (S-e). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(machine.fmpac(1) == nullptr && machine.fmpac(2) == nullptr, "Bare_NoFmPac");
        const std::string dump = machine.serialize_state_dump();
        expect(dump.find("[SRAM] size=0\n") != std::string::npos, "Bare_SramSectionEmpty");
    }

    const std::filesystem::path sram_path =
        std::filesystem::temp_directory_path() / "m36_fmpac_test.sram";
    std::filesystem::remove(sram_path);

    // --- Load + detect + unlock + SRAM write, then persist. ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.set_fmpac_sram_path(sram_path);
        machine.cold_boot();

        const auto result = machine.load_cartridge(1, CMT::FmPac, fmpac_rom);
        expect(result == sony_msx::devices::cartridge::CartridgeLoadResult::Ok, "Load_Ok");
        expect(machine.fmpac(1) != nullptr, "Load_FmPacAccessorNonNull");

        select_slot1_page1(machine);

        // AC-d2: PAC2OPLL detect marker readable through the bus at 0x4018.
        std::string marker;
        for (std::uint16_t a = 0x4018; a < 0x4020; ++a) {
            marker.push_back(static_cast<char>(machine.debug_bus_read(a)));
        }
        expect(marker == "PAC2OPLL", "Bus_Pac2OpllMarkerReadable");

        // AC-d2: magic unlock through the bus (both bytes).
        expect(!machine.fmpac(1)->sram_enabled(), "Bus_LockedBeforeMagic");
        machine.debug_bus_write(0x5FFE, 0x4D);
        machine.debug_bus_write(0x5FFF, 0x69);
        expect(machine.fmpac(1)->sram_enabled(), "Bus_UnlockedAfterMagic");

        // SRAM write through the bus, read-back through the bus.
        machine.debug_bus_write(0x4000, 0xAB);
        machine.debug_bus_write(0x4001, 0xCD);
        machine.debug_bus_write(0x5FFD, 0xEF);  // last addressable SRAM byte
        expect(machine.debug_bus_read(0x4000) == 0xAB, "Bus_SramWrite0");
        expect(machine.debug_bus_read(0x4001) == 0xCD, "Bus_SramWrite1");
        expect(machine.debug_bus_read(0x5FFD) == 0xEF, "Bus_SramWriteLast");

        // State dump now reflects the inserted FM-PAC's 8 KB SRAM (S-e relocate).
        const std::string dump = machine.serialize_state_dump();
        expect(dump.find("[SRAM] size=8192\n") != std::string::npos, "Dump_SramReflectsCart");

        // AC-d4: coexistence -- the built-in MSX-MUSIC (#7C/#7D) is a DISTINCT
        // chip, unaffected by the FM-PAC. Drive the internal OPLL and confirm
        // the FM-PAC's own OPLL did not receive it.
        machine.debug_io_write(0x7C, 0x30);
        machine.debug_io_write(0x7D, 0x55);
        expect(machine.ym2413().register_value(0x30) == 0x55, "Coexist_InternalMsxMusicWorks");
        expect(machine.fmpac(1)->opll().register_value(0x30) == 0x00,
               "Coexist_FmPacOpllIsSeparate");

        // Persist the battery SRAM.
        expect(machine.flush_fmpac_sram(), "Persist_FlushSramOk");
    }

    // --- AC-d5: a fresh machine loads the persisted SRAM on insertion. ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.set_fmpac_sram_path(sram_path);
        machine.cold_boot();
        const auto result = machine.load_cartridge(1, CMT::FmPac, fmpac_rom);
        expect(result == sony_msx::devices::cartridge::CartridgeLoadResult::Ok, "Reload_Ok");
        // The SRAM store carries the previously-saved bytes (battery survived).
        expect(machine.fmpac(1)->sram().read(0x0000) == 0xAB, "Reload_SramByte0");
        expect(machine.fmpac(1)->sram().read(0x0001) == 0xCD, "Reload_SramByte1");
        expect(machine.fmpac(1)->sram().read(0x1FFD) == 0xEF, "Reload_SramByteLast");
    }

    std::filesystem::remove(sram_path);

    // --- AC-d5: absent .sram file -> deterministic zero state (never fabricated). ---
    {
        Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.set_fmpac_sram_path(std::filesystem::temp_directory_path() / "m36_fmpac_absent.sram");
        machine.cold_boot();
        expect(machine.load_cartridge(1, CMT::FmPac, fmpac_rom) == sony_msx::devices::cartridge::CartridgeLoadResult::Ok, "AbsentSram_Setup_FmPacLoaded");
        expect(machine.fmpac(1)->sram().read(0) == 0x00, "AbsentSram_ZeroDefault");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvFmPacCartridge_Integration cases passed\n";
    return 0;
}
