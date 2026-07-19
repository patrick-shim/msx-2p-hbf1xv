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
#include <cstdlib>
#include <iostream>
#include <utility>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AudioEmission_Integration (M57, DEC-0085, docs/m57-planner-
// package.md §3.6). The PERMANENT audio-emission oracle -- closes the "ctest green
// but the shipped build is SILENT" blind spot (DEF-1). It drives a REAL Sdl3App
// through its actual run_one_frame() audio path under SDL_AUDIO_DRIVER=dummy,
// hidden-window, and asserts that REAL (non-silence-prime) PCM bytes genuinely
// reach SDL_PutAudioStreamData -- the exact layer ctest could never previously
// "hear".
//
// SCOPE (S1 verdict, DEC-0085 R6): this guards the UNIVERSAL produce/push path,
// which is byte-identical between the audible v1.2.2 and v1.3.0 (zero core/
// devices/machine + zero audio-machinery diff) and is DEMONSTRABLY HEALTHY on HEAD
// (this test passes on HEAD). It is NON-tautological: it counts only REAL-sample
// bytes (Sdl3AudioPresenter::real_sample_bytes_pushed(), never the silence-prime
// puts), so breaking the produce/push path drives the counter to 0 and FAILS this
// test (adversarial-revert proven in the M57 implementation report). It does NOT
// use a real playback device, so it is deterministic and device-drain-independent.

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
    config.hidden_window = true;  // menu_ == null: guards the universal produce/push path.

    sony_msx::frontend::Sdl3App app(std::move(config));
    const bool init_ok = app.init();
    expect(init_ok, "Init_SucceedsUnderDummyDrivers");
    if (!init_ok) {
        std::cerr << "  last_error=" << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    expect(app.audio_presenter() != nullptr && app.audio_presenter()->stream() != nullptr,
           "Init_AudioStreamCreated");
    // DEF-2 companion guard: the hidden-window presenter must keep the legacy
    // full-window path (inset 0), so no menu inset perturbs the deterministic path.
    expect(app.video_presenter() != nullptr && app.video_presenter()->top_inset() == 0,
           "HiddenWindow_TopInset_IsZero");

    // Before any frame runs, no real samples have been pushed.
    expect(app.audio_presenter()->real_sample_bytes_pushed() == 0,
           "PreRun_RealSampleBytes_AreZero");

    // The exact deterministic non-silent PSG channel-A square wave the presenter
    // test uses (sdl3_audio_presenter_integration_test.cpp), programmed AFTER
    // init()/reset (so cold_boot() doesn't clear it).
    auto program_tone = [](sony_msx::devices::audio::PsgYm2149& psg) {
        psg.write_address(0);
        psg.write_data(1);
        psg.write_address(1);
        psg.write_data(0);
        psg.write_address(8);
        psg.write_data(15);
        psg.write_address(7);
        psg.write_data(0x3E);
    };
    program_tone(app.machine().psg());

    // Sustained-flow floor, GUARANTEED regardless of the dummy device's drain
    // timing: the host queue must fill with REAL samples up to its cap before the
    // pacer trims, so at least kMaxQueuedSamples frames' worth of bytes are pushed.
    const std::uint64_t floor_bytes =
        sony_msx::frontend::Sdl3AudioPresenter::kMaxQueuedSamples *
        sony_msx::frontend::Sdl3AudioPresenter::kBytesPerSampleFrame;

    // --- Case A: BOOT-path audio flows (the universal produce/push guard). ---
    // A long enough pre-reset run that the pacer accumulates a large cumulative
    // sample count (so the post-reset assertion below is adversarial-proof).
    constexpr int kPreFrames = 180;
    for (int i = 0; i < kPreFrames; ++i) {
        app.run_one_frame();
    }
    const std::uint64_t pre = app.audio_presenter()->real_sample_bytes_pushed();
    // Core non-tautology: REAL (non-silence-prime) audio bytes genuinely flowed.
    expect(pre > 0, "BootAudio_RealSampleBytes_NonZero");
    expect(pre >= floor_bytes, "BootAudio_SustainedRealSamples_AtLeastQueueFill");

    // --- Case B: audio STILL flows AFTER a runtime menu-driven reset. ---
    // M57 (DEC-0085 H7) regression guard for the DEF-1 root cause: reset_machine()
    // restarts the machine's elapsed_cycles at 0 while the Sdl3AudioPresenter (and
    // its AudioPacer) SURVIVE. Without reset_pacing() rewinding the pacer's
    // cumulative samples_produced_, plan() trims every post-reset batch to 0 real
    // samples -> permanent silence. This assertion FAILS on the unfixed HEAD
    // (post-reset delta == 0) and PASSES with the fix (audio resumes immediately).
    app.request_reset();      // the exact menu action (Machine>Reset / implied by insert)
    app.run_one_frame();      // drain the reset frame
    program_tone(app.machine().psg());  // cold_boot cleared the PSG; re-arm the tone
    const std::uint64_t mid = app.audio_presenter()->real_sample_bytes_pushed();
    constexpr int kPostFrames = 60;
    for (int i = 0; i < kPostFrames; ++i) {
        app.run_one_frame();
    }
    const std::uint64_t post_delta = app.audio_presenter()->real_sample_bytes_pushed() - mid;
    expect(post_delta > 0, "PostResetAudio_RealSampleBytes_NonZero");
    expect(post_delta >= floor_bytes, "PostResetAudio_SustainedRealSamples_AtLeastQueueFill");

    const std::uint64_t pushed = app.audio_presenter()->real_sample_bytes_pushed();
    app.shutdown();
    // shutdown() resets the probe so a re-init counts cleanly.
    expect(app.audio_presenter() == nullptr, "Shutdown_AudioPresenterReleased");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed  (real_sample_bytes_pushed=" << pushed
                  << ", floor=" << floor_bytes << ")\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AudioEmission_Integration cases passed  (real_sample_bytes_pushed="
              << pushed << ")\n";
    return 0;
}
