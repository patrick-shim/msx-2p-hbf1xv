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

#include "devices/cartridge/cartridge_mapper_type.h"
#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"
#include "frontend/session_summary.h"

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
  found (or pass full paths). With NO options it boots to MSX-BASIC (the "Ok"
  prompt) as a READY-TO-GAME config: 512 KB RAM, fast-disk on, and an FM-PAC
  auto-loaded into slot 2 (for SRAM saves) if roms\fmpac.rom is present. Want the
  authentic bare 1988 machine instead? add --stock. -- try booting first.

GETTING STARTED  (copy a line, then change the paths to your files)

  Just boot to BASIC (convenience defaults):
    sony_msx_sdl3.exe

  The authentic bare HB-F1XV (64 KB, accurate disk timing, no FM-PAC):
    sony_msx_sdl3.exe --stock

  Play a cartridge game (the cartridge type is detected automatically):
    sony_msx_sdl3.exe --slot1 roms\aleste.rom

  Boot a floppy (MSX-DOS, or a disk game):
    sony_msx_sdl3.exe --disk disks\msxdos22.dsk

  A multi-disk game -- press F11 in the window to swap to the next disk:
    sony_msx_sdl3.exe --disk disks\game1.dsk --disk disks\game2.dsk

  Save your progress TO DISK (writes are stored back into the .dsk on exit):
    sony_msx_sdl3.exe --disk disks\rpg.dsk --disk-writable

  A game that saves TO SRAM -- the FM-PAC auto-loads into slot 2 by default, so
  a slot-1 game and the FM-PAC coexist (its save auto-persists):
    sony_msx_sdl3.exe --disk disks\ys2.dsk

  Bigger + sharper window (RAM is already 512 KB by default):
    sony_msx_sdl3.exe --slot1 roms\big.rom --scale 4 --filter nearest

OPTIONS

  What to load
    --bios-dir <path>       Folder holding the 7 Sony BIOS ROMs (default: bios).
    --disk <path>           Insert a floppy (.dsk). Repeatable -- the first one
                            is in the drive at boot; F11 cycles through the rest.
    --slot1 <path>          Cartridge in slot 1 (a .rom game, or the FM-PAC).
    --slot2 <path>          Cartridge in slot 2 (overrides the FM-PAC auto-load).
    --slot1-type <name>     Force the cartridge mapper instead of auto-detecting.
    --slot2-type <name>       Names: auto (default), KonamiSCC, Konami, ASCII8,
                              ASCII16, FMPAC. "auto" is correct for almost every
                              cartridge -- only set this if a game misdetects.
                              (--cart1/--cart2/--cartN-type are accepted as
                              silent aliases for the --slotN spellings.)
    --softwaredb <path>     Override the game database used for auto-detection.

  Machine
    --stock                 Boot the authentic bare 1988 HB-F1XV: 64 KB RAM,
                            accurate (non-fast) disk timing, no auto FM-PAC.
                            Individual flags (e.g. --ram 512) still override it.
    --ram <64|128|256|512>  Main RAM in KB. DEFAULT 512 (convenience); --ram 64
                            or --stock = the real HB-F1XV. 512 KB is the machine's
                            built-in ceiling (more would need a RAM cartridge).
    --no-fmpac              Do NOT auto-load the FM-PAC into slot 2 (SRAM saves
                            then need an explicit --slot1/--slot2 roms\fmpac.rom).

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
    --fast-disk             Near-instant disk loads (Alt+D toggles it live). ON
                            by default (convenience); may affect rare
                            copy-protected disks.
    --no-fast-disk          Use 100% accurate (slower) FDC timing instead (also
                            what --stock selects). Alt+D still toggles live.
    --fmpac-sram <path>     Where the FM-PAC's battery save is stored (default:
                            roms\fmpac.rom.sram for the auto-loaded FM-PAC, or
                            <cart>.rom.sram for an explicit one; auto-written).
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
    --record-input <path>   Record your live keystrokes + F11 disk swaps into a
                            replayable input-script (replay via --input-script;
                            each F11 swap prints a --swap-disk-frame <N> value).
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

// Describe a cartridge bay for the banner "Slot N" line, reading the RESOLVED
// machine state (planner §2.6): the mounted mapper type when loaded (with an
// "auto-loaded" tag for the M46 FM-PAC that has no user-supplied --slotN path),
// else "(empty)".
std::string describe_slot(const sony_msx::devices::cartridge::CartridgeSlot& slot,
                          const std::optional<std::string>& user_path,
                          const std::string& autoload_path_if_fmpac) {
    if (!slot.loaded()) {
        return "(empty)";
    }
    const auto type = slot.mapper_type();
    const std::string type_name(sony_msx::devices::cartridge::to_string(type));
    if (user_path.has_value()) {
        return *user_path + "  (" + type_name + ")";
    }
    // No user path but the bay is loaded -> the M46 FM-PAC slot-2 auto-load.
    return autoload_path_if_fmpac + "  (" + type_name + ", auto-loaded)";
}

