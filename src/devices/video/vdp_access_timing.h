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

#pragma once

#include <cstdint>

// A strictly non-gating, additive VDP access-timing FOUNDATION. Pure
// functions/constants over explicit parameters -- no VDP device/bus/slot
// knowledge of its own, and it never delays or gates the CPU-stepping path
// (step_cpu_instruction()/run_cycles()/run_until_cycle() take no wait states
// from it).
//
// What this header provides:
//   - the 15 named VdpAccessDelta VDP-cycle offsets openMSX's own access-
//     slot calculator is keyed on (independently re-derived from the enum's
//     own labels -- NOT the ~340-entry numeric slot tables,
//     openMSX 21.0: src/video/VDPAccessSlots.cc:14-141, which
//     stay deliberately un-transcribed);
//   - a raster-position derivation (CPU T-states since the last VSync ->
//     position within the current 1368-VDP-cycle scanline);
//   - two deliberately separate, cited latency facts: the V9938+/
//     V9958 CPU-VRAM-access scheduler's FLOOR delta (minimum_request_
//     latency_tstates), and the fact sheet's independently-documented
//     "~29 Z80-cycle safe access" software convention
//     (kDocumentedSafeAccessGapTStates). These are non-interchangeable
//     hardware facts and must never be combined/derived from one another
//     anywhere in this codebase (a dedicated unit test enforces this).
//
// WARNING (read before touching a CPU-execution-gating path): this
// calculator is purely informational/queryable for the CPU path. Wiring it
// into step_cpu_instruction()/run_cycles()/run_until_cycle() to actually
// delay or drop CPU-initiated VDP port accesses requires FIRST re-validating
// the system-test CPU-program fixtures --
// tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62 (three
// consecutive LD A,n / OUT (#98),A pairs, zero spacer instructions) and
// tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78
// (five consecutive pairs, same pattern) -- against real gating/drop
// behavior (V9958 fact sheet §2: "Too-fast access drops requests"). Do not
// assume this header is safe to gate CPU timing on without that
// re-validation; a regression test
// (tests/integration/machine/hbf1xv_m23_vdp_access_timing_non_gating_
// integration_test.cpp) proves those exact fixtures are byte-identical,
// with and without this header, to their prior T-state totals.
//
// Grounding: openMSX 21.0: src/video/VDPAccessSlots.hh:15-34
// (Delta enum labels/values); VDP.hh:76 (TICKS_PER_LINE = 1368); VDP.cc:
// 842-844 (Delta::D16 CPU-VRAM-access-scheduler floor for V99x8, D28 for
// MSX1-class TMS99x8 VDPs -- not applicable to this V9958-equipped
// machine); Yamaha V9958 VDP fact sheet §2/§7.

