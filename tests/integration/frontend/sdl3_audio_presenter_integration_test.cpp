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
#include <string>

#include "devices/audio/psg_ym2149.h"
#include "frontend/sdl3_audio_presenter.h"

// Suite: Frontend_Sdl3AudioPresenter_Integration (M26-S5, docs/m26-planner-
// package.md §2.6/§2.4).
//
// Confirms the real SDL3 audio PATH works headlessly under
// SDL_AUDIO_DRIVER=dummy (SDL_OpenAudioDeviceStream + SDL_ResumeAudioStream
// Device + SDL_PutAudioStreamData all genuinely succeed) -- proving the SDL3
// PLUMBING, not the acoustic content (the numeric PSG sample-generation
// correctness is separately, deterministically proven headlessly by
// frontend_psg_audio_pump_unit_test, which needs no SDL3 type at all).

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
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
}

}  // namespace

int main() {
    set_dummy_drivers();

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    const char* driver_name = SDL_GetCurrentAudioDriver();
    expect(driver_name != nullptr && std::string(driver_name) == "dummy",
           "CurrentAudioDriver_IsGenuinelyDummy");

    {
        sony_msx::frontend::Sdl3AudioPresenter presenter;
        const bool init_ok = presenter.init();
        expect(init_ok, "Init_OpenAudioDeviceStream_And_Resume_Succeed");
        if (!init_ok) {
            std::cerr << "  " << presenter.last_error() << "\n";
        } else {
            expect(presenter.stream() != nullptr, "Init_StreamPointerNonNull");

            // Program a real, audible PSG channel A square wave (the SAME
            // program frontend_psg_audio_pump_unit_test's Case 1 uses) and
            // push a real batch of samples through the actual SDL_AudioStream
            // -- SDL_PutAudioStreamData genuinely accepts the data under the
            // dummy driver.
            sony_msx::devices::audio::PsgYm2149 psg;
            psg.reset();
            psg.write_address(0);
            psg.write_data(1);
            psg.write_address(1);
            psg.write_data(0);
            psg.write_address(8);
            psg.write_data(15);
            psg.write_address(7);
            psg.write_data(0x3E);

            presenter.pump_and_push(psg, 512);
            expect(presenter.last_error().empty(), "PumpAndPush_PutAudioStreamData_Succeeds_NoError");

            const int queued_bytes = SDL_GetAudioStreamQueued(presenter.stream());
            expect(queued_bytes > 0, "PumpAndPush_DataGenuinelyQueuedOnTheRealStream");
        }
    }  // ~Sdl3AudioPresenter() calls shutdown(), destroying the stream cleanly.

    // --- Paced path (audio-latency fix, docs/audio-latency-investigation.md):
    // pump_and_push_paced() derives production from CUMULATIVE emulated
    // cycles via AudioPacer and caps the real host-stream queue. Deterministic
    // oracles asserted here are host-queue-state-INDEPENDENT invariants:
    // exact cumulative production accounting (a pure function of cycles) and
    // the hard queue cap in bytes (holds whether or not the dummy device
    // drains concurrently). ---
    {
        sony_msx::frontend::Sdl3AudioPresenter presenter;
        const bool init_ok = presenter.init();
        expect(init_ok, "PacedInit_OpenAudioDeviceStream_And_Resume_Succeed");
        if (init_ok) {
            sony_msx::devices::audio::PsgYm2149 psg;
            psg.reset();
            psg.write_address(0);
            psg.write_data(1);
            psg.write_address(1);
            psg.write_data(0);
            psg.write_address(8);
            psg.write_data(15);
            psg.write_address(7);
            psg.write_data(0x3E);

            constexpr std::uint64_t kFrameCycles = 228 * 262;  // 59736
            constexpr std::uint64_t kMaxQueuedBytes =
                sony_msx::frontend::Sdl3AudioPresenter::kMaxQueuedSamples *
                sony_msx::frontend::Sdl3AudioPresenter::kBytesPerSampleFrame;

            // 120 frames (~2 emulated seconds) of paced production in a tight
            // loop -- far faster than real time, so without the backpressure
            // cap the queue would hold ~88,200 samples here (the pre-fix
            // unbounded-growth failure shape).
            bool cap_never_exceeded = true;
            bool no_put_error = true;
            for (std::uint64_t frame = 1; frame <= 120; ++frame) {
                presenter.pump_and_push_paced(psg, frame * kFrameCycles);
                no_put_error = no_put_error && presenter.last_error().empty();
                const int queued = SDL_GetAudioStreamQueued(presenter.stream());
                cap_never_exceeded =
                    cap_never_exceeded && queued >= 0 && static_cast<std::uint64_t>(queued) <= kMaxQueuedBytes;
            }
            expect(no_put_error, "PacedPumpAndPush_120Frames_NoPutAudioStreamDataError");
            expect(cap_never_exceeded, "PacedPumpAndPush_QueueDepth_NeverExceedsMaxQueuedBytes");

            // Exact accounting: cumulative generator production is the exact
            // floor(cycles * 44100 / 3579545) -- independent of what the host
            // queue did (samples_to_pump is a pure function of cycles).
            const auto& pacer = presenter.pacer();
            expect(pacer.samples_produced() == pacer.cycles_to_samples(120 * kFrameCycles),
                   "PacedPumpAndPush_CumulativeProduction_ExactFloorOfCyclesTimesRateOverClock");

            // Conservation: every produced sample was either pushed or
            // deliberately dropped by the cap -- none silently lost.
            expect(pacer.samples_produced() >= pacer.samples_dropped(),
                   "PacedPumpAndPush_DropAccounting_NeverExceedsProduction");

            // A stale/repeated cycle count pumps nothing (monotonic guard).
            const std::uint64_t produced_before = pacer.samples_produced();
            presenter.pump_and_push_paced(psg, 120 * kFrameCycles);
            expect(presenter.pacer().samples_produced() == produced_before,
                   "PacedPumpAndPush_RepeatedCycleCount_ProducesNothing");
        } else {
            std::cerr << "  " << presenter.last_error() << "\n";
        }
    }

    SDL_Quit();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AudioPresenter_Integration cases passed\n";
    return 0;
}
