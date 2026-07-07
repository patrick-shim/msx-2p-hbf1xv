#pragma once

#include <cstdint>

// devices/video/vdp_access_timing.h
//
// M23-S2: a strictly NON-GATING, additive VDP access-timing FOUNDATION for
// backlog C1/D4 (docs/m23-planner-package.md §2/§3). This header is pure
// functions/constants over explicit parameters -- it carries no VDP device/
// bus/slot knowledge of its own and is consulted by NOTHING in the CPU-
// stepping path this milestone (A-M23-6; verified by `git diff` showing
// step_cpu_instruction()/run_cycles()/run_until_cycle() unchanged).
//
// What ships this cycle:
//   - the 15 named VdpAccessDelta VDP-cycle offsets openMSX's own access-
//     slot calculator is keyed on (independently re-derived from the enum's
//     OWN labels -- NOT the ~340-entry numeric slot tables,
//     references/openmsx-21.0/src/video/VDPAccessSlots.cc:14-141, which
//     remain explicitly OUT of scope this cycle, R-M23-4);
//   - a raster-position derivation (CPU T-states since the last VSync ->
//     position within the current 1368-VDP-cycle scanline, per §3.2);
//   - two DELIBERATELY SEPARATE, cited latency facts (A-M23-8): the V9938+/
//     V9958 CPU-VRAM-access scheduler's FLOOR delta (minimum_request_
//     latency_tstates), and the fact-sheet's independently-documented
//     "~29 Z80-cycle safe access" software convention
//     (kDocumentedSafeAccessGapTStates). These are non-interchangeable
//     hardware facts and must NEVER be combined/derived from one another
//     anywhere in this codebase (a dedicated unit test enforces this).
//
// WARNING (R-M23-3, read before touching a CPU-execution-gating path): this
// calculator is purely informational/queryable this cycle. Wiring it into
// step_cpu_instruction()/run_cycles()/run_until_cycle() to actually delay or
// drop CPU-initiated VDP port accesses requires FIRST re-validating the
// M21/M22 system-test CPU-program fixtures --
// tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62 (three
// consecutive LD A,n / OUT (#98),A pairs, ZERO spacer instructions) and
// tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78
// (five consecutive pairs, same pattern) -- against real gating/drop
// behavior (V9958 fact-sheet §2: "Too-fast access drops requests"). Do NOT
// assume this header is safe to gate CPU timing on without that
// re-validation; a same-cycle regression test
// (tests/integration/machine/hbf1xv_m23_vdp_access_timing_non_gating_
// integration_test.cpp) proves those exact fixtures are byte-identical,
// pre- and post-this-header, to their pre-M23 (v1.0.22) T-state totals.
//
// Grounding: references/openmsx-21.0/src/video/VDPAccessSlots.hh:15-34
// (Delta enum labels/values); VDP.hh:76 (TICKS_PER_LINE = 1368); VDP.cc:
// 842-844 (Delta::D16 CPU-VRAM-access-scheduler floor for V99x8, D28 for
// MSX1-class TMS99x8 VDPs -- not applicable to this V9958-equipped
// machine); references/fact-sheets/Yamaha V9958 VDP.md §2/§7.

