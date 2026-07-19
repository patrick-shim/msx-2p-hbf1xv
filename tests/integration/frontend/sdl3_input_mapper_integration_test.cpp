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

#include "devices/chipset/mb670836_pause.h"
#include "frontend/sdl3_input_mapper.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"
#include "peripherals/rensha_turbo.h"

// Suite: Frontend_Sdl3InputMapper_Integration (M26-S6, docs/m26-planner-
// package.md §2.4/§2.7).
//
// Uses the REAL `SDL_PushEvent` injection technique (references/sdl3/
// include/SDL3/SDL_events.h:1449, confirmed present) to exercise the app's
// real, unmodified `SDL_PollEvent`-based dispatch code headlessly: injects a
// synthetic `SDL_Event` into SDL3's OWN event queue, dequeues it via the
// real `SDL_PollEvent`, and hands it to `Sdl3InputMapper::dispatch_event()`
// -- proving the actual input-mapping code path end-to-end, entirely under
// the dummy video driver, with no real keyboard/joystick hardware anywhere
// in the loop. EVERY entry in `Sdl3InputMapper::scancode_map()` gets a
// dedicated case (planner Acceptance Criterion 4's exhaustive-coverage
// requirement).

namespace {

using sony_msx::devices::chipset::Mb670836PauseController;
using sony_msx::frontend::Sdl3InputMapper;
using sony_msx::peripherals::JoystickPorts;
using sony_msx::peripherals::KeyboardMatrix;
using sony_msx::peripherals::RenshaTurbo;

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
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
}

SDL_Event make_key_event(const SDL_Scancode scancode, const bool down, const bool repeat = false) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = down;
    event.key.repeat = repeat;
    return event;
}

SDL_Event make_joy_button_event(const Uint8 button, const bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_JOYSTICK_BUTTON_DOWN : SDL_EVENT_JOYSTICK_BUTTON_UP;
    event.jbutton.which = 1;
    event.jbutton.button = button;
    event.jbutton.down = down;
    return event;
}

SDL_Event make_joy_axis_event(const Uint8 axis, const Sint16 value) {
    SDL_Event event{};
    event.type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
    event.jaxis.which = 1;
    event.jaxis.axis = axis;
    event.jaxis.value = value;
    return event;
}

// Exact content match (never a loose type-only match -- see the push_and_poll
// doc comment below for why that would be unsound).
bool same_event(const SDL_Event& a, const SDL_Event& b) {
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return a.key.scancode == b.key.scancode && a.key.down == b.key.down && a.key.repeat == b.key.repeat;
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        return a.jbutton.which == b.jbutton.which && a.jbutton.button == b.jbutton.button &&
               a.jbutton.down == b.jbutton.down;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        return a.jaxis.which == b.jaxis.which && a.jaxis.axis == b.jaxis.axis && a.jaxis.value == b.jaxis.value;
    default:
        return true;
    }
}

