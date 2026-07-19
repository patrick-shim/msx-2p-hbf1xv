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

// Suite: Frontend_Sdl3AppBiosFolder_Integration (M60, DEC-0089). The permanent
// oracle for Sdl3App::apply_bios_folder() -- the Machine > BIOS Folder...
// runtime BIOS-directory selector. Driven under hidden_window=true (no menu,
// no OS dialog), the request_power_cycle()/apply_open_disks precedent. Cases:
//   (a) transactional abort, EMPTY folder: bios_dir unchanged, machine state
//       byte-identical (dump == pre-apply dump), still runs.
//   (b) transactional abort, PARTIAL folder (6 of the 7 ROMs): same guarantees.
//   (c) transactional abort, UNREADABLE set (all 7 present, one zero-byte):
//       same guarantees (the validator must READ, not just stat).
//   (d) success: a full copy of the real BIOS set -> bios_dir/asset_root move
//       to the new folder and the post-apply state == the post-init fresh-boot
//       state (power-cycle fresh-boot equivalence, identical ROM bytes).
//   (e) hidden-window dialog inertness: open_bios_folder_dialog() with
//       menu_ == null (no mailbox mutex) is a total no-op.

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/sdl3_app.h"
#include "machine/emulator_config.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const std::string& case_name) {
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

sony_msx::frontend::Sdl3AppConfig base_config() {
    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    return config;
}

// The 7 role-keyed BIOS filenames, single-sourced from the SAME defaults
// apply_bios_folder validates (a bare BiosRoms{} == the strict HB-F1XV set).
std::vector<std::string> bios_filenames() {
    const sony_msx::machine::EmulatorConfig::BiosRoms roms{};
    return {roms.bios,     roms.sub,        roms.kanji_driver, roms.disk,
            roms.fm_music, roms.kanji_font, roms.firmware};
}

std::filesystem::path make_temp_dir(const std::string& stem) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (stem + std::to_string(static_cast<unsigned long long>(SDL_GetPerformanceCounter())));
    std::filesystem::create_directories(p);
    return p;
}

// Copy the tracked bios/ set (all 7 files) into `dir`, skipping `skip` (empty =
// copy everything). Returns false on any copy failure (a test-setup error).
bool copy_bios_set(const std::filesystem::path& dir, const std::string& skip) {
    for (const std::string& name : bios_filenames()) {
        if (name == skip) {
            continue;
        }
        std::error_code ec;
        std::filesystem::copy_file(std::filesystem::path(SONY_MSX_BIOS_DIR) / name, dir / name,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "setup: copy of " << name << " failed: " << ec.message() << "\n";
            return false;
        }
    }
    return true;
}

void remove_all_quiet(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

// Shared abort oracle: apply_bios_folder(bad) must leave bios_dir, asset_root,
// and the FULL serialized machine state untouched, and the machine must still
// advance frames afterward.
void expect_transactional_abort(sony_msx::frontend::Sdl3App& app, const std::string& bad_dir,
                                const std::string& tag) {
    const std::string dir_before = app.bios_dir();
    const std::filesystem::path root_before = app.machine().asset_root();
    const std::string dump_before = app.machine().serialize_state_dump();

    app.apply_bios_folder(bad_dir);

    expect(app.bios_dir() == dir_before, tag + "_BiosDirUnchanged");
    expect(app.machine().asset_root() == root_before, tag + "_AssetRootUnchanged");
    expect(app.machine().serialize_state_dump() == dump_before, tag + "_MachineStateUntouched");
    for (int i = 0; i < 3; ++i) {
        app.run_one_frame();  // the old machine keeps running (no crash, no reboot)
    }
    expect(app.machine().serialize_state_dump() != dump_before, tag + "_MachineStillAdvances");
}

}  // namespace

