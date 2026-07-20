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

// Suite: Devices_Fdc_Wd2793Type4_Unit
//
// WD2793 core Type IV (Force Interrupt) i3 (immediate INTRQ) and i2
// (index-pulse-scheduled INTRQ) semantics, isolated from the Type II
// interrupted-mid-transfer case already covered by
// tests/unit/devices/fdc/wd2793_type3_unit_test.cpp.
//
// Grounding (read only, never copied - GPL isolation):
// openMSX 21.0: src/fdc/WD2793.cc:1035-1060 startType4Cmd —
//   flags==0x00            -> immediateIRQ = false (no forced INTRQ)
//   flags & IDX_IRQ (i2)   -> irqTime = drive.getTimeTillIndexPulse(time), i.e.
//                             INTRQ is armed to fire at the NEXT index pulse,
//                             NOT immediately (:1049-1050)
//   flags & IMM_IRQ (i3)   -> immediateIRQ = true (INTRQ true right away, :1054-1055)
//   unconditionally        -> DRQ cleared, Busy cleared (:1058-1059)
// FDC for Sony HB-F1XV fact sheet §3 Type IV row, §3 "Force
// Interrupt" notes, §8 "Force Interrupt / status-after-FI".

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
    // --- i3 (0x08): immediate INTRQ, no clock advance needed. ---
    {
        Fixture f;
        f.clock.cycles = 1000;
        // Seek to track 0 (already there -> zero step pulses) WITH the V
        // (verify/settle) flag, so the command stays genuinely Busy for the
        // settle delay without moving the head off track 0 (Busy set, no INTRQ
        // pending yet) before the Force Interrupt lands.
        f.fdc.write_data(0x00);      // seek target track 0 (data reg pre-load)
        f.fdc.write_command(0x14);   // Seek (Type I) + V flag -> settle delay
        expect((f.fdc.peek_status() & 0x01) != 0, "I3_PreFI_SeekBusy");
        expect(!f.fdc.intrq(), "I3_PreFI_NoIntrqYet");

        f.fdc.write_command(0xD8);   // Force Interrupt, i3 (immediate)
        expect(f.fdc.intrq(), "I3_ImmediateInterrupt_AssertedRightAway");
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x01) == 0, "I3_ForceInterrupt_ClearsBusyImmediately");
        // command_reg hi nibble == 0xD0 -> Type-I status layout active; the
        // drive never left track 0, so Track00 (bit2) reads set.
        expect((status & 0x04) != 0, "I3_TypeIStatusLayout_Track00BitReflectsDrive");
    }

    // --- i2 (0x04): INTRQ is armed for the NEXT index pulse, not immediate. ---
    {
        Fixture f;
        // Position the clock just past the first pulse window's end, well
        // before the next period boundary, so "cycles_until_index_pulse" must
        // wait rather than fire immediately.
        f.clock.cycles = DiskDrive::kIndexPulseWidthCycles + 1000;
        expect(f.drive.ready(), "I2_Setup_DriveReady");
        expect(!f.drive.index_pulse(f.clock.cycles), "I2_Setup_NotCurrentlyInPulseWindow");

        f.fdc.write_command(0xD4);   // Force Interrupt, i2 (index-pulse IRQ)
        expect(!f.fdc.intrq(), "I2_NotAssertedImmediately_AfterForceInterrupt");
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x01) == 0, "I2_ForceInterrupt_ClearsBusyImmediately");

        // Still not asserted just before the deadline. The first pulse window
        // is [0, kIndexPulseWidthCycles); starting from a clock value already
        // past that window and below one period, the next pulse (and thus the
        // scheduled INTRQ) lands exactly at kIndexPeriodCycles.
        f.clock.cycles = DiskDrive::kIndexPeriodCycles - 1;
        expect(!f.fdc.intrq(), "I2_StillNotAsserted_JustBeforeDeadline");

        // At/after the deadline, INTRQ fires exactly once.
        f.clock.cycles = DiskDrive::kIndexPeriodCycles;
        expect(f.fdc.intrq(), "I2_Asserted_AtNextIndexPulseDeadline");

        // Reading status clears the latched INTRQ (real WD2793 semantics); the
        // one-shot schedule does not re-arm on its own.
        const std::uint8_t after = f.fdc.read_status();
        (void)after;
        expect(!f.fdc.intrq(), "I2_IntrqClearedByStatusRead_NoRearm");
        f.clock.cycles += 2 * DiskDrive::kIndexPeriodCycles;
        expect(!f.fdc.intrq(), "I2_NoRearm_AcrossLaterIndexPulses");
    }

    // --- i2 with drive NOT ready: never arms, INTRQ never fires from it. ---
    {
        Fixture f;
        f.drive.set_available(false);
        expect(!f.drive.ready(), "I2NotReady_Setup_DriveNotReady");
        f.clock.cycles = 0;
        f.fdc.write_command(0xD4);   // Force Interrupt, i2
        f.clock.cycles = 5 * DiskDrive::kIndexPeriodCycles;
        expect(!f.fdc.intrq(), "I2NotReady_NeverAsserted_EvenAfterManyPeriods");
    }

    // --- A new (non-FI) command supersedes a still-pending i2 schedule. ---
    {
        Fixture f;
        f.clock.cycles = DiskDrive::kIndexPulseWidthCycles + 1000;
        f.fdc.write_command(0xD4);   // Force Interrupt, i2 armed for next pulse
        // Before the deadline, issue a fresh Type I Restore (short, drive already
        // at track 0 -> completes essentially immediately).
        f.fdc.write_command(0x00);   // Restore
        expect(f.fdc.intrq(), "Superseded_RestoreCompletesImmediately_IntrqFromRestore");
        (void)f.fdc.read_status();  // ack the Restore's own INTRQ
        expect(!f.fdc.intrq(), "Superseded_IntrqClearedAfterRead");
        // Advance well past what would have been the stale i2 deadline: the
        // superseded schedule must NOT resurrect a spurious INTRQ.
        f.clock.cycles = DiskDrive::kIndexPeriodCycles + 10;
        expect(!f.fdc.intrq(), "Superseded_StaleIndexIrqSchedule_NeverFires");
    }

    // --- flags == 0x00 mid-command: terminates without forcing INTRQ. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(1);
        f.fdc.write_command(0x80);  // Read Sector (Type II)
        expect((f.fdc.peek_status() & 0x01) != 0, "FlagsZero_PreFI_ReadSectorBusy");
        expect(!f.fdc.intrq(), "FlagsZero_PreFI_NoIntrqYet");

        f.fdc.write_command(0xD0);  // Force Interrupt, flags = 0
        expect(!f.fdc.intrq(), "FlagsZero_ForceInterrupt_DoesNotForceIntrq");
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x01) == 0, "FlagsZero_ForceInterrupt_ClearsBusy");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
