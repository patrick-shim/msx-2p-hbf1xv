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

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_fmpac_rom.h"
#include "devices/cartridge/cartridge_mapper_type.h"
#include "frontend/sdl3_app.h"
#include "frontend/session_summary.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Suite: Frontend_Sdl3App_FmPacAutoload_Integration (M46, DEC-0071, AC-5..9).
// Proves the FM-PAC slot-2 auto-load wiring in Sdl3App::load_configured_assets():
//   - auto-load into slot 2 + SRAM bound + slot 1 free (AC-5),
//   - coexist with a slot-1 game (AC-6),
//   - an explicit slot-2 cart wins (auto-load skipped, AC-7),
//   - fmpac_autoload==false (stock config default / --no-fmpac / --stock) inserts
//     NO FM-PAC and fabricates NO SRAM -- DEC-0050 "NO S-RAM AVAILABLE" (AC-8),
//   - graceful skip on an absent OR corrupt ROM (boot proceeds, AC-9),
//   - an already-present FM-PAC (the human's slot-1 habit) suppresses the
//     slot-2 auto-load (no double FM-PAC).
//
// To NEVER touch a real user save under roms/, the FM-PAC ROM is COPIED into a
// temp dir and the auto-load path (Sdl3AppConfig::fmpac_autoload_rom_path) is
// pointed at the copy, so the derived <copy>.rom.sram lives entirely under temp.

namespace {

using sony_msx::frontend::FmPacAutoloadOutcome;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void set_dummy_drivers() {
#if defined(_WIN32)
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

sony_msx::frontend::Sdl3AppConfig base_config() {
    sony_msx::frontend::Sdl3AppConfig config;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    config.hidden_window = true;
    return config;
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path roms_fmpac = std::filesystem::path(SONY_MSX_ROMS_DIR) / "fmpac.rom";
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    const std::filesystem::path cart_copy = temp_dir / "sdl3_fmpac_autoload_test.rom";
    const std::filesystem::path derived_sram = temp_dir / "sdl3_fmpac_autoload_test.rom.sram";
    const std::filesystem::path corrupt_rom = temp_dir / "sdl3_fmpac_autoload_corrupt.rom";
    const std::filesystem::path slot_rom = temp_dir / "sdl3_fmpac_autoload_slotgame.rom";
    const std::filesystem::path absent_rom = temp_dir / "sdl3_fmpac_autoload_absent_does_not_exist.rom";

    // A synthetic non-FM-PAC 16 KB ROM (Generic16kB accepts any N*16 KB) for the
    // slot-occupancy cases -- avoids depending on a specific game asset.
    write_file(slot_rom, std::vector<std::uint8_t>(0x4000, 0x00));
    std::filesystem::remove(absent_rom);

    // --- AC-8: fmpac_autoload==false (the STOCK config default -- what --stock /
    // --no-fmpac resolve to) inserts NO FM-PAC and binds NO SRAM path. DEC-0050
    // "NO S-RAM AVAILABLE" holds. Needs no FM-PAC asset. ---
    {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.fmpac_autoload = false;  // the anti-drift default
        sony_msx::frontend::Sdl3App app(config);
        const bool init_ok = app.init();
        expect(init_ok, "StockDefault_InitSuccess");
        if (init_ok) {
            expect(app.machine().fmpac(1) == nullptr && app.machine().fmpac(2) == nullptr,
                   "StockDefault_NoFmPacInEitherSlot");
            expect(app.machine().fmpac_sram_path().empty(), "StockDefault_NoSramPathBound_DEC0050");
            expect(!app.machine().flush_fmpac_sram(), "StockDefault_FlushIsNoOp");
            expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::NotAttempted,
                   "StockDefault_OutcomeNotAttempted");
            app.shutdown();
        }
    }

    // --- AC-9 (absent): auto-load ON but the ROM path does not exist -> boot
    // proceeds, no FM-PAC, outcome SkippedAbsent. Needs no FM-PAC asset. ---
    {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.fmpac_autoload = true;
        config.fmpac_autoload_rom_path = absent_rom.string();
        sony_msx::frontend::Sdl3App app(config);
        const bool init_ok = app.init();
        expect(init_ok, "AbsentRom_InitStillSucceeds_NeverFailsBoot");
        if (init_ok) {
            expect(app.machine().fmpac(2) == nullptr, "AbsentRom_NoFmPac");
            expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::SkippedAbsent,
                   "AbsentRom_OutcomeSkippedAbsent");
            app.shutdown();
        }
    }

