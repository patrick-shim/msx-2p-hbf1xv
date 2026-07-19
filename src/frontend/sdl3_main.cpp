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
#include "frontend/config_paths.h"  // DEC-0097: project-root discovery + asset-path resolution
#include "frontend/config_runtime.h"
#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"
#include "frontend/session_summary.h"
#include "machine/emulator_config.h"

namespace {

// M50-S2 (DEC-0077): the directory holding this executable, for the config
// auto-load search order (<exe-dir>/sony_msx_hbf1xv.xml first, then <cwd>). A
// best-effort resolution of argv[0]; an empty result simply drops the exe-dir
// candidate (the CWD candidate still applies).
std::string exe_directory(const char* argv0) {
    if (argv0 == nullptr) {
        return std::string();
    }
    std::error_code ec;
    const std::filesystem::path self(argv0);
    const std::filesystem::path dir = self.parent_path();
    if (dir.empty()) {
        return std::string();
    }
    const std::filesystem::path abs = std::filesystem::absolute(dir, ec);
    return ec ? dir.string() : abs.lexically_normal().string();
}

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
  Tip: run it from the project root folder so that bios/, roms/ and disks/ are
  found (or pass full paths). With NO options it boots to MSX-BASIC (the "Ok"
  prompt) as a READY-TO-GAME config: 512 KB RAM, fast-disk on, and an FM-PAC
  auto-loaded into slot 2 (for SRAM saves) if roms/fmpac.rom is present. Want the
  authentic bare 1988 machine instead? add --stock. -- try booting first.

GETTING STARTED  (copy a line, then change the paths to your files)

  Just boot to BASIC (convenience defaults):
    sony_msx_sdl3

  The authentic bare HB-F1XV (64 KB, accurate disk timing, no FM-PAC):
    sony_msx_sdl3 --stock

  Play a cartridge game (the cartridge type is detected automatically):
    sony_msx_sdl3 --slot1 games/roms/game.rom

  Boot a floppy (MSX-DOS, or a disk game):
    sony_msx_sdl3 --disk disks/msxdos23.dsk

  A multi-disk game -- press F11 in the window to swap to the next disk:
    sony_msx_sdl3 --disk games/disks/game-d1.dsk --disk games/disks/game-d2.dsk

  Save your progress TO DISK (writes are stored back into the .dsk on exit):
    sony_msx_sdl3 --disk games/disks/rpg.dsk --disk-writable

  A game that saves TO SRAM -- the FM-PAC auto-loads into slot 2 by default, so
  a slot-1 game and the FM-PAC coexist (its save auto-persists):
    sony_msx_sdl3 --disk games/disks/rpg-d1.dsk

