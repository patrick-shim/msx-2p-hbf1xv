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
#include <iostream>
#include <vector>

#include "machine/cpm_bdos_harness.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_CpmBdosHarness_Unit (M24-S1, backlog C3 -- ZEXDOC/ZEXALL full
// parity sweep).
//
// Proves the GENERIC CpmBdosHarness mechanism in complete isolation from the
// real GPL zexall.com/zexdoc.com binaries, using ONLY tiny, hand-assembled Z80
// programs written for THIS test file. None of these fixtures are derived
// from or resemble zexall.z80/zexdoc.z80's own content in any way (different
// structure, different messages, different byte layout) -- they exist purely
// to exercise the harness's own CP/M-loading-convention mechanics (the load
// base, the top-of-memory word, the CALL-5/JP-0 traps, and the RET
// synthesis) before the real binaries are ever loaded (see
// tests/system/zexall_system_test.cpp for that later step).
//
// Every fixture is documented byte-by-byte below. Standard, well-known Z80
// opcodes used: 0x11 = LD DE,nn; 0x0E = LD C,n; 0xCD = CALL nn; 0x1E = LD E,n;
// 0xC3 = JP nn.

namespace {

using sony_msx::machine::CpmBdosHarness;
using sony_msx::machine::Hbf1xvMachine;

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Fixture A: exercises BOTH BDOS functions (C=9 print-string, C=2 char-out),
// FOUR separate BDOS calls total (proving the RET-synthesis correctly resumes
// execution after each one, not just the first), then a clean CP/M warm-boot.
//
// Program (loaded at base 0x0100):
//   0x0100: 11 21 01   LD DE,0x0121      ; DE -> "HI$" (see layout below)
//   0x0103: 0E 09      LD C,9            ; BDOS function 9 = print $-string
//   0x0105: CD 05 00   CALL 5            ; -> captures "HI"
//   0x0108: 1E 21      LD E,'!'          ; (0x21)
//   0x010A: 0E 02      LD C,2            ; BDOS function 2 = console char-out
//   0x010C: CD 05 00   CALL 5            ; -> captures '!'
//   0x010F: 11 24 01   LD DE,0x0124      ; DE -> "BYE$"
//   0x0112: 0E 09      LD C,9
//   0x0114: CD 05 00   CALL 5            ; -> captures "BYE"
//   0x0117: 1E 3F      LD E,'?'          ; (0x3F)
//   0x0119: 0E 02      LD C,2
//   0x011B: CD 05 00   CALL 5            ; -> captures '?'
//   0x011E: C3 00 00   JP 0              ; CP/M warm boot -> finished = true
//   0x0121: 48 49 24         "HI$"
//   0x0124: 42 59 45 24      "BYE$"
// Expected captured_output (in exact order): "HI" + '!' + "BYE" + '?'
//                                          = 48 49 21 42 59 45 3F
std::vector<std::uint8_t> multi_bdos_fixture() {
    return {
        0x11, 0x21, 0x01,  // LD DE,0x0121
        0x0E, 0x09,        // LD C,9
        0xCD, 0x05, 0x00,  // CALL 5           (1st: C=9 "HI")
        0x1E, 0x21,        // LD E,'!'
        0x0E, 0x02,        // LD C,2
        0xCD, 0x05, 0x00,  // CALL 5           (2nd: C=2 '!')
        0x11, 0x24, 0x01,  // LD DE,0x0124
        0x0E, 0x09,        // LD C,9
        0xCD, 0x05, 0x00,  // CALL 5           (3rd: C=9 "BYE")
        0x1E, 0x3F,        // LD E,'?'
        0x0E, 0x02,        // LD C,2
        0xCD, 0x05, 0x00,  // CALL 5           (4th: C=2 '?')
        0xC3, 0x00, 0x00,  // JP 0             (warm boot)
        'H', 'I', 0x24,           // "HI$"  at 0x0121
        'B', 'Y', 'E', 0x24,      // "BYE$" at 0x0124
    };
}

// Fixture B: a single BDOS call with an "unexpected" function code (99, i.e.
// neither 9 nor 2), then a clean warm boot. Proves unexpected_bdos_calls is
// populated (non-fatally) and the RET-synthesis still resumes correctly
// regardless of which function was dispatched.
//   0x0100: 0E 63      LD C,99  (0x63)
//   0x0102: CD 05 00   CALL 5
//   0x0105: C3 00 00   JP 0
std::vector<std::uint8_t> unexpected_bdos_fixture() {
    return {
        0x0E, 0x63,        // LD C,99
        0xCD, 0x05, 0x00,  // CALL 5
        0xC3, 0x00, 0x00,  // JP 0
    };
}

// Fixture C: an infinite loop that never reaches PC==0 or PC==5 -- proves an
// out-of-budget run honestly reports finished=false.
//   0x0100: C3 00 01   JP 0x0100   ; jumps to itself, forever
std::vector<std::uint8_t> infinite_loop_fixture() {
    return {
        0xC3, 0x00, 0x01,  // JP 0x0100 (self)
    };
}

}  // namespace

