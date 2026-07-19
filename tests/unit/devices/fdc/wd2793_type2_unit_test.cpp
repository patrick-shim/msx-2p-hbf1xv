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

// Suite: Devices_Fdc_Wd2793Type2_Unit
//
// WD2793 core Type II (Read Sector / Write Sector): DRQ cadence, Lost Data (read
// vs write semantics differ), Record Not Found, multi-record track-boundary stop.
//
// Grounding (read only, never copied - GPL isolation): openMSX 21.0:
// src/fdc/WD2793.cc startType2Cmd (:522), getDataReg/LOST_DATA (:249-268),
// setDataReg (:235-247), RECORD_NOT_FOUND; fact-sheet "FDC for Sony HB-F1XV.md"
// §3 (Type II table), §8 ("Lost Data semantics differ read vs write").

#include <cstdint>
#include <iostream>
#include <vector>

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

constexpr std::uint64_t kReadStartCycles = 2 * 114;  // write / read-addr / read-track tail
constexpr std::uint64_t kCyclesPerByte = 114;
// Read Sector first-DRQ = rotational wait (until the sector's ID mark rotates
// under the head) + this fixed intra-sector ID-header->data span (DEC-0055).
// 47 * 114 = 5358 (Wd2793::kReadSectorHeaderCycles; openMSX startReadSector
// gapLength+2, WD2793.cc:624-644). See the rotational-latency case below.
constexpr std::uint64_t kReadSectorHeaderCycles = 47 * kCyclesPerByte;
constexpr std::uint64_t kIndexPeriodCycles = 715909;  // DiskDrive::kIndexPeriodCycles

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

    // Poll (1 emulated cycle at a time - a unit test, not perf-critical) until DRQ
    // asserts, mirroring the disk-BIOS polled service loop (fact-sheet §6). Never
    // overshoots the exact DRQ-arm cycle, so it cannot spuriously trigger the Lost
    // Data catch-up path.
    void wait_for_drq() {
        int guard = 0;
        while (!fdc.drq() && guard < 4'000'000) {
            ++clock.cycles;
            ++guard;
        }
    }
};

}  // namespace

