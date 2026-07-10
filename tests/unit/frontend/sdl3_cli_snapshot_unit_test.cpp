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

#include <iostream>
#include <string>
#include <vector>

#include "frontend/sdl3_cli.h"

// Suite: Frontend_Sdl3CliSnapshot_Unit (M36 Phase 3 slice S5, parse-regression
// guard). parse_sdl3_cli() lives in sony_msx_core (no SDL3 dependency), so this
// is a plain headless unit test. Mirrors the M27 AC-10 parse-regression shape:
// the new --snapshot flag parses, and every pre-existing case stays green (the
// default is std::nullopt = OFF).

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

sony_msx::frontend::ParsedSdl3Cli parse(const std::vector<std::string>& args) {
    return sony_msx::frontend::parse_sdl3_cli(args);
}

}  // namespace

int main() {
    // --- --snapshot <dir> is captured; no errors. ---
    {
        const auto p = parse({"--snapshot", "out/snaps"});
        expect(p.errors.empty(), "Snapshot_NoErrors");
        expect(p.snapshot_dir.has_value() && *p.snapshot_dir == "out/snaps", "Snapshot_DirCaptured");
    }

    // --- Absent flag -> default OFF (std::nullopt). Regression guard. ---
    {
        const auto p = parse({"--bios-dir", "bios", "--disk", "a.dsk"});
        expect(p.errors.empty(), "Absent_NoErrors");
        expect(!p.snapshot_dir.has_value(), "Absent_SnapshotDirNullopt");
        // Pre-existing fields still parse.
        expect(p.bios_dir.has_value() && *p.bios_dir == "bios", "Absent_BiosStillParses");
        expect(p.disk_paths.size() == 1 && p.disk_paths[0] == "a.dsk", "Absent_DiskStillParses");
    }

    // --- Missing value argument is a hard error (never silently swallowed). ---
    {
        const auto p = parse({"--snapshot"});
        expect(!p.errors.empty(), "MissingValue_IsError");
        expect(!p.snapshot_dir.has_value(), "MissingValue_NoDir");
    }

    // --- Coexists with the other additive M27 flags, all independent. ---
    {
        const auto p = parse({"--dump-state", "s.dump", "--snapshot", "snaps", "--max-frames", "10"});
        expect(p.errors.empty(), "Combined_NoErrors");
        expect(p.snapshot_dir.has_value() && *p.snapshot_dir == "snaps", "Combined_Snapshot");
        expect(p.dump_state_filename.has_value() && *p.dump_state_filename == "s.dump",
               "Combined_DumpState");
        expect(p.max_frames.has_value() && *p.max_frames == 10u, "Combined_MaxFrames");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_Sdl3CliSnapshot_Unit cases passed\n";
    return 0;
}
