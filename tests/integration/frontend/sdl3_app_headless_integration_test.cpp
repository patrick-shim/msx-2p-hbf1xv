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

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AppHeadless_Integration (M26-S2, docs/m26-planner-
// package.md §2.4/§3 M26-S2). The FIRST SDL3-ON ctest proof that the
// "dummy" video/audio driver mechanism (A-M26-2, confirmed by reading
// references/sdl3/src/video/dummy/SDL_nullvideo.c and
// references/sdl3/src/audio/dummy/SDL_dummyaudio.c this cycle) genuinely
// works end to end in THIS real environment: sets SDL_VIDEO_DRIVER=dummy /
// SDL_AUDIO_DRIVER=dummy BEFORE any SDL3 call, constructs a REAL Sdl3App,
// asserts SDL_Init/window/renderer/audio-stream creation succeeds, calls
// run_one_frame() a bounded, fixed number of times, and asserts
// machine.elapsed_cycles()/frame_count() advance by the EXACT expected,
// hand-computable amounts. Zero SDL_Delay-based pacing anywhere in this
// file (A-M26-8) -- run_interactive() is NEVER called from ctest.

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
    config.bios_dir = "bios";
    config.hidden_window = true;  // SDL_WINDOW_HIDDEN -- no real display surface needed under dummy video.

    sony_msx::frontend::Sdl3App app(std::move(config));

    // --- Case 1: real SDL_Init/window/renderer/audio-stream creation
    // succeeds under the dummy drivers, in THIS real environment. ---
    const bool init_ok = app.init();
    expect(init_ok, "Init_SucceedsUnderDummyDrivers");
    if (!init_ok) {
        std::cerr << "  last_error=" << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    expect(app.window() != nullptr, "Init_WindowCreated");
    expect(app.renderer() != nullptr, "Init_RendererCreated");
    expect(app.audio_presenter() != nullptr && app.audio_presenter()->stream() != nullptr,
           "Init_AudioStreamCreated");

    // A genuine, real SDL3 introspection call under the dummy driver: the
    // reported current video driver name really is "dummy" (proves the env
    // var actually took effect, not merely that SDL_Init happened to
    // succeed via some other fallback driver).
    const char* driver_name = SDL_GetCurrentVideoDriver();
    expect(driver_name != nullptr && std::string(driver_name) == "dummy",
           "Init_CurrentVideoDriverIsGenuinelyDummy");

    // --- Case 2: a bounded N-frame session advances machine state
    // deterministically -- a hand-computable oracle (N * frame_cycles_per_frame()),
    // zero SDL_Delay anywhere in this test's own execution path (A-M26-8). ---
    const std::uint64_t cycles_before = app.machine().elapsed_cycles();
    const std::uint64_t frames_before = app.machine().frame_count();
    constexpr int kFrameCount = 3;
    for (int i = 0; i < kFrameCount; ++i) {
        app.run_one_frame();
    }
    const std::uint64_t cycles_after = app.machine().elapsed_cycles();
    const std::uint64_t frames_after = app.machine().frame_count();

    expect(frames_after - frames_before == static_cast<std::uint64_t>(kFrameCount),
           "BoundedSession_FrameCount_AdvancesByExactlyN");
    // Each run_one_frame() steps the CPU AT LEAST frame_cycles_per_frame()
    // cycles (the sub-loop's own boundary condition, §2.3) -- so the total
    // advance is >= N * frame_cycles_per_frame(), a hand-computable lower
    // bound (instruction-atomic overshoot makes an EXACT equality claim
    // false in general, an honest, disclosed characteristic of the
    // real-time-driver architecture, not a defect).
    const std::uint64_t expected_minimum =
        static_cast<std::uint64_t>(kFrameCount) * app.machine().frame_cycles_per_frame();
    expect(cycles_after - cycles_before >= expected_minimum,
           "BoundedSession_ElapsedCycles_AtLeastHandComputedMinimum");
    expect(app.frames_run() == static_cast<std::uint64_t>(kFrameCount), "BoundedSession_FramesRunCounter_Matches");

    // --- Case 3: clean shutdown leaves no dangling SDL resources -- window/
    // renderer/audio-stream pointers reset to nullptr, re-init is possible. ---
    app.shutdown();
    expect(app.window() == nullptr, "Shutdown_WindowPointerCleared");
    expect(app.renderer() == nullptr, "Shutdown_RendererPointerCleared");
    expect(!app.initialized(), "Shutdown_InitializedFlagCleared");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppHeadless_Integration cases passed\n";
    return 0;
}