  Bigger + sharper window (RAM is already 512 KB by default):
    sony_msx_sdl3 --slot1 games/roms/game.rom --scale 4 --filter nearest

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
                            then need an explicit --slot1/--slot2 roms/fmpac.rom).

  Screen
    --scale <1..8>          Picture size = 320N x 240N (default 3 = 960x720); the
                            window is that plus the menu-bar strip on top, so the
                            MSX picture stays fully visible below the menu. The
                            window is resizable and stays 4:3 at any size.
    --filter nearest|linear linear = smooth (default); nearest = sharp pixels.
    --fullscreen            Start in fullscreen (Alt+Enter toggles it live).
    --border / --no-border  Framed 4:3 canvas, or bare edge-to-edge (the default,
                            the Sony-original look).
    --persistence <0..100>  Phosphor-persistence blend: keep this % of the
                            previous frame to smooth sprite-multiplexing flicker
                            (like a real CRT's glow). Default 0 = OFF. Alt+B /
                            Shift+Alt+B step it +/-10% live. Try 50 if sprites
                            flicker.
    --persistence-mode avg|peak
                            How the blend combines frames. avg (default) = a
                            weighted mean (can dim/ghost sprites). peak = peak-
                            hold: a pixel bright in EITHER frame stays FULL
                            brightness (fixes sprite flicker WITHOUT dimming);
                            old trails fade at the --persistence rate. Alt+M
                            toggles it live. Try: --persistence 50
                            --persistence-mode peak.

  Sound
    --volume <0..100>       Master volume percent. DEFAULT 100 (full); 0 = mute.
                            Attenuation only (never amplifies). Alt+D / Alt+U step
                            it -/+10% live. SDL3 presentation only -- it never
                            affects emulation, determinism, or headless output.

  Saving & disks
    --disk-writable         Save in-game disk writes back to the .dsk file (on a
                            clean exit, or an F11 swap). This is now ON BY DEFAULT
                            (a real MSX writes its floppies); Alt+S toggles it live.
    --no-disk-writable      Make disks READ-ONLY: writes stay in memory and are
                            discarded on exit (the escape hatch now that writable
                            is the default). Alt+S still toggles live.
    --fast-disk             Near-instant disk loads (Alt+F toggles it live). ON
                            by default (convenience); may affect rare
                            copy-protected disks.
    --no-fast-disk          Use 100% accurate (slower) FDC timing instead (also
                            what --stock selects). Alt+F still toggles live.
    --fmpac-sram <path>     Where the FM-PAC's battery save is stored (default:
                            roms/fmpac.rom.sram for the auto-loaded FM-PAC, or
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
    --config <path>         Load settings from a strict-XML config file (see
                            sony_msx_hbf1xv.xml). Without this, an interactive
                            launch auto-loads sony_msx_hbf1xv.xml from next to the
                            exe or the current folder; explicit CLI flags always
                            win over the file.
    -h, --help              Show this help and exit.

IN-WINDOW HOTKEYS
    F11        swap to the next disk        F12    write a debug snapshot
    F6 / F7    Speed Controller slow -/+    F8/F9  Ren-Sha auto-fire slow -/+
    Alt+Enter  toggle fullscreen            Alt+F  toggle fast-disk
    Alt+D / Alt+U  master volume -/+10%     Alt+S  toggle disk-writable
    Alt+B / Shift+Alt+B  phosphor persistence +/-10%
    Alt+M      toggle phosphor mode avg/peak   PAUSE  hardware PAUSE button
    F10        live capture (needs --capture on)

GOOD TO KNOW
    - Paths are relative to the folder you run from -- launch from the project
      root, or use full paths (e.g. --bios-dir <full path to your bios folder>).
    - Game saves: to SRAM -> add the FM-PAC cartridge; to disk -> ON by default
      now (a real MSX writes its floppies); pass --no-disk-writable for read-only.
    - A save disk must be a FORMATTED .dsk. Create one with the bundled disk
      tool (msx-diskutil --create yourdisk.dsk), or tools/gen/format-blank-disk.ps1 --
      an all-zero blank has no filesystem and a game cannot write to it.
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
        // M52 (DEC-0079): disk-writable default is now ON; Alt+S toggles live.
        std::cerr << (config.disk_writable
                          ? "  [writable -- saves persist on swap/exit; Alt+S toggles]\n"
                          : "  [read-only -- Alt+S or drop --no-disk-writable to save]\n");
    }
    std::cerr << "    Fast-disk  : "
              << (config.fast_disk ? "ON (near-instant loads; Alt+F toggles)\n"
                                   : "OFF (accurate timing; Alt+F toggles)\n");
    // M52 (DEC-0079): SDL3 master volume (Alt+D down / Alt+U up; --volume 0..100).
    std::cerr << "    Volume     : " << config.master_volume << "%"
              << (config.master_volume == 100 ? " (full; Alt+D -10 / Alt+U +10)\n"
                                              : " (Alt+D -10 / Alt+U +10)\n");
    std::cerr << "    Speed      : " << config.speed_level.value_or(0)
              << (config.speed_level.value_or(0) == 0 ? " (full speed)\n" : "\n");
    std::cerr << "    Video      : " << config.window_width << "x" << config.window_height
              << (config.fullscreen ? ", fullscreen" : "")
              << (config.border_enabled ? ", framed border" : ", edge-to-edge (Sony)")
              << (config.texture_filter == SDL_SCALEMODE_NEAREST ? ", nearest" : ", linear");
    if (config.persistence > 0) {
        std::cerr << ", phosphor " << config.persistence << "% "
                  << (config.persistence_mode == sony_msx::frontend::PhosphorMode::Peak ? "peak" : "avg");
    }
    std::cerr << "\n";

    std::cerr <<
        "----------------------------------------------------------------------\n"
        "  Hotkeys:\n"
        "    F11  swap disk            F12  debug snapshot\n"
        "    F6 / F7   speed slow -/+  (Speed Controller: a slow-motion aid)\n"
        "    F8 / F9   Ren-Sha auto-fire -/+        PAUSE  hardware pause\n"
        "    Alt+Enter  toggle fullscreen           Alt+F  toggle fast-disk\n"
        "    Alt+D / Alt+U  master volume -/+10%    Alt+S  toggle disk-writable\n"
        "    Alt+B / Shift+Alt+B  phosphor persistence +/-10% (smooths sprite flicker)\n"
        "    Alt+M  toggle phosphor mode avg<->peak (peak keeps sprites full-bright)\n";
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

    // M50-S2 (DEC-0077, docs/m50-planner-package.md §4.6): the DETERMINISM gate.
    // A genuinely interactive SDL3 launch (no --hidden-window) MAY auto-load a
    // config; --config <path> forces a load in any mode. --hidden-window (the
    // ctest/CI harness mode) NEVER auto-loads, so the deterministic suite stays
    // byte-identical. When the gate is false, `cfg` stays all-built-in-defaults
    // and resolve_runtime_config() reproduces the exact pre-M50 resolution.
    // DEC-0097: locate the PROJECT ROOT from the executable's own location. The
    // assets (bios/, roms/, disks/, games/) and the config file all live there,
    // but the exe does not -- it is <root>/build/Debug/ on Windows and
    // <root>/build/ on macOS/Linux. Walking up until we find bios/ absorbs that
    // difference with no per-platform branching, and makes the emulator
    // independent of the WORKING DIRECTORY (launching from build/ used to leave
    // every relative asset path unresolvable, so BIOS and FM-PAC silently
    // failed). std::nullopt = no root found -> keep the historical CWD behavior.
    const std::string exe_dir = exe_directory(argv[0]);
    const std::optional<std::filesystem::path> project_root =
        exe_dir.empty() ? std::nullopt
                        : sony_msx::frontend::discover_project_root(std::filesystem::path(exe_dir));

    sony_msx::machine::EmulatorConfig cfg;  // built-in defaults
    {
        const bool interactive = !parsed.hidden_window;
        if (sony_msx::frontend::config_should_load(interactive, parsed.config_path.has_value())) {
            std::vector<std::string> auto_paths;
            // Project root FIRST: the config belongs beside the assets it names.
            if (project_root.has_value()) {
                auto_paths.push_back((*project_root / "sony_msx_hbf1xv.xml").string());
            }
            // Back-compat: configs written beside the exe by v1.6.0-v1.6.2.
            if (!exe_dir.empty()) {
                auto_paths.push_back((std::filesystem::path(exe_dir) / "sony_msx_hbf1xv.xml").string());
            }
            auto_paths.emplace_back("sony_msx_hbf1xv.xml");  // CWD candidate
            std::vector<std::string> config_warnings;
            cfg = sony_msx::frontend::load_config_with_search(parsed.config_path, auto_paths,
                                                              config_warnings);
            for (const std::string& w : config_warnings) {
                std::cerr << w << "\n";
            }
        }
        // Resolve relative asset paths against the project root, NOT the working
        // directory. This is what makes `bios`, `roms/fmpac.rom` etc. work from
        // any launch directory on every platform. Absolute values (a user's
        // explicit choice, or a BIOS folder picked via the menu) pass through
        // untouched.
        if (project_root.has_value()) {
            cfg = sony_msx::frontend::with_absolute_asset_paths(cfg, *project_root);

            // DEC-0098: a configured asset path that does NOT exist must never be
            // honoured silently. A config written by v1.6.2 could carry an
            // absolute path valid only for the directory it was once launched
            // from; later versions passed absolute paths through untouched, so
            // FM-PAC auto-load looked up a missing file, skipped, and left slot 2
            // empty with nothing on screen or stderr to explain it. Fall back to
            // the standard project-root location when that exists, and SAY SO.
            // Only the two assets whose absence is silently fatal are checked --
            // fmpac.rom.sram and softwaredb are legitimately optional, so
            // checking them would only add noise.
            const auto dir_exists = [](const std::string& p) {
                std::error_code ec;
                return std::filesystem::is_directory(p, ec);
            };
            const auto file_exists = [](const std::string& p) {
                std::error_code ec;
                return std::filesystem::is_regular_file(p, ec);
            };
            const sony_msx::machine::EmulatorConfig defaults{};
            auto [bios_dir, bios_msg] = sony_msx::frontend::resolve_existing_asset(
                "BIOS directory", cfg.bios_dir, defaults.bios_dir, *project_root, dir_exists);
            cfg.bios_dir = bios_dir;
            if (!bios_msg.empty()) {
                std::cerr << bios_msg << "\n";
            }
            auto [fmpac_rom, fmpac_msg] = sony_msx::frontend::resolve_existing_asset(
                "FM-PAC ROM", cfg.fmpac_rom, defaults.fmpac_rom, *project_root, file_exists);
            cfg.fmpac_rom = fmpac_rom;
            if (!fmpac_msg.empty()) {
                std::cerr << fmpac_msg << "\n";
            }
        }
    }
    // Apply precedence CLI > XML(cfg) > built-in default across every S2-scope
    // knob (single tested seam, config_runtime.h). With no config loaded this is
    // byte-identical to pre-M50.
    const sony_msx::frontend::ResolvedRuntimeConfig runtime =
        sony_msx::frontend::resolve_runtime_config(cfg, parsed);
    // M50-S3 (DEC-0077): the machine-sizing/path fields (BIOS dir + filenames,
    // FM-PAC ROM/SRAM, softwaredb) resolved with the SAME precedence. Byte-
    // identical to pre-M50 when no config was loaded.
    const sony_msx::frontend::ResolvedMachineConfig machine_cfg =
        sony_msx::frontend::resolve_machine_config(cfg, parsed);

    sony_msx::frontend::Sdl3AppConfig config;
    // M50-S3: BIOS dir + the 7 role-keyed filenames (CLI --bios-dir > XML > "bios").
    config.bios_dir = machine_cfg.bios_dir;
    config.bios_roms = machine_cfg.bios_roms;
    // M64: in-window file-dialog default directories (XML <machine><cartridge
    // dir>/<disk dir> > built-in "roms"/"disks"; no CLI flag). Dialog UX only.
    config.cartridge_dir = machine_cfg.cartridge_dir;
    config.disk_dir = machine_cfg.disk_dir;
    config.disk_paths = parsed.disk_paths;  // M35-S1: repeatable --disk list
    config.max_frames = parsed.max_frames;
    config.hidden_window = parsed.hidden_window;
    // M50-S2: border / disk-writable resolved with config precedence (CLI > XML
    // > default). runtime.* == parsed.* when no config was loaded (byte-identical).
    config.border_enabled = runtime.border_enabled;
    config.disk_writable = runtime.disk_writable;  // M36-S-c
    config.dump_state_filename = parsed.dump_state_filename;
    config.trace_cpu_filename = parsed.trace_cpu_filename;
    config.event_log_filename = parsed.event_log_filename;
    config.input_script_path = parsed.input_script_path;
    config.record_input_path = parsed.record_input_path;  // DEC-0072 input recorder
    config.snapshot_dir = parsed.snapshot_dir;  // M36 Phase 3: --snapshot <dir>
    config.swap_disk_frame = parsed.swap_disk_frame;  // DEC-0072 scripted disk swap
    config.fingerprint_path = parsed.fingerprint_path;  // DEC-0072 per-frame CPU fingerprint
    config.stream_light = parsed.stream_light;  // DEC-0052: F10 arms lightweight mode
    // M37/M50-S2: launch-time initial Speed Controller level, resolved with
    // config precedence (CLI > XML > default). Level 0 (full speed) is mapped to
    // std::nullopt so it stays byte-identical to the pre-M50 "untouched" path.
    config.speed_level = (runtime.speed_level == 0) ? std::optional<int>{}
                                               : std::optional<int>{runtime.speed_level};
    // M37/M50-S2: --fullscreen / --filter / --capture / --persistence(-mode) all
    // resolved with config precedence. runtime.* == parsed.* when no config was loaded.
    config.fullscreen = runtime.fullscreen;
    config.capture_enabled = runtime.capture_enabled;
    config.texture_filter = (runtime.filter == sony_msx::frontend::TextureFilter::Nearest)
                                ? SDL_SCALEMODE_NEAREST
                                : SDL_SCALEMODE_LINEAR;
    config.persistence = runtime.persistence;
    config.persistence_mode = runtime.persistence_mode;
    // M52 (DEC-0079): resolved SDL3 master volume (CLI --volume > XML > default
    // 100). runtime.master_volume == parsed.volume when no config was loaded;
    // default 100 (unity) is byte-identical to pre-M52.
    config.master_volume = runtime.master_volume;
    // M46/M50-S2: the flipped convenience-vs-stock session defaults, now with the
    // externalized config as the base default (CLI > XML > convenience). The
    // Sdl3AppConfig struct defaults stay stock (anti-drift, A-2); resolve_runtime_
    // config() -- via resolve_session_defaults() -- is the ONLY place they flip.
    sony_msx::frontend::ResolvedSessionDefaults resolved;
    resolved.ram_bytes = runtime.ram_bytes;
    resolved.fast_disk = runtime.fast_disk;
    resolved.fmpac_autoload = runtime.fmpac_autoload;
    resolved.is_stock = runtime.is_stock;
    config.ram_bytes = runtime.ram_bytes;
    config.fast_disk = runtime.fast_disk;               // convenience default ON (Alt+D toggles live)
    config.fmpac_autoload = runtime.fmpac_autoload;      // FM-PAC auto-load (skips gracefully)
    config.fmpac_autoload_slot = runtime.fmpac_autoload_slot;  // M50-S2: configurable slot (default 2)
    // M50-S3: the FM-PAC auto-load ROM path (XML > built-in "roms/fmpac.rom"; an
    // explicit --slot2 occupies the bay and the auto-load skips gracefully).
    config.fmpac_autoload_rom_path = machine_cfg.fmpac_autoload_rom;
    // --scale N -> 320N x 240N window; runtime.scale is the resolved scale (CLI > XML >
    // default 3 = 960x720). Always set explicitly from the resolved value.
    config.window_width = 320 * runtime.scale;
    config.window_height = 240 * runtime.scale;
    // M36 FM-PAC SRAM persistence: override/opt-out of the auto-derived
    // <fmpac-cart>.rom.sram default (default persistence is automatic). M50-S3:
    // resolved CLI --fmpac-sram > XML > auto-derive (std::nullopt = auto-derive,
    // byte-identical to pre-M50 when no config set it).
    config.fmpac_sram_path = machine_cfg.fmpac_sram;
    config.fmpac_sram_disabled = parsed.fmpac_sram_disabled;
    // M30 (backlog G2): carry the parser's type_was_explicit through so a
    // type-less --cartN triggers auto-identification inside
    // load_configured_assets() (the ONE shared resolver); an explicit
    // --cartN-type keeps byte-for-byte pre-M30 behavior.
    // M50-S3: softwaredb path resolved CLI --softwaredb > XML > default
    // (std::nullopt = kDefaultSoftwareDbPath, byte-identical to pre-M50).
    config.softwaredb_path = machine_cfg.softwaredb;
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

    // DEC-0095 (frontend-only settings + recent persistence): hand the app the
    // loaded config as the re-emit BASELINE, and -- ONLY for a genuinely
    // interactive launch with no explicit --config -- the beside-exe save paths.
    // THIS IS THE DETERMINISM GATE: --hidden-window / headless / --config leave
    // both paths unset, so the app never reads or writes them and the ctest suite
    // stays byte-identical. --config means "the user manages this file", so we
    // neither split-brain (load X, save Y) nor clobber their named file.
    config.config_baseline = cfg;
    if (!parsed.hidden_window && !parsed.config_path.has_value()) {
        const std::string exe_dir = exe_directory(argv[0]);
        // DEC-0097: persist to the PROJECT ROOT -- beside the assets the config
        // names, and identical on every platform (the exe dir is build/Debug on
        // Windows but build/ elsewhere, so "beside the exe" was inherently
        // platform-specific). Falls back to the exe dir, then the CWD, when no
        // project root can be found (a detached/installed binary).
        const auto persist_to = [&exe_dir, &project_root](const char* name) {
            if (project_root.has_value()) {
                return (*project_root / name).string();
            }
            return exe_dir.empty() ? std::string(name)
                                   : (std::filesystem::path(exe_dir) / name).string();
        };
        config.settings_save_path = persist_to("sony_msx_hbf1xv.xml");
        config.recent_save_path = persist_to("sony_msx_recent.txt");
        // DEC-0097: hand the app the root so it can persist asset paths RELATIVE
        // to it (portable + survives moving the project directory).
        if (project_root.has_value()) {
            config.project_root = project_root->string();
        }
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
