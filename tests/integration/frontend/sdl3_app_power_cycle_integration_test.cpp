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

// Suite: Frontend_Sdl3AppPowerCycle_Integration (M57, DEC-0085-AMENDMENT-A,
// addendum §5). The permanent oracle for Sdl3App::request_power_cycle() -- the
// live RAM-size change (Machine>RAM). Driven under hidden_window=true (no menu,
// no dialog), the request_reset() precedent. Cases:
//   (a) fresh-boot equivalence: power_cycle(Y) dump == fresh-boot(Y) dump (bare).
//   (b) size-change proof: dram_size()==Y AND the M42 #FC segment walk (Y/16
//       distinct banks + fold-wrap) -- the mapper segment count actually moved.
//   (c) disk survival: an in-session write marker survives the cycle.
//   (d) transactional abort: an occupied cart whose file is deleted pre-cycle
//       ABORTS with the OLD machine intact (same RAM, cart still loaded).
//   (e) hidden-window gate: menu_active()==false; the public seam drives it with
//       menu_ == null.

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/sdl3_app.h"

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

sony_msx::frontend::Sdl3AppConfig config_ram(const std::size_t ram_bytes) {
    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;
    config.ram_bytes = ram_bytes;
    return config;
}

// The M42 #FC page-0 segment walk (hbf1xv_ram_size_integration_test.cpp) applied
// to a live Sdl3App machine: proves the mapper exposes exactly size_kb/16 distinct
// banks with fold-wrap at size_kb/16 (i.e. the RAM size genuinely changed).
void expect_segment_size(sony_msx::frontend::Sdl3App& app, const int size_kb, const std::string& tag) {
    auto& m = app.machine();
    const int num = size_kb / 16;
    expect(m.dram_size() == static_cast<std::size_t>(size_kb) * 1024u, tag + "_dram_size==Y");
    m.map_flat_ram();
    for (int seg = 0; seg < num; ++seg) {
        m.debug_io_write(0xFC, static_cast<std::uint8_t>(seg));
        m.debug_bus_write(0x0000, static_cast<std::uint8_t>(0x10 + seg));
    }
    bool distinct = true;
    for (int seg = 0; seg < num; ++seg) {
        m.debug_io_write(0xFC, static_cast<std::uint8_t>(seg));
        if (m.debug_bus_read(0x0000) != static_cast<std::uint8_t>(0x10 + seg)) {
            distinct = false;
        }
    }
    expect(distinct, tag + "_AllSegmentsDistinct_NoAliasing");
    m.debug_io_write(0xFC, 0x00);
    m.debug_bus_write(0x0000, 0xA0);
    m.debug_io_write(0xFC, static_cast<std::uint8_t>(num));  // first segment past the top
    m.debug_bus_write(0x0000, 0xB0);                          // writes the MIRROR of segment 0
    m.debug_io_write(0xFC, 0x00);
    expect(m.debug_bus_read(0x0000) == 0xB0, tag + "_FirstPastTop_FoldWrap");
}

std::filesystem::path make_temp(const std::string& stem, const std::vector<std::uint8_t>& bytes) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (stem + std::to_string(static_cast<unsigned long long>(SDL_GetPerformanceCounter())) + ".bin");
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return p;
}

}  // namespace

