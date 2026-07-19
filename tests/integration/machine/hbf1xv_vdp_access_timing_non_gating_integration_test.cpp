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

#include <array>
#include <cstdint>
#include <iostream>

#include "devices/video/vdp_access_timing.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvVdpAccessTimingNonGating_Integration  (M23-S2,
// backlog C1/D4 partial)
//
// The concrete, checkable "non-gating" proof required by the planner package
// (§2.3/§4/§6 item 5): re-runs the EXACT M21 and M22 system-test CPU-program
// byte sequences (tests/system/vdp_render_system_test.cpp:53-64/
// 81-89/106-114/137-149 and
// tests/system/sprites_command_engine_system_test.cpp:49-82/
// 134-172 -- copied verbatim here) through step_cpu_instruction() and asserts
// the cumulative T-state total for each is BYTE-IDENTICAL to a literal,
// captured-before-this-change value -- a real before/after regression proof,
// not merely "the program still runs". These fixtures already contain
// back-to-back LD A,n / OUT (#98/#99),A pairs with ZERO spacer instructions,
// which is exactly the pattern real CPU-execution gating on VDP access
// timing would be most likely to disturb (§2.3) -- proving they are
// unaffected demonstrates the new devices/video/vdp_access_timing.h
// calculator is genuinely inert for every existing call path.
//
// Separately (same test), the new vdp_cycle_position()/
// cycles_since_last_vsync() machine accessors and the header's
// minimum_request_latency_tstates() are exercised and asserted to return
// sane, non-crashing, in-range values -- proving the calculator exists and
// works AND that nothing during real CPU stepping currently consults it
// (verified independently via `git diff v1.0.22` showing
// step_cpu_instruction()/run_cycles()/run_until_cycle() byte-for-byte
// unchanged, and vdp_frame_renderer.*/vdp_command_engine.*/sprite_engine.*
// untouched, A-M23-6).

namespace {

using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::uint64_t run_to_halt_summing(Hbf1xvMachine& machine, const int max_steps) {
    std::uint64_t total = 0;
    for (int i = 0; i < max_steps && !machine.cpu().state().halted(); ++i) {
        total += machine.step_cpu_instruction();
    }
    return total;
}

}  // namespace

