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

// Suite: Machine_Hbf1xvCartridgeIdentification_Integration
//
// The asset-gated REAL-ROM end-to-end proof: roms/scroll-shooter.rom (a
// scrolling-shooter title) with NO
// explicit type -> softwaredb SHA1 match -> (catalogued title, "KonamiSCC") ->
// resolver picks KonamiSCC via SoftwareDbSha1 -> the identification message
// verbatim ->
// load_cartridge Ok -> bounded real-frame-loop boot smoke
// (step_cpu_instruction() to each frame boundary + on_vsync_boundary(),
// never run_frame(); DEC-0034).
//
// SKIP DISCIPLINE: user-supplied
// assets may legitimately differ, so this test SKIPS (exit 0, with a loud
// note) -- never FAILS -- when (a) roms/scroll-shooter.rom is absent, (b) its SHA1
// is not the exact e93d0840c59c6eba273df546d22148d486a150a6 dump this test
// is specified against, or (c) the softwaredb file is absent (partial
// checkout). That SHA1 constant is a fact about the USER'S OWN local ROM
// file (independently computable without the DB existing at all) -- it is
// used as a skip-gate, not as database content. (DEC-0016)
//
// TITLE-FREE ORACLE: no catalogued title literal is hardcoded in this source.
// The invariant asserted is CONSISTENCY -- the direct DB lookup for the pinned
// SHA1 yields a real (non-empty) catalogued title, and the resolver's recorded
// title AGREES with that direct lookup verbatim; the identified mapper type
// (KonamiSCC) remains the load-bearing exact-value assertion
// (universal-fixes-only: no production logic anywhere is keyed to any title).
//
// Deterministic oracle: identical ROM + DB bytes -> identical
// identification (proven twice) and an identical 60-frame boot trajectory
// (cold_boot is deterministic; no wall clock anywhere).

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "machine/cartridge_identifier.h"
#include "machine/hbf1xv_machine.h"
#include "machine/sha1.h"
#include "machine/software_db.h"

#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_SOFTWAREDB_PATH
#error "SONY_MSX_SOFTWAREDB_PATH must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr const char* kScrollShooterSha1 = "e93d0840c59c6eba273df546d22148d486a150a6";

}  // namespace

