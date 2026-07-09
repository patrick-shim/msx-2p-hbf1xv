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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/cpm_bdos_harness.h"
#include "machine/hbf1xv_machine.h"

// Suite: Hbf1xvM24Zexall_System (M24-S3, backlog C3 -- ZEXDOC/ZEXALL full
// parity sweep, THE first point any real GPL binary is loaded this cycle).
//
// Loads the REAL, committed references/zexall/zexall.com and (separately)
// references/zexall/zexdoc.com and runs each to genuine CP/M warm-boot
// completion via the GENERIC CpmBdosHarness, in-process (no subprocess
// spawn). Asserts:
//   1. finished == true for BOTH suites (a genuine warm-boot completion, NOT
//      a budget-exhaustion timeout silently accepted as success).
//   2. Exactly 67 PASS-or-FAIL markers ("  OK" / "  ERROR **** crc
//      expected:") appear in each captured output stream -- these are
//      OBSERVED RUNTIME OUTPUT of the binary (the same category of thing as
//      this project's existing practice of capturing raw openMSX Tcl
//      session output), NOT transcribed source (SS1.5 of
//      docs/m24-planner-package.md). The marker-scan here is a simple sanity
//      count (not the full per-group parser -- that is tools/zexall-report.py,
//      which additionally guards against decoy-substring edge cases, see
//      R-M24-4); this test independently confirmed no group's own printed
//      name contains "OK"/"ERROR" as a substring (`grep -i` over the tmsg
//      call sites), so a bare substring count is a safe, sufficient sanity
//      gate at this level.
//   3. Reports the REAL per-suite PASS/FAIL disposition to stderr for
//      convenient inspection alongside the ctest log (the full structured
//      table is produced by tools/zexall-report.py from this test's/the
//      --cpm-run CLI's captured-output logs).
//
// Wiring: SONY_MSX_ZEXALL_DIR is a compile definition (tests/CMakeLists.txt)
// mirroring the established SONY_MSX_BIOS_DIR pattern EXACTLY, per DEC-0016's
// standing watch-item -- a test needing a real asset path must wire its own
// compile definition or it silently degrades under ctest's differing working
// directory. This test fails LOUDLY (non-zero exit, explicit stderr message)
// if either .com file cannot be opened, rather than silently treating a
// missing/misconfigured asset path as a vacuous pass.
//
// Runtime disclosure: this test genuinely executes the full combinatorial
// sweep of both real exercisers (~1.7 million "instruction under test"
// executions each, per docs/m24-planner-package.md SS1.3 A-M24-6, PLUS this
// project's own driver/bookkeeping overhead) -- empirically measured to take
// on the order of tens of minutes total wall-clock time (see
// docs/m24-implementation-report.md for the actual measured figure). This is
// a disclosed, deliberate consequence of Acceptance Criterion 3 (a genuine,
// non-truncated run), not an oversight.

namespace {

using sony_msx::machine::CpmBdosHarness;
using sony_msx::machine::Hbf1xvMachine;

// A generous instruction budget informed by this milestone's own empirical
// measurement (docs/m24-implementation-report.md): comfortably above the
// REAL measured completion count for either suite, so a genuine engine
// regression (not budget exhaustion) is what would cause `finished` to read
// false here.
constexpr std::uint64_t kMaxInstructions = 40'000'000'000ULL;

std::vector<std::uint8_t> read_file_or_die(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "FATAL: cannot open required ZEXALL/ZEXDOC asset: " << path
                  << " (SONY_MSX_ZEXALL_DIR wiring or references/zexall/ contents missing)\n";
        std::exit(1);
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        std::cerr << "FATAL: required ZEXALL/ZEXDOC asset is empty: " << path << "\n";
        std::exit(1);
    }
    return bytes;
}

// Non-overlapping occurrence count of `marker` within `haystack`, scanning in
// order of appearance -- OBSERVED OUTPUT recognition, not transcribed source
// (see the file-level comment above / planner package SS1.5).
int count_marker_occurrences(const std::string& haystack, const std::string& marker) {
    int count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(marker, pos)) != std::string::npos) {
        ++count;
        pos += marker.size();
    }
    return count;
}

