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

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"

namespace {

void print_usage(const char* argv0) {
    std::cout <<
        "Sony HB-F1XV  -  a Sony HitBit F1XV (MSX2+, 1988) home-computer emulator.\n"
        "Opens a window and runs the real machine at true speed: Z80A @ 3.58 MHz,\n"
        "Yamaha V9958 video (128 KB VRAM), PSG + MSX-MUSIC FM + Konami SCC sound,\n"
        "a 720 KB floppy drive, keyboard and joystick.\n"
        "\n"
        "USAGE\n"
        "  " << argv0 << " [options]\n"
        << R"HELP(
  Tip: run it from the project root folder so that bios\, roms\ and disks\ are
  found (or pass full paths). With NO options it boots the Sony BIOS straight to
  MSX-BASIC (the "Ok" prompt) -- try that first.

GETTING STARTED  (copy a line, then change the paths to your files)

  Just boot to BASIC:
    sony_msx_sdl3.exe

  Play a cartridge game (the cartridge type is detected automatically):
    sony_msx_sdl3.exe --cart1 roms\aleste.rom

  Boot a floppy (MSX-DOS, or a disk game):
    sony_msx_sdl3.exe --disk disks\msxdos22.dsk

  A multi-disk game -- press F11 in the window to swap to the next disk:
    sony_msx_sdl3.exe --disk disks\game1.dsk --disk disks\game2.dsk

  Save your progress TO DISK (writes are stored back into the .dsk on exit):
    sony_msx_sdl3.exe --disk disks\rpg.dsk --disk-writable

  A game that saves TO SRAM -- add an FM-PAC cartridge (its save auto-persists):
    sony_msx_sdl3.exe --disk disks\ys2.dsk --cart1 roms\fmpac.rom

  Bigger + sharper window, and extra RAM for a large game:
    sony_msx_sdl3.exe --cart1 roms\big.rom --scale 4 --filter nearest --ram 512

OPTIONS

  What to load
    --bios-dir <path>       Folder holding the 7 Sony BIOS ROMs (default: bios).
    --disk <path>           Insert a floppy (.dsk). Repeatable -- the first one
                            is in the drive at boot; F11 cycles through the rest.
    --cart1 <path>          Cartridge in slot 1 (a .rom game, or the FM-PAC).
    --cart2 <path>          Cartridge in slot 2.
    --cart1-type <name>     Force the cartridge mapper instead of auto-detecting.
    --cart2-type <name>       Names: auto (default), KonamiSCC, Konami, ASCII8,
                              ASCII16, FMPAC. "auto" is correct for almost every
                              cartridge -- only set this if a game misdetects.
    --softwaredb <path>     Override the game database used for auto-detection.

  Memory
    --ram <64|128|256|512>  Main RAM in KB. Default 64 = the real HB-F1XV.
                            128/256/512 are opt-in extras for large games; 512 KB
                            is the machine's built-in ceiling (more would need a
                            RAM-expansion cartridge).

  Screen
    --scale <1..8>          Window size = 320N x 240N (default 3 = 960x720). The
                            window is resizable and stays 4:3 at any size.
    --filter nearest|linear linear = smooth (default); nearest = sharp pixels.
    --fullscreen            Start in fullscreen (Alt+Enter toggles it live).
    --border / --no-border  Framed 4:3 canvas, or bare edge-to-edge (the default,
                            the Sony-original look).

  Saving & disks
    --disk-writable         Save in-game disk writes back to the .dsk file (on a
                            clean exit, or an F11 swap). Without this, disks are
                            READ-ONLY and any writes are discarded when you quit.
    --fast-disk             Near-instant disk loads (Alt+D toggles it live). Off
                            by default (accurate timing); may affect rare
                            copy-protected disks.
    --fmpac-sram <path>     Where an inserted FM-PAC's battery save is stored
                            (default: <cart>.rom.sram, written automatically).
    --no-fmpac-sram         Keep FM-PAC SRAM in memory only (no save file).

  Speed
    --speed <0..7>          Sony Speed Controller: a CPU SLOW-DOWN aid, NOT a
                            turbo. 0 = full speed (default); 7 = maximum slow
                            motion. F6/F7 also change it live.