// One-time human-readable startup summary: what the machine is, what was
// resolved/loaded for this session (from the RESOLVED machine + config state),
// and the runtime hotkeys. Printed after a successful init(). M46 (DEC-0071)
// enriches it with the machine-mode tag, VRAM, FM-PAC status, SRAM availability,
// resolved slot contents, and fast-disk -- the deterministic label text comes
// from the pure frontend/session_summary.h helpers (AC-14 ctest seam).
void print_startup_summary(const sony_msx::frontend::Sdl3App& app,
                           const sony_msx::frontend::Sdl3AppConfig& config,
                           const sony_msx::frontend::ResolvedSessionDefaults& resolved,
                           const bool no_fmpac) {
    namespace fe = sony_msx::frontend;
    const sony_msx::machine::Hbf1xvMachine& machine = app.machine();

    std::cerr <<
        "\n"
        "======================================================================\n"
        "  Sony HB-F1XV  -  MSX2+ emulator                        "
              << fe::format_mode_tag(resolved.is_stock) << "\n"
        "  Z80A @ 3.58 MHz | Yamaha V9958 VDP (128 KB VRAM)\n"
        "  Audio: PSG (YM2149) + MSX-MUSIC FM (YM2413) + Konami SCC\n"
        "  Storage: WD2793 FDC, 720 KB 3.5\" floppy\n"
        "----------------------------------------------------------------------\n"
        "  This session:\n";

    // Main RAM (stock/modded label) + fixed 128 KB V9958 VRAM.
    std::cerr << "    Main RAM   : " << fe::format_ram_line(machine.dram_size()) << "\n";
    std::cerr << "    VRAM       : 128 KB\n";
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

    // Slots (renamed from "Cartridge 1/2"): resolved bay contents.
    std::cerr << "    Slot 1     : "
              << describe_slot(machine.cartridge_slot1(), config.cart1_path,
                               config.fmpac_autoload_rom_path)
              << "\n";
    std::cerr << "    Slot 2     : "
              << describe_slot(machine.cartridge_slot2(), config.cart2_path,
                               config.fmpac_autoload_rom_path)
              << "\n";

    // FM-PAC status + SRAM availability (DEC-0050). loaded_slot is authoritative.
    const int fmpac_slot = machine.fmpac(1) != nullptr ? 1 : (machine.fmpac(2) != nullptr ? 2 : 0);
    fe::FmPacBannerInfo fmpac_info;
    fmpac_info.loaded_slot = fmpac_slot;
    fmpac_info.outcome = app.fmpac_autoload_outcome();
    fmpac_info.is_stock = resolved.is_stock;
    fmpac_info.no_fmpac = no_fmpac;
    fmpac_info.autoload_rom_path = config.fmpac_autoload_rom_path;
    std::cerr << "    FM-PAC     : " << fe::format_fmpac_line(fmpac_info) << "\n";

    const bool sram_available = fmpac_slot != 0 && !machine.fmpac_sram_path().empty();
    std::string sram_path_shown;
    if (sram_available) {
        std::error_code ec;
        const std::filesystem::path abs = std::filesystem::absolute(machine.fmpac_sram_path(), ec);
        sram_path_shown = ec ? machine.fmpac_sram_path().string() : abs.lexically_normal().string();
    }
    std::cerr << "    SRAM       : " << fe::format_sram_line(sram_available, sram_path_shown) << "\n";

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
    std::cerr << "    Fast-disk  : "
              << (config.fast_disk ? "ON (near-instant loads; Alt+D toggles)\n"
                                   : "OFF (accurate timing; Alt+D toggles)\n");
    std::cerr << "    Speed      : " << config.speed_level.value_or(0)
              << (config.speed_level.value_or(0) == 0 ? " (full speed)\n" : "\n");
    std::cerr << "    Video      : " << config.window_width << "x" << config.window_height
              << (config.fullscreen ? ", fullscreen" : "")
              << (config.border_enabled ? ", framed border" : ", edge-to-edge (Sony)")
              << (config.texture_filter == SDL_SCALEMODE_NEAREST ? ", nearest\n" : ", linear\n");

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
    config.dump_state_filename = parsed.dump_state_filename;
    config.trace_cpu_filename = parsed.trace_cpu_filename;
    config.event_log_filename = parsed.event_log_filename;
    config.input_script_path = parsed.input_script_path;
    config.record_input_path = parsed.record_input_path;  // DEC-0072 input recorder
    config.snapshot_dir = parsed.snapshot_dir;  // M36 Phase 3: --snapshot <dir>
    config.swap_disk_frame = parsed.swap_disk_frame;  // DEC-0072 scripted disk swap
    config.fingerprint_path = parsed.fingerprint_path;  // DEC-0072 per-frame CPU fingerprint
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
    // M46 (DEC-0071): resolve the flipped convenience-vs-stock defaults in the
    // CLI layer (the anti-drift seam, planner §2.7 -- the Sdl3AppConfig struct
    // defaults ABOVE stay stock; this is the ONLY place they flip to convenience).
    // Empty CLI -> {512 KB RAM, fast-disk ON, FM-PAC slot-2 auto-load ON};
    // `--stock`/`--ram 64`/`--no-fast-disk`/`--no-fmpac` peel it back. --ram is
    // the explicit per-field override (order-independent, wins over --stock).
    const sony_msx::frontend::ResolvedSessionDefaults resolved =
        sony_msx::frontend::resolve_session_defaults(
            {parsed.ram_kb, parsed.fast_disk_opt, parsed.no_fmpac, parsed.stock});
    config.ram_bytes = resolved.ram_bytes;
    config.fast_disk = resolved.fast_disk;          // convenience default ON (Alt+D toggles live)
    config.fmpac_autoload = resolved.fmpac_autoload; // FM-PAC slot-2 auto-load (skips gracefully)
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

    print_startup_summary(app, config, resolved, parsed.no_fmpac);

    const int rc = app.run_interactive();
    app.shutdown();
    return rc;
}