int main() {
    set_dummy_drivers();

    // ===== (a)+(b)+(c) transactional aborts + (e) dialog inertness, one app. ====
    {
        const std::filesystem::path empty_dir = make_temp_dir("m60_bios_empty_");
        const std::filesystem::path partial_dir = make_temp_dir("m60_bios_partial_");
        const std::filesystem::path zerofile_dir = make_temp_dir("m60_bios_zero_");

        // partial: 6 of 7 (the DISK ROM missing). zerofile: all 7 copied, then
        // the SUB ROM truncated to zero bytes (present but unreadable content).
        const sony_msx::machine::EmulatorConfig::BiosRoms roms{};
        bool setup_ok = copy_bios_set(partial_dir, /*skip=*/roms.disk);
        setup_ok = copy_bios_set(zerofile_dir, /*skip=*/"") && setup_ok;
        {
            std::ofstream truncate(zerofile_dir / roms.sub,
                                   std::ios::binary | std::ios::trunc);  // now 0 bytes
        }
        expect(setup_ok, "Abort_Setup_CopiesSucceeded");

        sony_msx::frontend::Sdl3App app(base_config());
        if (app.init()) {
            // (e) hidden-window: no menu, no mailbox mutex -- the dialog launcher
            // must be a total no-op (and must not crash / mark anything pending).
            expect(!app.menu_active(), "Inert_HiddenWindow_NoMenu");
            app.open_bios_folder_dialog();
            expect(app.bios_dir() == std::string(SONY_MSX_BIOS_DIR),
                   "Inert_DialogLauncher_NoEffect");

            for (int i = 0; i < 3; ++i) {
                app.run_one_frame();  // advance so state differs from cold boot
            }

            expect_transactional_abort(app, empty_dir.string(), "AbortEmptyFolder");
            expect_transactional_abort(app, partial_dir.string(), "AbortPartialFolder");
            expect_transactional_abort(app, zerofile_dir.string(), "AbortZeroByteRom");
            // A nonexistent path aborts identically (missing every file).
            expect_transactional_abort(app, (empty_dir / "no_such_subdir").string(),
                                       "AbortNonexistentFolder");
            app.shutdown();
        } else {
            expect(false, "Abort_Init");
        }

        remove_all_quiet(empty_dir);
        remove_all_quiet(partial_dir);
        remove_all_quiet(zerofile_dir);
    }

    // ===== (d) success: a full copy of the real BIOS set. ======================
    {
        const std::filesystem::path full_dir = make_temp_dir("m60_bios_full_");
        const bool setup_ok = copy_bios_set(full_dir, /*skip=*/"");
        expect(setup_ok, "Success_Setup_CopySucceeded");

        sony_msx::frontend::Sdl3App app(base_config());
        if (setup_ok && app.init()) {
            // The fresh-boot reference: state at init (frame 0). The copied set is
            // byte-identical to bios/, so a power-cycle into it must reproduce
            // EXACTLY this state (the M57 fresh-boot-equivalence oracle).
            const std::string dump_fresh = app.machine().serialize_state_dump();

            for (int i = 0; i < 5; ++i) {
                app.run_one_frame();  // diverge from the fresh-boot state first
            }
            expect(app.machine().serialize_state_dump() != dump_fresh,
                   "Success_StateDivergedBeforeApply");

            app.apply_bios_folder(full_dir.string());

            expect(app.bios_dir() == full_dir.string(), "Success_BiosDirMovedToNewFolder");
            expect(app.machine().asset_root() == full_dir, "Success_AssetRootMovedToNewFolder");
            expect(app.machine().serialize_state_dump() == dump_fresh,
                   "Success_PowerCycled_FreshBootEquivalence");
            expect(app.machine().dram_size() == 64u * 1024u, "Success_RamSizeUnchanged");
            for (int i = 0; i < 5; ++i) {
                app.run_one_frame();  // the rebooted machine runs on
            }
            expect(app.machine().serialize_state_dump() != dump_fresh,
                   "Success_RebootedMachineAdvances");
            app.shutdown();
        } else if (setup_ok) {
            expect(false, "Success_Init");
        }

        remove_all_quiet(full_dir);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppBiosFolder_Integration cases passed\n";
    return 0;
}
