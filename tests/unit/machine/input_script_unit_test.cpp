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
#include "peripherals/keyboard_matrix.h"

// Suite: Machine_InputScript_Unit (M27-S6, "Production Hardening +
// Debug/Test Tooling" item 3, docs/m27-planner-package.md §2.4).
//
// Proves the deterministic timed key-event script format's parser/
// serializer round-trips exactly, rejects EVERY named malformed-input class
// (never silently returns a partial/garbage result, mirrors
// frame_dump.h:44-45's discipline), and that InputScriptPlayer's monotonic
// cursor applies every due event EXACTLY once, in order, never skipped,
// never re-applied -- even across multiple apply_due() calls with the same
// or a stale current_tstate.

namespace {

using sony_msx::machine::InputScriptEvent;
using sony_msx::machine::InputScriptPlayer;
using sony_msx::machine::parse_input_script;
using sony_msx::machine::serialize_input_script;
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

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_InputScript_Unit cases passed\n";
    return 0;
}