namespace sony_msx::devices::video::vdp_access_timing {

// The 15 named VDP-cycle offsets openMSX's own access-slot calculator is
// keyed on (VDPAccessSlots.hh:17-33, `enum class Delta`). Values here are
// the literal VDP-cycle deltas the names denote -- not openMSX's internal
// `n * TICKS_PER_LINE` slot-table-ROW encoding (an implementation detail of
// the out-of-scope slot-table lookup, not a hardware fact in its own right;
// re-deriving the plain, human-meaningful values keeps this header on the
// "hardware fact, not copied code" side of the GPL-isolation guardrail).
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

// Fixed CPU:VDP clock ratio (Z80A fact sheet §1 / V9958 fact sheet §1:
// 21.477270 MHz VDP clock / 6 = 3.579545 MHz CPU clock).
constexpr int kCpuTstatesPerVdpCycleRatio = 6;

// CPU T-states per scanline: 1368 / 6 = 228 exactly (no remainder).
// Cross-check: this is the same 228 factor already encoded in the
// existing `kFrameCycles = 228 * 262` constant at hbf1xv_machine.cpp:14. A
// unit test asserts `kCpuTstatesPerLine * 262 == kFrameCycles` via that
// shared constant.
constexpr int kCpuTstatesPerLine = kVdpCyclesPerLine / kCpuTstatesPerVdpCycleRatio;

// ---------------------------------------------------------------------------
// The three tier-1 per-line CPU/command VRAM ACCESS-SLOT COUNTS of the
// average-throughput contention model. These three integers
// (plus kVdpCyclesPerLine=1368 and kCpuTstatesPerVdpCycleRatio=6 above) are
// the ENTIRE numeric surface of that model. There is deliberately NO per-slot
// POSITION array here: the ~340-entry per-mode slot-position tables at
// openMSX 21.0: src/video/VDPAccessSlots.cc:14-141
// stay BANNED and un-transcribed (read for effect only; GPL license
// isolation). Values are the published per-line slot
// counts from the Yamaha V9958 VDP fact sheet §8: "Actual
// speed depends on access-slot availability: 154 slots/line (screen off), 88
// (sprites off), 31 (sprites on)." (DEC-0075)
constexpr int kSlotsPerLineDisplayOff = 154;  // display OFF / border / vblank
constexpr int kSlotsPerLineSpritesOff = 88;   // display ON, sprites disabled
constexpr int kSlotsPerLineSpritesOn = 31;    // display ON, sprites enabled

// Average CPU-T-state cost of ONE VRAM access slot when `slots_per_line` slots
// are distributed across the 1368-VDP-cycle line: the mean VDP-cycle spacing
// between adjacent slots is 1368 / slots_per_line, converted to CPU T-states by
// the fixed /6 clock ratio -- a single ceiling:
//   ceil(1368 / slots_per_line / 6)
//   = ceil(kVdpCyclesPerLine / (kCpuTstatesPerVdpCycleRatio * slots_per_line)).
// This is an AVERAGE RATE derived PURELY from the slot COUNT (fact sheet
// §7 = 1368; §1 = /6); it carries NO positional information --
// that positional knowledge is exactly the banned VDPAccessSlots.cc table. Used
// by the CPU-priority slot-steal model (one stolen slot's worth of
// time per concurrent CPU #98 access). For the three tier-1 counts this yields
// 154 -> 2, 88 -> 3, 31 -> 8 T-states/slot.
constexpr int slot_cost_tstates(const int slots_per_line) {
    const int divisor = kCpuTstatesPerVdpCycleRatio * slots_per_line;
    return (kVdpCyclesPerLine + divisor - 1) / divisor;
}

// Raster-position derivation (the purely informational half of this header).
// Returns the CPU-T-state-granularity position (0..kCpuTstatesPerLine-1)
// within the current scanline, given CPU T-states elapsed since the
// machine's most recent on_vsync() call.
//
// Documented, tested boundary condition: this position is
// relative to the most recent on_vsync() call, or to program start (cycle
// 0) if on_vsync() has never fired -- true for every system test that
// drives the CPU purely via step_cpu_instruction() loops
// and never calls run_frame() (so never triggers on_vsync()). By-design
// semantic, not an oversight or an undefined value.
constexpr int cpu_tstate_within_line(std::uint64_t cpu_tstates_since_vsync) {
    return static_cast<int>(cpu_tstates_since_vsync %
                             static_cast<std::uint64_t>(kCpuTstatesPerLine));
}

// VDP-cycle-granularity position within the current scanline
// (0..kVdpCyclesPerLine-1, in steps of kCpuTstatesPerVdpCycleRatio -- this
// project's clock model advances at CPU-T-state granularity, coarser than
// the underlying VDP clock, so intermediate VDP-cycle positions within a
// single CPU T-state are not resolvable from it; the full slot-table/mid-
// frame reconciliation needed to go finer is deliberately not modeled).
constexpr int vdp_cycle_within_line(std::uint64_t cpu_tstates_since_vsync) {
    return cpu_tstate_within_line(cpu_tstates_since_vsync) * kCpuTstatesPerVdpCycleRatio;
}

// The V9938+/V9958 CPU-VRAM-access scheduler's FLOOR delta (VDP.cc:842-844,
// `Delta::D16` for V99x8), converted to a lower-bound-only CPU-T-state count
// via ceil(delta_vdp_cycles / 6). NOT the actual wait a real access would
// incur -- the full (unmodeled) per-mode slot table would
// refine this floor upward depending on raster position and display mode
// (VDPAccessSlots.cc:14-141; e.g. the screen-off table's slots can leave
// gaps up to 44 VDP cycles wide even though the floor is only 16).
constexpr int minimum_request_latency_tstates(VdpAccessDelta delta) {
    const int delta_vdp_cycles = static_cast<int>(delta);
    return (delta_vdp_cycles + kCpuTstatesPerVdpCycleRatio - 1) / kCpuTstatesPerVdpCycleRatio;
}

// The V9958 fact sheet's own, separate, coarser "safe access" software-
// discipline convention (fact sheet §2: "The general consensus seems to be
// 'at least 29 Z80 cycles between two accesses'. For example an OUT(#99),A
// instruction takes 12 cycles (on MSX), so you need 17 extra cycles before
// the [next access].") -- an empirically-documented MSX-programmer
// convention, NOT the literal internal scheduler mechanism
// minimum_request_latency_tstates() models above. These are two
// genuinely different, non-interchangeable hardware facts that do not
// arithmetically reconcile via simple division (16 VDP cycles / 6 ~= 2.67T,
// nowhere near a 29T total gap) -- keep them separately named, separately
// cited, and never combine/derive one from the other (enforced by a
// dedicated unit test).
constexpr int kDocumentedSafeAccessGapTStates = 29;

}  // namespace sony_msx::devices::video::vdp_access_timing
