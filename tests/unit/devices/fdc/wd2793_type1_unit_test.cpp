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

// Suite: Devices_Fdc_Wd2793Type1_Unit
//
// WD2793 core: 5 registers, master-reset TR=0xFF, Type I Restore/Seek, Type-I
// status-bit layout (Busy/Track00/Seek-Error/Index/Head-Loaded), INTRQ set/clear,
// step-rate/settle timing computed off an injected deterministic cycle clock
// (never wall-clock).
//
// Grounding (read only, never copied - GPL isolation): openMSX 21.0:
// src/fdc/WD2793.cc status constants (:15-26), reset (:60-77), startType1Cmd/seek/
// step/endType1Cmd (:420-519); fact-sheet "FDC for Sony HB-F1XV.md" §3 (TR=0xFF at
// reset), §8 ("Seek to track 0" edge cases: up to 255 step pulses, Seek Error if
// !TR00 never asserts and V=1).

#include <cstdint>
#include <iostream>

#include "devices/fdc/disk_drive.h"
#include "devices/fdc/disk_image.h"
#include "devices/fdc/fdc_clock_source.h"
#include "devices/fdc/wd2793.h"

namespace {

using sony_msx::devices::fdc::DiskDrive;
using sony_msx::devices::fdc::DiskImage;
using sony_msx::devices::fdc::FdcClockSource;
using sony_msx::devices::fdc::Wd2793;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

class FakeClock final : public FdcClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycles; }
    std::uint64_t cycles = 0;
};

struct Fixture {
    FakeClock clock;
    DiskImage image;
    DiskDrive drive;
    Wd2793 fdc;

    Fixture() {
        drive.attach_image(&image);
        fdc.attach_clock_source(&clock);
        fdc.attach_drive(&drive);
        fdc.reset();
        drive.reset();
        drive.attach_image(&image);
    }
};

}  // namespace

