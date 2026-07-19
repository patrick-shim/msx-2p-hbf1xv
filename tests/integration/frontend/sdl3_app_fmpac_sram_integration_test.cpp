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
#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Suite: Frontend_Sdl3App_FmPacSram_Integration (M36 "FM-PAC S-RAM saves work"
// for the SDL3 build). The machine core + headless already persist FM-PAC
// battery SRAM; this proves the SDL3 frontend (Sdl3App) does too:
//  - default SRAM path is auto-derived from the FM-PAC cart's ROM path
//    (<cart>.rom -> <cart>.rom.sram), a real FM-PAC persisting beside itself,
//  - an end-to-end round-trip: write a pattern via the bus -> shutdown()
//    flushes -> a FRESH Sdl3App load-on-insert restores it byte-for-byte,
//  - --fmpac-sram <path> overrides the derived default,
//  - --no-fmpac-sram opts out (no path bound, in-memory-only),
//  - NO FM-PAC cart is a clean no-op (no path, flush is harmless),
//  - the CLI parser accepts --fmpac-sram / --no-fmpac-sram.
//
// Harness mirrors sdl3_app_multi_disk_integration_test.cpp (dummy SDL video/
// audio drivers + hidden window) and the bus write/unlock protocol from
// hbf1xv_m36_fmpac_cartridge_integration_test.cpp. To avoid ever touching a
// real user's roms/fmpac.rom.sram save, the FM-PAC ROM is COPIED into a temp
// dir and the derived <copy>.sram lives entirely under temp.

namespace {

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

sony_msx::frontend::Sdl3AppConfig make_config(const std::string& cart_path) {
    sony_msx::frontend::Sdl3AppConfig config;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    config.hidden_window = true;  // test convenience
    config.cart1_path = cart_path;
    // Leave cart1_type_explicit at its default (true) but let the auto-identifier
    // run so the FM-PAC signature ("PAC2OPLL") resolves to FmPac regardless of
    // the default Mirrored type -- mirror what a CLI `--cart1 roms/fmpac.rom`
    // with no --cart1-type does (type_was_explicit == false).
    config.cart1_type_explicit = false;
    return config;
}

// Drive the bus like a game does: select slot 1 in page 1, magic-unlock the
// FM-PAC SRAM, then write a recognizable pattern. Mirrors the M36 machine test.
void write_sram_pattern(sony_msx::machine::Hbf1xvMachine& m) {
    m.debug_io_write(0xA8, 0x04);       // page1 -> primary slot 1 (FM-PAC bay)
    m.debug_bus_write(0x5FFE, 0x4D);    // magic unlock byte 1
    m.debug_bus_write(0x5FFF, 0x69);    // magic unlock byte 2
    m.debug_bus_write(0x4000, 0xAB);
    m.debug_bus_write(0x4001, 0xCD);
    m.debug_bus_write(0x5FFD, 0xEF);    // last addressable SRAM byte
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path roms_fmpac =
        std::filesystem::path(SONY_MSX_ROMS_DIR) / "fmpac.rom";

    // Copy the FM-PAC ROM into temp so the derived <copy>.sram never collides
    // with a real user save under roms/.
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    const std::filesystem::path cart_copy = temp_dir / "sdl3_fmpac_sram_test.rom";
    const std::filesystem::path derived_sram = temp_dir / "sdl3_fmpac_sram_test.rom.sram";
    const std::filesystem::path override_sram = temp_dir / "sdl3_fmpac_sram_override.sram";

    // Guard: only run the ROM-dependent cases when the asset is present + shaped.
    // Size gate re-derived for the DEC-0063 canonical 64 KB FM-PAC swap: accept
    // any valid FM-PAC image (1..4 banks) so the round-trip actually exercises the
    // real canonical ROM, not just the retired 16 KB variant.
    const std::vector<std::uint8_t> fmpac_rom = read_file(roms_fmpac);
    const bool have_fmpac =
        sony_msx::devices::cartridge::CartridgeFmPacRom::is_valid_image_size(fmpac_rom.size()) &&
        fmpac_rom.size() >= 0x20 &&
        std::string(fmpac_rom.begin() + 0x18, fmpac_rom.begin() + 0x20) == "PAC2OPLL";

    // --- CLI parse: --fmpac-sram <path> + --no-fmpac-sram (no asset needed). ---
    {
        const auto parsed = sony_msx::frontend::parse_sdl3_cli(
            {"--fmpac-sram", "my/save.sram"});
        expect(parsed.errors.empty(), "Cli_FmpacSram_NoErrors");
        expect(parsed.fmpac_sram_path.has_value() && *parsed.fmpac_sram_path == "my/save.sram",
               "Cli_FmpacSram_PathCaptured");
        expect(!parsed.fmpac_sram_disabled, "Cli_FmpacSram_NotDisabled");
    }
    {
        const auto parsed = sony_msx::frontend::parse_sdl3_cli({"--no-fmpac-sram"});
        expect(parsed.errors.empty(), "Cli_NoFmpacSram_NoErrors");
        expect(parsed.fmpac_sram_disabled, "Cli_NoFmpacSram_DisabledSet");
        expect(!parsed.fmpac_sram_path.has_value(), "Cli_NoFmpacSram_NoPath");
    }
    {
        // Missing value -> a loud parse error (mirrors every other value flag).
        const auto parsed = sony_msx::frontend::parse_sdl3_cli({"--fmpac-sram"});
        expect(!parsed.errors.empty(), "Cli_FmpacSram_MissingValueErrors");
    }

    // --- No FM-PAC cartridge: complete no-op (no path bound, flush harmless). ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = SONY_MSX_BIOS_DIR;
        config.hidden_window = true;
        sony_msx::frontend::Sdl3App app(config);
        const bool init_ok = app.init();
        expect(init_ok, "NoCart_InitSuccess");
        if (init_ok) {
            expect(app.machine().fmpac(1) == nullptr && app.machine().fmpac(2) == nullptr,
                   "NoCart_NoFmPac");
            expect(app.machine().fmpac_sram_path().empty(), "NoCart_NoSramPathBound");
            expect(!app.machine().flush_fmpac_sram(), "NoCart_FlushIsNoOp");
            app.shutdown();
        }
    }