    // --- AC-9 (corrupt): a deliberately wrong-size file -> load_cartridge fails
    // -> graceful skip, boot proceeds, outcome SkippedInvalid. ---
    {
        write_file(corrupt_rom, std::vector<std::uint8_t>(100, 0xAA));  // 100 bytes: not 1..4 x 16 KB
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.fmpac_autoload = true;
        config.fmpac_autoload_rom_path = corrupt_rom.string();
        sony_msx::frontend::Sdl3App app(config);
        const bool init_ok = app.init();
        expect(init_ok, "CorruptRom_InitStillSucceeds_NeverFailsBoot");
        if (init_ok) {
            expect(app.machine().fmpac(2) == nullptr, "CorruptRom_NoFmPac");
            expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::SkippedInvalid,
                   "CorruptRom_OutcomeSkippedInvalid");
            app.shutdown();
        }
        std::filesystem::remove(corrupt_rom);
    }

    // Guard the real-asset cases: only run when roms/fmpac.rom is present + shaped
    // (mirrors sdl3_app_fmpac_sram_integration_test.cpp's discipline).
    const std::vector<std::uint8_t> fmpac_rom = read_file(roms_fmpac);
    const bool have_fmpac =
        sony_msx::devices::cartridge::CartridgeFmPacRom::is_valid_image_size(fmpac_rom.size()) &&
        fmpac_rom.size() >= 0x20 &&
        std::string(fmpac_rom.begin() + 0x18, fmpac_rom.begin() + 0x20) == "PAC2OPLL";

    if (have_fmpac) {
        std::filesystem::copy_file(roms_fmpac, cart_copy,
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(derived_sram);

        // --- AC-5: auto-load into slot 2, slot 1 free, SRAM path DERIVED beside
        // the (temp) ROM. Absent .sram -> zeroed SRAM (never fabricated). ---
        {
            sony_msx::frontend::Sdl3AppConfig config = base_config();
            config.fmpac_autoload = true;
            config.fmpac_autoload_rom_path = cart_copy.string();
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "Autoload_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac(2) != nullptr, "Autoload_FmPacInSlot2");
                expect(app.machine().fmpac(1) == nullptr, "Autoload_Slot1Free");
                expect(!app.machine().cartridge_slot1().loaded(), "Autoload_Slot1NotLoaded");
                expect(app.machine().fmpac_sram_path() ==
                           std::filesystem::path(cart_copy.string() + ".sram"),
                       "Autoload_SramPathDerivedBesideRom");
                expect(app.machine().fmpac(2)->sram().read(0x0000) == 0x00,
                       "Autoload_AbsentSramZeroed");
                expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::LoadedSlot2,
                       "Autoload_OutcomeLoadedSlot2");
                app.shutdown();
            }
        }
        std::filesystem::remove(derived_sram);

        // --- AC-6: coexist with a slot-1 game -> FM-PAC in slot 2 AND the game
        // mapper in slot 1. ---
        {
            sony_msx::frontend::Sdl3AppConfig config = base_config();
            config.cart1_path = slot_rom.string();
            config.cart1_type = sony_msx::devices::cartridge::CartridgeMapperType::Generic16kB;
            config.cart1_type_explicit = true;
            config.fmpac_autoload = true;
            config.fmpac_autoload_rom_path = cart_copy.string();
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "Coexist_InitSuccess");
            if (init_ok) {
                expect(app.machine().cartridge_slot1().loaded(), "Coexist_Slot1GameLoaded");
                expect(app.machine().fmpac(1) == nullptr, "Coexist_Slot1IsNotFmPac");
                expect(app.machine().fmpac(2) != nullptr, "Coexist_FmPacInSlot2");
                expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::LoadedSlot2,
                       "Coexist_OutcomeLoadedSlot2");
                app.shutdown();
            }
        }
        std::filesystem::remove(derived_sram);

        // --- AC-7: an explicit --slot2 cart wins; the auto-load is SKIPPED and
        // does NOT force an FM-PAC over the user cart. ---
        {
            sony_msx::frontend::Sdl3AppConfig config = base_config();
            config.cart2_path = slot_rom.string();
            config.cart2_type = sony_msx::devices::cartridge::CartridgeMapperType::Generic16kB;
            config.cart2_type_explicit = true;
            config.fmpac_autoload = true;
            config.fmpac_autoload_rom_path = cart_copy.string();
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "ExplicitSlot2_InitSuccess");
            if (init_ok) {
                expect(app.machine().cartridge_slot2().loaded(), "ExplicitSlot2_UserCartLoaded");
                expect(app.machine().fmpac(2) == nullptr, "ExplicitSlot2_NotForcedToFmPac");
                expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::SkippedSlot2InUse,
                       "ExplicitSlot2_OutcomeSkipped");
                app.shutdown();
            }
        }

        // --- Already-present FM-PAC (the human's slot-1 habit): an FM-PAC in
        // slot 1 suppresses the slot-2 auto-load (no double FM-PAC). ---
        {
            sony_msx::frontend::Sdl3AppConfig config = base_config();
            config.cart1_path = cart_copy.string();
            config.cart1_type_explicit = false;  // auto-identify -> FmPac (PAC2OPLL sig)
            config.fmpac_autoload = true;
            config.fmpac_autoload_rom_path = cart_copy.string();
            // Keep SRAM in-memory-only so the two FM-PAC .sram bindings can't clash.
            config.fmpac_sram_disabled = true;
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "AlreadyPresent_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac(1) != nullptr, "AlreadyPresent_FmPacInSlot1");
                expect(app.machine().fmpac(2) == nullptr, "AlreadyPresent_NoDoubleFmPacInSlot2");
                expect(app.fmpac_autoload_outcome() == FmPacAutoloadOutcome::SkippedAlreadyPresent,
                       "AlreadyPresent_OutcomeSkipped");
                app.shutdown();
            }
        }

        std::filesystem::remove(derived_sram);
        std::filesystem::remove(cart_copy);
    } else {
        std::cerr << "note: roms/fmpac.rom absent or not FM-PAC-shaped; ROM-dependent auto-load "
                     "cases skipped (stock-default + absent + corrupt cases still ran)\n";
    }

    std::filesystem::remove(slot_rom);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3App_FmPacAutoload_Integration cases passed\n";
    return 0;
}
