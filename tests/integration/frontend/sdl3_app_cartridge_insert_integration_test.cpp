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

// Suite: Frontend_Sdl3AppCartridgeInsert_Integration (M56, DEC-0084, planner §9
// test 7; slice S3).
//
// The F2 Open Cartridge apply proven through the DIRECT public
// apply_open_cartridge(slot, path) seam (hidden_window=true, no OS dialog):
//   * a valid ROM inserts (cartridge_slotN().loaded()); the machine RESETS
//     (elapsed_cycles back to power-on 0); the cart PERSISTS the reset.
//   * the FM-PAC path binds its SRAM BEFORE load (verified via a temp .sram path
//     override so a real roms/fmpac.rom.sram is NEVER touched).
//   * a missing ROM path is a graceful no-op (slot stays empty, no reset).
// Skip-gated on the untracked roms/fmpac.rom asset.

#include <SDL3/SDL.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "frontend/sdl3_app.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

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

sony_msx::frontend::Sdl3AppConfig base_config() {
    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    return config;
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::string fmpac = std::string(SONY_MSX_ROMS_DIR) + "/fmpac.rom";
    const bool have_fmpac = std::filesystem::exists(fmpac);
    if (!have_fmpac) {
        std::cout << "SKIP (cart-insert cases): roms/fmpac.rom not present -- "
                     "skip-when-absent asset guard\n";
    }

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "hbf1xv_cart_insert";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path temp_sram = dir / "fmpac_test.sram";

    // --- A valid FM-PAC ROM inserts into slot 1, resets, and persists the reset. ---
    if (have_fmpac) {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        // Redirect the FM-PAC SRAM to a temp path so the real save is never touched.
        config.fmpac_sram_path = temp_sram.string();
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            expect(!app.machine().cartridge_slot1().loaded(), "Insert_BootSlot1Empty");
            // Advance a few frames so elapsed_cycles > 0 before the insert-reset.
            for (int i = 0; i < 4; ++i) {
                app.run_one_frame();
            }
            expect(app.machine().elapsed_cycles() > 0, "Insert_ElapsedNonZeroBeforeInsert");

            app.apply_open_cartridge(1, fmpac);
            expect(app.machine().cartridge_slot1().loaded(), "Insert_Slot1LoadedAfterInsert");
            // Insert implied a reset -> the machine is back at power-on cycle 0.
            expect(app.machine().elapsed_cycles() == 0, "Insert_ResetToPowerOn");
            expect(app.machine().frame_count() == 0, "Insert_FrameCountZeroAfterReset");
            // FM-PAC SRAM was bound BEFORE load (to our temp path).
            expect(app.machine().fmpac(1) != nullptr, "Insert_Slot1IsFmPac");
            expect(app.machine().fmpac_sram_path() == std::filesystem::path(temp_sram),
                   "Insert_FmPacSramBoundToTempPath");

            // The cart survives a further explicit reset (cold_boot never unloads).
            app.request_reset();
            expect(app.machine().cartridge_slot1().loaded(), "Insert_CartPersistsFurtherReset");
            app.shutdown();
        }
    }

    // --- Insert into slot 2 independently. ---
    if (have_fmpac) {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.fmpac_sram_path = temp_sram.string();
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            app.apply_open_cartridge(2, fmpac);
            expect(app.machine().cartridge_slot2().loaded(), "InsertSlot2_Loaded");
            expect(!app.machine().cartridge_slot1().loaded(), "InsertSlot2_Slot1StaysEmpty");
            app.shutdown();
        }
    }

    // --- A missing ROM path is a graceful no-op (no insert, no reset). ---
    {
        sony_msx::frontend::Sdl3App app(base_config());
        if (app.init()) {
            for (int i = 0; i < 3; ++i) {
                app.run_one_frame();
            }
            const std::uint64_t before = app.machine().elapsed_cycles();
            app.apply_open_cartridge(1, (dir / "does_not_exist.rom").string());
            expect(!app.machine().cartridge_slot1().loaded(), "MissingRom_Slot1StaysEmpty");
            // No reset happened (elapsed did not drop back to 0).
            expect(app.machine().elapsed_cycles() >= before, "MissingRom_NoReset");
            app.shutdown();
        }
    }

    std::filesystem::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppCartridgeInsert_Integration cases passed\n";
    return 0;
}
