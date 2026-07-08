// Suite: Devices_VdpVrHrRasterStatus_Unit
//
// Bug-fix coverage (post-M28): S#2 bits VR (bit6, "vblank area") and HR (bit5,
// "hblank area") were previously hardcoded to 0 (a disclosed, non-gating M23
// simplification tied to backlog C1/D4). A real, CPU-driven BIOS boot revealed
// this was NOT harmless: the real BIOS polls S#2 bit6 (VR) during early boot,
// waiting for it to toggle, and hangs forever if it never does -- the emulator
// never configured a single VDP register or rendered a single non-black frame
// as a result. This suite proves the fix: VR/HR now genuinely toggle, derived
// from real elapsed raster time via an attached VdpClockSource, grounded in
// references/fact-sheets/Yamaha V9958 VDP.md §7's NTSC line/frame timing
// tables. S#2's OTHER bits (TR/BD/CE/EO/undocumented) and every other status
// register are unaffected -- this is an additive, opt-in (nullptr-by-default)
// change.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_access_timing.h"

namespace {

using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;
using sony_msx::devices::video::vdp_access_timing::kCpuTstatesPerLine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Deterministic fake clock: the test sets tstates_ directly.
class FakeClock final : public VdpClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_tstates_since_vsync() const override { return tstates_; }
    std::uint64_t tstates_ = 0;
};

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

}  // namespace

int main() {
    // --- No clock source attached (default): VR/HR stay 0, exact pre-fix
    //     reset value preserved -- byte-for-byte regression guard. ---
    {
        V9958Vdp vdp;
        expect(vdp.peek_status_register(2) == 0x0C, "NoClockSource_S2_UnchangedFromReset");
    }

    // --- Clock attached, tstates=0: per the fix's grounding (fact-sheet §7,
    //     on_vsync() fires "at the start of the lower border"), tstate 0 is
    //     the FIRST line of the bottom border -- inside the non-active
    //     region -- so VR (bit6) must be SET. ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        clock.tstates_ = 0;
        expect((vdp.peek_status_register(2) & 0x40) != 0, "AtVsyncBoundary_VR_Set");
    }

    // --- Deep into the active display region (LN=0, 192-line default):
    //     non-active span is 70 lines (262-192); well past that, in the
    //     middle of active display, VR must be CLEAR. ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        // Line 150 (well inside [70,262)'s active window) at cycle 0 of that line.
        clock.tstates_ = 150ULL * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        expect((vdp.peek_status_register(2) & 0x40) == 0, "MidActiveDisplay_LN0_VR_Clear");
    }

    // --- VR genuinely toggles across the non-active/active boundary (the
    //     exact property whose ABSENCE caused the real hang: a real BIOS
    //     poll-loop waiting for this bit to flip must observe both states). ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        clock.tstates_ = 0;
        const bool vr_at_start = (vdp.peek_status_register(2) & 0x40) != 0;
        clock.tstates_ = 150ULL * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        const bool vr_mid_active = (vdp.peek_status_register(2) & 0x40) != 0;
        expect(vr_at_start && !vr_mid_active, "VR_GenuinelyToggles_AcrossFrameBoundary");
    }

    // --- LN=1 (212-line mode, R#9 bit7 set) shrinks the non-active span to
    //     50 lines (262-212): line 55 is active in LN=1 but would be
    //     non-active in LN=0 (55 < 70) -- proves the LN dependency is real,
    //     not a hardcoded constant. ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        set_register(vdp, 9, 0x80);  // LN=1 (212 lines)
        clock.tstates_ = 55ULL * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        expect((vdp.peek_status_register(2) & 0x40) == 0, "LN1_Line55_IsActive_VR_Clear");

        V9958Vdp vdp_ln0;
        FakeClock clock_ln0;
        vdp_ln0.attach_clock_source(&clock_ln0);
        // LN=0 is the reset default; do not set R#9.
        clock_ln0.tstates_ = 55ULL * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        expect((vdp_ln0.peek_status_register(2) & 0x40) != 0, "LN0_Line55_IsNonActive_VR_Set");
    }

    // --- HR (bit5): per the fact-sheet §7 per-line breakdown, the display
    //     window is [258,1282) VDP cycles; a CPU-T-state position translates
    //     via vdp_cycle_within_line() (x6). T-state 0 (VDP cycle 0) is
    //     outside the display window -> HR set. A T-state comfortably inside
    //     the display window -> HR clear. ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        clock.tstates_ = 0;  // vdp cycle 0 -> outside [258,1282)
        expect((vdp.peek_status_register(2) & 0x20) != 0, "LineStart_HR_Set");

        // T-state 150 -> vdp cycle 900 (150*6), inside [258,1282).
        clock.tstates_ = 150;
        expect((vdp.peek_status_register(2) & 0x20) == 0, "MidLine_HR_Clear");
    }

    // --- Every other S#2 bit (TR/BD/CE via the command engine; the
    //     undocumented base 0x0C; EO) is completely unaffected by attaching
    //     a clock source -- this fix is additive-only. ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        clock.tstates_ = 150ULL * static_cast<std::uint64_t>(kCpuTstatesPerLine);
        const std::uint8_t s2 = vdp.peek_status_register(2);
        expect((s2 & 0x0C) == 0x0C, "UndocumentedBits_StillSet_WithClockAttached");
        expect((s2 & 0x01) == 0, "CE_StillClear_NoCommandRunning");
    }

    // --- Determinism: identical tstates -> identical result (pure function,
    //     no side effect on repeated peek). ---
    {
        V9958Vdp vdp;
        FakeClock clock;
        vdp.attach_clock_source(&clock);
        clock.tstates_ = 12345;
        const std::uint8_t first = vdp.peek_status_register(2);
        const std::uint8_t second = vdp.peek_status_register(2);
        expect(first == second, "Determinism_RepeatedPeek_SameResult");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_VdpVrHrRasterStatus_Unit cases passed\n";
    return 0;
}
