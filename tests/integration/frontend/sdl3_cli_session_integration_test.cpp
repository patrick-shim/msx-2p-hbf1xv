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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/fdc/disk_image.h"
#include "frontend/sdl3_app.h"
#include "frontend/sdl3_cli.h"
#include "machine/sha1.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_DISKS_DIR
#error "SONY_MSX_DISKS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_SOFTWAREDB_PATH
#error "SONY_MSX_SOFTWAREDB_PATH must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Suite: Frontend_Sdl3CliSession_Integration (M26-S7, docs/m26-planner-
// package.md §2.8/A-M26-6).
//
// Parses a REAL representative flag combination (--bios-dir/--cart1/--disk/
// --max-frames) via the SDL3 frontend's own sdl3_cli.h parser, builds an
// Sdl3App from it exactly as sdl3_main.cpp does, and asserts the resulting
// Hbf1xvMachine state (asset root, loaded cartridge, loaded disk image)
// matches expectations against REAL committed assets (roms/aleste.rom,
// disks/msxdos22.dsk -- the same fixtures the M19/M24 precedents already
// use). A negative case (bad --cart1 path) produces the SAME loud,
// non-zero-exit-style failure discipline the M19 cartridge CLI already
// established (src/main.cpp's load_cartridges_from_args precedent) -- never
// a silent fallback.

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

sony_msx::frontend::Sdl3AppConfig config_from_args(const std::vector<std::string>& args, bool* parse_ok) {
    const sony_msx::frontend::ParsedSdl3Cli parsed = sony_msx::frontend::parse_sdl3_cli(args);
    *parse_ok = parsed.errors.empty();

    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    if (parsed.bios_dir.has_value()) {
        config.bios_dir = *parsed.bios_dir;
    }
    config.disk_paths = parsed.disk_paths;  // M35-S1: repeatable --disk list
    config.max_frames = parsed.max_frames;
    // M30 (backlog G2): mirror sdl3_main.cpp's own config building exactly
    // (this helper's documented contract), incl. the new type_was_explicit
    // carry-through and --softwaredb. Cases 1/2 below are behaviorally
    // untouched: Case 1 passes an explicit type; Case 2 fails at file-open,
    // BEFORE type resolution.
    config.softwaredb_path = parsed.cartridges.softwaredb_path;
    if (parsed.cartridges.slot1.path.has_value()) {
        config.cart1_path = parsed.cartridges.slot1.path;
        config.cart1_type = parsed.cartridges.slot1.type;
        config.cart1_type_explicit = parsed.cartridges.slot1.type_was_explicit;
    }
    return config;
}

}  // namespace

