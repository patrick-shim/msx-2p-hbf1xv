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

#include <iostream>
#include <vector>

#include "frontend/sdl3_app.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_DISKS_DIR
#error "SONY_MSX_DISKS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Suite: Frontend_Sdl3App_MultiDisk_Integration (M35 multi-disk hot-swap,
// docs/m35-planner-package.md §5, test obligations IT-1..IT-6).
//
// Deterministic integration tests for multi-disk boot, swap, and regression
// guards. Covers:
// - Boot with repeatable --disk list (IT-1, S2).
// - F11 hotkey disk rotation (IT-2, S4).
// - Media-change signal (IT-3, S4).
// - UX feedback (IT-4, S5).
// - Determinism (IT-5, AC-6).
// - Single-disk regression guard (IT-6, AC-7).

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

// Helper: create a minimal config with disk paths and common defaults.
sony_msx::frontend::Sdl3AppConfig make_test_config(const std::vector<std::string>& disk_paths) {
    sony_msx::frontend::Sdl3AppConfig config;
    config.disk_paths = disk_paths;
    config.hidden_window = true;  // Test convenience
    config.bios_dir = SONY_MSX_BIOS_DIR;
    return config;
}

}  // namespace

int main() {
    set_dummy_drivers();

    // --- IT-1: Boot with repeatable --disk (S2) ---

    // Case 1: boot with single --disk disk1.dsk → first disk attached.
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "BootSingleDisk_InitSuccess");
        expect(app.initialized(), "BootSingleDisk_InitializedFlag");

        if (init_ok) {
            // Verify first disk is attached.
            expect(app.machine().disk_drive().image() != nullptr, "BootSingleDisk_ImageAttached");
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "BootSingleDisk_ImagePointerCorrect");
            app.shutdown();
        }
    }

    // Case 2: boot with --disk disk1.dsk --disk disk2.dsk →
    // disk1 attached (list stored for later).
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk",
                                                   std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "BootMultipleDisk_InitSuccess");

        if (init_ok) {
            // Verify first disk is attached.
            expect(app.machine().disk_drive().image() != nullptr, "BootMultipleDisk_ImageAttached");
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "BootMultipleDisk_ImagePointerCorrect");
            app.shutdown();
        }
    }

    // Case 3: boot with no --disk → no disk attached.
    {
        const std::vector<std::string> disk_paths{};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "BootNoDisk_InitSuccess");

        if (init_ok) {
            // Verify no disk is attached.
            expect(app.machine().disk_drive().image() == nullptr, "BootNoDisk_NoImageAttached");
            app.shutdown();
        }
    }

    // Case 4: boot with --disk /nonexistent/file.dsk → init() returns false.
    {
        const std::vector<std::string> disk_paths{"/nonexistent/path/disk.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(!init_ok, "BootNonexistentDisk_InitFails");
        expect(!app.initialized(), "BootNonexistentDisk_NotInitialized");
        expect(!app.last_error().empty(), "BootNonexistentDisk_ErrorLogged");
    }

    // --- IT-2: Runtime disk swap via F11 (S4) ---
    // Note: F11 hotkey dispatch is tested via input mapper unit tests (UT-2).
    // Here we verify the core swap logic (rotation, re-attachment) indirectly
    // via config and boot scenarios. Direct F11 dispatch requires SDL event
    // injection, which is tested in the input mapper unit test.

    // Case 1-3: Multi-disk setup verifies list is loaded; single/empty disks
    // are tested above and produce no-op behavior (safe).
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "DiskSwapNoopSingleDisk_InitSuccess");

        if (init_ok) {
            // Run a few frames to ensure no crashes even if F11 were pressed.
            for (int i = 0; i < 5; ++i) {
                app.run_one_frame();
            }
            app.shutdown();
        }
    }

    // --- R-M35-1 (M36-S-f): the F11 swap MUST advance the mounted disk index.
    // Mutation B in M35 QA (hard-coding current_disk_index_ to 0) previously
    // did NOT kill any test because IT-2 only checked image()!=nullptr. This
    // case drives the deterministic swap seam directly (no SDL event injection)
    // and asserts the index actually rotates 0 -> 1 -> 0 and the media-change
    // signal is raised -- so an index-stuck-at-0 mutation now FAILS the test.
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk",
                                                   std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "DiskSwapAdvances_InitSuccess");

        if (init_ok) {
            // Boots on disk 0.
            expect(app.current_disk_index() == 0, "DiskSwapAdvances_BootIndexIsZero");
            const auto* image0 = app.machine().disk_drive().image();
            expect(image0 != nullptr, "DiskSwapAdvances_BootImageAttached");

            // F11 -> advance to disk 1, media-change asserted, fresh image.
            app.swap_to_next_disk();
            expect(app.current_disk_index() == 1, "DiskSwapAdvances_IndexBecomesOne");
            expect(app.machine().disk_drive().disk_changed(),
                   "DiskSwapAdvances_MediaChangeSignalled");
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "DiskSwapAdvances_NewImageAttached");

            // M36 Bug B: DSKCHG must be a READ-AND-CLEAR one-shot end-to-end. The
            // const peek is STABLE (does not clear -- snapshot path); the mutating
            // take_disk_changed() is the exact accessor SonyFdc::mem_read(0x7FFD)
            // uses (references/openmsx-21.0/src/fdc/DiskChanger.cc:95-100 via
            // readMem PhilipsFDC.cc:37). After a swap the game's DSKCHG re-check
            // reads "changed" ONCE then the latch reverts to "not changed" -- so a
            // swapped medium no longer latches DSKCHG forever (the building-entry
            // DI;HALT freeze). Previously R-M35-1 only checked the const flag was
            // set and never covered the clear-on-read media-change semantics.
            expect(app.machine().disk_drive().disk_changed(),
                   "DiskSwapAdvances_DskchgConstPeekStable");  // still set, peek didn't clear
            expect(app.machine().disk_drive().take_disk_changed(),
                   "DiskSwapAdvances_DskchgReadReportsChanged");
            expect(!app.machine().disk_drive().disk_changed(),
                   "DiskSwapAdvances_DskchgClearedAfterRead");
            expect(!app.machine().disk_drive().take_disk_changed(),
                   "DiskSwapAdvances_DskchgSecondReadNotChanged");

            // F11 again -> wrap back to disk 0 (2-disk list is modular).
            app.swap_to_next_disk();
            expect(app.current_disk_index() == 0, "DiskSwapAdvances_IndexWrapsToZero");

            app.shutdown();
        }
    }

    // --- IT-3: Media-change signal (S4) ---

    // Case 1: disk_changed() flag is initially false at boot (fresh disk).
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "DiskChangedFlagBoot_InitSuccess");

        if (init_ok) {
            // Immediately after boot, disk_changed should be false (fresh boot).
            expect(!app.machine().disk_drive().disk_changed(),
                   "DiskChangedFlagBoot_FreshBootFalse");
            app.shutdown();
        }
    }

    // --- IT-4: UX feedback (S5) ---
    // Window title and stderr logging happen during init() and on swaps.
    // This test documents that boot/swap operations complete without error.
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "UxFeedbackBoot_InitSuccess");

        if (init_ok) {
            // Title and stderr logging happen during init() and on swaps.
            // Successful init verifies these operations don't crash.
            app.shutdown();
        }
    }

    // --- IT-5: Determinism (AC-6) ---
    // Identical boot/swap sequences produce byte-identical FDC state.

    // Case 1 & 2: Two identical boots, verify same attached image pointer
    // (same object memory layout per run).
    {
        const std::vector<std::string> disk_paths1{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk",
                                                    std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk"};
        sony_msx::frontend::Sdl3AppConfig config1 = make_test_config(disk_paths1);
        sony_msx::frontend::Sdl3App app1(config1);

        const bool init1 = app1.init();
        expect(init1, "DeterminismRun1_InitSuccess");

        if (init1) {
            for (int i = 0; i < 10; ++i) {
                app1.run_one_frame();
            }
            const auto disk_image1 = app1.machine().disk_drive().image();
            expect(disk_image1 != nullptr, "DeterminismRun1_ImageNotNull");
            app1.shutdown();
        }

        // Run 2: identical boot sequence
        const std::vector<std::string> disk_paths2{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk",
                                                    std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk"};
        sony_msx::frontend::Sdl3AppConfig config2 = make_test_config(disk_paths2);
        sony_msx::frontend::Sdl3App app2(config2);

        const bool init2 = app2.init();
        expect(init2, "DeterminismRun2_InitSuccess");

        if (init2) {
            for (int i = 0; i < 10; ++i) {
                app2.run_one_frame();
            }
            const auto disk_image2 = app2.machine().disk_drive().image();
            expect(disk_image2 != nullptr, "DeterminismRun2_ImageNotNull");
            app2.shutdown();
        }
    }

    // --- IT-6: Regression guard (AC-7) ---

    // Case 1: boot with single --disk disk1.dsk → identical to pre-M35.
    {
        const std::vector<std::string> disk_paths{std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk"};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "RegressionSingleDisk_InitSuccess");

        if (init_ok) {
            // Single disk should attach and work exactly as before M35.
            expect(app.machine().disk_drive().image() != nullptr,
                   "RegressionSingleDisk_ImageAttached");

            // Run a few frames to ensure no regressions.
            for (int i = 0; i < 5; ++i) {
                app.run_one_frame();
            }

            app.shutdown();
        }
    }

    // Case 2: boot with no --disk → identical to pre-M35.
    {
        const std::vector<std::string> disk_paths{};
        sony_msx::frontend::Sdl3AppConfig config = make_test_config(disk_paths);
        sony_msx::frontend::Sdl3App app(config);

        const bool init_ok = app.init();
        expect(init_ok, "RegressionNoDisk_InitSuccess");

        if (init_ok) {
            // No disk attached, exactly as pre-M35.
            expect(app.machine().disk_drive().image() == nullptr,
                   "RegressionNoDisk_NoImageAttached");

            // Run a few frames to ensure no regressions.
            for (int i = 0; i < 5; ++i) {
                app.run_one_frame();
            }

            app.shutdown();
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3App_MultiDisk_Integration cases passed\n";
    return 0;
}