    if (have_fmpac) {
        std::filesystem::copy_file(roms_fmpac, cart_copy,
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(derived_sram);
        std::filesystem::remove(override_sram);

        // --- Default derivation: <cart>.rom -> <cart>.rom.sram (no override). ---
        {
            sony_msx::frontend::Sdl3AppConfig config = make_config(cart_copy.string());
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "Default_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac(1) != nullptr, "Default_FmPacResolved");
                expect(app.machine().fmpac_sram_path() ==
                           std::filesystem::path(cart_copy.string() + ".sram"),
                       "Default_SramPathDerivedFromCart");
                // Absent .sram -> zeroed SRAM (never fabricated), M20/M36 pattern.
                expect(app.machine().fmpac(1)->sram().read(0x0000) == 0x00,
                       "Default_AbsentSramZeroed");
                app.shutdown();
            }
        }
        // The derivation case with a zeroed FM-PAC still flushes on shutdown
        // (a real battery always persists); a zero file is harmless. Clear it so
        // the round-trip below starts from a genuinely absent file.
        std::filesystem::remove(derived_sram);

        // --- End-to-end round-trip: write -> shutdown flush -> reload restore. ---
        {
            // Instance 1: write a recognizable pattern, shutdown flushes it.
            sony_msx::frontend::Sdl3AppConfig config = make_config(cart_copy.string());
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "RoundTrip_Write_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac(1) != nullptr, "RoundTrip_Write_FmPacResolved");
                write_sram_pattern(app.machine());
                expect(app.machine().debug_bus_read(0x4000) == 0xAB, "RoundTrip_Write_Byte0");
                expect(app.machine().debug_bus_read(0x4001) == 0xCD, "RoundTrip_Write_Byte1");
                expect(app.machine().debug_bus_read(0x5FFD) == 0xEF, "RoundTrip_Write_ByteLast");
                app.shutdown();  // flush_fmpac_sram() writes the derived .sram
            }
            expect(std::filesystem::exists(derived_sram), "RoundTrip_SramFileWritten");

            // Instance 2: a FRESH app with the SAME config restores on insert.
            sony_msx::frontend::Sdl3AppConfig config2 = make_config(cart_copy.string());
            sony_msx::frontend::Sdl3App app2(config2);
            const bool init2 = app2.init();
            expect(init2, "RoundTrip_Reload_InitSuccess");
            if (init2) {
                expect(app2.machine().fmpac(1) != nullptr, "RoundTrip_Reload_FmPacResolved");
                expect(app2.machine().fmpac(1)->sram().read(0x0000) == 0xAB,
                       "RoundTrip_Reload_Byte0");
                expect(app2.machine().fmpac(1)->sram().read(0x0001) == 0xCD,
                       "RoundTrip_Reload_Byte1");
                expect(app2.machine().fmpac(1)->sram().read(0x1FFD) == 0xEF,
                       "RoundTrip_Reload_ByteLast");
                app2.shutdown();
            }
        }

        // --- Explicit --fmpac-sram override wins over the derived default. ---
        {
            std::filesystem::remove(override_sram);
            sony_msx::frontend::Sdl3AppConfig config = make_config(cart_copy.string());
            config.fmpac_sram_path = override_sram.string();
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "Override_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac_sram_path() == override_sram,
                       "Override_SramPathIsOverride");
                app.shutdown();
            }
        }

        // --- --no-fmpac-sram opts out: no path bound, SRAM in-memory-only. ---
        {
            sony_msx::frontend::Sdl3AppConfig config = make_config(cart_copy.string());
            config.fmpac_sram_disabled = true;
            sony_msx::frontend::Sdl3App app(config);
            const bool init_ok = app.init();
            expect(init_ok, "OptOut_InitSuccess");
            if (init_ok) {
                expect(app.machine().fmpac(1) != nullptr, "OptOut_FmPacResolved");
                expect(app.machine().fmpac_sram_path().empty(), "OptOut_NoSramPathBound");
                expect(!app.machine().flush_fmpac_sram(), "OptOut_FlushIsNoOp");
                app.shutdown();
            }
        }

        // Cleanup temp artifacts (never touches roms/).
        std::filesystem::remove(derived_sram);
        std::filesystem::remove(override_sram);
        std::filesystem::remove(cart_copy);
    } else {
        std::cerr << "note: roms/fmpac.rom absent or not FM-PAC-shaped; "
                     "ROM-dependent cases skipped (CLI + no-cart cases still ran)\n";
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3App_FmPacSram_Integration cases passed\n";
    return 0;
}