  Advanced / debugging  (optional -- you can ignore all of these to start)
    --capture on|off        Arm the F10 live-capture hotkey (default off = F10
                            does nothing). --stream-light selects the light mode.
    --snapshot <dir>        Where F12 writes a debug snapshot (default: debug).
    --dump-state <name>     Write a final CPU + memory state dump.
    --trace-cpu <name>      Write a CPU instruction trace.
    --event-log <name>      Write a deterministic event log.
    --input-script <path>   Replay a scripted keyboard/joystick sequence.
    --max-frames <N>        Auto-quit after N frames (for non-interactive runs).
    --hidden-window         Run without showing a window (testing/automation).
    -h, --help              Show this help and exit.

IN-WINDOW HOTKEYS
    F11        swap to the next disk        F12    write a debug snapshot
    F6 / F7    Speed Controller slow -/+    F8/F9  Ren-Sha auto-fire slow -/+
    Alt+Enter  toggle fullscreen            Alt+D  toggle fast-disk
    PAUSE      hardware PAUSE button        F10    live capture (needs --capture on)

GOOD TO KNOW
    - Paths are relative to the folder you run from -- launch from the project
      root, or use full paths (e.g. --bios-dir C:\msx\bios).
    - Game saves: to SRAM -> add the FM-PAC cartridge; to disk -> --disk-writable.
    - A save disk must be a FORMATTED .dsk. Create one with
      tools\format-blank-disk.ps1 -- an all-zero blank has no filesystem and a
      game cannot write to it.
    - Quit by closing the window normally (do not force-kill / Ctrl+C) so disk
      and SRAM saves are flushed to their files.

Audio (PSG + Konami SCC + MSX-MUSIC/YM2413 FM) is live; video is the decoded
V9958 output; the machine runs at throttled real-time wall-clock pacing.
)HELP";
}

