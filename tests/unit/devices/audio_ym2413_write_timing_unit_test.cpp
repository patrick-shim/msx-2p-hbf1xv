#include <cstdint>
#include <iostream>

#include "devices/audio/ym2413_opll.h"

// Suite: Devices_AudioYm2413WriteTiming_Unit  (M28-S1, backlog E2)
//
// E2: YM2413 register-write minimum-spacing gate, grounded in
// references/fact-sheets/Yamaha YM2413 FM Chip.md §8 ("after an address
// write, wait >=12 master cycles ... after a data write, wait >=84 master
// cycles ... before the next write ... Violating the 84-cycle rule causes
// dropped/wrong register writes on real hardware"). Mirrors the M15 X4
// pattern: Ym2413ClockSource is consulted READ-ONLY, pull-style, from
// write_address()/write_data() only -- this test never touches
// step_cpu_instruction() or any CPU-timing formula.
//
// Regression pre-check (R-M28-1, performed before writing this file): `rg`
// against the EXISTING M17 tests
// (tests/unit/devices/audio_ym2413_opll_unit_test.cpp,
// tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp) found
// back-to-back #7C/#7D register writes with zero/near-zero cycle spacing
// (the unit test never attaches a clock source at all; the integration
// test's Hbf1xvMachine::debug_io_write() helper is a zero-cycle-advance raw
// bus poke). Per docs/m28-planner-package.md §2.1b, the resolution chosen
// here is (b): the gate DEFAULTS OFF (Ym2413Opll::write_timing_enforced()
// == false), matching openMSX's own documented default-disabled stance
// (fact-sheet §8), and is exposed as an explicit, unit-tested toggle. This
// keeps every existing M17 test byte-for-byte unmodified.

namespace {

using sony_msx::devices::audio::Ym2413ClockSource;
using sony_msx::devices::audio::Ym2413Opll;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Deterministic fake clock source: the test sets cycle_ directly, exactly
// mirroring how Hbf1xvMachine::Ym2413Clock reports scheduler_.total_cycles()
// (src/machine/hbf1xv_machine.cpp), but under full test control.
class FakeClock final : public Ym2413ClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycle_; }
    std::uint64_t cycle_ = 0;
};

}  // namespace

