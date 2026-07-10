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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "peripherals/keyboard_matrix.h"

namespace sony_msx::machine {

// Deterministic, timed key-event scripting format + parser + player (M27-S6,
// "Production Hardening + Debug/Test Tooling" item 3,
// docs/m27-planner-package.md §2.4). Enables keystroke-sequencing/
// scripted-input automation against either executable, driven through the
// same CPU sub-loop each already runs.
//
// Format (mirrors debug_dump.h/frame_dump.h's ASCII discipline: fixed field
// order, base-10 T-state values, '\n' line endings, zero wall-clock/
// environment content):
//   HBF1XV-INPUT-SCRIPT v1
//   T=<tstate-dec> KEY=<name> DOWN
//   T=<tstate-dec> KEY=<name> UP
//   [END]
//
// `T` is the cumulative machine T-state (Hbf1xvMachine::elapsed_cycles()'s
// deterministic clock basis -- the same field convention DebugEvent::tstate
// uses, debug_event_log.h:23). Events must be strictly non-decreasing in `T`
// in the file; `KEY` names are resolved via peripherals::key_name_to_row_col().
// Only keyboard events are a required deliverable this cycle (joystick-event
// scripting is an out-of-scope-unless-trivial extension,
// docs/m27-planner-package.md §1.2) -- so this format has no `JOY=` line kind.
inline constexpr const char* kInputScriptFormatTag = "HBF1XV-INPUT-SCRIPT v1";

struct InputScriptEvent {
    std::uint64_t at_tstate = 0;
    std::string key_name;
    bool pressed = false;
};

// Parses the format above. Throws std::runtime_error on any malformed input
// -- a missing/mismatched format tag, a line that is neither DOWN nor UP, an
// unrecognized KEY name (peripherals::key_name_to_row_col() returns
// std::nullopt), or a non-monotonic T value -- mirroring frame_dump.h's
// "throws on malformed input, never silently returns garbage" discipline
// (frame_dump.h:44-45).
[[nodiscard]] std::vector<InputScriptEvent> parse_input_script(const std::string& text);

// Deterministic serialization; a parse(serialize(x)) == x round-trip holds
// for any `events` already satisfying the non-decreasing-T invariant (this
// function does not itself re-validate that invariant -- callers that built
// the vector via parse_input_script() already satisfy it).
[[nodiscard]] std::string serialize_input_script(const std::vector<InputScriptEvent>& events);

// A monotonic-cursor player (M27-S6/S7, mirrors this project's established
// event-driven, monotonic-cursor architectural precedent -- the M16 FDC
// DRQ/INTRQ state machine, the M22 VDP command engine's LMCM/LMMC/HMMC
// event-driven commands -- an architectural pattern citation, not identical
// code). Applies every event with at_tstate <= current_tstate not yet
// applied, in file order, via peripherals::key_name_to_row_col() +
// KeyboardMatrix::set_key(). Never re-scans from the start; a stale or
// repeated current_tstate re-application is a safe no-op (every due event is
// applied exactly once, never skipped, never re-applied).
class InputScriptPlayer {
public:
    explicit InputScriptPlayer(std::vector<InputScriptEvent> events = {});

    void apply_due(std::uint64_t current_tstate, peripherals::KeyboardMatrix& keyboard);

    [[nodiscard]] std::size_t cursor() const { return cursor_; }
    [[nodiscard]] std::size_t event_count() const { return events_.size(); }
    [[nodiscard]] const std::vector<InputScriptEvent>& events() const { return events_; }

private:
    std::vector<InputScriptEvent> events_;
    std::size_t cursor_ = 0;
};

}  // namespace sony_msx::machine
