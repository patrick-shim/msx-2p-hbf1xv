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
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AppScaling_Integration (M37 Slices D + E + F, DEC-0056).
//
// M37 Slice F adds two end-to-end concerns on top of the Slice D/E coverage:
//   * the OUT-OF-BOX default window is now scale 3 = 960x720 (was 640x480);
//   * the F10 live stream-capture hotkey is GATED by config.capture_enabled
//     (--capture <on|off>, default OFF): with capture off a pushed F10 key
//     event, driven through the REAL Sdl3App event loop (run_one_frame ->
//     poll_and_dispatch_events), is INERT; with capture on the identical event
//     arms the stream. Both are asserted below with SDL_PushEvent injection --
//     no fabricated "it toggled" claim.
//
// End-to-end proof that Sdl3App::init() threads the M37 launch-time config
// into the LIVE machine/window/renderer/presenter, under the "dummy" video/
// audio driver (the same headless mechanism the M26 Sdl3App tests use):
//   * Slice D (--speed): config.speed_level flows to the Sony Speed Controller
//     (Mb670836PauseController) AFTER cold_boot() -- so the initial level is
//     honored and NOT clobbered by cold_boot()'s reset. Absent -> level 0.
//   * Slice E (--scale/--filter/window): the window is RESIZABLE, the renderer
//     uses a 320x240 LETTERBOX logical presentation (aspect-correct, never
//     distorted), and config.texture_filter reaches the video presenter.
//
// These are all deterministic, driver-independent internal states read back
// through real SDL3 introspection calls (SDL_GetWindowFlags,
// SDL_GetRenderLogicalPresentation) + the machine/presenter accessors -- NOT a
// tautology. Runtime fullscreen (Alt+Enter) is validated by the human at play
// time and is intentionally NOT asserted here (no fake "it went fullscreen").

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

    // --- Case 1 (Slice D): --speed applied -> the controller's initial level
    // is the requested value (survives cold_boot()'s reset). ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        config.speed_level = 5;

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "Speed_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            expect(app.machine().pause_controller().speed_level() == 5,
                   "Speed_InitialLevelApplied_AfterColdBoot");
            app.shutdown();
        }
    }

    // --- Case 2 (Slice D control): no --speed -> level 0 (cold_boot default),
    // proving the config path does not perturb the default. ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "SpeedDefault_InitSucceeds");
        if (init_ok) {
            expect(app.machine().pause_controller().speed_level() == 0,
                   "SpeedDefault_LevelZero_ByteIdenticalDefault");
            app.shutdown();
        }
    }

    // --- Case 3 (Slice E): resizable window + 320x240 LETTERBOX logical
    // presentation + the configured texture filter reaches the presenter. ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        config.window_width = 960;   // as if --scale 3
        config.window_height = 720;
        config.texture_filter = SDL_SCALEMODE_NEAREST;  // as if --filter nearest

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "Scaling_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            expect(app.window() != nullptr && app.renderer() != nullptr, "Scaling_WindowAndRendererCreated");

            // Slice E point 1: the window is resizable.
            const SDL_WindowFlags wflags = SDL_GetWindowFlags(app.window());
            expect((wflags & SDL_WINDOW_RESIZABLE) != 0, "Scaling_WindowIsResizable");

            // Slice E point 2: aspect-correct 320x240 letterbox logical presentation.
            int lw = 0;
            int lh = 0;
            SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
            const bool got = SDL_GetRenderLogicalPresentation(app.renderer(), &lw, &lh, &mode);
            expect(got && lw == 320 && lh == 240 && mode == SDL_LOGICAL_PRESENTATION_LETTERBOX,
                   "Scaling_LogicalPresentation_320x240_Letterbox");

            // Slice E point 3: the configured filter reached the presenter.
            expect(app.video_presenter() != nullptr &&
                       app.video_presenter()->scale_mode() == SDL_SCALEMODE_NEAREST,
                   "Scaling_TextureFilter_ThreadedIntoPresenter");

            app.shutdown();
        }
    }

    // --- Case 4 (M37 Slice F): the OUT-OF-BOX default window is now scale 3 =
    // 960x720 (was 640x480 / scale 2 through Slice E) -- changed by design in
    // Slice F so the human gets a comfortable window without passing --scale.
    // The Sdl3AppConfig default fields are the oracle (sdl3_main.cpp feeds these
    // straight to SDL_CreateWindowAndRenderer when --scale is absent), and the
    // live window created under the dummy driver reports that same size. The
    // logical presentation stays 320x240 letterbox (asserted in Case 3). ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        // No window_width/window_height override -> the struct defaults apply.
        const int default_w = config.window_width;
        const int default_h = config.window_height;
        expect(default_w == 960 && default_h == 720,
               "DefaultWindow_Scale3_960x720_M37F_ChangedByDesign");

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "DefaultWindow_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            int ww = 0;
            int wh = 0;
            const bool got = SDL_GetWindowSize(app.window(), &ww, &wh);
            expect(got && ww == 960 && wh == 720,
                   "DefaultWindow_LiveWindowIs960x720");
            app.shutdown();
        }
    }

    // A pushed fresh F10 key-down event, matching the poll_and_dispatch_events()
    // gate exactly (SDL_EVENT_KEY_DOWN + SDL_SCANCODE_F10 + repeat=false). Field
    // layout per references/sdl3/include/SDL3/SDL_events.h:373-386; SDL_PushEvent
    // returns bool per :1449.
    const auto push_f10_keydown = []() {
        SDL_Event ev{};
        ev.type = SDL_EVENT_KEY_DOWN;
        ev.key.type = SDL_EVENT_KEY_DOWN;
        ev.key.scancode = SDL_SCANCODE_F10;
        ev.key.down = true;
        ev.key.repeat = false;
        return SDL_PushEvent(&ev);
    };

    // --- Case 5 (M37 Slice F): --capture off (DEFAULT) makes F10 INERT. The
    // pushed F10 event, processed through the REAL event loop, does NOT arm
    // stream capture -- the user's exact requirement (a mis-struck F10 during
    // play does nothing). Zero filesystem writes (capture never arms). ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        config.capture_enabled = false;  // default OFF (gate closed)

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "CaptureOff_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            expect(!app.stream_capture_active(), "CaptureOff_StreamInactiveBefore");
            expect(push_f10_keydown(), "CaptureOff_F10Pushed");
            app.run_one_frame();  // polls + dispatches the F10 event
            expect(!app.stream_capture_active(),
                   "CaptureOff_F10Inert_StreamStillInactive");
            app.shutdown();
        }
    }

    // --- Case 6 (M37 Slice F): --capture on ARMS the F10 hotkey. The IDENTICAL
    // pushed F10 event now toggles stream capture ON through the real event
    // loop, proving the gate discriminates purely on config.capture_enabled.
    // Capture I/O is redirected to a temp root and removed afterwards, so the one
    // per-frame bundle written while armed leaves NO repo debris. ---
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path tmp_root = fs::temp_directory_path() / "sony_msx_m37f_f10gate";
        fs::remove_all(tmp_root, ec);  // pre-clean any prior run

        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        config.capture_enabled = true;            // --capture on (gate open)
        config.snapshot_dir = tmp_root.string();  // redirect all capture I/O to temp

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "CaptureOn_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            expect(!app.stream_capture_active(), "CaptureOn_StreamInactiveBefore");
            expect(push_f10_keydown(), "CaptureOn_F10Pushed");
            app.run_one_frame();  // polls + dispatches the F10 event -> arms capture
            expect(app.stream_capture_active(),
                   "CaptureOn_F10Armed_StreamActive");
            app.shutdown();
        }
        fs::remove_all(tmp_root, ec);  // clean up temp capture output (no repo debris)
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppScaling_Integration cases passed\n";
    return 0;
}
