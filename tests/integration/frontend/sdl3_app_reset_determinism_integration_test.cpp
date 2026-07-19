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

// Suite: Frontend_Sdl3AppResetDeterminism_Integration (M56, DEC-0084, planner
// §9 test 5; slice S1).
//
// The reset-determinism ORACLE for Sdl3App::request_reset() (the F4 Reset seam),
// driven directly under hidden_window=true (no menu, no dialog). Two variants
// over the deterministic golden serialize_state_dump() (CPU+DRAM+SRAM+VRAM,
// hbf1xv_machine.h:424):
//
//   Oracle A (return-to-power-on): init() -> dump0; run K frames; request_reset()
//     -> dump1; assert dump1 == dump0.
//   Oracle B (fresh-boot equivalence): App-FRESH init() -> run K -> dumpFresh;
//     App-RESET init() -> run M -> request_reset() -> run K -> dumpReset; assert
//     dumpReset == dumpFresh.
//
// Bare config (no cart) so the SRAM section is empty in both -- FM-PAC SRAM
// legitimately persists a reset and would otherwise diverge (a documented oracle
// boundary). Plus: a mounted disk SURVIVES the reset (image() != nullptr) and a
// loaded cart SURVIVES the reset (cartridge_slot1().loaded()). Both are
// skip-gated on their untracked assets.

#include <SDL3/SDL.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/sdl3_app.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
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

sony_msx::frontend::Sdl3AppConfig bare_config() {
    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    return config;
}

constexpr int kFramesK = 5;
constexpr int kFramesM = 3;

}  // namespace

int main() {
    set_dummy_drivers();

    // --- Oracle A: post-reset state == post-init state (bare config). ---
    {
        sony_msx::frontend::Sdl3App app(bare_config());
        const bool init_ok = app.init();
        expect(init_ok, "OracleA_InitSuccess");
        if (init_ok) {
            const std::string dump0 = app.machine().serialize_state_dump();
            for (int i = 0; i < kFramesK; ++i) {
                app.run_one_frame();
            }
            app.request_reset();
            const std::string dump1 = app.machine().serialize_state_dump();
            expect(dump1 == dump0, "OracleA_PostResetEqualsPostInit");
            // The reset returned the scheduler to power-on cycle 0.
            expect(app.machine().elapsed_cycles() == 0, "OracleA_ElapsedCyclesZeroAfterReset");
            expect(app.machine().frame_count() == 0, "OracleA_FrameCountZeroAfterReset");
            app.shutdown();
        }
    }

    // --- Oracle B: fresh-boot + K == reset + K (bare config). ---
    {
        std::string dump_fresh;
        {
            sony_msx::frontend::Sdl3App app(bare_config());
            const bool init_ok = app.init();
            expect(init_ok, "OracleB_FreshInitSuccess");
            if (init_ok) {
                for (int i = 0; i < kFramesK; ++i) {
                    app.run_one_frame();
                }
                dump_fresh = app.machine().serialize_state_dump();
                app.shutdown();
            }
        }
        std::string dump_reset;
        {
            sony_msx::frontend::Sdl3App app(bare_config());
            const bool init_ok = app.init();
            expect(init_ok, "OracleB_ResetInitSuccess");
            if (init_ok) {
                for (int i = 0; i < kFramesM; ++i) {
                    app.run_one_frame();
                }
                app.request_reset();
                for (int i = 0; i < kFramesK; ++i) {
                    app.run_one_frame();
                }
                dump_reset = app.machine().serialize_state_dump();
                app.shutdown();
            }
        }
        expect(!dump_fresh.empty() && dump_reset == dump_fresh,
               "OracleB_ResetPlusKEqualsFreshPlusK");
    }

    // --- Repeatability: request_reset() is idempotent across two resets. ---
    {
        sony_msx::frontend::Sdl3App app(bare_config());
        if (app.init()) {
            const std::string dump0 = app.machine().serialize_state_dump();
            for (int i = 0; i < kFramesK; ++i) {
                app.run_one_frame();
            }
            app.request_reset();
            for (int i = 0; i < kFramesM; ++i) {
                app.run_one_frame();
            }
            app.request_reset();
            const std::string dump2 = app.machine().serialize_state_dump();
            expect(dump2 == dump0, "DoubleReset_ReturnsToPowerOn");
            app.shutdown();
        }
    }

    // --- A mounted disk SURVIVES the reset (skip-gated on the untracked asset). ---
#if defined(SONY_MSX_DISKS_DIR)
    {
        const std::string disk = std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk";
        if (std::filesystem::exists(disk)) {
            sony_msx::frontend::Sdl3AppConfig config = bare_config();
            config.disk_paths = {disk};
            sony_msx::frontend::Sdl3App app(config);
            if (app.init()) {
                expect(app.machine().disk_drive().image() != nullptr,
                       "DiskSurvives_AttachedAtBoot");
                app.request_reset();
                expect(app.machine().disk_drive().image() != nullptr,
                       "DiskSurvives_StillAttachedAfterReset");
                expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                       "DiskSurvives_PointerReattached");
                expect(app.machine().disk_drive().disk_changed(),
                       "DiskSurvives_MediaChangeSignalledAfterReset");
                app.shutdown();
            }
        } else {
            std::cout << "SKIP DiskSurvives: disks/msxdos23.dsk not present\n";
        }
    }
#endif

    // --- A loaded cartridge SURVIVES the reset (skip-gated on the untracked
    //     FM-PAC ROM; SRAM binding disabled so no real save is touched). ---
#if defined(SONY_MSX_ROMS_DIR)
    {
        const std::string rom = std::string(SONY_MSX_ROMS_DIR) + "/fmpac.rom";
        if (std::filesystem::exists(rom)) {
            sony_msx::frontend::Sdl3AppConfig config = bare_config();
            config.cart1_path = rom;
            config.cart1_type_explicit = false;  // auto-identify via the shared resolver
            config.fmpac_sram_disabled = true;   // never touch a real roms/fmpac.rom.sram
            sony_msx::frontend::Sdl3App app(config);
            if (app.init()) {
                expect(app.machine().cartridge_slot1().loaded(), "CartSurvives_LoadedAtBoot");
                app.request_reset();
                expect(app.machine().cartridge_slot1().loaded(),
                       "CartSurvives_StillLoadedAfterReset");
                app.shutdown();
            }
        } else {
            std::cout << "SKIP CartSurvives: roms/fmpac.rom not present\n";
        }
    }
#endif

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppResetDeterminism_Integration cases passed\n";
    return 0;
}
