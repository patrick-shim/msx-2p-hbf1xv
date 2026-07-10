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

namespace sony_msx::devices::chipset {

// Sony MB670836 residual hardware-PAUSE / Speed-Controller layer (M25,
// backlog C8). Grounded in `references/fact-sheets/Yamaha S1985 MSX-ENGINE
// Chipset.md` §9: a second Sony custom LSI (MB670836) beside the S1985 handles
// DRAM address multiplexing plus the speed-controller (slow-motion) and
// hardware-PAUSE circuitry; the hardware PAUSE physically halts the CPU and
// cannot be bypassed in software. Per `references/fact-sheets/Zilog Z80A
// CPU.md` §6 item 3, the HB-F1XV has no CPU turbo mode -- the "Speed
// Controller" slider is NOT a clock-speed change, it is an autofire on the
// PAUSE button synced to VBlank, slowing games by pausing them intermittently.
//
// DRAM address multiplexing (fact-sheet §9) is NOT modeled here, matching the
// project-wide precedent that DRAM refresh/muxing is transparent to software
// (Z80A fact-sheet §7; only the Z80 R-register value is modeled, never actual
// refresh cycles). This class models ONLY the PAUSE/speed-controller function.
//
// Architecture (docs/m25-planner-package.md §2.3/§2.4): a machine-level
// CPU-EXECUTION GATE, consulted at the top of
// Hbf1xvMachine::step_cpu_instruction() before any opcode decode -- NOT part
// of Z80aCpu (zero edit to src/devices/cpu/*). Unlike the Z80's own HALT
// (CPU-internal; R keeps incrementing, released by any interrupt), this gate
// is external: every register stays frozen while engaged, matching the
// fact-sheet's "physically halts the CPU" wording; released only by the gate
// itself (PAUSE button pressed again, or the duty-cycle window ending).
//
// Two independent trigger sources OR into one combined cpu_should_pause() gate
// (A-M25-4): the manual PAUSE button (press_pause_button(), toggle semantics --
// A-M25-1, a first-principles design choice; no datasheet documents the real
// button's electrical behavior) and the Speed Controller's own VBlank-synced
// duty cycle (on_vsync() + set_speed_level()). The duty-cycle range
// (kPeriodFrames=8, levels 0..7) is likewise a documented, first-principles
// DESIGN DEFAULT (A-M25-3), not a measured hardware fact.
class Mb670836PauseController {
public:
    // Speed-Controller duty-cycle period, in VBlank frames. level=0 => never
    // paused (full speed); level=kMaxSpeedLevel => paused kMaxSpeedLevel of
    // every kPeriodFrames VBlanks (maximum slow-motion).
    static constexpr int kPeriodFrames = 8;
    static constexpr int kMaxSpeedLevel = kPeriodFrames - 1;  // 0..7

    // Deterministic power-on idle default: disengaged, speed level 0 (never
    // paused), frame counter reset to 0.
    void reset();

    // Physical PAUSE button (A-M25-1): one press toggles engaged/disengaged.
    void press_pause_button();
    [[nodiscard]] bool button_engaged() const;

    // Speed Controller slider (0 = full speed/disabled .. kMaxSpeedLevel).
    // Out-of-range input is clamped, never undefined.
    void set_speed_level(int level);
    [[nodiscard]] int speed_level() const;

    // VBlank hook (A-M25-4 / planner §2.3 point 4): advances the frame counter
    // and recomputes whether the frame that just started (the pre-increment
    // index) is paused: frame f is paused iff (f % kPeriodFrames) <
    // speed_level_. Called from Hbf1xvMachine::run_frame() OR directly by
    // tests driving the CPU via step_cpu_instruction() -- never both in the
    // same test, that double-counts elapsed cycles (R-M25-5).
    void on_vsync();
    // Introspection: the pause state established by the most recent
    // on_vsync() call (false before the first on_vsync() call ever made).
    [[nodiscard]] bool speed_controller_paused_this_frame() const;

    // Combined CPU-execution gate, consulted by step_cpu_instruction()
    // BEFORE any opcode decode. True if EITHER source is active (A-M25-4):
    // the manual button OR the Speed Controller's current duty-cycle window.
    [[nodiscard]] bool cpu_should_pause() const;

    // M36 Phase 3 debug snapshot: the internal VBlank frame counter, for a
    // restore-ready snapshot (planner §2.4 item 12). const, ZERO behavior change.
    [[nodiscard]] std::uint64_t frame_index() const { return frame_index_; }

private:
    bool engaged_ = false;
    int speed_level_ = 0;
    bool speed_paused_ = false;
    std::uint64_t frame_index_ = 0;
};

}  // namespace sony_msx::devices::chipset
