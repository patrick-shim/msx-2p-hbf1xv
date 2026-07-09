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

#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "devices/chipset/mb670836_pause.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"
#include "peripherals/rensha_turbo.h"

namespace sony_msx::frontend {

// One `SDL_Scancode` -> MSX 11x8 keyboard-matrix (row, column) binding.
struct ScancodeBinding {
    SDL_Scancode scancode;
    int row;
    int column;
};

// SDL3 real-input -> KeyboardMatrix/JoystickPorts/PAUSE/Speed-Controller/
// Ren-Sha-Turbo mapper (M26-S6, docs/m26-planner-package.md §2.7).
//
// **Keyboard.** Keys off `SDL_KeyboardEvent::scancode` (a PHYSICAL,
// layout-independent key identity, matching KeyboardMatrix's own physical
// row/column-matrix semantics -- SDL_events.h:380's own doc: "SDL physical
// key code"). `kScancodeMap` below is the well-known, standard MSX
// international 11x8 keyboard matrix layout (rows 0-8; the two numeric-
// keypad rows 9-10 are not mapped this cycle, a disclosed, bounded-scope
// simplification) -- independently cross-checked against this project's OWN
// already-established ground truth: `peripherals::RenshaTurbo`'s doc comment
// (src/peripherals/rensha_turbo.h, M25) cites "keyboard row 8 bit 0 (SPACE)"
// as the real autofire attach point, which matches `kScancodeMap`'s own
// row=8,column=0 = SDL_SCANCODE_SPACE entry exactly. This table is genuinely
// NEW to M26 (an SDL-scancode-keyed lookup on top of the already-correct,
// already-tested M15 row/column semantics) -- not a re-derivation of the
// matrix contract itself. `references/openmsx-21.0/src/input/
// UnicodeKeymap.hh` (confirmed present) is openMSX's own generic host-key ->
// MSX-matrix translation MECHANISM and may be consulted as an
// implementation-TECHNIQUE reference only -- never copied verbatim (GPL
// isolation).
//
// **Joystick.** Digital direction from axis 0/1 (a documented threshold, no
// analog depth) and triggers A/B from button indices 0/1 -- bound to port
// index 0 only this cycle (single-controller support; multi-controller port
// routing is an out-of-scope depth item, not attempted).
//
// **PAUSE / Speed Controller / Ren-Sha Turbo (A-M26-7 -- new, disclosed,
// first-principles PC-keybinding CHOICES, extending the M25 numeric-model
// design defaults to a physical PC key; NOT a hardware fact -- no PC
// keyboard has a physical PAUSE-toggle/slow-motion-slider/autofire-slider in
// the Sony sense):**
//   - PAUSE button -> SDL_SCANCODE_PAUSE (a genuine, dedicated PC key with an
//     obviously matching name).
//   - Speed Controller step down/up -> SDL_SCANCODE_F6/F7.
//   - Ren-Sha Turbo step down/up (steps of 10, clamped [0,100]) ->
//     SDL_SCANCODE_F8/F9.
// All three are revisable defaults if this project ever adds a remapping
// layer (explicitly out of scope this cycle).
class Sdl3InputMapper {
public:
    static constexpr SDL_Scancode kPauseButtonScancode = SDL_SCANCODE_PAUSE;
    static constexpr SDL_Scancode kSpeedDownScancode = SDL_SCANCODE_F6;
    static constexpr SDL_Scancode kSpeedUpScancode = SDL_SCANCODE_F7;
    static constexpr SDL_Scancode kReshaDownScancode = SDL_SCANCODE_F8;
    static constexpr SDL_Scancode kReshaUpScancode = SDL_SCANCODE_F9;
    static constexpr SDL_Scancode kDiskSwapScancode = SDL_SCANCODE_F11;  // M35-S3: F11 for multi-disk swap
    static constexpr int kReshaStep = 10;
    // Digital joystick axis threshold (Sint16 range -32768..32767) -- a
    // documented, first-principles simplification (no analog depth).
    static constexpr Sint16 kJoystickAxisThreshold = 16000;

    // The full keyboard scancode->(row,column) table (rows 0-8), exposed so
    // both production code and the exhaustive M26-S6 ctest (`SDL_PushEvent`-
    // injected, one case per entry) share the SAME single source of truth --
    // never duplicated/hand-copied between the two.
    static const std::array<ScancodeBinding, 71>& scancode_map();

    // Looks up one scancode in scancode_map(); std::nullopt for anything not
    // present (the regression-guard case: zero observable matrix effect).
    [[nodiscard]] static std::optional<std::pair<int, int>> map_scancode(SDL_Scancode scancode);

    // Dispatches ONE real SDL_Event (already dequeued via SDL_PollEvent, or
    // injected via SDL_PushEvent in a test) to the keyboard matrix /
    // joystick ports / PAUSE / Speed-Controller / Ren-Sha-Turbo bindings.
    // Returns true if the event type/scancode/button was recognized by this
    // mapper (informational only; callers never need to branch on it).
    bool dispatch_event(const SDL_Event& event, peripherals::KeyboardMatrix& keyboard,
                         peripherals::JoystickPorts& joystick,
                         devices::chipset::Mb670836PauseController& pause_controller,
                         peripherals::RenshaTurbo& rensha_turbo);

private:
    bool dispatch_key_event(const SDL_Event& event, peripherals::KeyboardMatrix& keyboard,
                            devices::chipset::Mb670836PauseController& pause_controller,
                            peripherals::RenshaTurbo& rensha_turbo);
    bool dispatch_joystick_event(const SDL_Event& event, peripherals::JoystickPorts& joystick);
};

}  // namespace sony_msx::frontend
