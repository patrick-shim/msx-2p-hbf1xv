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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include "frontend/sdl3_app.h"

// Suite: Frontend_Sdl3AppDebugSession_Integration (M27-S4, "Production
// Hardening + Debug/Test Tooling" items 1/4, docs/m27-planner-package.md
// §2.2). Mirrors the headless --debug-session mode's own state-dump/CPU-
// trace/event-log assertions (M27-S2/S3), under the dummy SDL3 drivers, via
// run_one_frame() called a bounded number of times (NEVER run_interactive(),
// A-M26-8) -- proving the additive Sdl3AppConfig fields
// (dump_state_filename/trace_cpu_filename/event_log_filename) are genuinely
// wired, AND (the hard regression guard, §4 Acceptance Criterion 10) that
// every pre-existing M26 Sdl3App/Sdl3AppConfig behavior is UNAFFECTED when
// these new fields are left at their default (std::nullopt) state.

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

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    set_dummy_drivers();

    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m27-sdl3-app-debug-session-integration-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);

    // --- Case 1: dump_state_filename/trace_cpu_filename/event_log_filename
    // SET -> real, well-formed output files, content matching the same
    // Hbf1xvMachine APIs the headless mode uses. ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        config.dump_state_filename = "session.state";
        config.trace_cpu_filename = "session.trace";
        config.event_log_filename = "session.log";

        sony_msx::frontend::Sdl3App app(std::move(config));
        app.machine().set_debug_root(temp_root / "set");

        const bool init_ok = app.init();
        expect(init_ok, "FieldsSet_InitSucceeds");
        if (!init_ok) {
            std::cerr << "  last_error=" << app.last_error() << "\n";
        } else {
            constexpr int kFrameCount = 2;
            for (int i = 0; i < kFrameCount; ++i) {
                app.run_one_frame();
            }
            // Directly callable after a bounded run_one_frame() loop (never
            // run_interactive(), which ctest never exercises, A-M26-8).
            app.flush_debug_session_outputs();

            const std::filesystem::path state_path = temp_root / "set" / "traces" / "session.state";
            const std::filesystem::path trace_path = temp_root / "set" / "traces" / "session.trace";
            const std::filesystem::path log_path = temp_root / "set" / "logs" / "session.log";
            expect(std::filesystem::exists(state_path), "FieldsSet_StateDumpFileCreated");
            expect(std::filesystem::exists(trace_path), "FieldsSet_CpuTraceFileCreated");
            expect(std::filesystem::exists(log_path), "FieldsSet_EventLogFileCreated");

            const std::string state_content = read_file(state_path);
            expect(state_content.rfind("HBF1XV-STATE-DUMP v1", 0) == 0, "FieldsSet_StateDump_StartsWithFormatTag");
            expect(!read_file(trace_path).empty(), "FieldsSet_CpuTrace_NonEmpty");

            // Item 4/R-M27-2: set_event_logging_enabled(true) was called
            // BEFORE cold_boot() inside Sdl3App::init(), so the Reset event
            // is genuinely present at sequence 0.
            const std::string log_content = read_file(log_path);
            expect(log_content.rfind("EVT=0000 T=0 TYPE=RESET", 0) == 0,
                   "FieldsSet_EventLog_ResetEventPresentAtSequence0_R_M27_2_OrderingHonored");

            app.shutdown();
        }
    }

    // --- Case 2 (REGRESSION GUARD, §4 Acceptance Criterion 10): every new
    // field left unset (the default) -> a genuine no-op, mirroring EVERY
    // pre-existing M26 Sdl3App test's own observable behavior exactly (no
    // new files created anywhere, machine still advances normally). ---
    {
        sony_msx::frontend::Sdl3AppConfig config;
        config.bios_dir = "bios";
        config.hidden_window = true;
        // dump_state_filename/trace_cpu_filename/event_log_filename/
        // input_script_path all left at their std::nullopt default.

        sony_msx::frontend::Sdl3App app(std::move(config));
        const std::filesystem::path unset_root = temp_root / "unset";
        app.machine().set_debug_root(unset_root);

        const bool init_ok = app.init();
        expect(init_ok, "FieldsUnset_InitSucceeds");
        if (init_ok) {
            const std::uint64_t cycles_before = app.machine().elapsed_cycles();
            constexpr int kFrameCount = 2;
            for (int i = 0; i < kFrameCount; ++i) {
                app.run_one_frame();
            }
            const std::uint64_t cycles_after = app.machine().elapsed_cycles();
            expect(cycles_after > cycles_before, "FieldsUnset_MachineAdvancesNormally_UnaffectedByNewFields");

            app.flush_debug_session_outputs();  // Must be a genuine no-op.
            expect(!std::filesystem::exists(unset_root), "FieldsUnset_NoDebugOutputDirectoryEverCreated_TrueNoOp");
            expect(!app.machine().event_logging_enabled(),
                   "FieldsUnset_EventLoggingRemainsDisabled_DefaultUnaffected");

            app.shutdown();
        }
    }

    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3AppDebugSession_Integration cases passed\n";
    return 0;
}
