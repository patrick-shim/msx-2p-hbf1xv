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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "machine/input_script.h"

// Suite: Machine_InputRecorder_Unit (DEC-0072 input-recorder tooling).
//
// Proves the write-side counterpart to parse_input_script()/InputScriptPlayer:
//   * serialize_input_script_line() is the single-source line format, and
//     serialize_input_script() is exactly (tag + per-line + '\n' + "[END]").
//   * InputScriptRecorder streams a byte-for-byte HBF1XV-INPUT-SCRIPT v1 file
//     that re-parses cleanly (the record->replay round-trip), with F11 swaps
//     captured as parser-skipped "# SWAP_DISK frame=<N>" comment lines.
//   * parse_input_script() SKIPS '#'-comment lines (so a recorded file with
//     swap markers is directly --input-script-loadable).
//   * a closed/never-opened recorder's record_*() calls are safe no-ops, and a
//     failed open() returns false.

namespace {

using sony_msx::machine::InputScriptEvent;
using sony_msx::machine::InputScriptRecorder;
using sony_msx::machine::InputEventKind;
using sony_msx::machine::JoyControl;
using sony_msx::machine::parse_input_script;
using sony_msx::machine::serialize_input_script;
using sony_msx::machine::serialize_input_script_line;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Strip the '#'-comment lines from a script's text (the deterministic oracle for
// "the event portion is byte-identical to serialize_input_script()").
std::string strip_comment_lines(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::string out;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] == '#') {
            continue;
        }
        out += line;
        out.push_back('\n');
    }
    return out;
}

}  // namespace

int main() {
    // --- Case 1: serialize_input_script_line() single-source byte format. ---
    {
        InputScriptEvent key_event;
        key_event.kind = InputEventKind::Key;
        key_event.at_tstate = 43012800;
        key_event.key_name = "SPACE";
        key_event.pressed = true;
        expect(serialize_input_script_line(key_event) == "T=43012800 KEY=SPACE DOWN",
               "Line_KeyDown_ExactBytes");
        key_event.pressed = false;
        expect(serialize_input_script_line(key_event) == "T=43012800 KEY=SPACE UP",
               "Line_KeyUp_ExactBytes");

        InputScriptEvent joy_event;
        joy_event.kind = InputEventKind::Joy;
        joy_event.at_tstate = 100;
        joy_event.joy_port = 2;
        joy_event.joy_control = JoyControl::TriggerA;
        joy_event.pressed = true;
        expect(serialize_input_script_line(joy_event) == "T=100 JOY=2 A DOWN", "Line_JoyDown_ExactBytes");

        // serialize_input_script() == tag + <line>\n... + "[END]\n" (proves the
        // whole-file serializer is built from the same single-source line fn).
        const std::vector<InputScriptEvent> events = {key_event};
        expect(serialize_input_script(events) ==
                   std::string("HBF1XV-INPUT-SCRIPT v1\n") + serialize_input_script_line(key_event) + "\n[END]\n",
               "WholeFile_BuiltFromLineFn");
    }

    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-input-recorder-unit-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root, ec);

    // --- Case 2: recorder streams a byte-exact, re-parseable file with an
    //     interleaved SWAP_DISK comment (the record->replay round-trip). ---
    {
        const std::filesystem::path script = temp_root / "session.script";
        {
            InputScriptRecorder rec;
            expect(rec.open(script.string()), "Recorder_Open_Succeeds");
            expect(rec.is_open(), "Recorder_IsOpenAfterOpen");
            // A realistic edge sequence: SPACE press/release, then an F11 swap at
            // frame 900, then RETURN press/release -- monotonic T (frame-stamped).
            rec.record_key(43012800, "SPACE", true);
            rec.record_key(43251760, "SPACE", false);
            rec.record_disk_swap(900);
            rec.record_key(66908800, "RETURN", true);
            rec.record_key(67147760, "RETURN", false);
            expect(rec.key_event_count() == 4, "Recorder_CountsFourKeyEvents");
            expect(rec.disk_swap_count() == 1, "Recorder_CountsOneSwap");
            rec.close();
            expect(!rec.is_open(), "Recorder_IsClosedAfterClose");
        }

        const std::string bytes = read_file(script);
        const std::string expected =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "T=43012800 KEY=SPACE DOWN\n"
            "T=43251760 KEY=SPACE UP\n"
            "# SWAP_DISK frame=900\n"
            "T=66908800 KEY=RETURN DOWN\n"
            "T=67147760 KEY=RETURN UP\n"
            "[END]\n";
        expect(bytes == expected, "Recorder_FileBytes_ExactMatch");

        // The recorded file re-parses cleanly (the comment is SKIPPED, not an
        // error) and yields exactly the four recorded key events, in order.
        const std::vector<InputScriptEvent> events = parse_input_script(bytes);
        expect(events.size() == 4, "Recorder_Reparse_FourKeyEvents");
        expect(events[0].at_tstate == 43012800 && events[0].key_name == "SPACE" && events[0].pressed,
               "Recorder_Reparse_Event0");
        expect(events[3].at_tstate == 67147760 && events[3].key_name == "RETURN" && !events[3].pressed,
               "Recorder_Reparse_Event3");

        // The EVENT portion (comments stripped) is byte-identical to
        // serialize_input_script(parse(...)) -- the byte-for-byte round-trip.
        expect(strip_comment_lines(bytes) == serialize_input_script(events),
               "Recorder_EventPortion_ByteIdenticalToSerialize");
    }

    // --- Case 3: parse_input_script() skips '#'-comment lines anywhere (a
    //     hand-written script with a leading + interior comment parses fine). ---
    {
        const std::string text =
            "HBF1XV-INPUT-SCRIPT v1\n"
            "# a leading human comment\n"
            "T=10 KEY=A DOWN\n"
            "# SWAP_DISK frame=5\n"
            "T=20 KEY=A UP\n"
            "[END]\n";
        const std::vector<InputScriptEvent> events = parse_input_script(text);
        expect(events.size() == 2, "CommentSkip_TwoEventsOnly");
        expect(events[0].key_name == "A" && events[0].pressed && events[0].at_tstate == 10,
               "CommentSkip_Event0Correct");
        expect(events[1].key_name == "A" && !events[1].pressed && events[1].at_tstate == 20,
               "CommentSkip_Event1Correct");
    }

    // --- Case 4: a never-opened recorder's record_*() calls are safe no-ops. ---
    {
        InputScriptRecorder rec;
        expect(!rec.is_open(), "ClosedRecorder_NotOpen");
        rec.record_key(1, "A", true);       // no-op, must not crash
        rec.record_disk_swap(0);            // no-op, must not crash
        rec.close();                        // no-op on a never-opened recorder
        expect(rec.key_event_count() == 0 && rec.disk_swap_count() == 0,
               "ClosedRecorder_NoOpCounts");
    }

    // --- Case 5: open() into a nonexistent directory fails (returns false,
    //     stays closed) -- never a silent partial state. ---
    {
        InputScriptRecorder rec;
        const std::filesystem::path bad = temp_root / "no_such_dir" / "x.script";
        expect(!rec.open(bad.string()), "Open_BadPath_ReturnsFalse");
        expect(!rec.is_open(), "Open_BadPath_StaysClosed");
    }

    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_InputRecorder_Unit cases passed\n";
    return 0;
}
