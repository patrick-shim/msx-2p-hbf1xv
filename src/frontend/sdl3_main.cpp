#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"

namespace {

void print_usage(const char* argv0) {
    std::cout << "usage: " << argv0
              << " [--bios-dir <path>] [--disk <path>] [--cart1 <path>] [--cart1-type <name>]"
                 " [--cart2 <path>] [--cart2-type <name>] [--max-frames <N>] [--hidden-window]"
                 " [--dump-state <name>] [--trace-cpu <name>] [--event-log <name>]"
                 " [--input-script <path>]\n"
                 "\n"
                 "Sony HB-F1XV MSX2+ emulator -- SDL3 frontend (backlog C9). Opens a real\n"
                 "window, drives the machine core at real (throttled) wall-clock pacing,\n"
                 "presents decoded V9958 video, plays real PSG (YM2149) audio, and reads\n"
                 "keyboard/joystick input.\n"
                 "\n"
                 "NOTE: YM2413 (OPLL/FM-PAC) channels are intentionally SILENT in the audio\n"
                 "mix -- this device has full register-file/channel/rhythm-decode fidelity\n"
                 "(M17, backlog B3) but ZERO waveform-synthesis capability (backlog E1,\n"
                 "still open). This is a disclosed, tracked scope boundary, not a bug.\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    for (const std::string& arg : args) {
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    const sony_msx::frontend::ParsedSdl3Cli parsed = sony_msx::frontend::parse_sdl3_cli(args);
    for (const std::string& err : parsed.errors) {
        std::cerr << "sdl3: " << err << "\n";
    }
    if (!parsed.errors.empty()) {
        return 2;
    }

    sony_msx::frontend::Sdl3AppConfig config;
    if (parsed.bios_dir.has_value()) {
        config.bios_dir = *parsed.bios_dir;
    }
    config.disk_path = parsed.disk_path;
    config.max_frames = parsed.max_frames;
    config.hidden_window = parsed.hidden_window;
    config.dump_state_filename = parsed.dump_state_filename;
    config.trace_cpu_filename = parsed.trace_cpu_filename;
    config.event_log_filename = parsed.event_log_filename;
    config.input_script_path = parsed.input_script_path;
    if (parsed.cartridges.slot1.path.has_value()) {
        config.cart1_path = parsed.cartridges.slot1.path;
        config.cart1_type = parsed.cartridges.slot1.type;
    }
    if (parsed.cartridges.slot2.path.has_value()) {
        config.cart2_path = parsed.cartridges.slot2.path;
        config.cart2_type = parsed.cartridges.slot2.type;
    }

    sony_msx::frontend::Sdl3App app(std::move(config));
    if (!app.init()) {
        std::cerr << "sdl3: initialization failed: " << app.last_error() << "\n";
        return 1;
    }

    std::cerr << "sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop"
                 " (PSG audio live; YM2413/FM-PAC intentionally silent -- backlog E1, still open)\n";

    const int rc = app.run_interactive();
    app.shutdown();
    return rc;
}
