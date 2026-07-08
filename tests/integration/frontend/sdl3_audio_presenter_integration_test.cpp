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

    SDL_Quit();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AudioPresenter_Integration cases passed\n";
    return 0;
}
