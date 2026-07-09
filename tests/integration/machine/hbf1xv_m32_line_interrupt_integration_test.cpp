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

// Suite: Machine_Hbf1xvM32LineInterrupt_Integration (M32-S2, the D-1
// ratified line-interrupt delivery leg, docs/m32-planner-package.md §2.5 /
// §3-S2 / test matrix row 4)
//
// The trigger relation BOTH behavior references agree on: the line
// interrupt fires when the raster enters SCREEN line M = (R#19 - R#23) &
// 0xFF (openMSX references/openmsx-21.0/src/video/VDP.cc:527-529; fMSX
// references/fmsx-60/source/fMSX/MSX.c:2094-2100). Cases:
//   1. Fire window: with R#19=100/R#23=0/IE1, /INT asserts within one line
//      of display-line-100 entry; S#1 FH reads set; the S#1 read clears FH
//      and releases the line (existing M14 semantics, v9958_vdp.cpp).
//   2. Once-per-crossing: after the S#1 clear, no re-fire within the same
//      262-line window; the NEXT window's crossing fires again.
//   3. The (R#19 - R#23) & 0xFF re-arm subtlety: R#23=60 moves the match to
//      screen line 40.
//   4. IE1 off => zero CPU-visible change: /INT never asserts, S#1 FH never
//      reads set, and two IE1-off machines with DIFFERENT R#19 values run
//      byte-identical CPU trajectories.
//   5. M >= active line count never fires (openMSX VDP.cc:554-559 "never
//      occurs" clamp, line-granular analogue).
//   6. Two-run determinism of the fire cycle.

#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerLine = 228;
constexpr std::uint64_t kFrameCycles = kCyclesPerLine * 262;
constexpr std::uint64_t kNonActiveLines = 70;  // R#9 LN=0 -> 192 active lines

void debug_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Fresh machine spinning a 2-byte JR-self loop from address 0 (flat RAM).
// The CPU never touches the VDP -- registers are staged via the debug seam
// (the M32 line-int cache re-fingerprints by VALUE, independent of the
// write path).
void boot_spinner(Hbf1xvMachine& m) {
    m.cold_boot();
    m.map_flat_ram();
    const std::uint8_t spin[2] = {0x18, 0xFE};  // JR $
    m.load_memory(0x0000, spin, 2);
}

// Steps until the VDP /INT level asserts or `limit_cycles` elapses; returns
// the elapsed-cycle count at which the assert was first observed (or 0).
std::uint64_t step_until_irq(Hbf1xvMachine& m, const std::uint64_t limit_cycles) {
    while (m.elapsed_cycles() < limit_cycles) {
        m.step_cpu_instruction();
        if (m.vdp().irq_active()) {
            return m.elapsed_cycles();
        }
    }
    return 0;
}

// Reads S#1 through the debug seam (R#15 dance) -- destructive, clears FH.
std::uint8_t read_s1(Hbf1xvMachine& m) {
    debug_set_register(m, 15, 1);
    const std::uint8_t s1 = m.debug_io_read(0x99);
    debug_set_register(m, 15, 0);
    return s1;
}

}  // namespace

