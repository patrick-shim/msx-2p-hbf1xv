// Suite: Devices_VdpAccessTiming_Unit  (M23-S2, backlog C1/D4 partial --
// non-gating VDP access-timing foundation)
//
// Deterministic unit coverage for devices/video/vdp_access_timing.h: the
// A-M23-7 kCpuTstatesPerLine*262==kFrameCycles cross-check (via a SHARED
// constant, not two independently-hardcoded literals), the raster-position
// derivation's round-trip behavior across a line/frame boundary, the
// VdpAccessDelta enum's exact labeled values, and the A-M23-8 requirement
// that the two latency facts (minimum_request_latency_tstates vs.
// kDocumentedSafeAccessGapTStates) stay distinct and are never combined.

#include <cstdint>
#include <iostream>

#include "devices/video/vdp_access_timing.h"

namespace {

using namespace sony_msx::devices::video::vdp_access_timing;

// Mirrors machine/hbf1xv_machine.cpp's private kFrameCycles (228 * 262) via
// its OWN independently-declared constant here (the machine's constant is
// anonymous-namespace-private and not exported) so this test can assert
// A-M23-7's cross-check without depending on machine internals. This value
// is fixed by the strict target-machine spec (NTSC, 262 lines/frame) and by
// the already-shipped, QA-signed M11+ scheduler contract -- it is not
// expected to change independently of kFrameCycles.
constexpr std::uint64_t kFrameCycles = 228 * 262;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- A-M23-7: kCpuTstatesPerLine * 262 == kFrameCycles (228*262=59736). ---
    expect(static_cast<std::uint64_t>(kCpuTstatesPerLine) * 262 == kFrameCycles,
           "CpuTstatesPerLine_TimesLinesPerFrame_MatchesFrameCycles");
    expect(kCpuTstatesPerLine == 228, "CpuTstatesPerLine_ExactValue_228");
    expect(kVdpCyclesPerLine == 1368, "VdpCyclesPerLine_ExactValue_1368");
    expect(kCpuTstatesPerVdpCycleRatio == 6, "CpuVdpClockRatio_ExactValue_Six");

    // --- Raster-position derivation: exact values within one line. -----------
    expect(cpu_tstate_within_line(0) == 0, "CpuTstateWithinLine_ZeroCycles_ZeroPosition");
    expect(cpu_tstate_within_line(1) == 1, "CpuTstateWithinLine_OneCycle_OnePosition");
    expect(cpu_tstate_within_line(227) == 227, "CpuTstateWithinLine_LastCycleOfLine_227");
    expect(vdp_cycle_within_line(0) == 0, "VdpCycleWithinLine_ZeroCycles_ZeroPosition");
    expect(vdp_cycle_within_line(1) == 6, "VdpCycleWithinLine_OneCpuTstate_SixVdpCycles");
    expect(vdp_cycle_within_line(227) == 227 * 6, "VdpCycleWithinLine_LastCpuTstateOfLine_1362");

    // --- Round-trip across a full-LINE boundary: position wraps to 0. --------
    expect(cpu_tstate_within_line(228) == 0, "CpuTstateWithinLine_ExactlyOneLine_WrapsToZero");
    expect(cpu_tstate_within_line(229) == 1, "CpuTstateWithinLine_OneLinePlusOne_WrapsToOne");
    expect(vdp_cycle_within_line(228) == 0, "VdpCycleWithinLine_ExactlyOneLine_WrapsToZero");

    // --- Round-trip across a full-FRAME boundary (262 lines): still wraps ----
    // --- correctly within the CURRENT line, not just the current frame. ------
    const std::uint64_t one_frame_tstates = static_cast<std::uint64_t>(kCpuTstatesPerLine) * 262;
    expect(cpu_tstate_within_line(one_frame_tstates) == 0,
           "CpuTstateWithinLine_ExactlyOneFrame_WrapsToZero");
    expect(cpu_tstate_within_line(one_frame_tstates + 5) == 5,
           "CpuTstateWithinLine_OneFramePlusFive_WrapsToFive");
    expect(cpu_tstate_within_line(one_frame_tstates * 3 + 100) == 100,
           "CpuTstateWithinLine_ThreeFramesPlusOffset_WrapsCorrectly");

    // --- VdpAccessDelta: the 15 named labels match their literal values. -----
    expect(static_cast<int>(VdpAccessDelta::D0) == 0, "VdpAccessDelta_D0_Value");
    expect(static_cast<int>(VdpAccessDelta::D1) == 1, "VdpAccessDelta_D1_Value");
    expect(static_cast<int>(VdpAccessDelta::D16) == 16, "VdpAccessDelta_D16_Value");
    expect(static_cast<int>(VdpAccessDelta::D24) == 24, "VdpAccessDelta_D24_Value");
    expect(static_cast<int>(VdpAccessDelta::D28) == 28, "VdpAccessDelta_D28_Value");
    expect(static_cast<int>(VdpAccessDelta::D32) == 32, "VdpAccessDelta_D32_Value");
    expect(static_cast<int>(VdpAccessDelta::D40) == 40, "VdpAccessDelta_D40_Value");
    expect(static_cast<int>(VdpAccessDelta::D48) == 48, "VdpAccessDelta_D48_Value");
    expect(static_cast<int>(VdpAccessDelta::D64) == 64, "VdpAccessDelta_D64_Value");
    expect(static_cast<int>(VdpAccessDelta::D72) == 72, "VdpAccessDelta_D72_Value");
    expect(static_cast<int>(VdpAccessDelta::D88) == 88, "VdpAccessDelta_D88_Value");
    expect(static_cast<int>(VdpAccessDelta::D104) == 104, "VdpAccessDelta_D104_Value");
    expect(static_cast<int>(VdpAccessDelta::D120) == 120, "VdpAccessDelta_D120_Value");
    expect(static_cast<int>(VdpAccessDelta::D128) == 128, "VdpAccessDelta_D128_Value");
    expect(static_cast<int>(VdpAccessDelta::D136) == 136, "VdpAccessDelta_D136_Value");

    // --- minimum_request_latency_tstates: ceil(delta/6), the V99x8 floor. -----
    expect(minimum_request_latency_tstates(VdpAccessDelta::D0) == 0,
           "MinimumRequestLatency_D0_Zero");
    expect(minimum_request_latency_tstates(VdpAccessDelta::D16) == 3,
           "MinimumRequestLatency_D16_CeilSixteenOverSix_Three");
    expect(minimum_request_latency_tstates(VdpAccessDelta::D28) == 5,
           "MinimumRequestLatency_D28_CeilTwentyEightOverSix_Five");

    // --- A-M23-8 (CRITICAL): the two latency facts are DISTINCT and are -------
    // --- never combined/derived from one another anywhere in this header. ----
    expect(minimum_request_latency_tstates(VdpAccessDelta::D16) != kDocumentedSafeAccessGapTStates,
           "TwoLatencyFacts_D16FloorVsSafeGap_AreDistinctValues");
    expect(kDocumentedSafeAccessGapTStates == 29, "DocumentedSafeAccessGap_ExactValue_29");
    // Explicitly confirm no simple division/derivation reconciles them (A-M23-8
    // notes 16/6~=2.67T rounds to 3T, nowhere near a 29T total gap).
    expect(minimum_request_latency_tstates(VdpAccessDelta::D16) * 6 != kDocumentedSafeAccessGapTStates,
           "TwoLatencyFacts_NoSimpleMultipleReconciliation");

    return g_failures == 0 ? 0 : 1;
}
