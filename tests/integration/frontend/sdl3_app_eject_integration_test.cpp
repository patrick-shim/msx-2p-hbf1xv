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

// Suite: Frontend_Sdl3AppEject_Integration (M56, DEC-0084, planner §9 test 8;
// slice S3).
//
// The F3 Eject matrix proven through the DIRECT public eject_disk() /
// eject_cartridge(slot) seams (hidden_window=true, no OS dialog):
//   * eject_disk() flushes-if-writable then detaches (image() == nullptr,
//     disk_count == 0); the flushed bytes persist to host; NO machine reset.
//   * eject_cartridge(slot) unloads + resets (slotN not loaded; elapsed back to 0).
// The disk cases use synthesized temp .dsk files; the cartridge case is
// skip-gated on the untracked roms/fmpac.rom asset.

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
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
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
        s[i] = static_cast<std::uint8_t>((i * 11u + seed) & 0xFFu);
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
        std::filesystem::temp_directory_path() / "hbf1xv_m56_eject";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // --- Eject Disk: detach + empty drive, no reset. ---
    {
        const std::filesystem::path d = dir / "eject_a.dsk";
        write_synth_disk(d);
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {d.string()};
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            for (int i = 0; i < 3; ++i) {
                app.run_one_frame();
            }
            const std::uint64_t before = app.machine().elapsed_cycles();
            expect(app.machine().disk_drive().image() != nullptr, "EjectDisk_MountedBeforeEject");

            app.eject_disk();
            expect(app.machine().disk_drive().image() == nullptr, "EjectDisk_Detached");
            expect(app.disk_count() == 0, "EjectDisk_DiskCountZero");
            expect(app.current_disk_index() == 0, "EjectDisk_IndexReset");
            // Eject does NOT reset the machine.
            expect(app.machine().elapsed_cycles() >= before, "EjectDisk_NoReset");
            app.shutdown();
        }
        std::filesystem::remove(d);
    }

    // --- Eject Disk (writable + dirty): flushes to host before detaching. ---
    {
        const std::filesystem::path d = dir / "eject_flush.dsk";
        write_synth_disk(d);
        sony_msx::frontend::Sdl3AppConfig config = base_config();
        config.disk_paths = {d.string()};
        config.disk_writable = true;
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            // Write via LBA so the read-back is LBA-consistent (write_sector()
            // addresses CHS at the current track/side, not LBA).
            const std::vector<std::uint8_t> written = make_sector(0x7E);
            expect(app.machine().disk_image().write_lba(5, written.data()), "EjectFlush_WriteLbaOk");
            app.eject_disk();
            expect(app.machine().disk_drive().image() == nullptr, "EjectFlush_Detached");

            DiskImage reloaded(read_file(d));
            std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
            expect(reloaded.read_lba(5, back.data()), "EjectFlush_HostReloadReadOk");
            expect(back == written, "EjectFlush_WritePersistedToHost");
            app.shutdown();
        }
        std::filesystem::remove(d);
    }

    // --- Eject Cartridge: unload + reset (skip-gated on the FM-PAC ROM). ---
    {
        const std::string fmpac = std::string(SONY_MSX_ROMS_DIR) + "/fmpac.rom";
        if (std::filesystem::exists(fmpac)) {
            sony_msx::frontend::Sdl3AppConfig config = base_config();
            config.fmpac_sram_path = (dir / "eject_fmpac.sram").string();
            sony_msx::frontend::Sdl3App app(config);
            if (app.init()) {
                app.apply_open_cartridge(1, fmpac);
                expect(app.machine().cartridge_slot1().loaded(), "EjectCart_LoadedBeforeEject");
                for (int i = 0; i < 3; ++i) {
                    app.run_one_frame();
                }
                app.eject_cartridge(1);
                expect(!app.machine().cartridge_slot1().loaded(), "EjectCart_Unloaded");
                // Cart removal implies a reset.
                expect(app.machine().elapsed_cycles() == 0, "EjectCart_ResetToPowerOn");
                app.shutdown();
            }
        } else {
            std::cout << "SKIP EjectCartridge: roms/fmpac.rom not present\n";
        }
    }

    std::filesystem::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppEject_Integration cases passed\n";
    return 0;
}
