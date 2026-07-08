#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AppInputScript_Integration (M27-S7, "Production
// Hardening + Debug/Test Tooling" item 3, docs/m27-planner-package.md
// §2.4/§3 M27-S7 -- Acceptance Criterion 7's "same script drives both
// executables to the same final state" claim, the SDL3-configuration half).
//
// Runs the SAME documented input-script text (T=15/30/50 KEY=A/SPACE
// DOWN/UP) tests/integration/machine/hbf1xv_m27_input_script_integration_test.cpp
// (the headless configuration's own test) drives, through
// Sdl3App::run_one_frame() (NEVER run_interactive(), A-M26-8) instead of a
// raw step_cpu_instruction() loop, asserting the SAME documented,
// hand-computed FINAL keyboard-matrix state: after settling, both "A" and
// "SPACE" end up released. An independent proof in this SEPARATE build
// configuration (`ctest` case), not a single test spanning both
// configurations at once, per §4 Acceptance Criterion 12's precise framing.
//
// A small flat-RAM NOP+HALT driver program (mirrors src/main.cpp's own
// established map_flat_ram()+load_memory() technique) is loaded AFTER
// Sdl3App::init() completes (init() itself performs cold_boot(), which
// would otherwise wipe out any earlier setup) so ONE run_one_frame() call
// (which steps AT LEAST one whole frame's worth of cycles, comfortably
// covering the script's T<=50 window) reaches HALT well before the frame's
// cycle budget is exhausted, then idles harmlessly for the remainder.

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

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m27-sdl3-app-input-script-integration-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root, ec);

    // The SAME script text the headless configuration's own test uses.
    const std::filesystem::path script_path = temp_root / "script.txt";
    {
        std::ofstream out(script_path, std::ios::binary | std::ios::trunc);
        out << "HBF1XV-INPUT-SCRIPT v1\n"
               "T=15 KEY=A DOWN\n"
               "T=30 KEY=A UP\n"
               "T=30 KEY=SPACE DOWN\n"
               "T=50 KEY=SPACE UP\n"
               "[END]\n";
    }

    sony_msx::frontend::Sdl3AppConfig config;
    config.bios_dir = "bios";
    config.hidden_window = true;
    config.input_script_path = script_path.string();

    sony_msx::frontend::Sdl3App app(std::move(config));
    const bool init_ok = app.init();
    expect(init_ok, "Init_WithInputScriptConfigured_Succeeds");
    if (!init_ok) {
        std::cerr << "  last_error=" << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // A flat-RAM NOP+HALT driver program, loaded AFTER init() (which already
    // performed cold_boot()) -- 2000 NOPs (10,000 T-states, comfortably
    // covering the script's T<=50 window) then HALT, well under one whole
    // frame's cycle budget (frame_cycles_per_frame(), 228*262=59,736), so a
    // SINGLE run_one_frame() call reaches HALT and then idles harmlessly for
    // the remainder -- never wandering into unprogrammed memory.
    app.machine().map_flat_ram();
    std::vector<std::uint8_t> program(2000, 0x00);
    program.push_back(0x76);  // HALT
    app.machine().load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
    app.machine().cpu().state().regs().pc = 0x0000;

    app.run_one_frame();

    // "A" -> (row=2, column=6); "SPACE" -> (row=8, column=0), per
    // msx_key_names.cpp (the SAME oracle the headless test uses).
    expect(!app.machine().keyboard().key(2, 6), "FinalState_AKey_Released_SameOracleAsHeadlessConfiguration");
    expect(!app.machine().keyboard().key(8, 0), "FinalState_SpaceKey_Released_SameOracleAsHeadlessConfiguration");

    app.shutdown();
    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppInputScript_Integration cases passed\n";
    return 0;
}
