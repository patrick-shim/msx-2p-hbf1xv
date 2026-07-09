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

#include "frontend/sdl3_input_mapper.h"

#include <algorithm>

namespace sony_msx::frontend {

const std::array<ScancodeBinding, 71>& Sdl3InputMapper::scancode_map() {
    // The standard MSX international 11x8 keyboard matrix, rows 0-8 (see the
    // header doc comment for the row8/SPACE cross-check against
    // peripherals::RenshaTurbo's own already-established M25 fact). Row
    // 2 column 0 (":") is intentionally left unmapped -- no dedicated
    // physical PC scancode exists for a bare colon key (it is a SHIFT
    // combination on a standard PC keyboard, not a distinct physical key),
    // so it is honestly omitted rather than guessed.
    static const std::array<ScancodeBinding, 71> kMap{{
        // Row 0: digits 0-7
        {SDL_SCANCODE_0, 0, 0},
        {SDL_SCANCODE_1, 0, 1},
        {SDL_SCANCODE_2, 0, 2},
        {SDL_SCANCODE_3, 0, 3},
        {SDL_SCANCODE_4, 0, 4},
        {SDL_SCANCODE_5, 0, 5},
        {SDL_SCANCODE_6, 0, 6},
        {SDL_SCANCODE_7, 0, 7},
        // Row 1: 8 9 - = \ [ ] ;
        {SDL_SCANCODE_8, 1, 0},
        {SDL_SCANCODE_9, 1, 1},
        {SDL_SCANCODE_MINUS, 1, 2},
        {SDL_SCANCODE_EQUALS, 1, 3},
        {SDL_SCANCODE_BACKSLASH, 1, 4},
        {SDL_SCANCODE_LEFTBRACKET, 1, 5},
        {SDL_SCANCODE_RIGHTBRACKET, 1, 6},
        {SDL_SCANCODE_SEMICOLON, 1, 7},
        // Row 2: (":" col0 unmapped) ' , . / ` A B
        {SDL_SCANCODE_APOSTROPHE, 2, 1},
        {SDL_SCANCODE_COMMA, 2, 2},
        {SDL_SCANCODE_PERIOD, 2, 3},
        {SDL_SCANCODE_SLASH, 2, 4},
        {SDL_SCANCODE_GRAVE, 2, 5},
        {SDL_SCANCODE_A, 2, 6},
        {SDL_SCANCODE_B, 2, 7},
        // Row 3: C D E F G H I J
        {SDL_SCANCODE_C, 3, 0},
        {SDL_SCANCODE_D, 3, 1},
        {SDL_SCANCODE_E, 3, 2},
        {SDL_SCANCODE_F, 3, 3},
        {SDL_SCANCODE_G, 3, 4},
        {SDL_SCANCODE_H, 3, 5},
        {SDL_SCANCODE_I, 3, 6},
        {SDL_SCANCODE_J, 3, 7},
        // Row 4: K L M N O P Q R
        {SDL_SCANCODE_K, 4, 0},
        {SDL_SCANCODE_L, 4, 1},
        {SDL_SCANCODE_M, 4, 2},
        {SDL_SCANCODE_N, 4, 3},
        {SDL_SCANCODE_O, 4, 4},
        {SDL_SCANCODE_P, 4, 5},
        {SDL_SCANCODE_Q, 4, 6},
        {SDL_SCANCODE_R, 4, 7},
        // Row 5: S T U V W X Y Z
        {SDL_SCANCODE_S, 5, 0},
        {SDL_SCANCODE_T, 5, 1},
        {SDL_SCANCODE_U, 5, 2},
        {SDL_SCANCODE_V, 5, 3},
        {SDL_SCANCODE_W, 5, 4},
        {SDL_SCANCODE_X, 5, 5},
        {SDL_SCANCODE_Y, 5, 6},
        {SDL_SCANCODE_Z, 5, 7},
        // Row 6: SHIFT CTRL GRAPH CAPS CODE F1 F2 F3
        {SDL_SCANCODE_LSHIFT, 6, 0},
        {SDL_SCANCODE_LCTRL, 6, 1},
        {SDL_SCANCODE_LALT, 6, 2},
        {SDL_SCANCODE_CAPSLOCK, 6, 3},
        {SDL_SCANCODE_LGUI, 6, 4},
        {SDL_SCANCODE_F1, 6, 5},
        {SDL_SCANCODE_F2, 6, 6},
        {SDL_SCANCODE_F3, 6, 7},
        // Row 7: F4 F5 ESC TAB STOP BS SELECT RETURN
        {SDL_SCANCODE_F4, 7, 0},
        {SDL_SCANCODE_F5, 7, 1},
        {SDL_SCANCODE_ESCAPE, 7, 2},
        {SDL_SCANCODE_TAB, 7, 3},
        {SDL_SCANCODE_END, 7, 4},
        {SDL_SCANCODE_BACKSPACE, 7, 5},
        {SDL_SCANCODE_PAGEUP, 7, 6},
        {SDL_SCANCODE_RETURN, 7, 7},
        // Row 8: SPACE HOME INS DEL LEFT UP DOWN RIGHT
        {SDL_SCANCODE_SPACE, 8, 0},
        {SDL_SCANCODE_HOME, 8, 1},
        {SDL_SCANCODE_INSERT, 8, 2},
        {SDL_SCANCODE_DELETE, 8, 3},
        {SDL_SCANCODE_LEFT, 8, 4},
        {SDL_SCANCODE_UP, 8, 5},
        {SDL_SCANCODE_DOWN, 8, 6},
        {SDL_SCANCODE_RIGHT, 8, 7},
    }};
    return kMap;
}

std::optional<std::pair<int, int>> Sdl3InputMapper::map_scancode(const SDL_Scancode scancode) {
    for (const ScancodeBinding& binding : scancode_map()) {
        if (binding.scancode == scancode) {
            return std::make_pair(binding.row, binding.column);
        }
    }
    return std::nullopt;
}

bool Sdl3InputMapper::dispatch_key_event(const SDL_Event& event, peripherals::KeyboardMatrix& keyboard,
                                         devices::chipset::Mb670836PauseController& pause_controller,
                                         peripherals::RenshaTurbo& rensha_turbo) {
    const bool pressed = event.key.down;
    const SDL_Scancode scancode = event.key.scancode;

    // PAUSE/Speed-Controller/Ren-Sha-Turbo bindings: act ONLY on a genuine
    // fresh key-down (never on key-up, never on OS key-repeat) -- toggle/step
    // semantics, matching the M25 API's own "one press = one toggle/step"
    // contract.
    if (scancode == kPauseButtonScancode) {
        if (pressed && !event.key.repeat) {
            pause_controller.press_pause_button();
        }
        return true;
    }
    if (scancode == kSpeedDownScancode) {
        if (pressed && !event.key.repeat) {
            pause_controller.set_speed_level(std::max(0, pause_controller.speed_level() - 1));
        }
        return true;
    }
    if (scancode == kSpeedUpScancode) {
        if (pressed && !event.key.repeat) {
            pause_controller.set_speed_level(
                std::min(devices::chipset::Mb670836PauseController::kMaxSpeedLevel, pause_controller.speed_level() + 1));
        }
        return true;
    }
    if (scancode == kReshaDownScancode) {
        if (pressed && !event.key.repeat) {
            rensha_turbo.set_speed(std::max(0, rensha_turbo.speed() - kReshaStep));
        }
        return true;
    }
    if (scancode == kReshaUpScancode) {
        if (pressed && !event.key.repeat) {
            rensha_turbo.set_speed(std::min(100, rensha_turbo.speed() + kReshaStep));
        }
        return true;
    }

    const std::optional<std::pair<int, int>> coord = map_scancode(scancode);
    if (!coord.has_value()) {
        return false;  // Unmapped scancode: zero observable matrix effect (regression guard).
    }
    keyboard.set_key(coord->first, coord->second, pressed);
    return true;
}

bool Sdl3InputMapper::dispatch_joystick_event(const SDL_Event& event, peripherals::JoystickPorts& joystick) {
    // Single-controller support this cycle: all joystick input targets port
    // index 0 (documented, first-principles simplification -- multi-
    // controller port routing is an out-of-scope depth item).
    constexpr int kPortIndex = 0;

    if (event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN || event.type == SDL_EVENT_JOYSTICK_BUTTON_UP) {
        peripherals::JoystickPorts::PortState state = joystick.port(kPortIndex);
        if (event.jbutton.button == 0) {
            state.trigger_a = event.jbutton.down;
        } else if (event.jbutton.button == 1) {
            state.trigger_b = event.jbutton.down;
        } else {
            return false;
        }
        joystick.set_port(kPortIndex, state);
        return true;
    }

    if (event.type == SDL_EVENT_JOYSTICK_AXIS_MOTION) {
        peripherals::JoystickPorts::PortState state = joystick.port(kPortIndex);
        if (event.jaxis.axis == 0) {
            state.left = event.jaxis.value < -kJoystickAxisThreshold;
            state.right = event.jaxis.value > kJoystickAxisThreshold;
        } else if (event.jaxis.axis == 1) {
            state.up = event.jaxis.value < -kJoystickAxisThreshold;
            state.down = event.jaxis.value > kJoystickAxisThreshold;
        } else {
            return false;
        }
        joystick.set_port(kPortIndex, state);
        return true;
    }

    return false;
}

bool Sdl3InputMapper::dispatch_event(const SDL_Event& event, peripherals::KeyboardMatrix& keyboard,
                                     peripherals::JoystickPorts& joystick,
                                     devices::chipset::Mb670836PauseController& pause_controller,
                                     peripherals::RenshaTurbo& rensha_turbo) {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        return dispatch_key_event(event, keyboard, pause_controller, rensha_turbo);
    }
    if (event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN || event.type == SDL_EVENT_JOYSTICK_BUTTON_UP ||
        event.type == SDL_EVENT_JOYSTICK_AXIS_MOTION) {
        return dispatch_joystick_event(event, joystick);
    }
    return false;
}

}  // namespace sony_msx::frontend