int main() {
    // --- Fixture 1/4: GRAPHIC1 (verbatim from vdp_render_system_test.cpp) ------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 37> program{
            0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x98,
            0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x42, 0xD3, 0x99, 0x3E, 0xFF, 0xD3, 0x98,
            0x3E, 0x08, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0xF0, 0xD3, 0x98,
            0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic1_ReachesHalt");
        // Captured value (pre-M23 v1.0.22-equivalent -- HALT-R (M23-S1) never
        // fires here since run_to_halt_summing's loop stops exactly at the
        // halt boundary and never steps again while already halted, so this
        // fixture is unaffected by BOTH M23 slices; see A-M23-3).
        expect(total == 185, "Graphic1_CumulativeTstates_ByteIdentical");
    }

    // --- M21 fixture 2/4: GRAPHIC4 (verbatim from :81-89). --------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 21> program{
            0x3E, 0x06, 0xD3, 0x99, 0x3E, 0x80, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x99,
            0x3E, 0x40, 0xD3, 0x99, 0x3E, 0xF0, 0xD3, 0x98, 0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic4_ReachesHalt");
        expect(total == 105, "Graphic4_CumulativeTstates_ByteIdentical");
    }

    // --- M21 fixture 3/4: GRAPHIC6 (verbatim from :106-114). ------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 25> program{
            0x3E, 0x0A, 0xD3, 0x99, 0x3E, 0x80, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x40,
            0xD3, 0x99, 0x3E, 0xF0, 0xD3, 0x98, 0x3E, 0x00, 0xD3, 0x98, 0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 64);
        expect(machine.cpu().state().halted(), "Graphic6_ReachesHalt");
        expect(total == 125, "Graphic6_CumulativeTstates_ByteIdentical");
    }

    // --- M21 fixture 4/4: YJK/SCREEN12 (verbatim from :137-149). --------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 41> program{
            0x3E, 0x0E, 0xD3, 0x99, 0x3E, 0x80, 0xD3, 0x99, 0x3E, 0x08, 0xD3, 0x99, 0x3E, 0x99,
            0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x08, 0xD3, 0x98,
            0x3E, 0x10, 0xD3, 0x98, 0x3E, 0x20, 0xD3, 0x98, 0x3E, 0x00, 0xD3, 0x98, 0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 64);
        expect(machine.cpu().state().halted(), "Yjk_ReachesHalt");
        expect(total == 205, "Yjk_CumulativeTstates_ByteIdentical");
    }

    // --- M22 fixture 1/2: sprite setup + collision (verbatim from :49-82). ----
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 89> program{
            0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x81, 0xD3, 0x99, 0x3E, 0x01, 0xD3, 0x99, 0x3E, 0x86,
            0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x98,
            0x3E, 0x01, 0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x98, 0x3E, 0x02,
            0xD3, 0x99, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x98, 0x3E, 0x03, 0xD3, 0x99,
            0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x01, 0xD3, 0x98, 0x3E, 0x04, 0xD3, 0x99, 0x3E, 0x40,
            0xD3, 0x99, 0x3E, 0xD0, 0xD3, 0x98, 0x3E, 0x00, 0xD3, 0x99, 0x3E, 0x48, 0xD3, 0x99,
            0x3E, 0xFF, 0xD3, 0x98, 0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 200);
        expect(machine.cpu().state().halted(), "Sprites_ReachesHalt");
        expect(total == 445, "Sprites_CumulativeTstates_ByteIdentical");
    }

    // --- M22 fixture 2/2: command engine HMMV+LMMC (verbatim from :134-172). --
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const std::array<std::uint8_t, 105> program{
            0x3E, 0x06, 0xD3, 0x99, 0x3E, 0x80, 0xD3, 0x99, 0x3E, 0x0A, 0xD3, 0x99, 0x3E, 0xA4,
            0xD3, 0x99, 0x3E, 0x02, 0xD3, 0x99, 0x3E, 0xA8, 0xD3, 0x99, 0x3E, 0x01, 0xD3, 0x99,
            0x3E, 0xAA, 0xD3, 0x99, 0x3E, 0x5A, 0xD3, 0x99, 0x3E, 0xAC, 0xD3, 0x99, 0x3E, 0xC0,
            0xD3, 0x99, 0x3E, 0xAE, 0xD3, 0x99, 0x3E, 0x00, 0xD3, 0x99, 0x3E, 0xA4, 0xD3, 0x99,
            0x3E, 0x04, 0xD3, 0x99, 0x3E, 0xA8, 0xD3, 0x99, 0x3E, 0xB0, 0xD3, 0x99, 0x3E, 0xAE,
            0xD3, 0x99, 0x3E, 0x01, 0xD3, 0x99, 0x3E, 0xAC, 0xD3, 0x99, 0x3E, 0x02, 0xD3, 0x99,
            0x3E, 0xAC, 0xD3, 0x99, 0x3E, 0x03, 0xD3, 0x99, 0x3E, 0xAC, 0xD3, 0x99, 0x3E, 0x04,
            0xD3, 0x99, 0x3E, 0xAC, 0xD3, 0x99, 0x76,
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        const std::uint64_t total = run_to_halt_summing(machine, 200);
        expect(machine.cpu().state().halted(), "CommandEngine_ReachesHalt");
        expect(total == 525, "CommandEngine_CumulativeTstates_ByteIdentical");
    }

    // --- Separately: the new accessors/calculator exist, work, and return -----
    // --- sane, non-crashing values -- proving the calculator is usable --------
    // --- WITHOUT anything above having consulted it (it was never called ------
    // --- until here). ----------------------------------------------------------
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        // No run_frame() call yet: documented "no vsync yet" boundary (R-M23-5)
        // -- position is relative to program start (cycle 0).
        expect(machine.cycles_since_last_vsync() == machine.elapsed_cycles(),
               "NonGating_NoVsyncYet_RelativeToProgramStart");
        expect(machine.vdp_cycle_position() >= 0 &&
                   machine.vdp_cycle_position() < sony_msx::devices::video::vdp_access_timing::kVdpCyclesPerLine,
               "NonGating_VdpCyclePosition_InRange");

        machine.map_flat_ram();
        // A short run of NOPs (not just one byte): PC must land on a KNOWN
        // opcode for every step below, not whatever byte the M13 DRAM
        // power-on pattern (initialize_dram_pattern(), alternating 00/FF)
        // happens to leave at the next address.
        const std::array<std::uint8_t, 4> nop_program{0x00, 0x00, 0x00, 0x00};
        machine.load_memory(0x0000, nop_program.data(), static_cast<std::uint32_t>(nop_program.size()));
        machine.step_cpu_instruction();  // NOP: advances elapsed_cycles(), still no vsync
        expect(machine.cycles_since_last_vsync() == machine.elapsed_cycles(),
               "NonGating_StillNoVsync_TracksElapsedCyclesDirectly");

        machine.run_frame();  // first on_vsync() -- last_vsync_cycle_ now set
        expect(machine.cycles_since_last_vsync() == 0,
               "NonGating_ImmediatelyAfterRunFrame_CyclesSinceVsyncIsZero");
        expect(machine.vdp_cycle_position() == 0,
               "NonGating_ImmediatelyAfterRunFrame_VdpCyclePositionIsZero");

        machine.step_cpu_instruction();  // NOP again (PC==1, still in the buffer), now relative to the vsync
        // NOP is 4 (bare datasheet) + 1 (S1985 M1 wait) = 5T at the machine
        // level (the SAME formula every other opcode fetch uses, unaffected
        // by M23 -- NOP is not the halted-idle branch).
        expect(machine.cycles_since_last_vsync() == 5,
               "NonGating_OneNopAfterVsync_CyclesSinceVsyncIsFive");
        expect(machine.vdp_cycle_position() == 30,
               "NonGating_OneNopAfterVsync_VdpCyclePositionIsThirty");

        using sony_msx::devices::video::vdp_access_timing::VdpAccessDelta;
        using sony_msx::devices::video::vdp_access_timing::minimum_request_latency_tstates;
        expect(minimum_request_latency_tstates(VdpAccessDelta::D16) == 3,
               "NonGating_MinimumRequestLatency_D16_Three");
        expect(minimum_request_latency_tstates(VdpAccessDelta::D136) == 23,
               "NonGating_MinimumRequestLatency_D136_TwentyThree");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