// One-time human-readable startup summary: what the machine is, what was
// loaded for this session (from the resolved config), and the runtime hotkeys.
// Printed after a successful init() so the user immediately sees their
// configuration and controls -- replaces the old internal-jargon banner.
void print_startup_summary(const sony_msx::frontend::Sdl3AppConfig& config) {
    std::cerr <<
        "\n"
        "======================================================================\n"
        "  Sony HB-F1XV  -  MSX2+ emulator\n"
        "  Z80A @ 3.58 MHz | Yamaha V9958 VDP (128 KB VRAM)\n"
        "  Audio: PSG (YM2149) + MSX-MUSIC FM (YM2413) + Konami SCC\n"
        "  Storage: WD2793 FDC, 720 KB 3.5\" floppy\n"
        "----------------------------------------------------------------------\n"
        "  This session:\n";

    std::cerr << "    Main RAM   : " << (config.ram_bytes / 1024) << " KB"
              << (config.ram_bytes == 64u * 1024u ? "  (stock)\n" : "  (opt-in --ram)\n");
    // BIOS: resolve to an absolute path and list the ROM files actually present,
    // so the user sees exactly where (and what) was loaded regardless of the CWD.
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path bios_path(config.bios_dir);
        const fs::path abs = fs::absolute(bios_path, ec);
        const std::string shown = ec ? config.bios_dir : abs.lexically_normal().string();

        std::vector<std::string> roms;
        for (fs::directory_iterator it(bios_path, ec), end; !ec && it != end; it.increment(ec)) {
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".rom") {
                roms.push_back(it->path().filename().string());
            }
        }
        std::sort(roms.begin(), roms.end());

        std::cerr << "    BIOS       : " << shown << "  (" << roms.size() << " ROM"
                  << (roms.size() == 1 ? "" : "s") << ")\n";
        for (std::size_t i = 0; i < roms.size(); ++i) {
            std::cerr << (i % 3 == 0 ? "                 " : "") << roms[i]
                      << ((i % 3 == 2 || i + 1 == roms.size()) ? "\n" : "  ");
        }
    }

    if (config.disk_paths.empty()) {
        std::cerr << "    Disk       : (none inserted)\n";
    } else {
        std::cerr << "    Disk A     : " << config.disk_paths.front();
        if (config.disk_paths.size() > 1) {
            std::cerr << "  (+" << (config.disk_paths.size() - 1) << " more; F11 cycles)";
        }
        std::cerr << (config.disk_writable ? "  [writable -- saves persist]\n"
                                           : "  [read-only -- add --disk-writable to save]\n");
    }
    if (config.cart1_path.has_value()) {
        std::cerr << "    Cartridge 1: " << *config.cart1_path << "\n";
    }
    if (config.cart2_path.has_value()) {
        std::cerr << "    Cartridge 2: " << *config.cart2_path << "\n";
    }
    std::cerr << "    Video      : " << config.window_width << "x" << config.window_height
              << (config.fullscreen ? ", fullscreen" : "")
              << (config.border_enabled ? ", framed border" : ", edge-to-edge (Sony)")
              << (config.texture_filter == SDL_SCALEMODE_NEAREST ? ", nearest\n" : ", linear\n");
    if (config.fast_disk) {
        std::cerr << "    Fast-disk  : ON (near-instant loads; Alt+D toggles)\n";
    }

    std::cerr <<
        "----------------------------------------------------------------------\n"
        "  Hotkeys:\n"
        "    F11  swap disk            F12  debug snapshot\n"
        "    F6 / F7   speed slow -/+  (Speed Controller: a slow-motion aid)\n"
        "    F8 / F9   Ren-Sha auto-fire -/+        PAUSE  hardware pause\n"
        "    Alt+Enter  toggle fullscreen           Alt+D  toggle fast-disk\n";
    std::cerr << "    F10  live capture "
              << (config.capture_enabled ? "(armed)\n" : "(disarmed; launch with --capture on)\n");
    std::cerr <<
        "  Run with --help for all launch options.\n"
        "======================================================================\n\n";
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
    config.fast_disk = parsed.fast_disk;          // opt-in FDC turbo (Alt+D toggles live)
    config.dump_state_filename = parsed.dump_state_filename;
    config.trace_cpu_filename = parsed.trace_cpu_filename;
    config.event_log_filename = parsed.event_log_filename;
    config.input_script_path = parsed.input_script_path;
    config.snapshot_dir = parsed.snapshot_dir;  // M36 Phase 3: --snapshot <dir>
    config.stream_light = parsed.stream_light;  // DEC-0052: F10 arms lightweight mode
    // M37 Slice D (DEC-0056): --speed <0..7> launch-time initial Speed
    // Controller level (std::nullopt -> untouched, level 0/full speed).
    config.speed_level = parsed.speed_level;
    // M37 Slice E (DEC-0056): --fullscreen / --filter / --scale window scaling.
    config.fullscreen = parsed.fullscreen;
    // M37 Slice F: --capture <on|off> gates the F10 stream-capture hotkey
    // (default off = F10 inert). No effect on gameplay when off.
    config.capture_enabled = parsed.capture_enabled;
    config.texture_filter = (parsed.filter == sony_msx::frontend::TextureFilter::Nearest)
                                ? SDL_SCALEMODE_NEAREST
                                : SDL_SCALEMODE_LINEAR;
    // M42 (DEC-0061): --ram <64|128|256|512> main-RAM size. Absent -> keep the
    // Sdl3AppConfig default (stock 64 KB). Present -> KB * 1024 bytes.
    if (parsed.ram_kb.has_value()) {
        config.ram_bytes = static_cast<std::size_t>(*parsed.ram_kb) * 1024u;
    }
    // --scale N -> 320N x 240N window; absent keeps the default 960x720 (= scale 3,
    // M37 Slice F / DEC-0057; the default lives in Sdl3AppConfig).
    if (parsed.scale.has_value()) {
        config.window_width = 320 * *parsed.scale;
        config.window_height = 240 * *parsed.scale;
    }
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

    // Copy (not move) config into the app so it stays valid for the startup
    // summary printed after a successful init(); a one-time startup copy is
    // negligible.
    sony_msx::frontend::Sdl3App app(config);
    if (!app.init()) {
        std::cerr << "sdl3: initialization failed: " << app.last_error() << "\n";
        return 1;
    }

    print_startup_summary(config);

    const int rc = app.run_interactive();
    app.shutdown();
    return rc;
}