int main() {
    // --- Default: the gate is OFF until a caller explicitly enables it. ---
    {
        Ym2413Opll ym;
        ym.reset();
        expect(!ym.write_timing_enforced(), "Default_WriteTimingGate_Disabled");
    }

    // --- Gate disabled (even with a clock attached): writes land regardless
    //     of spacing -- exact M17 back-to-back behaviour is preserved. ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        // write_timing_enforced_ left at its default (false).
        clock.cycle_ = 0;
        ym.write_address(0x10);
        clock.cycle_ = 0;  // zero spacing
        ym.write_data(0x5A);
        expect(ym.register_value(0x10) == 0x5A, "GateDisabled_ZeroSpacingWrites_StillLand");
    }

    // --- Gate enabled, no clock source attached: acts as a no-op passthrough
    //     (documented fallback -- cannot measure elapsed cycles without a
    //     clock, so the gate stays inert). ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.set_write_timing_enforced(true);
        ym.write_address(0x10);
        ym.write_data(0x5A);
        expect(ym.register_value(0x10) == 0x5A, "GateEnabled_NoClockSource_ActsAsPassthrough");
    }

    // --- First write ever (no prior write recorded) is always accepted,
    //     regardless of the gate. ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        ym.set_write_timing_enforced(true);
        clock.cycle_ = 12345;  // arbitrary non-zero start
        ym.write_address(0x02);
        // Confirm the address landed by completing a same-instant data write
        // that itself deliberately violates the 12-cycle rule -- must be
        // dropped, proving the address write above DID land (register 0x02
        // stays at its pre-write reset value of 0, not overwritten).
        ym.write_data(0x99);  // dropped: 0 cycles elapsed since the address write
        expect(ym.register_value(0x02) == 0x00,
               "FirstWrite_AlwaysAccepted_FollowUpDataWriteAtZeroSpacing_Dropped");
    }

    // --- Address-write boundary: exactly 12 cycles accepts, 11 drops. ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        ym.set_write_timing_enforced(true);

        clock.cycle_ = 0;
        ym.write_address(0x05);  // seeds last_write = address @ cycle 0
        clock.cycle_ = 11;       // 11 < 12 -> dropped
        ym.write_data(0x11);
        expect(ym.register_value(0x05) == 0x00, "AddressWrite_At11Cycles_DataWriteDropped");

        clock.cycle_ = 12;  // 12 >= 12 -> accepted
        ym.write_data(0x22);
        expect(ym.register_value(0x05) == 0x22, "AddressWrite_At12Cycles_DataWriteAccepted");
    }

    // --- Data-write boundary: exactly 84 cycles accepts, 83 drops. The next
    //     register's ADDRESS write is what is gated here (fact-sheet §8:
    //     "after a data write, wait >=84 master cycles ... before the next
    //     write"). ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        ym.set_write_timing_enforced(true);

        clock.cycle_ = 0;
        ym.write_address(0x06);
        clock.cycle_ = 12;  // satisfies the 12-cycle address->data rule
        ym.write_data(0x33);  // last_write = data @ cycle 12
        expect(ym.register_value(0x06) == 0x33, "Setup_DataWrite_Lands");

        clock.cycle_ = 12 + 83;  // 83 < 84 -> the NEXT address write is dropped
        ym.write_address(0x07);
        clock.cycle_ = 12 + 83;  // same instant: would land in 0x07 if accepted
        ym.write_data(0x44);
        expect(ym.register_value(0x07) == 0x00, "DataWrite_At83Cycles_NextAddressWriteDropped");

        clock.cycle_ = 12 + 84;  // 84 >= 84 -> accepted
        ym.write_address(0x07);
        clock.cycle_ = 12 + 84 + 12;  // satisfy the address->data 12-cycle rule too
        ym.write_data(0x55);
        expect(ym.register_value(0x07) == 0x55, "DataWrite_At84Cycles_NextAddressWriteAccepted");
    }

    // --- A dropped write does NOT reset the timing reference: the busy
    //     window is measured from the last VALID write, not the ignored
    //     attempt (mirrors real-hardware "ignored while busy" behaviour). ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        ym.set_write_timing_enforced(true);

        clock.cycle_ = 0;
        ym.write_address(0x08);       // last_write = address @ cycle 0
        clock.cycle_ = 5;             // < 12 -> dropped, reference stays @ cycle 0
        ym.write_data(0xAA);
        clock.cycle_ = 12;            // 12 - 0 == 12 -> accepted (NOT 12 - 5)
        ym.write_data(0xBB);
        expect(ym.register_value(0x08) == 0xBB,
               "DroppedWrite_DoesNotResetTimingReference");
    }

    // --- reset() clears the tracker: the first write after reset() is
    //     always accepted even at the same clock cycle as a pre-reset write. ---
    {
        Ym2413Opll ym;
        FakeClock clock;
        ym.reset();
        ym.attach_clock_source(&clock);
        ym.set_write_timing_enforced(true);

        clock.cycle_ = 100;
        ym.write_address(0x09);
        ym.write_data(0x00);  // dropped (0 spacing), harmless -- just seeds state

        ym.reset();  // clears has_last_write_, NOT clock_source_/enforced flag
        expect(ym.write_timing_enforced(), "Reset_LeavesEnforcedToggleUntouched");

        clock.cycle_ = 100;  // same cycle as before reset -- still accepted
        ym.write_address(0x09);
        clock.cycle_ = 100;  // 0 spacing from the FRESH last-write -> dropped
        ym.write_data(0xCC);
        expect(ym.register_value(0x09) == 0x00, "Reset_ClearsTracker_ButAddressWriteLatchedNotData");

        // Prove the address write itself landed (post-reset first write
        // always accepted) by completing a properly-spaced data write.
        clock.cycle_ = 112;  // 100 + 12
        ym.write_data(0xDD);
        expect(ym.register_value(0x09) == 0xDD, "Reset_FirstWriteAfterReset_Accepted");
    }

    // --- Two-run determinism: identical clock-driven write sequence ->
    //     identical register state (including which writes were dropped). ---
    {
        auto run = []() {
            Ym2413Opll ym;
            FakeClock clock;
            ym.reset();
            ym.attach_clock_source(&clock);
            ym.set_write_timing_enforced(true);
            std::uint64_t t = 0;
            for (int reg = 0; reg < 0x40; ++reg) {
                clock.cycle_ = t;
                ym.write_address(static_cast<std::uint8_t>(reg));
                // Deliberately alternate spacing: even registers get exactly
                // the minimum (12), odd registers get 1 cycle short (11,
                // dropped) to exercise both branches identically across runs.
                t += (reg % 2 == 0) ? 12 : 11;
                clock.cycle_ = t;
                ym.write_data(static_cast<std::uint8_t>((reg * 7 + 3) & 0xFF));
                t += 90;  // always satisfies the 84-cycle data->next-address rule
            }
            return ym;
        };
        const Ym2413Opll a = run();
        const Ym2413Opll b = run();
        bool identical = true;
        for (int reg = 0; reg < 0x40; ++reg) {
            if (a.register_value(static_cast<std::uint8_t>(reg)) !=
                b.register_value(static_cast<std::uint8_t>(reg))) {
                identical = false;
            }
        }
        expect(identical, "TwoRunDeterminism_ClockDrivenGate_IdenticalState");

        // Sanity: confirm the alternation actually produced a MIX of landed
        // and dropped data writes (not a degenerate all-drop/all-land run),
        // so this test is genuinely exercising both gate outcomes.
        const Ym2413Opll sample = run();
        bool saw_landed = false;
        bool saw_dropped = false;
        for (int reg = 0; reg < 0x40; reg += 2) {
            if (sample.register_value(static_cast<std::uint8_t>(reg)) ==
                static_cast<std::uint8_t>((reg * 7 + 3) & 0xFF)) {
                saw_landed = true;
            }
        }
        for (int reg = 1; reg < 0x40; reg += 2) {
            if (sample.register_value(static_cast<std::uint8_t>(reg)) !=
                static_cast<std::uint8_t>((reg * 7 + 3) & 0xFF)) {
                saw_dropped = true;
            }
        }
        expect(saw_landed, "Determinism_SampleRun_SawAtLeastOneLandedWrite");
        expect(saw_dropped, "Determinism_SampleRun_SawAtLeastOneDroppedWrite");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_AudioYm2413WriteTiming_Unit cases passed\n";
    return 0;
}