int main() {
    // --- Master reset: TR = 0xFF (fact-sheet §3 + §8). ---
    {
        Fixture f;
        expect(f.fdc.track_register() == 0xFF, "Reset_TrackRegister_0xFF");
    }

    // --- Restore (0x00, no flags): drive already at track 0 -> TR=0, Track00 set. ---
    {
        Fixture f;
        f.clock.cycles = 1000;
        f.fdc.write_command(0x00);  // Restore, rate0, no V/H flags
        // Advance the clock past the computed Busy deadline (0 steps since already
        // at track 0 -> only the step-rate table lookup, no settle since V=0).
        f.clock.cycles += 1;
        // intrq() syncs the FSM and reports pending INTRQ WITHOUT the read_status()
        // clear-on-read side effect, so it must be checked before read_status().
        expect(f.fdc.intrq(), "Restore_Intrq_SetAfterCompletion");
        const std::uint8_t status = f.fdc.peek_status();
        expect(f.fdc.track_register() == 0, "Restore_AlreadyAtTrack0_TrackRegisterZero");
        expect((status & 0x04) != 0, "Restore_Track00Bit_Set");
        expect((status & 0x01) == 0, "Restore_Busy_ClearedAfterCompletion");
    }

    // --- Restore when NOT already at track 0: steps until TR00, then TR=0. ---
    {
        Fixture f;
        f.drive.set_physical_track(5);
        f.clock.cycles = 0;
        f.fdc.write_command(0x03);  // Restore, rate3 (slowest, matches WD2793 reset default)
        // Busy should still be set immediately after issuing the command (before the
        // computed step time elapses).
        expect((f.fdc.read_status() & 0x01) != 0, "Restore_Busy_SetImmediatelyAfterIssue");
        // Advance well past the worst-case step time (5 steps at the slowest rate).
        f.clock.cycles += 10 * 107386ull + 1;
        expect(f.fdc.intrq(), "Restore_FromTrack5_IntrqSet");
        const std::uint8_t status = f.fdc.peek_status();
        expect(f.fdc.track_register() == 0, "Restore_FromTrack5_EndsAtTrack0");
        expect((status & 0x04) != 0, "Restore_FromTrack5_Track00BitSet");
        expect((status & 0x01) == 0, "Restore_FromTrack5_BusyClearedAfterCompletion");
    }

    // --- Seek: writing the Data Register then Seek (0x10) updates TR to match. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_data(42);
        f.fdc.write_command(0x10);  // Seek, rate0, no V
        f.clock.cycles += 200000ull;  // past any plausible step/settle deadline
        (void)f.fdc.read_status();
        expect(f.fdc.track_register() == 42, "Seek_TrackRegister_MatchesDataRegister");
    }

    // --- Seek Error: no drive selected (available=false) -> TR00 never asserts;
    //     with V=1 the Restore reports Seek Error after the (capped) step budget. ---
    {
        Fixture f;
        f.drive.set_available(false);  // simulates drive B/NONE selected (not-ready)
        f.clock.cycles = 0;
        f.fdc.write_command(0x04);  // Restore, rate0, V=1 (kVFlag=0x04)
        // 255-pulse cap at the fastest step rate (rate0) + settle (V=1, kSettleCycles).
        f.clock.cycles += 255ull * 21477ull + 107386ull + 1;
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x10) != 0, "SeekError_SetWhenTr00NeverAsserts_WithV");
        expect((status & 0x01) == 0, "SeekError_BusyClearedAfterCommandCompletes");
    }

    // --- Without V=1, the same never-reaches-track-00 condition does NOT report
    //     Seek Error (fact-sheet §3: "only reported if V=1"). ---
    {
        Fixture f;
        f.drive.set_available(false);
        f.clock.cycles = 0;
        f.fdc.write_command(0x00);  // Restore, rate0, V=0
        f.clock.cycles += 255ull * 21477ull + 1;
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x10) == 0, "SeekError_NotReported_WithoutV");
    }

    // --- Busy set->clear with INTRQ after the computed cycle deadline (Step cmd). ---
    {
        Fixture f;
        f.clock.cycles = 500;
        f.fdc.write_command(0x20);  // Step, rate0, no V
        expect(!f.fdc.intrq(), "Step_Intrq_NotYetSet");
        expect((f.fdc.peek_status() & 0x01) != 0, "Step_Busy_SetRightAfterIssue");
        f.clock.cycles += 21477ull + 1;  // past the 1-step rate0 deadline
        expect(f.fdc.intrq(), "Step_Intrq_SetAfterComputedCycles");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "Step_Busy_ClearedAfterComputedCycles");
    }

    // --- Force Interrupt (Type IV) reverts STR to Type-I semantics; clears Busy. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.drive.set_physical_track(7);  // != 0, so Track00 bit should read clear
        f.fdc.write_command(0x40);      // Step-In (rate0, no T flag): steps to 8, Busy set
        expect((f.fdc.peek_status() & 0x01) != 0, "PreFI_Busy_Set");
        f.fdc.write_command(0xD0);  // Force Interrupt, flags=0 (terminate, no INTRQ)
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x01) == 0, "ForceInterrupt_ClearsBusy");
        // Type-I status layout is active (command_reg hi nibble == 0xD0): Track00
        // reflects the drive's actual physical position (stepped to 8 -> bit clear).
        expect((status & 0x04) == 0, "ForceInterrupt_Track00BitReflectsDrive_NotAtZero");
    }

    // --- Force Interrupt with i3 (immediate IRQ) sets INTRQ immediately. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_command(0xD8);  // Force Interrupt, i3=1 (immediate INTRQ)
        expect(f.fdc.intrq(), "ForceInterrupt_ImmediateIrq_IntrqSet");
    }

    // --- read_status() clears INTRQ (side effect); a fresh command re-arms it. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_command(0x00);  // Restore, already at track 0 -> completes fast
        f.clock.cycles += 1;
        expect(f.fdc.intrq(), "Intrq_SetAfterRestoreCompletes");
        (void)f.fdc.read_status();
        expect(!f.fdc.intrq(), "Intrq_ClearedByReadStatus");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
