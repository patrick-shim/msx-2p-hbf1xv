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
#include <system_error>
#include <utility>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AppSnapshot_Integration (M36 Phase 3 slice S5). Drives the
// F12 snapshot path (via the public request_snapshot() seam -- the R-M35-1
// swap_to_next_disk() precedent) under the dummy SDL3 drivers, and proves: the
// capture is deferred to a frame boundary + writes a complete bundle to
// <dir>/snapshot/<id>/ carrying the frontend disk index (A4); and the
// default-off no-op guard (no F12 -> no snapshot dir).

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
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

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    using sony_msx::frontend::Sdl3App;
    using sony_msx::frontend::Sdl3AppConfig;

    set_dummy_drivers();

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m36-snapshot-integration";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    // --- Case A: F12 (request_snapshot) writes a complete bundle. ---
    {
        Sdl3AppConfig config;
        config.bios_dir = SONY_MSX_BIOS_DIR;
        config.hidden_window = true;
        config.snapshot_dir = (root / "captured").string();

        Sdl3App app(std::move(config));
        expect(app.init(), "Init_Succeeds");

        for (int i = 0; i < 3; ++i) {
            app.run_one_frame();
        }
        expect(!app.snapshot_requested(), "NoRequest_FlagClear");

        // No F12 pressed yet -> no snapshot dir created (default-off guard).
        expect(!std::filesystem::exists(root / "captured" / "snapshot"), "BeforeF12_NoSnapshotDir");

        app.request_snapshot();  // the F12 seam
        expect(app.snapshot_requested(), "AfterRequest_FlagSet");

        app.run_one_frame();  // services the deferred capture at the frame boundary
        expect(!app.snapshot_requested(), "AfterService_FlagClear");
        // The capture wrote at the END of that run_one_frame(), so the id used
        // for the folder is the machine's post-frame id (read it now).
        const std::string id = app.machine().snapshot_id();

        const std::filesystem::path dir = root / "captured" / "snapshot" / id;
        expect(std::filesystem::exists(dir / "manifest.txt"), "Bundle_Manifest");
        expect(std::filesystem::exists(dir / "cpu.txt") && std::filesystem::exists(dir / "vdp.txt") &&
                   std::filesystem::exists(dir / "vram.txt") &&
                   std::filesystem::exists(dir / "fdc.txt") &&
                   std::filesystem::exists(dir / "cartridges.txt"),
               "Bundle_ComponentFiles");

        // The manifest carries the frontend multi-disk index (A4).
        const std::string manifest = read_file(dir / "manifest.txt");
        expect(manifest.find("disk_index=0") != std::string::npos, "Manifest_DiskIndexNote");
        expect(manifest.find("HBF1XV-SNAPSHOT v1") == 0, "Manifest_VersionTagged");

        app.shutdown();
    }

    // --- Case B: a session that NEVER presses F12 writes NO snapshot dir. ---
    {
        Sdl3AppConfig config;
        config.bios_dir = SONY_MSX_BIOS_DIR;
        config.hidden_window = true;
        config.snapshot_dir = (root / "untouched").string();

        Sdl3App app(std::move(config));
        expect(app.init(), "Init2_Succeeds");
        for (int i = 0; i < 3; ++i) {
            app.run_one_frame();
        }
        app.shutdown();
        expect(!std::filesystem::exists(root / "untouched" / "snapshot"), "NoF12_NoSnapshotDir");
    }

    std::filesystem::remove_all(root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppSnapshot_Integration cases passed\n";
    return 0;
}