int main() {
    int failures = 0;

    // --- The defensive size-guard rejects an oversized image (exact boundary:
    // --- 0x0100 + size == top_of_memory must be REJECTED, not merely
    // --- "greater than"). ---------------------------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const std::vector<std::uint8_t> oversized(0x10, 0xAA);  // 0x100+0x10 == 0x110
        const auto result = CpmBdosHarness::load_com(machine, oversized, 0x0110);
        failures += expect_true(result == CpmBdosHarness::LoadResult::ImageTooLargeForMemory,
                                 "LoadCom_ExactlyAtTopOfMemoryBoundary_Rejected")
                        ? 0
                        : 1;
    }

    // --- The same boundary, one byte smaller, is accepted (0x100+0x0F ---
    // --- == 0x10F < 0x110). --------------------------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const std::vector<std::uint8_t> fits(0x0F, 0xAA);
        const auto result = CpmBdosHarness::load_com(machine, fits, 0x0110);
        failures += expect_true(result == CpmBdosHarness::LoadResult::Ok,
                                 "LoadCom_OneByteUnderTopOfMemoryBoundary_Accepted")
                        ? 0
                        : 1;
    }

    // --- load_com writes the top-of-memory word little-endian at 0x0006 and
    // --- sets PC = 0x0100. ---------------------------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const auto program = multi_bdos_fixture();
        const auto result = CpmBdosHarness::load_com(machine, program, 0xFF00);
        const bool ok = result == CpmBdosHarness::LoadResult::Ok &&
                         machine.read_memory(0x0006) == 0x00 &&  // low(0xFF00)
                         machine.read_memory(0x0007) == 0xFF &&  // high(0xFF00)
                         machine.cpu().state().regs().pc == 0x0100;
        failures += expect_true(ok, "LoadCom_TopOfMemoryWordAndPc_SeededCorrectly") ? 0 : 1;
    }

    // --- The captured-output buffer accumulates BOTH a C=9 string and C=2
    // --- characters, in the correct order, across FOUR sequential BDOS
    // --- calls (proving the RET-synthesis resumes correctly after every one,
    // --- not just the first) -- and the CP/M warm-boot trap sets
    // --- finished=true and stops cleanly. -----------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const auto program = multi_bdos_fixture();
        const auto load_result = CpmBdosHarness::load_com(machine, program);
        const auto run_result = CpmBdosHarness::run(machine, 10'000);

        const std::vector<std::uint8_t> expected_output = {'H', 'I', '!', 'B', 'Y', 'E', '?'};
        failures += expect_true(load_result == CpmBdosHarness::LoadResult::Ok,
                                 "MultiBdosFixture_Load_Ok")
                        ? 0
                        : 1;
        failures += expect_true(run_result.finished, "MultiBdosFixture_WarmBootReached_FinishedTrue") ? 0 : 1;
        failures += expect_true(run_result.captured_output == expected_output,
                                 "MultiBdosFixture_CapturedOutput_InterleavedOrderCorrect")
                        ? 0
                        : 1;
        failures += expect_true(run_result.unexpected_bdos_calls.empty(),
                                 "MultiBdosFixture_NoUnexpectedBdosCalls")
                        ? 0
                        : 1;
        failures += expect_true(run_result.instructions_executed > 0,
                                 "MultiBdosFixture_RealInstructionsWereExecuted")
                        ? 0
                        : 1;
    }

    // --- Any OTHER BDOS function code (register C not 9 or 2) is recorded as
    // --- a non-fatal "unexpected BDOS call" diagnostic, and execution still
    // --- resumes correctly afterward (the program reaches its own warm
    // --- boot). ---------------------------------------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const auto program = unexpected_bdos_fixture();
        CpmBdosHarness::load_com(machine, program);
        const auto run_result = CpmBdosHarness::run(machine, 10'000);

        failures += expect_true(run_result.finished, "UnexpectedBdosFixture_WarmBootReached_FinishedTrue")
                        ? 0
                        : 1;
        failures += expect_true(
                        run_result.unexpected_bdos_calls.size() == 1 && run_result.unexpected_bdos_calls[0] == 99,
                        "UnexpectedBdosFixture_FunctionCodeRecorded_NinetyNine")
                        ? 0
                        : 1;
        failures += expect_true(run_result.captured_output.empty(),
                                 "UnexpectedBdosFixture_NoOutputCaptured")
                        ? 0
                        : 1;
    }

    // --- An out-of-budget run (a fixture that deliberately never reaches
    // --- JP 0) reports finished=false HONESTLY, rather than silently
    // --- treating budget exhaustion as success. -----------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const auto program = infinite_loop_fixture();
        CpmBdosHarness::load_com(machine, program);
        constexpr std::uint64_t kBudget = 5;
        const auto run_result = CpmBdosHarness::run(machine, kBudget);

        failures += expect_true(!run_result.finished,
                                 "InfiniteLoopFixture_OutOfBudget_FinishedFalse")
                        ? 0
                        : 1;
        failures += expect_true(run_result.instructions_executed == kBudget,
                                 "InfiniteLoopFixture_OutOfBudget_InstructionsExecutedEqualsBudget")
                        ? 0
                        : 1;
    }

    return failures == 0 ? 0 : 1;
}