int main() {
    // --- 1 + 2. Fire window, FH visibility, S#1 clear, once-per-crossing,
    //     next-window re-fire. ---
    {
        Hbf1xvMachine m;
        boot_spinner(m);
        debug_set_register(m, 19, 100);
        debug_set_register(m, 0, 0x10);  // IE1 (mode bits 0 -> GRAPHIC1)

        const std::uint64_t match_entry = (kNonActiveLines + 100) * kCyclesPerLine;
        const std::uint64_t fired_at = step_until_irq(m, 2 * kFrameCycles);
        expect(fired_at != 0, "Ie1Match_Line100_IrqAsserts");
        expect(fired_at >= match_entry && fired_at <= match_entry + kCyclesPerLine,
               "Ie1Match_FireCycle_WithinOneLineOfMatchEntry");
        expect((m.vdp().peek_status_register(1) & 0x01) != 0, "Ie1Match_S1FH_ReadsSet");

        const std::uint8_t s1 = read_s1(m);
        expect((s1 & 0x01) != 0, "S1Read_ReturnsFH");
        expect(!m.vdp().irq_active(), "S1Read_ReleasesLineIrq");
        expect((m.vdp().peek_status_register(1) & 0x01) == 0, "S1Read_ClearsFH");

        // Once per crossing: the remainder of this 262-line window is quiet.
        const std::uint64_t refire_in_window = step_until_irq(m, kFrameCycles);
        expect(refire_in_window == 0, "OncePerCrossing_NoRefireWithinSameWindow");

        // The next window's crossing fires again.
        const std::uint64_t next_entry = kFrameCycles + match_entry;
        const std::uint64_t second_fire = step_until_irq(m, 3 * kFrameCycles);
        expect(second_fire >= next_entry && second_fire <= next_entry + kCyclesPerLine,
               "NextWindowCrossing_FiresAgain_WithinOneLine");
    }

    // --- 3. The (R#19 - R#23) & 0xFF relation: R#23=60 moves the match to
    //     screen line 40. ---
    {
        Hbf1xvMachine m;
        boot_spinner(m);
        debug_set_register(m, 19, 100);
        debug_set_register(m, 23, 60);
        debug_set_register(m, 0, 0x10);

        const std::uint64_t match_entry = (kNonActiveLines + 40) * kCyclesPerLine;
        const std::uint64_t fired_at = step_until_irq(m, kFrameCycles);
        expect(fired_at >= match_entry && fired_at <= match_entry + kCyclesPerLine,
               "RearmRelation_R23Offset_MatchAtScreenLine40");
    }

    // --- 4. IE1 off => zero CPU-visible change. ---
    {
        Hbf1xvMachine a;
        Hbf1xvMachine b;
        boot_spinner(a);
        boot_spinner(b);
        debug_set_register(a, 19, 50);
        debug_set_register(b, 19, 200);  // different match lines, both IE1-off

        bool a_irq = false;
        bool a_fh = false;
        for (int i = 0; i < 6000; ++i) {
            a.step_cpu_instruction();
            b.step_cpu_instruction();
            a_irq = a_irq || a.vdp().irq_active();
            a_fh = a_fh || (a.vdp().peek_status_register(1) & 0x01) != 0;
        }
        expect(!a_irq, "Ie1Off_IrqNeverAsserts");
        expect(!a_fh, "Ie1Off_S1FH_NeverSet");
        expect(a.cpu().state().regs().pc == b.cpu().state().regs().pc &&
                   a.elapsed_cycles() == b.elapsed_cycles(),
               "Ie1Off_DifferentR19Values_ByteIdenticalCpuTrajectory");
    }

    // --- 5. M >= active line count never fires (192-line mode, M=220). ---
    {
        Hbf1xvMachine m;
        boot_spinner(m);
        debug_set_register(m, 19, 220);
        debug_set_register(m, 0, 0x10);
        expect(step_until_irq(m, 2 * kFrameCycles) == 0,
               "MatchBeyondActiveLines_NeverFires_OpenMsxNeverOccursClamp");
    }

    // --- 6. Determinism: two identical machines record the identical fire
    //     cycle. ---
    {
        const auto run = [] {
            Hbf1xvMachine m;
            boot_spinner(m);
            debug_set_register(m, 19, 100);
            debug_set_register(m, 0, 0x10);
            return step_until_irq(m, kFrameCycles);
        };
        const std::uint64_t fire_a = run();
        const std::uint64_t fire_b = run();
        expect(fire_a != 0 && fire_a == fire_b, "Determinism_TwoRuns_IdenticalFireCycle");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM32LineInterrupt_Integration cases passed\n";
    return 0;
}