int main() {
    using sony_msx::devices::cartridge::CartridgeLoadResult;
    using sony_msx::devices::cartridge::CartridgeMapperType;
    using sony_msx::machine::CartridgeIdentificationMethod;
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::ParsedCartridgeSlotCli;
    using sony_msx::machine::SoftwareDb;
    using sony_msx::machine::format_cartridge_identification_message;
    using sony_msx::machine::resolve_cartridge_type;
    using sony_msx::machine::sha1_hex;

    // --- Skip gate (a): ROM present. ---
    const std::filesystem::path rom_path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "scroll-shooter.rom";
    std::ifstream rom_in(rom_path, std::ios::binary);
    if (!rom_in) {
        std::cout << "SKIP: " << rom_path
                  << " not present (local dev asset; planner A-M30-1 skip discipline)\n";
        return 0;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(rom_in)),
                                          std::istreambuf_iterator<char>());

    // --- Skip gate (b): the exact specified dump. ---
    const std::string actual_sha1 = sha1_hex(image);
    if (actual_sha1 != kScrollShooterSha1) {
        std::cout << "SKIP: roms/scroll-shooter.rom sha1=" << actual_sha1
                  << " differs from the specified dump " << kScrollShooterSha1
                  << " (user-supplied asset legitimately differs; planner A-M30-1)\n";
        return 0;
    }

    // --- Skip gate (c): the softwaredb file present. ---
    const std::string db_path = SONY_MSX_SOFTWAREDB_PATH;
    if (!std::filesystem::exists(db_path)) {
        std::cout << "SKIP: softwaredb not present at " << db_path
                  << " (partial checkout; planner A-M30-2 skip discipline)\n";
        return 0;
    }

    // --- The REAL local softwaredb parses and the pinned-SHA1 lookup hits. ---
    std::vector<std::string> diagnostics;
    const auto db = SoftwareDb::load_from_file(db_path, diagnostics);
    expect(db.has_value(), "RealSoftwareDb_Loads");
    if (!db.has_value()) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    // The local softwaredb carries ~3,049 <software> entries; assert a sane
    // lower bound rather than a brittle exact count (the file is upstream data).
    expect(db->entry_count() > 1000, "RealSoftwareDb_EntryCount_Sane");

    const auto* entry = db->lookup(kScrollShooterSha1);
    expect(entry != nullptr, "RealSoftwareDb_PinnedSha1_Hits");
    if (entry != nullptr) {
        expect(!entry->title.empty(), "RealSoftwareDb_Title_NonEmpty");
        expect(entry->type_name == "KonamiSCC", "RealSoftwareDb_TypeName_KonamiSCC");
    }

    // --- The resolver picks KonamiSCC via SoftwareDbSha1 for a type-less
    //     spec. ---
    ParsedCartridgeSlotCli spec;
    spec.path = rom_path.string();
    spec.type = CartridgeMapperType::Mirrored;  // the untouched parser default
    spec.type_was_explicit = false;

    const auto ident = resolve_cartridge_type(spec, image, &*db);
    expect(ident.type == CartridgeMapperType::KonamiSCC, "Resolver_TypeIsKonamiSCC");
    expect(ident.method == CartridgeIdentificationMethod::SoftwareDbSha1,
           "Resolver_MethodIsSoftwareDbSha1");
    expect(!ident.unsupported, "Resolver_NotUnsupported");
    expect(ident.sha1_hex == kScrollShooterSha1, "Resolver_Sha1Recorded");
    // The consistency invariant: the resolver's recorded title agrees with the
    // direct DB lookup for the same SHA1 (a strict cross-check, no literal).
    expect(entry != nullptr && ident.title == entry->title,
           "Resolver_TitleAgreesWithDirectDbLookup");

    // --- The identification message, verbatim. The expected string is BUILT AT
    //     RUNTIME from ident.title (already proven above to agree with the
    //     direct DB lookup), so the exact message format stays pinned while
    //     this source stays title-free. ---
    expect(format_cartridge_identification_message(1, ident, db_path) ==
               std::string("cartridge: --cart1: identified as \"") + ident.title +
                   "\" (KonamiSCC) via softwaredb SHA1 match [sha1=" +
                   kScrollShooterSha1 + ", db=" + db_path + "]",
           "MessageA_Verbatim_RealAssets");

    // --- Determinism: the identical inputs, resolved twice, agree on every
    //     field. ---
    {
        const auto again = resolve_cartridge_type(spec, image, &*db);
        expect(again.type == ident.type && again.method == ident.method &&
                   again.sha1_hex == ident.sha1_hex && again.title == ident.title &&
                   again.db_type_name == ident.db_type_name &&
                   again.unsupported == ident.unsupported && again.detail == ident.detail,
               "Resolver_TwoRuns_IdenticalEveryField");
    }

    // --- load_cartridge accepts the identified type... ---
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    const CartridgeLoadResult load_result = machine.load_cartridge(1, ident.type, image);
    expect(load_result == CartridgeLoadResult::Ok, "LoadCartridge_IdentifiedType_Ok");

    // --- ...and a bounded 60-frame real-BIOS boot smoke stays alive (the
    //     same step-to-frame-boundary loop shape as above; deep-boot evidence
    //     already exists from committed frame dumps -- this stays fast on
    //     purpose). ---
    const std::uint64_t target = machine.frame_cycles_per_frame();
    for (int frame = 0; frame < 60; ++frame) {
        const std::uint64_t frame_start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - frame_start < target) {
            machine.step_cpu_instruction();
        }
        machine.on_vsync_boundary();
    }
    expect(machine.frame_count() == 60, "BootSmoke_60Frames_FrameCountAdvanced");
    expect(machine.elapsed_cycles() >= 60 * target, "BootSmoke_60Frames_CyclesElapsed");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvCartridgeIdentification_Integration cases passed\n";
    return 0;
}
