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

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"

namespace sony_msx::machine {

// Deterministic, timed key-event scripting format + parser + player.
// Enables keystroke-sequencing/
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
//
// A second, ADDITIVE line
// kind, `JOY=`, lets the same deterministic script format inject joystick
// direction/trigger state for the A/B production-QA suite's STICK/STRIG
// scenarios:
//   T=<tstate-dec> JOY=<port> <UP|DOWN|LEFT|RIGHT|A|B> DOWN
//   T=<tstate-dec> JOY=<port> <UP|DOWN|LEFT|RIGHT|A|B> UP
// where <port> is 1 or 2 (mapped to peripherals::JoystickPorts index port-1)
// and the trailing DOWN/UP is the press-state (DOWN = pressed). The KEY= line
// kind and its parse/serialize output are byte-identical to before -- a script
// with no JOY= line behaves exactly as it did before JOY= was added (hard
// regression guard, input_script_unit_test.cpp). `A`/`B` name trigger A/B;
// `UP/DOWN/LEFT/RIGHT` name the four directions.
inline constexpr const char* kInputScriptFormatTag = "HBF1XV-INPUT-SCRIPT v1";

// Discriminates the two line kinds. `Key` is the default so a value-initialized
// or aggregate-initialized {tstate, key_name, pressed} event (the original
// construction form used by existing tests) is a keyboard event unchanged.
enum class InputEventKind : std::uint8_t { Key, Joy };

// The six joystick controls a JOY= line can name. `A`/`B` are trigger A/B;
// the other four are directions. Maps 1:1 onto peripherals::JoystickPorts::
// PortState's fields in InputScriptPlayer::apply_due().
enum class JoyControl : std::uint8_t { Up, Down, Left, Right, TriggerA, TriggerB };

struct InputScriptEvent {
    std::uint64_t at_tstate = 0;
    // KEY events only (empty for JOY events).
    std::string key_name;
    // Shared press-state: DOWN = true, UP = false (both line kinds).
    bool pressed = false;
    // Additive JOY-support fields; the default (Key) keeps aggregate-init
    // {tstate, key_name, pressed} events keyboard events.
    InputEventKind kind = InputEventKind::Key;
    // JOY events only: 1 or 2 (port index = joy_port - 1).
    int joy_port = 0;
    JoyControl joy_control = JoyControl::Up;
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

// Serializes ONE event to its single script line WITHOUT a trailing newline
// (the "T=<t> KEY=<name> DOWN|UP" / "T=<t> JOY=<port> <control> DOWN|UP" body).
// The SINGLE source of truth for a script line's exact byte format, consumed by
// both serialize_input_script() above and the streaming InputScriptRecorder
// below -- so a live-recorded KEY/JOY line is byte-identical to what a
// serialize_input_script() of the same event would emit (the record->replay
// byte-for-byte round-trip guarantee).
[[nodiscard]] std::string serialize_input_script_line(const InputScriptEvent& event);

// Deterministic STREAMING recorder -- the write-side counterpart to
// parse_input_script()/InputScriptPlayer. Streams a live keyboard + disk-swap
// session into an HBF1XV-INPUT-SCRIPT v1 file AS IT HAPPENS (each line flushed,
// so an abrupt exit still leaves a usable prefix), producing a file that:
//   * replays deterministically via --input-script (KEY lines are byte-identical
//     to serialize_input_script_line(), and re-parse cleanly), and
//   * carries the F11 disk hot-swaps as "# SWAP_DISK frame=<N>" comment lines --
//     a marker the parser SKIPS (parse_input_script() ignores '#'-comment
//     lines), so the file stays directly --input-script-loadable while the
//     replay side passes --swap-disk-frame <N> (the existing headless swap flag,
//     src/main.cpp) to reproduce the swap at the same frame.
// SDL3-independent (operates purely on cycle-stamp + key-name + frame), so it
// lives in the headless core and is unit-testable without SDL3.
class InputScriptRecorder {
public:
    InputScriptRecorder() = default;
    ~InputScriptRecorder() { close(); }

    InputScriptRecorder(const InputScriptRecorder&) = delete;
    InputScriptRecorder& operator=(const InputScriptRecorder&) = delete;

    // Opens `path` (truncating) and writes the format-tag line. Returns false
    // (leaving is_open() false) if the file cannot be opened for writing.
    [[nodiscard]] bool open(const std::string& path);
    [[nodiscard]] bool is_open() const { return out_.is_open(); }

    // Appends "T=<tstate> KEY=<name> DOWN|UP" (via serialize_input_script_line,
    // so byte-identical to serialize_input_script) and flushes. No-op if not
    // open. `pressed` == true -> DOWN.
    void record_key(std::uint64_t tstate, const std::string& key_name, bool pressed);

    // Appends the informational "# SWAP_DISK frame=<N>" comment line and flushes
    // (the replay side reproduces it via --swap-disk-frame <N>). No-op if not open.
    void record_disk_swap(std::uint64_t frame);

    // Writes the trailing "[END]" line and closes the file. Safe to call
    // repeatedly / on a never-opened recorder (a no-op). The destructor calls it.
    void close();

    [[nodiscard]] std::size_t key_event_count() const { return key_event_count_; }
    [[nodiscard]] std::size_t disk_swap_count() const { return disk_swap_count_; }

private:
    std::ofstream out_;
    std::size_t key_event_count_ = 0;
    std::size_t disk_swap_count_ = 0;
};

// A monotonic-cursor player (mirrors this project's established
// event-driven, monotonic-cursor architectural precedent -- the FDC
// DRQ/INTRQ state machine, the VDP command engine's LMCM/LMMC/HMMC
// event-driven commands -- an architectural pattern citation, not identical
// code). Applies every event with at_tstate <= current_tstate not yet
// applied, in file order, via peripherals::key_name_to_row_col() +
// KeyboardMatrix::set_key(). Never re-scans from the start; a stale or
// repeated current_tstate re-application is a safe no-op (every due event is
// applied exactly once, never skipped, never re-applied).
class InputScriptPlayer {
public:
    explicit InputScriptPlayer(std::vector<InputScriptEvent> events = {});

    // Optional joystick sink for JOY= events (default null). When null,
    // JOY= events are applied as safe no-ops (the monotonic cursor still
    // advances so no KEY= event is ever skipped) -- so the keyboard-only
    // apply_due() path is byte-for-byte unchanged from before JOY= support.
    // Attaching a
    // JoystickPorts lets apply_due() drive JoystickPorts::set_port() per event.
    void attach_joystick(peripherals::JoystickPorts* joystick);

    void apply_due(std::uint64_t current_tstate, peripherals::KeyboardMatrix& keyboard);

    [[nodiscard]] std::size_t cursor() const { return cursor_; }
    [[nodiscard]] std::size_t event_count() const { return events_.size(); }
    [[nodiscard]] const std::vector<InputScriptEvent>& events() const { return events_; }

private:
    std::vector<InputScriptEvent> events_;
    std::size_t cursor_ = 0;
    // Optional joystick target + a shadow per-port PortState the JOY=
    // events accumulate into (so setting LEFT does not clear a held trigger).
    peripherals::JoystickPorts* joystick_ = nullptr;
    std::array<peripherals::JoystickPorts::PortState, 2> joy_state_{};
};

}  // namespace sony_msx::machine
