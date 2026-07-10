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
#include <string>
#include <utility>
#include <vector>

#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"

namespace {

void print_usage(const char* argv0) {
    std::cout << "usage: " << argv0
              << " [--bios-dir <path>] [--disk <path>] [--cart1 <path>] [--cart1-type <name>|auto]"
                 " [--cart2 <path>] [--cart2-type <name>|auto] [--softwaredb <path>]"
                 " [--max-frames <N>] [--hidden-window]"
                 " [--border] [--dump-state <name>] [--trace-cpu <name>] [--event-log <name>]"
                 " [--input-script <path>] [--snapshot <dir>]"
                 " [--fmpac-sram <path>] [--no-fmpac-sram]\n"
                 "\n"
                 "--snapshot <dir> sets the debug-snapshot output root (default debug/); press\n"
                 "F12 in-session to write a comprehensive per-component snapshot to\n"
                 "<dir>/snapshot/<id>/ (F12 is always active; read-only, never perturbs the run).\n"
                 "\n"
                 "FM-PAC battery SRAM persists AUTOMATICALLY when an inserted cartridge is an\n"
                 "FM-PAC: the save file defaults to <cart-rom-path>.sram (e.g. roms/fmpac.rom ->\n"
                 "roms/fmpac.rom.sram), loaded on insert and flushed on exit -- matching a real\n"
                 "FM-PAC's battery-backed SRAM. --fmpac-sram <path> overrides the default path;\n"
                 "--no-fmpac-sram keeps the SRAM in-memory-only (no host file). All three are\n"
                 "no-ops when no FM-PAC cartridge is inserted.\n"
                 "\n"
                 "--cartN-type is optional (M30): when omitted (or 'auto') the mapper type is\n"
                 "auto-identified -- softwaredb SHA1 match first, then a bank-write heuristic.\n"
                 "--softwaredb overrides the default DB path\n"
                 "(references/openmsx-21.0/share/softwaredb.xml, relative to the working dir).\n"
                 "\n"
                 "Sony HB-F1XV MSX2+ emulator -- SDL3 frontend (backlog C9). Opens a real\n"
                 "window, drives the machine core at real (throttled) wall-clock pacing,\n"
                 "presents decoded V9958 video, plays real PSG (YM2149) audio, and reads\n"
                 "keyboard/joystick input.\n"
                 "\n"
                 "Audio mix: PSG (YM2149) + Konami SCC + YM2413 (OPLL/MSX-MUSIC) FM synthesis\n"
                 "are all live (backlog E1 closed by M31; mix calibration M32/DEC-0040).\n";
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
    config.disk_paths = parsed.disk_paths;  // M35-S1: repeatable --disk list
    config.max_frames = parsed.max_frames;
    config.hidden_window = parsed.hidden_window;
    config.border_enabled = parsed.border_enabled;
    config.disk_writable = parsed.disk_writable;  // M36-S-c
    config.dump_state_filename = parsed.dump_state_filename;
    config.trace_cpu_filename = parsed.trace_cpu_filename;
    config.event_log_filename = parsed.event_log_filename;
    config.input_script_path = parsed.input_script_path;
    config.snapshot_dir = parsed.snapshot_dir;  // M36 Phase 3: --snapshot <dir>
    config.stream_light = parsed.stream_light;  // DEC-0052: F10 arms lightweight mode
    // M36 FM-PAC SRAM persistence: override/opt-out of the auto-derived
    // <fmpac-cart>.rom.sram default (default persistence is automatic).
    config.fmpac_sram_path = parsed.fmpac_sram_path;
    config.fmpac_sram_disabled = parsed.fmpac_sram_disabled;
    // M30 (backlog G2): carry the parser's type_was_explicit through so a
    // type-less --cartN triggers auto-identification inside
    // load_configured_assets() (the ONE shared resolver); an explicit
    // --cartN-type keeps byte-for-byte pre-M30 behavior.
    config.softwaredb_path = parsed.cartridges.softwaredb_path;
    if (parsed.cartridges.slot1.path.has_value()) {
        config.cart1_path = parsed.cartridges.slot1.path;
        config.cart1_type = parsed.cartridges.slot1.type;
        config.cart1_type_explicit = parsed.cartridges.slot1.type_was_explicit;
    }
    if (parsed.cartridges.slot2.path.has_value()) {
        config.cart2_path = parsed.cartridges.slot2.path;
        config.cart2_type = parsed.cartridges.slot2.type;
        config.cart2_type_explicit = parsed.cartridges.slot2.type_was_explicit;
    }

    sony_msx::frontend::Sdl3App app(std::move(config));
    if (!app.init()) {
        std::cerr << "sdl3: initialization failed: " << app.last_error() << "\n";
        return 1;
    }

    std::cerr << "sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop"
                 " (PSG + SCC + YM2413 FM audio live -- backlog E1 closed by M31)\n";

    const int rc = app.run_interactive();
    app.shutdown();
    return rc;
}
