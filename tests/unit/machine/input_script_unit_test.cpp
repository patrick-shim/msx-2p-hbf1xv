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
#include <stdexcept>
#include <string>
#include <vector>

#include "machine/input_script.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"

// Suite: Machine_InputScript_Unit
//
// Proves the deterministic timed key-event script format's parser/
// serializer round-trips exactly, rejects EVERY named malformed-input class
// (never silently returns a partial/garbage result, mirrors
// frame_dump.h:44-45's discipline), and that InputScriptPlayer's monotonic
// cursor applies every due event EXACTLY once, in order, never skipped,
// never re-applied -- even across multiple apply_due() calls with the same
// or a stale current_tstate.

namespace {

using sony_msx::machine::InputEventKind;
using sony_msx::machine::InputScriptEvent;
using sony_msx::machine::InputScriptPlayer;
using sony_msx::machine::JoyControl;
using sony_msx::machine::parse_input_script;
using sony_msx::machine::serialize_input_script;
using sony_msx::peripherals::JoystickPorts;
using sony_msx::peripherals::KeyboardMatrix;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

bool throws_runtime_error(const std::string& text) {
    try {
        (void)parse_input_script(text);
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;  // Wrong exception type -- still a failure to report.
    }
    return false;
}

}  // namespace

int main() {
    // --- Case 1: parse/serialize round-trip (deterministic oracle). ---
    {
        const std::string text =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "T=0 KEY=A DOWN\n"
            "T=10 KEY=A UP\n"
            "T=10 KEY=SPACE DOWN\n"
            "T=25 KEY=SPACE UP\n"
            "[END]\n";
        const std::vector<InputScriptEvent> events = parse_input_script(text);
        expect(events.size() == 4, "RoundTrip_ParsesFourEvents");
        expect(events[0].at_tstate == 0 && events[0].key_name == "A" && events[0].pressed,
               "RoundTrip_Event0_MatchesOracle");
        expect(events[1].at_tstate == 10 && events[1].key_name == "A" && !events[1].pressed,
               "RoundTrip_Event1_MatchesOracle");
        expect(events[2].at_tstate == 10 && events[2].key_name == "SPACE" && events[2].pressed,
               "RoundTrip_Event2_MatchesOracle_SameTAsPreviousAllowed");
        expect(events[3].at_tstate == 25 && events[3].key_name == "SPACE" && !events[3].pressed,
               "RoundTrip_Event3_MatchesOracle");

        const std::string serialized = serialize_input_script(events);
        expect(serialized == text, "RoundTrip_Serialize_ByteIdenticalToOriginal");

        const std::vector<InputScriptEvent> reparsed = parse_input_script(serialized);
        expect(reparsed.size() == events.size(), "RoundTrip_Reparse_SameEventCount");
        bool all_match = true;
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (reparsed[i].at_tstate != events[i].at_tstate || reparsed[i].key_name != events[i].key_name ||
                reparsed[i].pressed != events[i].pressed) {
                all_match = false;
            }
        }
        expect(all_match, "RoundTrip_Reparse_EventsByteIdentical");
    }

    // --- Case 2: malformed-input rejection -- every named class throws
    // std::runtime_error, never silently returns a partial/garbage result. ---
    expect(throws_runtime_error("NOT-THE-RIGHT-TAG\n[END]\n"), "Malformed_MissingFormatTag_Throws");
    expect(throws_runtime_error(""), "Malformed_EmptyInput_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=10 KEY=A SIDEWAYS\n[END]\n"),
           "Malformed_NeitherDownNorUp_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=10 KEY=NOSUCHKEY DOWN\n[END]\n"),
           "Malformed_UnrecognizedKeyName_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=abc KEY=A DOWN\n[END]\n"),
           "Malformed_NonNumericTValue_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=10 KEY=A DOWN\nT=5 KEY=A UP\n[END]\n"),
           "Malformed_OutOfOrderT_NonDecreasing_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nKEY=A DOWN\n[END]\n"), "Malformed_MissingTToken_Throws");

    // --- Case 3: a well-formed, strictly non-decreasing-T script parses
    // cleanly (regression guard against Case 2's negative checks being
    // vacuously over-broad). ---
    {
        const std::string text = "HBF1XV-INPUT-SCRIPT v1\nT=0 KEY=A DOWN\nT=0 KEY=A UP\n[END]\n";
        bool threw = false;
        try {
            const std::vector<InputScriptEvent> events = parse_input_script(text);
            expect(events.size() == 2, "SameTTwice_NonDecreasing_Allowed_ParsesTwoEvents");
        } catch (const std::exception&) {
            threw = true;
        }
        expect(!threw, "SameTTwice_NonDecreasing_DoesNotThrow");
    }

    // --- Case 4: InputScriptPlayer's monotonic cursor -- applies every due
    // event EXACTLY once, in order; never skipped, never re-applied, even
    // across multiple apply_due() calls with the same or a stale
    // current_tstate. ---
    {
        std::vector<InputScriptEvent> events;
        events.push_back(InputScriptEvent{0, "A", true});
        events.push_back(InputScriptEvent{10, "A", false});
        events.push_back(InputScriptEvent{10, "B", true});
        events.push_back(InputScriptEvent{50, "B", false});

        InputScriptPlayer player(events);
        KeyboardMatrix keyboard;
        keyboard.reset();

        // T=0: only event 0 (A DOWN) is due.
        player.apply_due(0, keyboard);
        expect(player.cursor() == 1, "Player_AtT0_CursorAdvancesByOne");
        // "A" resolves to (row=2, column=6) per msx_key_names.cpp; key()
        // returns true when pressed (already-normalized semantics,
        // keyboard_matrix.cpp:22-28).
        expect(keyboard.key(2, 6), "Player_AtT0_AIsPressed");

        // A repeated call at the SAME current_tstate is a safe no-op (no
        // event re-applied, cursor unchanged).
        player.apply_due(0, keyboard);
        expect(player.cursor() == 1, "Player_RepeatedSameTstate_CursorUnchanged_NoOp");

        // A STALE (lower) current_tstate is likewise a safe no-op.
        player.apply_due(0, keyboard);
        expect(player.cursor() == 1, "Player_StaleTstate_CursorUnchanged_NoOp");

        // T=10: two events due at once (A UP, B DOWN) -- both applied in
        // file order, cursor advances by 2 in one call.
        player.apply_due(10, keyboard);
        expect(player.cursor() == 3, "Player_AtT10_BothDueEventsApplied_CursorAdvancesByTwo");
        expect(!keyboard.key(2, 6), "Player_AtT10_AIsReleased");
        // "B" resolves to (row=2, column=7).
        expect(keyboard.key(2, 7), "Player_AtT10_BIsPressed");

        // T=49: nothing new due yet (next event is at T=50).
        player.apply_due(49, keyboard);
        expect(player.cursor() == 3, "Player_BeforeNextEvent_CursorUnchanged");

        // T=50: final event due.
        player.apply_due(50, keyboard);
        expect(!keyboard.key(2, 7), "Player_AtT50_BIsReleased");
        expect(player.cursor() == 4, "Player_AtFinalEventTstate_CursorReachesEventCount");
        expect(player.cursor() == player.event_count(), "Player_CursorReachesEventCount_AllEventsApplied");

        // Calling apply_due() again past the end is a safe no-op (never
        // re-applies, never throws, never over-advances).
        player.apply_due(999999, keyboard);
        expect(player.cursor() == 4, "Player_PastEnd_RemainsAtEventCount_NoOp");
    }

    // --- Case 5: KEY-only byte-identity REGRESSION GUARD. A script
    // with zero JOY= lines must parse to KEY events and serialize back
    // byte-for-byte identically to the output before the JOY verb existed --
    // the additive JOY verb must not perturb the KEY path in any way. ---
    {
        const std::string key_only =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "T=15 KEY=A DOWN\n"
            "T=30 KEY=A UP\n"
            "T=30 KEY=SPACE DOWN\n"
            "T=50 KEY=SPACE UP\n"
            "[END]\n";
        const std::vector<InputScriptEvent> events = parse_input_script(key_only);
        bool all_key = true;
        for (const InputScriptEvent& e : events) {
            if (e.kind != InputEventKind::Key) {
                all_key = false;
            }
        }
        expect(all_key, "KeyOnly_AllEventsAreKeyKind");
        expect(serialize_input_script(events) == key_only, "KeyOnly_SerializeByteIdenticalToPreM41");
    }

    // --- Case 6: JOY= parse/serialize round-trip, interleaved with
    // KEY= events (both line kinds coexist in one script). ---
    {
        const std::string text =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "T=0 JOY=1 LEFT DOWN\n"
            "T=5 KEY=SPACE DOWN\n"
            "T=10 JOY=1 LEFT UP\n"
            "T=10 JOY=2 A DOWN\n"
            "T=20 JOY=1 UP DOWN\n"
            "T=20 JOY=1 DOWN DOWN\n"
            "T=30 JOY=2 B UP\n"
            "[END]\n";
        const std::vector<InputScriptEvent> events = parse_input_script(text);
        expect(events.size() == 7, "JoyMixed_ParsesSevenEvents");
        expect(events[0].kind == InputEventKind::Joy && events[0].joy_port == 1 &&
                   events[0].joy_control == JoyControl::Left && events[0].pressed,
               "JoyMixed_Event0_LeftDown");
        expect(events[1].kind == InputEventKind::Key && events[1].key_name == "SPACE" && events[1].pressed,
               "JoyMixed_Event1_KeyStillParses");
        expect(events[3].kind == InputEventKind::Joy && events[3].joy_port == 2 &&
                   events[3].joy_control == JoyControl::TriggerA && events[3].pressed,
               "JoyMixed_Event3_Port2TriggerADown");
        // "JOY=1 DOWN DOWN" -- direction DOWN, press-state DOWN (the two DOWN
        // tokens are unambiguous by position).
        expect(events[5].kind == InputEventKind::Joy && events[5].joy_control == JoyControl::Down &&
                   events[5].pressed,
               "JoyMixed_Event5_DirectionDownPressed");
        expect(events[6].kind == InputEventKind::Joy && events[6].joy_control == JoyControl::TriggerB &&
                   !events[6].pressed,
               "JoyMixed_Event6_TriggerBReleased");

        const std::string serialized = serialize_input_script(events);
        expect(serialized == text, "JoyMixed_SerializeByteIdenticalToOriginal");
        const std::vector<InputScriptEvent> reparsed = parse_input_script(serialized);
        bool all_match = reparsed.size() == events.size();
        for (std::size_t i = 0; i < events.size() && all_match; ++i) {
            if (reparsed[i].at_tstate != events[i].at_tstate || reparsed[i].kind != events[i].kind ||
                reparsed[i].key_name != events[i].key_name || reparsed[i].pressed != events[i].pressed ||
                reparsed[i].joy_port != events[i].joy_port ||
                reparsed[i].joy_control != events[i].joy_control) {
                all_match = false;
            }
        }
        expect(all_match, "JoyMixed_Reparse_EventsByteIdentical");
    }

    // --- Case 7: every named malformed JOY class throws
    // std::runtime_error (mirrors Case 2's KEY discipline). ---
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 JOY=0 LEFT DOWN\n[END]\n"),
           "MalformedJoy_PortZero_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 JOY=3 LEFT DOWN\n[END]\n"),
           "MalformedJoy_PortThree_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 JOY=x LEFT DOWN\n[END]\n"),
           "MalformedJoy_NonNumericPort_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 JOY=1 DIAGONAL DOWN\n[END]\n"),
           "MalformedJoy_UnknownControl_Throws");
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 JOY=1 LEFT SIDEWAYS\n[END]\n"),
           "MalformedJoy_BadState_Throws");
    // A wholly-unknown second token (neither KEY= nor JOY=) still throws.
    expect(throws_runtime_error("HBF1XV-INPUT-SCRIPT v1\nT=0 PAD=1 LEFT DOWN\n[END]\n"),
           "MalformedJoy_UnknownLineKind_Throws");

    // --- Case 8: the player drives JoystickPorts for JOY= events,
    // accumulating into a per-port PortState (distinct controls coexist);
    // an unattached player treats JOY= as a cursor-advancing no-op. ---
    {
        const std::string text =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "T=0 JOY=1 LEFT DOWN\n"
            "T=0 JOY=1 A DOWN\n"
            "T=10 JOY=1 LEFT UP\n"
            "T=20 JOY=2 RIGHT DOWN\n"
            "[END]\n";
        const std::vector<InputScriptEvent> events = parse_input_script(text);

        // Unattached: JOY events are no-ops but the cursor still advances (no
        // KEY event would ever be skipped in a mixed script).
        {
            InputScriptPlayer player(events);
            KeyboardMatrix keyboard;
            keyboard.reset();
            player.apply_due(999, keyboard);
            expect(player.cursor() == player.event_count(),
                   "Player_NoJoystick_JoyEventsAdvanceCursorAsNoOp");
        }

        // Attached: LEFT+A both held at T=0 (accumulated into one PortState).
        {
            InputScriptPlayer player(events);
            JoystickPorts joy;
            joy.reset();
            player.attach_joystick(&joy);
            KeyboardMatrix keyboard;
            keyboard.reset();

            player.apply_due(0, keyboard);
            expect(joy.port(0).left && joy.port(0).trigger_a && !joy.port(0).up,
                   "Player_AtT0_Port1_LeftAndTriggerA_Accumulated");

            // T=10 releases LEFT but the held trigger A survives (shadow state).
            player.apply_due(10, keyboard);
            expect(!joy.port(0).left && joy.port(0).trigger_a,
                   "Player_AtT10_Port1_LeftReleased_TriggerAStillHeld");

            // T=20 sets port-2 RIGHT; port-1 state is independent.
            player.apply_due(20, keyboard);
            expect(joy.port(1).right && joy.port(0).trigger_a,
                   "Player_AtT20_Port2Right_Independent_Port1TriggerAHeld");
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_InputScript_Unit cases passed\n";
    return 0;
}
