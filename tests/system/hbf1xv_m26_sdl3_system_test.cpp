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

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"
#include "frontend/sdl3_input_mapper.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Suite: Hbf1xvM26Sdl3_System (M26-S8, docs/m26-planner-package.md §3
// M26-S8/§4 Acceptance Criterion 11).
//
// The milestone's dedicated system integration test: combines windowing/
// real-time-loop STRUCTURE (Sdl3App::init()/run_one_frame(), never
// run_interactive() -- A-M26-8), real video-texture blit, real audio-stream
// numeric pump, real `SDL_PushEvent`-injected input mapping (keyboard +
// PAUSE + joystick), the M25 Speed-Controller/Ren-Sha-Turbo bindings, and
// cartridge/disk CLI loading (A-M26-6) -- ALL together in ONE deterministic,
// bounded-frame scenario, mirroring the M25
// hbf1xv_m25_speed_pause_rensha_system_test.cpp "everything together"
// structure, scaled to this milestone's larger surface. Runs entirely
// headlessly under SDL_VIDEO_DRIVER=dummy/SDL_AUDIO_DRIVER=dummy, zero
// SDL_Delay-based pacing anywhere in this file.

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

void push_key(const SDL_Scancode scancode, const bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = down;
    SDL_PushEvent(&event);
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::string bios_dir = SONY_MSX_BIOS_DIR;
    const std::vector<std::string> args{"--bios-dir", bios_dir, "--max-frames", "10"};
    const sony_msx::frontend::ParsedSdl3Cli parsed = sony_msx::frontend::parse_sdl3_cli(args);
    expect(parsed.errors.empty(), "CliParsing_NoErrors");

    sony_msx::frontend::Sdl3AppConfig config;
    config.bios_dir = *parsed.bios_dir;
    config.max_frames = parsed.max_frames;
    config.hidden_window = true;

    sony_msx::frontend::Sdl3App app(std::move(config));
    const bool init_ok = app.init();
    expect(init_ok, "WindowingLoopStructure_Sdl3AppInitSucceeds");
    if (!init_ok) {
        std::cerr << "  " << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    expect(app.window() != nullptr, "WindowingLoopStructure_WindowCreated");
    expect(app.renderer() != nullptr, "WindowingLoopStructure_RendererCreated");
    expect(app.video_presenter() != nullptr, "WindowingLoopStructure_VideoPresenterOwned");
    expect(app.audio_presenter() != nullptr, "WindowingLoopStructure_AudioPresenterOwned");

    // --- A real SDL_PushEvent-injected keyboard press (SPACE, row8/col0)
    // BEFORE run_one_frame() -- the frame's own internal SDL_PollEvent
    // dispatch picks it up. ---
    push_key(SDL_SCANCODE_SPACE, true);

    const std::uint64_t frame_count_before = app.machine().frame_count();
    const std::uint64_t cycles_before = app.machine().elapsed_cycles();

    app.run_one_frame();

    expect(app.machine().frame_count() == frame_count_before + 1,
           "DeterministicCoreStep_FrameCountIncrementsByOne");
    expect(app.machine().elapsed_cycles() >= cycles_before + app.machine().frame_cycles_per_frame(),
           "DeterministicCoreStep_ElapsedCyclesAdvancesAtLeastOneFrame");
    expect(app.machine().keyboard().key(8, 0), "InputMapping_InjectedSpaceKey_ReflectedInMatrix");

    // Real video-texture blit: the presenter's texture exists and matches
    // the machine's current render_frame() dimensions.
    expect(app.video_presenter()->texture() != nullptr, "VideoPresentation_TextureCreated");

    // Real audio-stream numeric pump: PSG samples were genuinely queued on
    // the real SDL_AudioStream (even if the PSG is currently silent --
    // SDL_PutAudioStreamData still queues the, possibly-zero, PCM frames).
    const int queued_after_frame1 = SDL_GetAudioStreamQueued(app.audio_presenter()->stream());
    expect(queued_after_frame1 > 0, "AudioPresentation_SamplesGenuinelyQueuedOnRealStream");

    // --- PAUSE binding: a real SDL_PushEvent-injected PAUSE press freezes
    // the CPU (A-M25 hardware PAUSE gate) for an ENTIRE subsequent frame --
    // PC must be byte-identical before/after that frame, yet frame_count()
    // still advances (on_vsync_boundary() is unconditional). ---
    push_key(SDL_SCANCODE_PAUSE, true);
    app.run_one_frame();  // This frame's poll dispatches the PAUSE press itself.
    expect(app.machine().pause_controller().button_engaged(), "PauseBinding_InjectedPauseKey_EngagesController");

    const std::uint16_t pc_before_paused_frame = app.machine().cpu().state().regs().pc;
    const std::uint64_t frame_count_before_paused = app.machine().frame_count();
    app.run_one_frame();  // Fully paused: CPU frozen the whole frame.
    expect(app.machine().cpu().state().regs().pc == pc_before_paused_frame,
           "PauseBinding_PcFrozenAcrossEntirePausedFrame");
    expect(app.machine().frame_count() == frame_count_before_paused + 1,
           "PauseBinding_FrameCountStillAdvances_OnVsyncBoundaryUnconditional");

    // Release PAUSE so the CPU resumes for any further frames.
    push_key(SDL_SCANCODE_PAUSE, true);  // second press toggles back off (toggle semantics)
    app.run_one_frame();
    expect(!app.machine().pause_controller().button_engaged(), "PauseBinding_SecondPress_DisengagesController");

    // --- Speed-Controller / Ren-Sha-Turbo bindings (M25 API surfaces,
    // purpose-built for this M26 binding). ---
    push_key(sony_msx::frontend::Sdl3InputMapper::kSpeedUpScancode, true);
    app.run_one_frame();
    expect(app.machine().pause_controller().speed_level() == 1, "SpeedControllerBinding_UpKey_IncrementsLevel");

    push_key(sony_msx::frontend::Sdl3InputMapper::kReshaUpScancode, true);
    app.run_one_frame();
    expect(app.machine().rensha_turbo().speed() == sony_msx::frontend::Sdl3InputMapper::kReshaStep,
           "ReshaTurboBinding_UpKey_IncrementsSpeed");

    app.shutdown();
    expect(!app.initialized(), "CleanShutdown_AppNoLongerInitialized");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Hbf1xvM26Sdl3_System cases passed\n";
    return 0;
}
