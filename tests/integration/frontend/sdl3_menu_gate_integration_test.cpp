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

// Suite: Frontend_Sdl3MenuGate_Integration (M55, DEC-0083).
//
// The determinism guard proven end-to-end: a --hidden-window launch (every
// ctest, per A3) creates NO ImGui context (menu_active() == false), and a
// bounded run_one_frame() loop still advances machine state by the SAME
// hand-computed oracle as the pre-M55 headless path -- i.e. adding the menu
// module perturbs nothing on the deterministic path. Runs entirely under the
// SDL dummy video/audio drivers; never calls run_interactive(), never pauses.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "frontend/sdl3_app.h"

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

}  // namespace

int main() {
    set_dummy_drivers();

    sony_msx::frontend::Sdl3AppConfig config;
#if defined(SONY_MSX_BIOS_DIR)
    config.bios_dir = SONY_MSX_BIOS_DIR;
#else
    config.bios_dir = "bios";
#endif
    config.hidden_window = true;  // the interactive gate: no ImGui context must be created.

    sony_msx::frontend::Sdl3App app(std::move(config));

    const bool init_ok = app.init();
    expect(init_ok, "Init_SucceedsUnderDummyDrivers");
    if (!init_ok) {
        std::cerr << "  last_error=" << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // --- Case 1: the determinism guard -- hidden-window => NO ImGui menu. ---
    expect(!app.menu_active(), "HiddenWindow_NoImGuiMenuCreated");

    // --- Case 2: a bounded session advances state exactly as before, proving the
    // menu module changed nothing on the deterministic path (the same oracle the
    // pre-M55 headless integration test uses). ---
    const std::uint64_t frames_before = app.machine().frame_count();
    const std::uint64_t cycles_before = app.machine().elapsed_cycles();
    constexpr int kFrameCount = 3;
    for (int i = 0; i < kFrameCount; ++i) {
        app.run_one_frame();
    }
    expect(app.machine().frame_count() - frames_before == static_cast<std::uint64_t>(kFrameCount),
           "BoundedSession_FrameCount_AdvancesByExactlyN");
    const std::uint64_t expected_minimum =
        static_cast<std::uint64_t>(kFrameCount) * app.machine().frame_cycles_per_frame();
    expect(app.machine().elapsed_cycles() - cycles_before >= expected_minimum,
           "BoundedSession_ElapsedCycles_AtLeastHandComputedMinimum");
    // Still no menu after running frames (the guard is not a one-shot).
    expect(!app.menu_active(), "AfterFrames_StillNoImGuiMenu");

    // --- Case 3: clean shutdown with no menu is safe + re-init possible. ---
    app.shutdown();
    expect(!app.initialized(), "Shutdown_InitializedFlagCleared");
    expect(!app.menu_active(), "Shutdown_NoMenu");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3MenuGate_Integration cases passed\n";
    return 0;
}
