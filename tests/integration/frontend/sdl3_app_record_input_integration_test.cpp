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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "frontend/sdl3_app.h"
#include "machine/input_script.h"

// Suite: Frontend_Sdl3AppRecordInput_Integration (DEC-0072 input recorder).
//
// End-to-end proof of --record-input through the REAL Sdl3App poll path:
// synthetic SDL key events (SDL_PushEvent) flow through the app's own
// SDL_PollEvent-based poll_and_dispatch_events(), the recorder writes them to an
// HBF1XV-INPUT-SCRIPT v1 file with frame-stamped cycles, an F11 disk swap is
// captured as a "# SWAP_DISK frame=<N>" comment, and the produced file is then
// fed BACK through a second Sdl3App via --input-script -- proving the recorded
// file is directly replayable and the recorded keys fire again (record->replay
// round-trip). Runs entirely under the dummy video/audio drivers (A-M26-2),
// only via run_one_frame() (never run_interactive(), A-M26-8).

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
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
}

SDL_Event make_key_event(const SDL_Scancode scancode, const bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = down;
    event.key.repeat = false;
    return event;
}

// Load a flat-RAM NOP...HALT driver AFTER init() (mirrors the sibling
// sdl3_app_input_script integration test): keeps the CPU idling harmlessly so
// each run_one_frame() advances a whole frame's cycles deterministically, with
// no unprogrammed-memory wandering. The keyboard matrix is untouched by this
// program (only the recorder / input-script player drive it).
void load_idle_program(sony_msx::frontend::Sdl3App& app) {
    app.machine().map_flat_ram();
    std::vector<std::uint8_t> program(2000, 0x00);
    program.push_back(0x76);  // HALT
    app.machine().load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
    app.machine().cpu().state().regs().pc = 0x0000;
}

// Push a key event into SDL's own queue, then pump the app several frames so the
// app's SDL_PollEvent drain consumes + records it (SDL's poll batching can defer
// an event by a batch; a handful of frames is comfortably enough). Recording
// happens at the START of the consuming frame, so the cycle stamp is that
// frame's start (frame-aligned).
void push_key_and_pump(sony_msx::frontend::Sdl3App& app, const SDL_Scancode scancode, const bool down) {
    SDL_Event event = make_key_event(scancode, down);
    SDL_PushEvent(&event);
    for (int i = 0; i < 8; ++i) {
        app.run_one_frame();
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-sdl3-record-input-integration-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root, ec);

    // Two dummy but structurally-valid 720 KB disk images so the F11 hot-swap
    // path (disk_images_.size() > 1) actually fires and gets recorded.
    const std::vector<std::uint8_t> disk_bytes = sony_msx::devices::fdc::DiskImage::synthesize();
    const std::filesystem::path disk1 = temp_root / "d1.dsk";
    const std::filesystem::path disk2 = temp_root / "d2.dsk";
    for (const auto& p : {disk1, disk2}) {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(disk_bytes.data()),
                  static_cast<std::streamsize>(disk_bytes.size()));
    }

    const std::filesystem::path recorded = temp_root / "recorded.script";

    // ================= RECORD =================
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = SONY_MSX_BIOS_DIR;
        config.hidden_window = true;
        config.record_input_path = recorded.string();
        config.disk_paths = {disk1.string(), disk2.string()};

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "Record_Init_WithRecordInput_Succeeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
            std::cerr << g_failures << " case(s) failed\n";
            return 1;
        }
        load_idle_program(app);

        // A realistic navigation burst: SPACE press/release, an F11 swap, then
        // RETURN press/release.
        push_key_and_pump(app, SDL_SCANCODE_SPACE, true);
        push_key_and_pump(app, SDL_SCANCODE_SPACE, false);

        // F11 hot-swap (fresh key-down). Consumed as a host hotkey (never a
        // matrix key), rotates the mounted disk, and is recorded as a comment.
        {
            SDL_Event f11 = make_key_event(SDL_SCANCODE_F11, true);
            SDL_PushEvent(&f11);
            for (int i = 0; i < 8; ++i) {
                app.run_one_frame();
            }
        }
        expect(app.current_disk_index() == 1, "Record_F11_AdvancedDiskIndex");

        push_key_and_pump(app, SDL_SCANCODE_RETURN, true);
        push_key_and_pump(app, SDL_SCANCODE_RETURN, false);

        app.shutdown();  // writes "[END]"
    }

    // The produced file re-parses cleanly (comment-skip) into exactly the four
    // recorded matrix key edges, in order, with monotonic frame-stamped cycles.
    const std::string bytes = read_file(recorded);
    std::vector<sony_msx::machine::InputScriptEvent> events;
    bool parsed = true;
    try {
        events = sony_msx::machine::parse_input_script(bytes);
    } catch (const std::exception& e) {
        parsed = false;
        std::cerr << "  parse threw: " << e.what() << "\n";
    }
    expect(parsed, "Recorded_File_ReParsesCleanly");
    expect(events.size() == 4, "Recorded_FourKeyEvents");
    if (events.size() == 4) {
        expect(events[0].key_name == "SPACE" && events[0].pressed, "Recorded_Event0_SpaceDown");
        expect(events[1].key_name == "SPACE" && !events[1].pressed, "Recorded_Event1_SpaceUp");
        expect(events[2].key_name == "RETURN" && events[2].pressed, "Recorded_Event2_ReturnDown");
        expect(events[3].key_name == "RETURN" && !events[3].pressed, "Recorded_Event3_ReturnUp");
        // Frame-stamped: the RETURN pair is on a strictly later frame than the
        // SPACE pair (proves the cycle stamps track emulation time, not a
        // constant) and every T is non-decreasing.
        expect(events[0].at_tstate <= events[1].at_tstate && events[1].at_tstate <= events[2].at_tstate &&
                   events[2].at_tstate <= events[3].at_tstate,
               "Recorded_Cycles_NonDecreasing");
        expect(events[2].at_tstate > events[0].at_tstate, "Recorded_ReturnLaterFrameThanSpace");
    }
    // The F11 swap was captured as a parser-skipped comment (the replay side
    // passes --swap-disk-frame <N> with this frame).
    expect(bytes.find("# SWAP_DISK frame=") != std::string::npos, "Recorded_SwapDiskCommentPresent");

    // ================= REPLAY (round-trip) =================
    // Feed the recorded file straight back through --input-script. init()
    // SUCCEEDING is itself the proof the recorded file (comment line included)
    // is directly --input-script-loadable; then we confirm the keys actually
    // fire again: SPACE gets pressed at some frame and both keys end released.
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = SONY_MSX_BIOS_DIR;
        config.hidden_window = true;
        config.input_script_path = recorded.string();

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "Replay_Init_WithRecordedFileAsInputScript_Succeeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
            std::cerr << g_failures << " case(s) failed\n";
            return 1;
        }
        load_idle_program(app);

        // Run comfortably past the last recorded cycle so every event applies.
        // "SPACE" -> (row 8, col 0); "RETURN" -> (row 7, col 7) per msx_key_names.
        bool space_was_pressed = false;
        for (int frame = 0; frame < 60; ++frame) {
            app.run_one_frame();
            if (app.machine().keyboard().key(8, 0)) {
                space_was_pressed = true;
            }
        }
        expect(space_was_pressed, "Replay_SpaceKeyPressedAtSomeFrame");
        expect(!app.machine().keyboard().key(8, 0), "Replay_FinalState_SpaceReleased");
        expect(!app.machine().keyboard().key(7, 7), "Replay_FinalState_ReturnReleased");

        app.shutdown();
    }

    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppRecordInput_Integration cases passed\n";
    return 0;
}