int main() {
    // --- Read Sector, LBA 0 (track 0, side 0, sector 1): 512 DRQ bytes match the
    //     image; command completes with INTRQ and no error bits. ---
    {
        Fixture f;
        std::uint8_t expected[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 1, expected);

        f.clock.cycles = 0;
        f.fdc.write_track(0);   // TR = track 0 (matches drive's physical track)
        f.fdc.write_sector(1);  // SR = sector 1
        f.fdc.write_command(0x80);  // Read Sector, single, no flags

        std::vector<std::uint8_t> got;
        got.reserve(DiskImage::kSectorSize);
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            got.push_back(f.fdc.read_data());
        }
        bool identical = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (got[i] != expected[i]) {
                identical = false;
                break;
            }
        }
        expect(identical, "ReadSector_512Bytes_MatchImage");
        expect(f.fdc.intrq(), "ReadSector_Intrq_SetAfterCompletion");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "ReadSector_Busy_ClearedAfterCompletion");
        expect((status & 0x04) == 0, "ReadSector_LostData_NotSet");
        expect((status & 0x10) == 0, "ReadSector_RecordNotFound_NotSet");
    }

    // --- Missed DRQ on read: letting several byte-periods elapse before the CPU
    //     services the register sets Lost Data (byte(s) silently dropped). ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(1);
        f.fdc.write_command(0x80);
        // Let 5 whole byte-periods elapse past the ACTUAL first-DRQ window before
        // the first read_data() call (deliberate overshoot - the point of this
        // case). The first DRQ is index-pulse-relative (rotational wait +
        // intra-sector header), not the old fixed 2-byte kReadStartCycles
        // (DEC-0055), so the overshoot is measured from the device's actual
        // computed first-DRQ deadline. For sector 1 at rotational phase 0
        // (clock=0 at command issue) the rotational wait is 0, so that deadline is
        // exactly kReadSectorHeaderCycles (5358) -- asserted directly here. The
        // Lost-Data semantics under test are UNCHANGED (5 byte-periods late ->
        // Lost Data set); only the reference point moved.
        const std::uint64_t first_drq = f.fdc.drq_deadline();
        expect(first_drq == kReadSectorHeaderCycles, "ReadSector_FirstDrq_Sector1Phase0_HeaderOnly");
        f.clock.cycles = first_drq + 5 * kCyclesPerByte;
        f.fdc.read_data();
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x04) != 0, "ReadSector_MissedDrq_LostDataSet");
    }

    // --- Type-II Read Sector first-DRQ ROTATIONAL latency (DEC-0055).
    //     The first DRQ is scheduled index-pulse-relative: t + (cycles until the
    //     requested sector's ID mark rotates under the head) + kReadSectorHeader-
    //     Cycles. The rotational wait is VARIABLE in the sector index AND in the
    //     disk angle (phase = clock % kIndexPeriodCycles) at command start, unlike
    //     the OLD fixed 228-cycle model. Evenly-spaced sequential sector model:
    //     sector k (0-based) sits at angle (k * P) / 9 of the P=715909-cycle
    //     rotation (DiskDrive::cycles_until_sector_id); grounded in openMSX
    //     WD2793.cc:544/557/624 + RealDrive.cc:453 (documented sector-model
    //     approximation of the raw-track model). Every expected value is
    //     hand-derived below and asserted EXACTLY (hard oracle). ---
    {
        // Helper: first-DRQ deadline for (sector, issue_cycle). A fresh fixture so
        // the read is issued at a known absolute cycle (== rotational phase, since
        // issue_cycle < P here) with the head on track 0.
        auto first_drq_deadline = [](std::uint64_t issue_cycle,
                                     std::uint8_t sector) -> std::uint64_t {
            Fixture f;
            f.clock.cycles = issue_cycle;
            f.fdc.write_track(0);
            f.fdc.write_sector(sector);
            f.fdc.write_command(0x80);  // Read Sector, single
            return f.fdc.drq_deadline();
        };

        // Phase 0 (issue at cycle 0): rotational wait = ((k*P)/9 - 0) mod P = (k*P)/9.
        //   k=0 (SR1): 0        -> deadline 0      + 5358 =   5358
        //   k=1 (SR2): 715909/9=79545  -> 79545  + 5358 =  84903
        //   k=4 (SR5): 4*715909/9=318181 -> 318181 + 5358 = 323539
        //   k=8 (SR9): 8*715909/9=636363 -> 636363 + 5358 = 641721
        expect(first_drq_deadline(0, 1) == 5358, "RotLatency_Sector1_Phase0_Deadline");
        expect(first_drq_deadline(0, 2) == 84903, "RotLatency_Sector2_Phase0_Deadline");
        expect(first_drq_deadline(0, 5) == 323539, "RotLatency_Sector5_Phase0_Deadline");
        expect(first_drq_deadline(0, 9) == 641721, "RotLatency_Sector9_Phase0_Deadline");

        // Latency is VARIABLE across sectors at the same phase (the defect fix vs
        // the OLD fixed 228-cycle first DRQ).
        expect(first_drq_deadline(0, 1) != first_drq_deadline(0, 5) &&
                   first_drq_deadline(0, 5) != first_drq_deadline(0, 9),
               "RotLatency_VariesBySector");

        // Latency is VARIABLE across rotational phase for the SAME sector. Sector 1
        // (angle 0) issued at cycle 400000: wait = (0 + P - 400000) mod P = 315909;
        // deadline = 400000 + 315909 + 5358 = 721267.
        expect(first_drq_deadline(400000, 1) == 721267, "RotLatency_Sector1_Phase400k_Deadline");
        expect(first_drq_deadline(0, 1) != first_drq_deadline(400000, 1),
               "RotLatency_VariesByPhase");

        // DETERMINISM: identical issue cycle -> identical first-DRQ deadline across
        // independent runs (pure function of the emulated cycle count).
        expect(first_drq_deadline(0, 5) == first_drq_deadline(0, 5),
               "RotLatency_Deterministic_SameCycleSameDeadline");
        expect(first_drq_deadline(123456, 7) == first_drq_deadline(123456, 7),
               "RotLatency_Deterministic_ArbitraryPhase");
    }

    // --- Record Not Found: requested sector does not exist on the current track
    //     (synchronous - no clock advance needed). ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(99);  // out of range (1..9 valid)
        f.fdc.write_command(0x80);
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x10) != 0, "ReadSector_BadSector_RecordNotFoundSet");
        expect((status & 0x01) == 0, "ReadSector_BadSector_BusyClearedImmediately");
        expect(f.fdc.intrq(), "ReadSector_BadSector_IntrqSet");
    }

    // --- Multi-record read stops at the track boundary (sector 9 is the last on
    //     a 9-sector track; auto-increment past it ends the command). ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(8);       // second-to-last sector
        f.fdc.write_command(0x90);   // Read Sector, MULTI (M flag, hi=0x90)

        std::uint32_t bytes_read = 0;
        for (int sector_index = 0; sector_index < 2; ++sector_index) {
            for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
                f.wait_for_drq();
                f.fdc.read_data();
                ++bytes_read;
            }
        }
        expect(bytes_read == 2 * DiskImage::kSectorSize, "MultiRead_TwoSectorsStreamed");
        expect(f.fdc.intrq(), "MultiRead_Intrq_SetAfterTrackBoundary");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "MultiRead_Busy_ClearedAtTrackBoundary");
        expect((status & 0x10) == 0, "MultiRead_NoRecordNotFound_AtNaturalStop");
        expect(f.fdc.sector_register() > 9, "MultiRead_SectorRegister_IncrementedPastTrack");
    }

    // --- Write Sector: 512 bytes streamed via DR land in the image. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(2);
        f.fdc.write_command(0xA0);  // Write Sector, single

        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            f.fdc.write_data(static_cast<std::uint8_t>(i & 0xFFu));
        }
        expect(f.fdc.intrq(), "WriteSector_Intrq_SetAfterCompletion");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "WriteSector_Busy_ClearedAfterCompletion");
        expect((status & 0x04) == 0, "WriteSector_LostData_NotSet");

        std::uint8_t written[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 2, written);
        bool identical = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (written[i] != static_cast<std::uint8_t>(i & 0xFFu)) {
                identical = false;
                break;
            }
        }
        expect(identical, "WriteSector_ImageContainsWrittenBytes");
    }

    // --- Write Sector first-byte CHECK_WRITE gate + one-byte-pipeline late-byte
    //     tolerance (openMSX startWriteSector/checkStartWrite
    //     WD2793.cc:646-701 + writeSectorData :742-782). The OLD model modelled
    //     DRQ as a free-running absolute-cycle level deadline and, per byte-period
    //     the CPU trailed it, substituted 0x00 + LOST_DATA -- and each injected
    //     zero consumed a data slot, so the sector finished EARLY and the real
    //     tail bytes were dropped (dense zeros + shifted survivors + truncated
    //     tail: the disk-save corruption). The corrected edge handshake commits
    //     exactly one byte per CPU write and re-bases the next DRQ on the actual
    //     write time, so a byte that merely arrives "late" is committed (never
    //     lost); LOST_DATA is confined to a genuinely-missed FIRST byte. ---
    {
        // (a) A first byte that is a few byte-periods late but WITHIN the
        //     CHECK_WRITE window is accepted cleanly -- NO Lost Data, and the
        //     full sector lands byte-perfect (the well-behaved-write superset).
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(3);
        f.fdc.write_command(0xA0);
        // The write first-byte DRQ is rotational-search-
        // relative (begin_write_sector), the SAME model the read path uses -- NOT
        // the old fixed 2-byte read_start_cycles(). Supply the first byte a few
        // byte-periods after the ACTUAL (rotational) first-DRQ deadline: late, but
        // far inside the full-revolution CHECK_WRITE window -> committed, not lost.
        const std::uint64_t first_drq = f.fdc.drq_deadline();
        f.clock.cycles = first_drq + 3 * kCyclesPerByte;
        f.fdc.write_data(0x00);  // first byte (payload byte 0 == 0)
        expect((f.fdc.peek_status() & 0x04) == 0,
               "WriteSector_LateButWithinWindowFirstByte_NoLostData");
        for (std::uint32_t i = 1; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            f.fdc.write_data(static_cast<std::uint8_t>(i & 0xFFu));
        }
        expect((f.fdc.peek_status() & 0x04) == 0, "WriteSector_NormalTail_NoLostData");
        expect(f.fdc.intrq(), "WriteSector_LateFirstByte_StillCompletes");
        std::uint8_t written[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 3, written);
        bool ok = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (written[i] != static_cast<std::uint8_t>(i & 0xFFu)) {
                ok = false;
                break;
            }
        }
        expect(ok, "WriteSector_LateFirstByte_ImageByteExact");
    }
    {
        // (b) A first byte that never arrives before the CHECK_WRITE deadline
        //     ABORTS the command: LOST_DATA set, Busy cleared, INTRQ raised, and
        //     NOTHING is written to disk (no all-zero sector -- the sector keeps
        //     its prior content). openMSX checkStartWrite (WD2793.cc:674-682).
        Fixture f;
        std::uint8_t before[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 6, before);
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(6);
        f.fdc.write_command(0xA0);
        // Jump the clock one byte-period past the (rotational) CHECK_WRITE deadline
        // WITHOUT supplying any byte -> the gate aborts. The
        // window is the rotational first-DRQ + a full revolution, so the
        // overshoot is measured from the device's ACTUAL write_check_deadline().
        f.clock.cycles = f.fdc.write_check_deadline() + kCyclesPerByte;
        (void)f.fdc.drq();  // force a sync() -> the CHECK_WRITE gate aborts here
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x04) != 0, "WriteSector_FirstByteMissed_LostDataSet");
        expect((status & 0x01) == 0, "WriteSector_FirstByteMissed_BusyCleared");
        expect(f.fdc.intrq(), "WriteSector_FirstByteMissed_IntrqSet");
        std::uint8_t after[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 6, after);
        bool unchanged = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (before[i] != after[i]) {
                unchanged = false;
                break;
            }
        }
        expect(unchanged, "WriteSector_FirstByteMissed_ImageUnchanged_NoAllZeroSector");
    }

    // --- Write-protected medium blocks Write Sector (status bit + no image change). ---
    {
        Fixture f;
        f.image.set_write_protected(true);
        std::uint8_t before[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 4, before);

        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(4);
        f.fdc.write_command(0xA0);
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x40) != 0, "WriteSector_WriteProtected_StatusBitSet");
        expect((status & 0x01) == 0, "WriteSector_WriteProtected_BusyClearedImmediately");

        std::uint8_t after[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 4, after);
        bool unchanged = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (before[i] != after[i]) {
                unchanged = false;
                break;
            }
        }
        expect(unchanged, "WriteSector_WriteProtected_ImageUnchanged");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
