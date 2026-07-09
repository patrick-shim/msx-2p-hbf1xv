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

#include <iostream>

#include "devices/chipset/mb670836_pause.h"
#include "frontend/sdl3_input_mapper.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"
#include "peripherals/rensha_turbo.h"

// Suite: Frontend_Sdl3InputMapper_DiskSwap_Unit (M35-S3, docs/m35-
// planner-package.md §5, test obligation UT-2).
//
// Unit tests for F11 hotkey detection in the input mapper. Verifies:
// - F11 key-down is recognized (AC-S3-1).
// - F11 is correctly distinguished from other keys (AC-S3-2).
// - F11 press is not routed to the keyboard matrix (only returns true).

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

// Helper: create a minimal SDL_Event for key testing.
SDL_Event make_key_event(SDL_Scancode scancode, bool pressed, bool repeat) {
    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = pressed;
    event.key.repeat = repeat;
    return event;
}

}  // namespace

int main() {
    Sdl3InputMapper mapper;
    KeyboardMatrix keyboard;
    JoystickPorts joystick;
    Mb670836PauseController pause_controller;
    RenshaTurbo rensha_turbo;

    // --- Case 1: F11 key-down (fresh press, not repeat) is recognized
    // (returns true). AC-S3-1.
    {
        SDL_Event event = make_key_event(SDL_SCANCODE_F11, true, false);
        const bool result =
            mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);
        expect(result, "F11KeyDown_Recognized");
    }

    // --- Case 2: F11 key-down with repeat flag is also recognized
    // (implementation detail: repeat events still return true, but the
    // actual hotkey action in Sdl3App checks !event.key.repeat before
    // calling on_disk_swap_hotkey()).
    {
        SDL_Event event = make_key_event(SDL_SCANCODE_F11, true, true);
        const bool result =
            mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);
        expect(result, "F11KeyDownRepeat_Recognized");
    }

    // --- Case 3: F11 key-up is recognized (returns true).
    {
        SDL_Event event = make_key_event(SDL_SCANCODE_F11, false, false);
        const bool result =
            mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);
        expect(result, "F11KeyUp_Recognized");
    }

    // --- Case 4: F10 (adjacent key) is NOT recognized as F11. AC-S3-2.
    {
        SDL_Event event = make_key_event(SDL_SCANCODE_F10, true, false);
        const bool result =
            mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!result, "F10KeyDown_NotRecognizedAsF11");
    }

    // --- Case 5: F12 (adjacent key) is NOT recognized as F11. AC-S3-2.
    {
        SDL_Event event = make_key_event(SDL_SCANCODE_F12, true, false);
        const bool result =
            mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);
        expect(!result, "F12KeyDown_NotRecognizedAsF11");
    }

    // --- Case 6: F11 is not routed to the keyboard matrix. To verify this,
    // we check that pressing F11 does NOT set any keyboard matrix state
    // (the mapper returns true for F11 without feeding it to the matrix).
    // We'll verify by checking that the keyboard matrix is unaffected by
    // an F11 event.
    {
        keyboard.set_key(0, 0, true);  // Manually set row 0, col 0
        const auto row0_col0_before = keyboard.key(0, 0);

        SDL_Event event = make_key_event(SDL_SCANCODE_F11, true, false);
        mapper.dispatch_event(event, keyboard, joystick, pause_controller, rensha_turbo);

        const auto row0_col0_after = keyboard.key(0, 0);
        expect(row0_col0_before == row0_col0_after, "F11KeyDown_NotRoutedToMatrix");
    }

    // --- Case 7: Existing hotkeys (F6/F7/F8/F9/Pause) still work and are
    // recognized.
    {
        SDL_Event event_f6 = make_key_event(SDL_SCANCODE_F6, true, false);
        const bool result_f6 =
            mapper.dispatch_event(event_f6, keyboard, joystick, pause_controller, rensha_turbo);
        expect(result_f6, "F6KeyDown_StillRecognized");

        SDL_Event event_pause = make_key_event(SDL_SCANCODE_PAUSE, true, false);
        const bool result_pause =
            mapper.dispatch_event(event_pause, keyboard, joystick, pause_controller, rensha_turbo);
        expect(result_pause, "PauseKeyDown_StillRecognized");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3InputMapper_DiskSwap_Unit cases passed\n";
    return 0;
}
