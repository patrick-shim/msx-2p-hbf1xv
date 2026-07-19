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

// Suite: Frontend_Sdl3AppHiddenWindowGate_Integration (M56, DEC-0084, planner §9
// test 9; slice S2/S4, R6).
//
// The determinism guard: under --hidden-window (every ctest, per A4) the M56
// dialog/reset machinery is INERT. The dialog mailbox mutex is never allocated,
// so the dialog LAUNCHERS (open_disk_dialog / open_cartridge_dialog /
// new_blank_disk_dialog) early-return, the mailbox never becomes pending, and the
// drain (gated on menu_) never runs -- so no apply_* ever fires. A bounded
// run_one_frame() loop advances frame/cycle exactly as the pre-M56 headless
// oracle, proving the whole feature set perturbs nothing on the deterministic path.

#include <SDL3/SDL.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "frontend/sdl3_app.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

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

    sony_msx::frontend::Sdl3AppConfig config;
    config.hidden_window = true;
    config.bios_dir = SONY_MSX_BIOS_DIR;

    sony_msx::frontend::Sdl3App app(config);
    const bool init_ok = app.init();
    expect(init_ok, "Init_SucceedsUnderDummyDrivers");
    if (!init_ok) {
        std::cerr << "  last_error=" << app.last_error() << "\n";
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // No ImGui menu under --hidden-window (so the drain is never even called).
    expect(!app.menu_active(), "HiddenWindow_NoMenu");

    const std::uint64_t disks_before = app.disk_count();
    const std::uint64_t frames_before = app.machine().frame_count();
    const std::uint64_t cycles_before = app.machine().elapsed_cycles();

    // The dialog launchers are INERT headless (null mailbox mutex): they must not
    // launch a dialog, set the mailbox pending, or mutate any machine state.
    app.open_disk_dialog();
    app.open_cartridge_dialog(1);
    app.open_cartridge_dialog(2);
    app.new_blank_disk_dialog();

    constexpr int kFrameCount = 5;
    for (int i = 0; i < kFrameCount; ++i) {
        app.run_one_frame();  // drain gated on menu_ -> never runs; no apply_* ever fires
    }

    // The mailbox never fired: no disk mounted, no cart inserted, no reset.
    expect(app.disk_count() == disks_before, "Gate_NoDiskEverMounted");
    expect(!app.machine().cartridge_slot1().loaded(), "Gate_NoCartSlot1");
    expect(!app.machine().cartridge_slot2().loaded(), "Gate_NoCartSlot2");

    // Frame/cycle advanced exactly by the headless oracle (no reset dropped them).
    expect(app.machine().frame_count() - frames_before == static_cast<std::uint64_t>(kFrameCount),
           "Gate_FrameCountAdvancesByExactlyN");
    const std::uint64_t expected_minimum =
        static_cast<std::uint64_t>(kFrameCount) * app.machine().frame_cycles_per_frame();
    expect(app.machine().elapsed_cycles() - cycles_before >= expected_minimum,
           "Gate_ElapsedCyclesAtLeastHandComputedMinimum");
    expect(!app.menu_active(), "Gate_StillNoMenuAfterFrames");

    app.shutdown();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppHiddenWindowGate_Integration cases passed\n";
    return 0;
}
