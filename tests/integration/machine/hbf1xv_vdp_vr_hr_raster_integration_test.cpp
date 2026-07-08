// Suite: Machine_Hbf1xvVdpVrHrRaster_Integration
//
// Bug-fix coverage (post-M28): confirms the REAL machine-level wiring
// (Hbf1xvMachine::VdpRasterClock, attached to vdp_ in wire_bus()) drives S#2's
// VR/HR bits from genuine elapsed CPU cycles -- not just the isolated
// V9958Vdp-with-a-fake-clock unit coverage. This is the exact seam that was
// missing before the fix: a real BIOS boot polls S#2 bit6 (VR) waiting for it
// to toggle, and hung forever because nothing drove it. Reads status through
// the REAL #99/R#15 two-write protocol a real Z80 program would use, and
// cross-checks against machine.cycles_since_last_vsync() (the exact quantity
// the fix's VdpRasterClock reports) so the oracle is independently derived,
// not copy-pasted from the fix's own implementation.

#include <cstdint>
#include <iostream>

#include "devices/video/vdp_access_timing.h"
#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Reads status register `reg` via the real #99/R#15 two-write protocol (the
// SAME sequence a real Z80 program running on the real machine performs).
std::uint8_t read_status(sony_msx::machine::Hbf1xvMachine& machine, const std::uint8_t reg) {
    machine.debug_io_write(0x99, reg);
    machine.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | 15));  // R#15 = reg
    return machine.debug_io_read(0x99);
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::devices::video::vdp_access_timing::kCpuTstatesPerLine;

    // --- Right after cold_boot(), before any frame has run, VR reflects
    //     cycles_since_last_vsync()==0 -- the documented "no vsync yet"
    //     boundary semantic (R-M23-5), which the fix's grounding treats
    //     identically to "just past a vsync boundary" (line 0 of the
    //     non-active span) -- so VR must be SET. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(machine.cycles_since_last_vsync() == 0, "ColdBoot_CyclesSinceVsync_Zero");
        expect((read_status(machine, 2) & 0x40) != 0, "ColdBoot_VR_Set");
    }

    // --- Real CPU stepping through a bounded number of instructions advances
    //     elapsed_cycles() (and hence cycles_since_last_vsync(), since
    //     on_vsync_boundary() has not been called yet) into the middle of the
    //     active-display line range -- VR must clear, cross-checked against
    //     the SAME machine's own cycles_since_last_vsync() accessor. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        const std::uint64_t target_line = 150;  // well inside LN=0's active window
        const std::uint64_t target_cycles = target_line * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        // Bounded: step until at or past the target, or a generous safety cap.
        int guard = 0;
        while (machine.cycles_since_last_vsync() < target_cycles && guard < 2'000'000) {
            machine.step_cpu_instruction();
            ++guard;
        }
        expect(guard < 2'000'000, "ReachedTargetCycles_WithinGuard");
        expect((read_status(machine, 2) & 0x40) == 0, "MidActiveDisplay_RealCpuStepping_VR_Clear");
    }

    // --- Determinism: two independent machines, identical stepping, produce
    //     an IDENTICAL VR/HR reading (mirrors this project's standing
    //     determinism discipline). ---
    {
        Hbf1xvMachine a;
        Hbf1xvMachine b;
        a.cold_boot();
        b.cold_boot();
        for (int i = 0; i < 5000; ++i) {
            a.step_cpu_instruction();
            b.step_cpu_instruction();
        }
        expect(read_status(a, 2) == read_status(b, 2), "TwoMachines_IdenticalStepping_IdenticalS2");
    }

    // --- Reading S#2 does NOT itself perturb elapsed_cycles()/CPU state
    //     (non-destructive w.r.t. timing -- unlike S#0/S#1/S#5/S#9, S#2 has
    //     no documented read-side-effect on the VR/HR bits themselves). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        for (int i = 0; i < 1000; ++i) machine.step_cpu_instruction();
        const std::uint64_t before = machine.elapsed_cycles();
        (void)read_status(machine, 2);
        (void)read_status(machine, 2);
        expect(machine.elapsed_cycles() == before, "ReadingS2_DoesNotAdvanceElapsedCycles");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvVdpVrHrRaster_Integration cases passed\n";
    return 0;
}
