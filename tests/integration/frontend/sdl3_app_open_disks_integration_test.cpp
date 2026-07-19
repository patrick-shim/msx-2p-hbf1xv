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

// Suite: Frontend_Sdl3AppOpenDisks_Integration (M56, DEC-0084, planner §9 test 6;
// slice S2).
//
// The F1 Open Disk(s) apply proven through the DIRECT public apply_open_disks()
// seam (hidden_window=true, no real OS dialog -- the swap_to_next_disk() pattern):
//   * REPLACE semantics: the selection becomes the whole F11 cycle, index -> 0,
//     first mounts, disk_count == new size.
//   * dirty-flush-before-replace persists the outgoing writable disk to host.
//   * transactional abort on a bad path leaves the current disk intact.
//   * empty selection -> no-op.
//   * a media-change signal is raised on replace.
// Uses synthesized temp .dsk files so it never depends on an untracked asset.

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "frontend/sdl3_app.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

using sony_msx::devices::fdc::DiskImage;

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

void write_synth_disk(const std::filesystem::path& p) {
    const std::vector<std::uint8_t> bytes = DiskImage::synthesize();
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> make_sector(const std::uint8_t seed) {
    std::vector<std::uint8_t> s(DiskImage::kSectorSize);
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<std::uint8_t>((i * 5u + seed) & 0xFFu);
    }
    return s;
}

sony_msx::frontend::Sdl3AppConfig base_config() {
    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    return config;
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "hbf1xv_m56_open_disks";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path a = dir / "a.dsk";
    const std::filesystem::path b = dir / "b.dsk";
    const std::filesystem::path c = dir / "c.dsk";
    write_synth_disk(a);
    write_synth_disk(b);
    write_synth_disk(c);

    // --- REPLACE semantics: multi-select becomes the whole cycle, index -> 0. ---
    {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {a.string()};
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            expect(app.disk_count() == 1, "Replace_BootDiskCountOne");
            app.apply_open_disks({b.string(), c.string()});
            expect(app.disk_count() == 2, "Replace_DiskCountBecomesTwo");
            expect(app.current_disk_index() == 0, "Replace_IndexResetToZero");
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "Replace_FirstDiskMounted");
            expect(app.machine().disk_drive().disk_changed(), "Replace_MediaChangeSignalled");
            // F11 now cycles the NEW list (2 disks) 0 -> 1 -> 0.
            app.swap_to_next_disk();
            expect(app.current_disk_index() == 1, "Replace_NewListF11Cycles");
            app.swap_to_next_disk();
            expect(app.current_disk_index() == 0, "Replace_NewListF11Wraps");
            app.shutdown();
        }
    }

    // --- Dirty-flush-before-replace: the outgoing writable disk persists to host. ---
    {
        // Fresh copy of 'a' so this case's writes don't leak to other cases.
        const std::filesystem::path cur = dir / "flush_current.dsk";
        write_synth_disk(cur);

        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {cur.string()};
        config.disk_writable = true;  // Alt+S ON at boot -> host-bound
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            // Write via LBA so the read-back below is LBA-consistent (write_sector()
            // addresses CHS at the drive's current track/side, not LBA).
            const std::vector<std::uint8_t> written = make_sector(0x6C);
            expect(app.machine().disk_image().write_lba(6, written.data()), "Flush_WriteLbaOk");
            expect(app.machine().disk_image().dirty(), "Flush_DirtyBeforeReplace");

            app.apply_open_disks({b.string()});  // REPLACE -> the outgoing disk flushes
            expect(app.disk_count() == 1, "Flush_ReplacedToSingle");

            // The outgoing disk's host file now holds the write.
            DiskImage reloaded(read_file(cur));
            std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
            expect(reloaded.read_lba(6, back.data()), "Flush_HostReloadReadOk");
            expect(back == written, "Flush_OutgoingWritePersistedToHost");
            app.shutdown();
        }
        std::filesystem::remove(cur);
    }

    // --- Transactional abort: a bad path leaves the current mount intact. ---
    {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {a.string()};
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            const std::vector<std::uint8_t> marker = make_sector(0xA1);
            app.machine().disk_drive().write_sector(3, marker.data());
            const std::vector<std::uint8_t> before = app.machine().disk_image().data();

            app.apply_open_disks({b.string(), (dir / "missing.dsk").string()});
            // The bad path aborts the whole apply -> current mount untouched.
            expect(app.disk_count() == 1, "Abort_DiskCountUnchanged");
            expect(app.current_disk_index() == 0, "Abort_IndexUnchanged");
            expect(app.machine().disk_image().data() == before, "Abort_CurrentImageByteIntact");
            std::vector<std::uint8_t> still(DiskImage::kSectorSize, 0);
            app.machine().disk_drive().read_sector(3, still.data());
            expect(still == marker, "Abort_CurrentSectorPreserved");
            app.shutdown();
        }
    }

    // --- Empty selection -> no-op. ---
    {
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {a.string()};
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            app.apply_open_disks({});
            expect(app.disk_count() == 1, "EmptySelection_NoOp");
            app.shutdown();
        }
    }

    // --- Determinism: two identical applies -> identical mounted-disk bytes.
    //     Apps are constructed/torn down SEQUENTIALLY (never two live SDL contexts
    //     at once), matching the M35 multi-disk determinism pattern. ---
    {
        auto run_once = [&]() -> std::vector<std::uint8_t> {
            sony_msx::frontend::Sdl3AppConfig cfg = base_config();
            cfg.disk_paths = {a.string()};
            sony_msx::frontend::Sdl3App app(cfg);
            std::vector<std::uint8_t> out;
            if (app.init()) {
                app.apply_open_disks({b.string(), c.string()});
                out = app.machine().disk_image().data();
                app.shutdown();
            }
            return out;
        };
        const std::vector<std::uint8_t> r1 = run_once();
        const std::vector<std::uint8_t> r2 = run_once();
        expect(!r1.empty() && r1 == r2, "Determinism_TwoAppliesByteIdentical");
    }

    std::filesystem::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppOpenDisks_Integration cases passed\n";
    return 0;
}