namespace sony_msx::devices::video::vdp_access_timing {

// The 15 named VDP-cycle offsets openMSX's own access-slot calculator is
// keyed on (VDPAccessSlots.hh:17-33, `enum class Delta`). Values here are
// the literal VDP-cycle deltas the names denote -- NOT openMSX's internal
// `n * TICKS_PER_LINE` slot-table-ROW encoding (that encoding is an
// implementation detail of the out-of-scope-this-cycle slot-table lookup,
// not a hardware fact in its own right; re-deriving the plain, human-
// meaningful values keeps this header clearly on the "hardware fact, not
// copied code" side of the GPL-isolation guardrail).
enum class VdpAccessDelta : int {
    D0 = 0,
    D1 = 1,
    D16 = 16,
    D24 = 24,
    D28 = 28,
    D32 = 32,
    D40 = 40,
    D48 = 48,
    D64 = 64,
    D72 = 72,
    D88 = 88,
    D104 = 104,
    D120 = 120,
    D128 = 128,
    D136 = 136,
};

// VDP cycles per scanline (VDP.hh:76, TICKS_PER_LINE = 1368).
constexpr int kVdpCyclesPerLine = 1368;

// Fixed CPU:VDP clock ratio (Z80A fact-sheet §1 / V9958 fact-sheet §1:
// 21.477270 MHz VDP clock / 6 = 3.579545 MHz CPU clock).
constexpr int kCpuTstatesPerVdpCycleRatio = 6;

// CPU T-states per scanline: 1368 / 6 = 228 exactly (no remainder). A-M23-7's
// independently-discovered cross-check: this is the SAME 228 factor already
// silently encoded in the existing `kFrameCycles = 228 * 262` constant at
// hbf1xv_machine.cpp:14 (never previously documented as a per-line quantity
// anywhere in this project before M23). A unit test asserts
// `kCpuTstatesPerLine * 262 == kFrameCycles` via that SHARED constant.
constexpr int kCpuTstatesPerLine = kVdpCyclesPerLine / kCpuTstatesPerVdpCycleRatio;

// Raster-position derivation (the informational half, planner package §3.2).
// Returns the CPU-T-state-granularity position (0..kCpuTstatesPerLine-1)
// within the current scanline, given the number of CPU T-states elapsed
// since the machine's most recent on_vsync() call.
//
// Documented, tested boundary condition (R-M23-5): this position is
// RELATIVE TO the most recent on_vsync() call, or to program start (cycle 0)
// if on_vsync() has never fired -- true for every existing M21/M22 system
// test, which drive the CPU purely via step_cpu_instruction() loops and
// never call run_frame() (so never trigger on_vsync()). This is an honest,
// by-design semantic, not an oversight or an undefined value.
constexpr int cpu_tstate_within_line(std::uint64_t cpu_tstates_since_vsync) {
    return static_cast<int>(cpu_tstates_since_vsync %
                             static_cast<std::uint64_t>(kCpuTstatesPerLine));
}

// VDP-cycle-granularity position within the current scanline
// (0..kVdpCyclesPerLine-1, in steps of kCpuTstatesPerVdpCycleRatio -- this
// project's clock model advances at CPU-T-state granularity, which is
// coarser than the underlying VDP clock, so intermediate VDP-cycle positions
// WITHIN a single CPU T-state are not resolvable from it; the full slot-
// table/mid-frame reconciliation work needed to go finer is explicitly
// OUT of scope this cycle, §2.1 item 1 of the planner package).
constexpr int vdp_cycle_within_line(std::uint64_t cpu_tstates_since_vsync) {
    return cpu_tstate_within_line(cpu_tstates_since_vsync) * kCpuTstatesPerVdpCycleRatio;
}

// The V9938+/V9958 CPU-VRAM-access scheduler's FLOOR delta (VDP.cc:842-844,
// `Delta::D16` for V99x8), converted to a LOWER-BOUND-ONLY CPU-T-state count
// via ceil(delta_vdp_cycles / 6). This is explicitly NOT the actual wait a
// real access would incur -- the full (out-of-scope-this-cycle) per-mode
// slot table would refine this floor upward depending on raster position and
// display mode (VDPAccessSlots.cc:14-141; e.g. the screen-off table's slots
// can leave gaps up to 44 VDP cycles wide even though the floor is only 16).
constexpr int minimum_request_latency_tstates(VdpAccessDelta delta) {
    const int delta_vdp_cycles = static_cast<int>(delta);
    return (delta_vdp_cycles + kCpuTstatesPerVdpCycleRatio - 1) / kCpuTstatesPerVdpCycleRatio;
}

// The V9958 fact-sheet's own, SEPARATE, coarser "safe access" software-
// discipline convention (fact-sheet §2: "The general consensus seems to be
// 'at least 29 Z80 cycles between two accesses'. For example an OUT(#99),A
// instruction takes 12 cycles (on MSX), so you need 17 extra cycles before
// the [next access].") -- an empirically-documented MSX-programmer
// convention, NOT the literal internal scheduler mechanism
// minimum_request_latency_tstates() models above. A-M23-8: these are two
// genuinely different, non-interchangeable hardware facts that do NOT
// arithmetically reconcile via simple division (16 VDP cycles / 6 ~= 2.67T,
// nowhere near a 29T total gap) -- they must be kept separately named,
// separately cited, and NEVER combined/derived from one another anywhere in
// this codebase (enforced by a dedicated unit test).
constexpr int kDocumentedSafeAccessGapTStates = 29;

}  // namespace sony_msx::devices::video::vdp_access_timing