struct SuiteResult {
    bool finished = false;
    std::uint64_t instructions_executed = 0;
    int ok_markers = 0;
    int error_markers = 0;
    std::size_t unexpected_bdos_calls = 0;
};

SuiteResult run_suite(const std::string& com_path) {
    const std::vector<std::uint8_t> image = read_file_or_die(com_path);

    Hbf1xvMachine machine;
    machine.cold_boot();
    const auto load_result = CpmBdosHarness::load_com(machine, image);
    if (load_result != CpmBdosHarness::LoadResult::Ok) {
        std::cerr << "FATAL: " << com_path << " failed to load (top-of-memory collision)\n";
        std::exit(1);
    }

    const auto run_result = CpmBdosHarness::run(machine, kMaxInstructions);
    const std::string captured(run_result.captured_output.begin(), run_result.captured_output.end());

    SuiteResult result;
    result.finished = run_result.finished;
    result.instructions_executed = run_result.instructions_executed;
    result.ok_markers = count_marker_occurrences(captured, "  OK");
    result.error_markers = count_marker_occurrences(captured, "  ERROR **** crc expected:");
    result.unexpected_bdos_calls = run_result.unexpected_bdos_calls.size();
    return result;
}

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    int failures = 0;

#ifndef SONY_MSX_ZEXALL_DIR
    std::cerr << "FATAL: SONY_MSX_ZEXALL_DIR compile definition is missing (tests/CMakeLists.txt wiring)\n";
    return 1;
#else
    const std::string zexall_dir = SONY_MSX_ZEXALL_DIR;

    const SuiteResult zexall = run_suite(zexall_dir + "/zexall.com");
    std::cerr << "ZEXALL: finished=" << zexall.finished
              << " instructions_executed=" << zexall.instructions_executed
              << " ok_markers=" << zexall.ok_markers << " error_markers=" << zexall.error_markers
              << " unexpected_bdos_calls=" << zexall.unexpected_bdos_calls << "\n";
    failures += expect_true(zexall.finished, "Zexall_WarmBootReached_FinishedTrue") ? 0 : 1;
    failures += expect_true(zexall.ok_markers + zexall.error_markers == 67,
                             "Zexall_ExactlySixtySevenGroupVerdicts")
                    ? 0
                    : 1;
    failures += expect_true(zexall.unexpected_bdos_calls == 0, "Zexall_NoUnexpectedBdosCalls") ? 0 : 1;
    failures += expect_true(zexall.error_markers == 0, "Zexall_ZeroErrorMarkers") ? 0 : 1;

    const SuiteResult zexdoc = run_suite(zexall_dir + "/zexdoc.com");
    std::cerr << "ZEXDOC: finished=" << zexdoc.finished
              << " instructions_executed=" << zexdoc.instructions_executed
              << " ok_markers=" << zexdoc.ok_markers << " error_markers=" << zexdoc.error_markers
              << " unexpected_bdos_calls=" << zexdoc.unexpected_bdos_calls << "\n";
    failures += expect_true(zexdoc.finished, "Zexdoc_WarmBootReached_FinishedTrue") ? 0 : 1;
    failures += expect_true(zexdoc.ok_markers + zexdoc.error_markers == 67,
                             "Zexdoc_ExactlySixtySevenGroupVerdicts")
                    ? 0
                    : 1;
    failures += expect_true(zexdoc.unexpected_bdos_calls == 0, "Zexdoc_NoUnexpectedBdosCalls") ? 0 : 1;
    failures += expect_true(zexdoc.error_markers == 0, "Zexdoc_ZeroErrorMarkers") ? 0 : 1;

    if (zexall.error_markers > 0 || zexdoc.error_markers > 0) {
        std::cerr << "SIGNIFICANT FINDING: at least one group reported FAIL ('  ERROR **** crc "
                     "expected:'). See docs/m24-implementation-report.md for the required triage "
                     "disposition (Acceptance Criterion 4) -- this is NOT silently treated as a pass.\n";
    }

    return failures == 0 ? 0 : 1;
#endif
}