int main() {
    set_dummy_drivers();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_Quit();  // Re-init happens inside Sdl3App::init() below; this call only
                 // confirms the environment itself is viable before proceeding.

    const std::string bios_dir = SONY_MSX_BIOS_DIR;
    const std::string cart_path = std::string(SONY_MSX_ROMS_DIR) + "/aleste.rom";
    const std::string disk_path = std::string(SONY_MSX_DISKS_DIR) + "/msxdos22.dsk";

    // --- Case 1: a real, representative flag combination loads correctly. ---
    {
        // roms/aleste.rom loads mechanically as Generic8kB (openMSX's own
        // label; NOT a hardware-identification claim -- mirrors the M19
        // hbf1xv_m19_aleste_smoke_integration_test.cpp precedent exactly).
        // The default --cart1-type (Mirrored, when omitted) rejects this
        // file's size, so it must be specified explicitly here.
        const std::vector<std::string> args{
            "--bios-dir",   bios_dir,       "--cart1", cart_path, "--cart1-type", "8kB",
            "--disk",       disk_path,      "--max-frames", "1",
        };
        bool parse_ok = false;
        sony_msx::frontend::Sdl3AppConfig config = config_from_args(args, &parse_ok);
        expect(parse_ok, "RepresentativeFlags_ParseWithoutErrors");

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(init_ok, "RepresentativeFlags_Sdl3AppInitSucceeds");
        if (!init_ok) {
            std::cerr << "  " << app.last_error() << "\n";
        } else {
            expect(app.machine().asset_root() == bios_dir, "RepresentativeFlags_AssetRootMatchesBiosDir");
            expect(app.machine().cartridge_slot1().loaded(), "RepresentativeFlags_Cartridge1Loaded");

            // Real disk-image loading (A-M26-6): the machine's disk_image()
            // is now the REAL msxdos22.dsk content, not the synthesized
            // default -- confirmed via size AND a real content spot-check
            // (the FAT12 media descriptor byte at LBA0 offset 0x15, per
            // devices/fdc/disk_image.h's own documented BPB layout).
            std::ifstream real_file(disk_path, std::ios::binary);
            const std::vector<std::uint8_t> real_bytes((std::istreambuf_iterator<char>(real_file)),
                                                        std::istreambuf_iterator<char>());
            expect(app.machine().disk_image().size() == real_bytes.size(),
                   "RepresentativeFlags_DiskImageSize_MatchesRealFile");
            bool disk_bytes_match = !real_bytes.empty();
            for (std::size_t i = 0; i < real_bytes.size() && disk_bytes_match; ++i) {
                if (app.machine().disk_image().byte_at(static_cast<std::uint32_t>(i)) != real_bytes[i]) {
                    disk_bytes_match = false;
                }
            }
            expect(disk_bytes_match, "RepresentativeFlags_DiskImageContent_ByteIdenticalToRealFile");

            // The disk drive genuinely has the new image attached (not just
            // machine().disk_image() reassigned in isolation).
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "RepresentativeFlags_DiskDriveAttachedToNewImage");

            app.shutdown();
        }
    }

    // --- Case 2 (negative): a bad --cart1 path produces a loud, recorded
    // failure -- never a silent fallback to "no cartridge". ---
    {
        const std::vector<std::string> args{
            "--bios-dir", bios_dir, "--cart1", "this/path/genuinely/does/not/exist.rom", "--max-frames", "1",
        };
        bool parse_ok = false;
        sony_msx::frontend::Sdl3AppConfig config = config_from_args(args, &parse_ok);
        expect(parse_ok, "BadCartPath_StillParsesSyntactically");

        sony_msx::frontend::Sdl3App app(std::move(config));
        const bool init_ok = app.init();
        expect(!init_ok, "BadCartPath_Sdl3AppInitFails_NotSilentFallback");
        expect(!app.last_error().empty(), "BadCartPath_LastErrorIsRecordedAndNonEmpty");
        expect(!app.initialized(), "BadCartPath_AppLeftUninitialized_NoPartialState");
    }

    // --- Case 3 (M30 additive, backlog G2, docs/m30-planner-package.md
    // §4-S5): --cart1 <aleste> with NO type flag under dummy drivers ->
    // auto-identified via softwaredb SHA1 match -> the session starts
    // (covers the Sdl3App path end-to-end through the ONE shared resolver).
    // Skip-gated with the SAME discipline as the headless M30 integration
    // test: ROM present AND the exact specified dump AND the DB present --
    // a differing user-supplied asset SKIPS this case, never fails it. ---
    {
        const std::string softwaredb_path = SONY_MSX_SOFTWAREDB_PATH;
        std::ifstream rom_in(cart_path, std::ios::binary);
        bool skip = false;
        std::string skip_reason;
        std::vector<std::uint8_t> rom_bytes;
        if (!rom_in) {
            skip = true;
            skip_reason = "roms/aleste.rom not present";
        } else {
            rom_bytes.assign((std::istreambuf_iterator<char>(rom_in)), std::istreambuf_iterator<char>());
            const std::string sha1 = sony_msx::machine::sha1_hex(rom_bytes);
            if (sha1 != "e93d0840c59c6eba273df546d22148d486a150a6") {
                skip = true;
                skip_reason = "roms/aleste.rom is a different dump (sha1=" + sha1 + ")";
            }
        }
        if (!skip && !std::filesystem::exists(softwaredb_path)) {
            skip = true;
            skip_reason = "softwaredb not present at " + softwaredb_path;
        }

        if (skip) {
            std::cout << "SKIP (Case 3, M30 auto-identification): " << skip_reason
                      << " (planner A-M30-1/A-M30-2 skip discipline)\n";
        } else {
            const std::vector<std::string> args{
                "--bios-dir", bios_dir,        "--cart1", cart_path,
                "--softwaredb", softwaredb_path, "--max-frames", "1",
            };
            bool parse_ok = false;
            sony_msx::frontend::Sdl3AppConfig config = config_from_args(args, &parse_ok);
            expect(parse_ok, "AutoIdentify_ParsesWithoutErrors");
            expect(!config.cart1_type_explicit, "AutoIdentify_TypeNotExplicit_InConfig");

            sony_msx::frontend::Sdl3App app(std::move(config));
            const bool init_ok = app.init();
            expect(init_ok, "AutoIdentify_SessionStarts_TypelessCart1");
            if (!init_ok) {
                std::cerr << "  " << app.last_error() << "\n";
            } else {
                expect(app.machine().cartridge_slot1().loaded(),
                       "AutoIdentify_Cartridge1Loaded_ViaSharedResolver");
                app.shutdown();
            }
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3CliSession_Integration cases passed\n";
    return 0;
}