int main() {
    set_dummy_drivers();

    // ===== (a) fresh-boot equivalence + (b) size-change proof, per target. =====
    // One Sdl3App (each holds its own SDL init/window/audio) is live at a time --
    // dumps are captured as strings and the app released before the next, the
    // reset-determinism Oracle B pattern.
    for (const int y_kb : {64, 128, 256, 512}) {
        const std::size_t y = static_cast<std::size_t>(y_kb) * 1024u;
        const std::string tag = "PowerCycle" + std::to_string(y_kb);

        // app_A boots at 64 KB then power-cycles to Y; capture dumps + prove size.
        std::string dump_a0;
        std::string dump_a5;
        {
            sony_msx::frontend::Sdl3App app_a(config_ram(64u * 1024u));
            if (!app_a.init()) {
                expect(false, tag + "_AppA_Init");
                continue;
            }
            app_a.request_power_cycle(y);
            dump_a0 = app_a.machine().serialize_state_dump();
            for (int i = 0; i < 5; ++i) {
                app_a.run_one_frame();
            }
            dump_a5 = app_a.machine().serialize_state_dump();
            // (b) the mapper segment count actually moved to Y (this MUTATES RAM,
            // so it runs AFTER the dumps are captured).
            expect_segment_size(app_a, y_kb, tag);
            // (e) hidden-window: no ImGui context; the seam drove it with menu_==null.
            expect(!app_a.menu_active(), tag + "_menu_active_false");
            app_a.shutdown();
        }

        // app_B boots FRESH at Y; capture the same-frame dumps.
        std::string dump_b0;
        std::string dump_b5;
        {
            sony_msx::frontend::Sdl3App app_b(config_ram(y));
            if (!app_b.init()) {
                expect(false, tag + "_AppB_Init");
                continue;
            }
            dump_b0 = app_b.machine().serialize_state_dump();
            for (int i = 0; i < 5; ++i) {
                app_b.run_one_frame();
            }
            dump_b5 = app_b.machine().serialize_state_dump();
            app_b.shutdown();
        }

        // (a) power_cycle(Y) IS a fresh boot of size Y -- equal at frame 0 and 5.
        expect(!dump_a0.empty() && dump_a0 == dump_b0, tag + "_dump==FreshBoot(frame0)");
        expect(dump_a5 == dump_b5, tag + "_dump==FreshBoot(frame5)");
    }

    // ===== (c) disk survival with an in-session write marker. =====
    {
        // A 720 KB scratch disk (content need not be a valid FS for a raw-sector
        // round-trip). Written to a temp file so config.disk_paths can load it.
        const std::filesystem::path disk = make_temp("m57_pc_disk_", std::vector<std::uint8_t>(737280u, 0u));
        sony_msx::frontend::Sdl3AppConfig config = config_ram(128u * 1024u);
        config.disk_paths = {disk.string()};
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            expect(app.machine().disk_drive().image() != nullptr, "DiskSurvival_AttachedAtBoot");
            // Write an in-session marker sector (LBA 100) directly through the
            // live DiskImage (a real MSX write would land here too).
            std::vector<std::uint8_t> marker(512u, 0xC7);
            marker[0] = 0x5A;
            const bool wrote = app.machine().disk_image().write_lba(100u, marker.data());
            expect(wrote, "DiskSurvival_InSessionWrite");

            app.request_power_cycle(512u * 1024u);  // change RAM too, media survives

            expect(app.machine().disk_drive().image() != nullptr, "DiskSurvival_ReattachedAfterCycle");
            expect(app.machine().disk_drive().image() == &app.machine().disk_image(),
                   "DiskSurvival_PointerReattached");
            std::vector<std::uint8_t> back(512u, 0u);
            const bool read = app.machine().disk_image().read_lba(100u, back.data());
            expect(read && back[0] == 0x5A && back[1] == 0xC7, "DiskSurvival_MarkerSurvivedCycle");
            expect(app.machine().dram_size() == 512u * 1024u, "DiskSurvival_RamAlsoChanged");
            app.shutdown();
        } else {
            expect(false, "DiskSurvival_Init");
        }
        std::error_code ec;
        std::filesystem::remove(disk, ec);
    }

    // ===== (d) transactional abort when an occupied cart file vanishes. =====
    {
        // A 32 KB explicit-Mirrored ROM (deterministic, asset-free): loads at init
        // -> cart_path_[0] tracked. Then delete the file so the power-cycle
        // pre-read fails and MUST abort with the old machine intact.
        const std::filesystem::path rom = make_temp("m57_pc_cart_", std::vector<std::uint8_t>(32u * 1024u, 0xAA));
        sony_msx::frontend::Sdl3AppConfig config = config_ram(256u * 1024u);
        config.cart1_path = rom.string();
        config.cart1_type = sony_msx::devices::cartridge::CartridgeMapperType::Mirrored;
        config.cart1_type_explicit = true;   // pass-through: no content identification needed
        config.fmpac_sram_disabled = true;   // never touch a real .sram
        sony_msx::frontend::Sdl3App app(config);
        if (app.init()) {
            expect(app.machine().cartridge_slot1().loaded(), "Abort_CartLoadedAtBoot");
            expect(app.machine().dram_size() == 256u * 1024u, "Abort_RamIs256AtBoot");

            // Delete the cart file, then attempt a power-cycle to 512 KB.
            std::error_code ec;
            std::filesystem::remove(rom, ec);
            app.request_power_cycle(512u * 1024u);

            // Transactional abort: NOTHING changed -- RAM still 256, cart still in.
            expect(app.machine().dram_size() == 256u * 1024u, "Abort_RamUnchanged_StillRunning");
            expect(app.machine().cartridge_slot1().loaded(), "Abort_CartStillLoaded");
            app.shutdown();
        } else {
            expect(false, "Abort_Init");
            std::error_code ec;
            std::filesystem::remove(rom, ec);
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppPowerCycle_Integration cases passed\n";
    return 0;
}