// Push, then dequeue via the REAL SDL_PollEvent -- never inspects the pushed
// event directly (proves the real queue round-trip, not a shortcut).
//
// SDL3's SDL_PollEvent implements "poll batching" via an internal sentinel
// event (references/sdl3/src/events/SDL_events.c:1709-1750,
// SDL_EVENT_POLL_SENTINEL): a single SDL_PollEvent call can genuinely return
// false merely because it reached the END OF THE CURRENT BATCH, even though
// a just-pushed real event is queued immediately behind that boundary --
// discovered empirically this cycle while grounding this exact mechanism
// (never assumed). A single "while(SDL_PollEvent()) break-on-type-match"
// drain is therefore UNSOUND: it can (a) give up too early on a genuine
// false-from-sentinel, leaving the real event still queued, and (b), on a
// LATER call, dequeue that now-stale leftover event and falsely match it by
// type alone against an unrelated, currently-being-searched-for push. This
// helper fixes both: it retries across multiple poll batches (bounded), and
// matches on EXACT event content (same_event()), never type alone -- so a
// stale, differently-scancoded/valued leftover can never masquerade as a
// match.
bool push_and_poll(const SDL_Event& pushed, SDL_Event* out) {
    SDL_Event copy = pushed;
    if (!SDL_PushEvent(&copy)) {
        return false;
    }
    // A bounded number of attempts: SDL_PollEvent's own sentinel-boundary
    // false-return does not distinguish "genuinely empty queue" from "batch
    // boundary reached, the real event is still queued right behind it" --
    // calling it again is always safe/cheap (it just pumps again), so this
    // simply retries rather than trying to disambiguate the two cases.
    for (int attempt = 0; attempt < 16; ++attempt) {
        while (SDL_PollEvent(out)) {
            if (same_event(*out, pushed)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

int main() {
    set_dummy_drivers();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    Sdl3InputMapper mapper;
    KeyboardMatrix keyboard;
    keyboard.reset();
    JoystickPorts joystick;
    joystick.reset();
    Mb670836PauseController pause_controller;
    pause_controller.reset();
    RenshaTurbo rensha_turbo;
    rensha_turbo.reset();

    // --- Exhaustive per-key coverage: EVERY entry in scancode_map() gets a
    // dedicated press+release round-trip case. ---
    int key_cases_failed = 0;
    for (const auto& binding : Sdl3InputMapper::scancode_map()) {
        SDL_Event polled{};
        const bool down_ok = push_and_poll(make_key_event(binding.scancode, true), &polled);
        if (!down_ok) {
            ++key_cases_failed;
            continue;
        }
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        if (!keyboard.key(binding.row, binding.column)) {
            std::cerr << "Case failed: KeyDown scancode=" << binding.scancode << " row=" << binding.row
                       << " col=" << binding.column << " expected pressed=true\n";
            ++key_cases_failed;
        }

        SDL_Event polled_up{};
        const bool up_ok = push_and_poll(make_key_event(binding.scancode, false), &polled_up);
        if (!up_ok) {
            ++key_cases_failed;
            continue;
        }
        mapper.dispatch_event(polled_up, keyboard, joystick, pause_controller, rensha_turbo);
        if (keyboard.key(binding.row, binding.column)) {
            std::cerr << "Case failed: KeyUp scancode=" << binding.scancode << " row=" << binding.row
                       << " col=" << binding.column << " expected pressed=false\n";
            ++key_cases_failed;
        }
    }
    expect(key_cases_failed == 0, "ExhaustiveScancodeMap_EveryEntry_PressAndReleaseRoundTrips");
    std::cerr << "  (" << Sdl3InputMapper::scancode_map().size() << " scancode-map entries exercised, "
               << key_cases_failed << " failed)\n";

    // --- New Right-Ctrl binding: the MSX ":"/"*" key (matrix row 2,
    // column 0). Right-Ctrl -> ":", Shift+Right-Ctrl -> "*". Confirms the
    // freshly-added entry maps to (2,0) and round-trips press+release. ---
    {
        const std::optional<std::pair<int, int>> rctrl = Sdl3InputMapper::map_scancode(SDL_SCANCODE_RCTRL);
        expect(rctrl.has_value() && rctrl->first == 2 && rctrl->second == 0,
               "RightCtrl_MapsTo_Row2Column0_MsxColonStarKey");

        SDL_Event polled{};
        expect(push_and_poll(make_key_event(SDL_SCANCODE_RCTRL, true), &polled),
               "RightCtrl_KeyDown_EventRoundTripsThroughRealQueue");
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(keyboard.key(2, 0), "RightCtrl_KeyDown_SetsRow2Column0");

        SDL_Event polled_up{};
        expect(push_and_poll(make_key_event(SDL_SCANCODE_RCTRL, false), &polled_up),
               "RightCtrl_KeyUp_EventRoundTripsThroughRealQueue");
        mapper.dispatch_event(polled_up, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!keyboard.key(2, 0), "RightCtrl_KeyUp_ClearsRow2Column0");
    }

    // --- Regression guard: the RCTRL addition must not have disturbed any
    // neighboring/pre-existing mapping. Spot-check the other Ctrl (LCTRL is
    // the MSX CTRL modifier at (6,1)), the adjacent Row-2 entries, and
    // LSHIFT -- all unchanged. (The exhaustive loop above already round-trips
    // every entry; this makes the "nothing else moved" intent explicit.) ---
    {
        const auto is = [](const std::optional<std::pair<int, int>>& v, int r, int c) {
            return v.has_value() && v->first == r && v->second == c;
        };
        expect(is(Sdl3InputMapper::map_scancode(SDL_SCANCODE_LCTRL), 6, 1),
               "Regression_LeftCtrl_StillRow6Column1");
        expect(is(Sdl3InputMapper::map_scancode(SDL_SCANCODE_APOSTROPHE), 2, 1),
               "Regression_Apostrophe_StillRow2Column1");
        expect(is(Sdl3InputMapper::map_scancode(SDL_SCANCODE_LSHIFT), 6, 0),
               "Regression_LeftShift_StillRow6Column0");
    }

    // --- Regression guard: an UNMAPPED scancode (not in scancode_map(), not
    // one of the PAUSE/Speed/Ren-Sha special bindings) produces ZERO
    // observable matrix change. ---
    {
        constexpr SDL_Scancode kUnmapped = SDL_SCANCODE_NONUSBACKSLASH;
        bool matrix_unchanged_before = true;
        for (int row = 0; row < KeyboardMatrix::kRows; ++row) {
            for (int col = 0; col < KeyboardMatrix::kColumns; ++col) {
                if (keyboard.key(row, col)) {
                    matrix_unchanged_before = false;
                }
            }
        }
        expect(matrix_unchanged_before, "RegressionGuardSetup_MatrixIsIdleBeforeUnmappedInjection");

        SDL_Event polled{};
        const bool ok = push_and_poll(make_key_event(kUnmapped, true), &polled);
        expect(ok, "UnmappedScancode_EventRoundTripsThroughRealQueue");
        const bool consumed = mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!consumed, "UnmappedScancode_DispatchReturnsFalse");

        bool matrix_still_idle = true;
        for (int row = 0; row < KeyboardMatrix::kRows; ++row) {
            for (int col = 0; col < KeyboardMatrix::kColumns; ++col) {
                if (keyboard.key(row, col)) {
                    matrix_still_idle = false;
                }
            }
        }
        expect(matrix_still_idle, "UnmappedScancode_ZeroObservableMatrixChange");
    }

    // --- PAUSE button: toggle semantics, gated on a genuine fresh key-down
    // (never key-up, never OS key-repeat). ---
    {
        expect(!pause_controller.button_engaged(), "PauseButton_InitiallyDisengaged");

        SDL_Event polled{};
        push_and_poll(make_key_event(Sdl3InputMapper::kPauseButtonScancode, true), &polled);
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.button_engaged(), "PauseButton_KeyDown_TogglesEngaged");

        // A REPEATED key-down (OS auto-repeat) must NOT toggle again.
        SDL_Event polled_repeat{};
        push_and_poll(make_key_event(Sdl3InputMapper::kPauseButtonScancode, true, /*repeat=*/true), &polled_repeat);
        mapper.dispatch_event(polled_repeat, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.button_engaged(), "PauseButton_KeyRepeat_DoesNotReToggle");

        // Key-up must NOT toggle either.
        SDL_Event polled_up{};
        push_and_poll(make_key_event(Sdl3InputMapper::kPauseButtonScancode, false), &polled_up);
        mapper.dispatch_event(polled_up, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.button_engaged(), "PauseButton_KeyUp_DoesNotToggle");

        // A second genuine fresh key-down toggles back off.
        SDL_Event polled2{};
        push_and_poll(make_key_event(Sdl3InputMapper::kPauseButtonScancode, true), &polled2);
        mapper.dispatch_event(polled2, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!pause_controller.button_engaged(), "PauseButton_SecondKeyDown_TogglesDisengaged");
    }

    // --- Speed Controller step keys. ---
    {
        expect(pause_controller.speed_level() == 0, "SpeedController_InitialLevelZero");
        SDL_Event polled{};
        push_and_poll(make_key_event(Sdl3InputMapper::kSpeedUpScancode, true), &polled);
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.speed_level() == 1, "SpeedController_UpKey_IncrementsLevel");

        SDL_Event polled_down{};
        push_and_poll(make_key_event(Sdl3InputMapper::kSpeedDownScancode, true), &polled_down);
        mapper.dispatch_event(polled_down, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.speed_level() == 0, "SpeedController_DownKey_DecrementsLevel");

        // Clamped at 0 -- never goes negative.
        SDL_Event polled_down2{};
        push_and_poll(make_key_event(Sdl3InputMapper::kSpeedDownScancode, true), &polled_down2);
        mapper.dispatch_event(polled_down2, keyboard, joystick, pause_controller, rensha_turbo);
        expect(pause_controller.speed_level() == 0, "SpeedController_DownKey_ClampedAtZero");
    }

    // --- Ren-Sha Turbo step keys. ---
    {
        expect(rensha_turbo.speed() == 0, "ReshaTurbo_InitialSpeedZero");
        SDL_Event polled{};
        push_and_poll(make_key_event(Sdl3InputMapper::kReshaUpScancode, true), &polled);
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(rensha_turbo.speed() == Sdl3InputMapper::kReshaStep, "ReshaTurbo_UpKey_IncrementsBySteps");

        SDL_Event polled_down{};
        push_and_poll(make_key_event(Sdl3InputMapper::kReshaDownScancode, true), &polled_down);
        mapper.dispatch_event(polled_down, keyboard, joystick, pause_controller, rensha_turbo);
        expect(rensha_turbo.speed() == 0, "ReshaTurbo_DownKey_DecrementsBySteps");
    }

    // --- Joystick button/axis mapping (port index 0). ---
    {
        SDL_Event polled{};
        push_and_poll(make_joy_button_event(0, true), &polled);
        mapper.dispatch_event(polled, keyboard, joystick, pause_controller, rensha_turbo);
        expect(joystick.port(0).trigger_a, "Joystick_Button0Down_SetsTriggerA");

        SDL_Event polled_up{};
        push_and_poll(make_joy_button_event(0, false), &polled_up);
        mapper.dispatch_event(polled_up, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!joystick.port(0).trigger_a, "Joystick_Button0Up_ClearsTriggerA");

        SDL_Event polled_b{};
        push_and_poll(make_joy_button_event(1, true), &polled_b);
        mapper.dispatch_event(polled_b, keyboard, joystick, pause_controller, rensha_turbo);
        expect(joystick.port(0).trigger_b, "Joystick_Button1Down_SetsTriggerB");

        SDL_Event polled_axis_x_pos{};
        push_and_poll(make_joy_axis_event(0, 32000), &polled_axis_x_pos);
        mapper.dispatch_event(polled_axis_x_pos, keyboard, joystick, pause_controller, rensha_turbo);
        expect(joystick.port(0).right && !joystick.port(0).left, "Joystick_AxisXPositive_SetsRight");

        SDL_Event polled_axis_x_neg{};
        push_and_poll(make_joy_axis_event(0, -32000), &polled_axis_x_neg);
        mapper.dispatch_event(polled_axis_x_neg, keyboard, joystick, pause_controller, rensha_turbo);
        expect(joystick.port(0).left && !joystick.port(0).right, "Joystick_AxisXNegative_SetsLeft");

        SDL_Event polled_axis_y_pos{};
        push_and_poll(make_joy_axis_event(1, 32000), &polled_axis_y_pos);
        mapper.dispatch_event(polled_axis_y_pos, keyboard, joystick, pause_controller, rensha_turbo);
        expect(joystick.port(0).down && !joystick.port(0).up, "Joystick_AxisYPositive_SetsDown");
    }

    SDL_Quit();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3InputMapper_Integration cases passed\n";
    return 0;
}
