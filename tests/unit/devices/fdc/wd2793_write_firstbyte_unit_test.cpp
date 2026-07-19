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

// Suite: Devices_Fdc_Wd2793WriteFirstByte_Unit  (DEF-M45-WRITEDRQ window fix)
//
// ANTI-REGRESSION for the WD2793 Write Sector FIRST-BYTE CHECK_WRITE window that
// the original M45 fix made TOO SHORT and which then ABORTED a legitimate
// multi-disk-RPG-title
// in-game disk SAVE (worse than the corruption it replaced: the user could no
// longer save at all -- a Type-I Step/Restore retry hang after the write aborted
// with STATUS=04 LOST_DATA, DATA_INDEX=0).
//
// Root cause: begin_write_sector set the first-byte DRQ window to the FIXED
// read_start_cycles() (2 byte-periods, ZERO rotational latency) plus a fixed
// 8-byte CHECK_WRITE tail ~= 1140 cycles. On a real WD2793 / openMSX the first
// data-byte DRQ is asserted only AFTER head-load + the ROTATIONAL sector-search
// (up to ~a full revolution): startWriteSector schedules DRQ + CHECK_WRITE from
// type2Search/getNextSector's POST-search time (references/openmsx-21.0/src/fdc/
// WD2793.cc:646-701, understanding only -- never copied, GPL isolation). A game
// that does save-buffer setup before writing byte 1 easily exceeds 1140 cycles,
// so the old gate mis-fired on a VALID write.
//
// The fix (begin_write_sector) gives the write's first-byte DRQ the SAME
// rotational sector-search latency the READ path uses, and gives the CHECK_WRITE
// a FULL further disk revolution -- so a valid slow-first-byte write is NEVER
// aborted, in BOTH --fast-disk modes, while a genuinely-absent first byte still
// aborts deterministically (LOST_DATA, command completes, no BUSY hang).
//
// The pre-existing wd2793_type2/fastdisk/write_stall oracles all supplied byte 0
// PROMPTLY (right after DRQ), so none exercised the first-byte window -- this
// suite deliberately inserts a realistic setup delay (pure CPU work, NO FDC
// register access, so sync() is not called during it) BEFORE byte 1, reproducing
// the RPG-title cadence. Adversarial-revert proof: restore the fixed
// `drq_deadline_ = t + read_start_cycles();` +
// `write_check_deadline_ = drq_deadline_ + <8 byte-periods>` and the
// SlowFirstByte_* cases FAIL (the write aborts: completed=false, lost_data=true).
// It passes against the fix.

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

// One 300 rpm revolution (DiskDrive::kIndexPeriodCycles / Wd2793::
// kWriteFirstByteWindowCycles). "Within a rotation" bounds a realistic setup.
constexpr std::uint64_t kRevolutionCycles = 715909;

// Distinct GUARANTEED-NON-ZERO payload byte for (sector, offset): a substituted
// 0x00 or a shifted/dropped byte is directly visible as a mismatch, and cannot
// collide with the synthesized default medium fill.
std::uint8_t payload(int sector, int offset) {
    std::uint32_t x = static_cast<std::uint32_t>((sector * 2654435761u) + offset * 40503u);
    x ^= x >> 13;
    return static_cast<std::uint8_t>((x & 0xFEu) | 0x01u);  // force non-zero
}

struct Fixture {
    FakeClock clock;
    DiskImage image;
    DiskDrive drive;
    Wd2793 fdc;

    explicit Fixture(bool fast) {
        drive.attach_image(&image);
        fdc.attach_clock_source(&clock);
        fdc.attach_drive(&drive);
        fdc.reset();
        drive.reset();
        drive.attach_image(&image);
        fdc.set_fast_disk(fast);
        drive.set_fast_disk(fast);
    }
};

struct WriteOutcome {
    bool completed = false;   // INTRQ raised AND Busy cleared AND all 512 committed
    bool byte_exact = false;  // every written byte == payload
    bool lost_data = false;   // LOST_DATA status bit
    bool image_unchanged = false;  // sector still holds its pre-command content
    int bytes_written = 0;    // DRQ byte-transfers the loop actually completed
};

const char* mode(bool fast) { return fast ? "fast" : "accurate"; }

// Drive a single-sector WriteSector where the CPU performs `setup_delay` cycles
// of pure buffer setup (NO FDC register access -> no sync()) BEFORE it starts its
// poll-DRQ-and-write loop for byte 1 -- the RPG-title in-game-save cadence.
WriteOutcome run_slow_first_byte(bool fast, std::uint8_t sector, std::uint64_t setup_delay) {
    Fixture f(fast);
    f.drive.set_side(0);
    f.drive.set_physical_track(0);

    std::uint8_t before[DiskImage::kSectorSize] = {};
    f.image.read_chs(0, 0, sector, before);

    f.clock.cycles = 1000;
    f.fdc.write_track(0);
    f.fdc.write_sector(sector);
    f.fdc.write_command(0xA0);  // Write Sector, single

    // Save-buffer setup: pure CPU work, well past the OLD ~1140-cycle window but
    // within a rotation. No register touch -> the CHECK_WRITE gate cannot fire
    // "mid-setup"; it can only fire on the CPU's NEXT sync (its first DRQ poll).
    f.clock.cycles += setup_delay;

    // The game's poll-DRQ-and-write loop. Bounded, and it bails out the instant
    // the command aborts (BUSY cleared) so the OLD-code abort is detected FAST
    // (no 40M spin): with the old too-short gate, byte 0's first drq() poll
    // aborts, so this loop breaks at i == 0 with lost_data set.
    WriteOutcome r;
    for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
        int guard = 0;
        while (!f.fdc.drq() && (f.fdc.peek_status() & Wd2793::kBusy) != 0 &&
               guard < 3'000'000) {
            ++f.clock.cycles;
            ++guard;
        }
        if ((f.fdc.peek_status() & Wd2793::kBusy) == 0) {
            break;  // command finished or aborted before this byte
        }
        f.fdc.write_data(payload(sector, i));
        ++r.bytes_written;
    }
    f.clock.cycles += 200'000;  // settle

    const std::uint8_t status = f.fdc.peek_status();
    r.lost_data = (status & Wd2793::kLostData) != 0;
    r.completed = f.fdc.intrq() && (status & Wd2793::kBusy) == 0 &&
                  r.bytes_written == static_cast<int>(DiskImage::kSectorSize);

    std::uint8_t after[DiskImage::kSectorSize] = {};
    f.image.read_chs(0, 0, sector, after);
    r.byte_exact = true;
    r.image_unchanged = true;
    for (int off = 0; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
        if (after[off] != payload(sector, off)) r.byte_exact = false;
        if (after[off] != before[off]) r.image_unchanged = false;
    }
    return r;
}

}  // namespace

int main() {
    // --- (1) SlowFirstByte: the exact RPG-title regression. A first byte supplied
    //     after a realistic setup delay (8000 cycles -- ~7x the old 1140-cycle
    //     window, but well within a rotation) COMPLETES byte-perfect with NO
    //     LOST_DATA in BOTH modes. Against the OLD too-short window this ABORTS
    //     (completed=false, lost_data=true) -- the un-break-the-save proof. ---
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_slow_first_byte(fast, /*sector=*/1, /*setup=*/8000);
        expect(r.completed, "SlowFirstByte_8k_Completed");
        expect(r.byte_exact, "SlowFirstByte_8k_ByteExact");
        expect(!r.lost_data, "SlowFirstByte_8k_NoLostData");
        expect(r.bytes_written == static_cast<int>(DiskImage::kSectorSize),
               "SlowFirstByte_8k_All512Written");
        if (!r.completed || !r.byte_exact) {
            std::cerr << "  [" << mode(fast) << "] completed=" << r.completed
                      << " byte_exact=" << r.byte_exact << " lost_data=" << r.lost_data
                      << " bytes_written=" << r.bytes_written << "\n";
        }
    }

    // --- (2) A MUCH larger setup delay (100000 cycles, still within a rotation)
    //     also completes cleanly: the window is genuinely GENEROUS (a full
    //     revolution after the rotational DRQ), not merely nudged past 1140.
    //     Exercised on a different sector (rotational angle differs) in BOTH
    //     modes. ---
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_slow_first_byte(fast, /*sector=*/5, /*setup=*/100'000);
        expect(r.completed, "SlowFirstByte_100k_Completed");
        expect(r.byte_exact, "SlowFirstByte_100k_ByteExact");
        expect(!r.lost_data, "SlowFirstByte_100k_NoLostData");
    }

    // --- (3) A setup delay just past the OLD window boundary (1500 > 1140) still
    //     completes -- the specific value that used to trip the gate no longer
    //     does, in BOTH modes. ---
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_slow_first_byte(fast, /*sector=*/3, /*setup=*/1500);
        expect(r.completed, "SlowFirstByte_JustPastOldWindow_Completed");
        expect(r.byte_exact, "SlowFirstByte_JustPastOldWindow_ByteExact");
        expect(!r.lost_data, "SlowFirstByte_JustPastOldWindow_NoLostData");
    }

    // --- (4) GENUINELY-ABSENT first byte: the CPU issues Write Sector but never
    //     supplies byte 1. Advancing past the (rotational) CHECK_WRITE deadline
    //     aborts GRACEFULLY -- LOST_DATA set, Busy cleared, INTRQ raised, NOTHING
    //     written (image unchanged, no all-zero sector), and NO infinite BUSY
    //     hang -- in BOTH modes. This is the fault-detection the gate exists for;
    //     the fix widens the window, it does not remove the gate. ---
    for (bool fast : {false, true}) {
        Fixture f(fast);
        f.drive.set_side(0);
        f.drive.set_physical_track(0);
        std::uint8_t before[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 6, before);

        f.clock.cycles = 1000;
        f.fdc.write_track(0);
        f.fdc.write_sector(6);
        f.fdc.write_command(0xA0);
        // One byte-period past the device's actual write-check deadline, WITHOUT
        // supplying any data byte.
        f.clock.cycles = f.fdc.write_check_deadline() + Wd2793::kCyclesPerByte;
        (void)f.fdc.drq();  // force a sync() -> the CHECK_WRITE gate aborts here

        const std::uint8_t status = f.fdc.peek_status();
        expect((status & Wd2793::kLostData) != 0, "AbsentFirstByte_LostDataSet");
        expect((status & Wd2793::kBusy) == 0, "AbsentFirstByte_BusyCleared");
        expect(f.fdc.intrq(), "AbsentFirstByte_IntrqSet");
        std::uint8_t after[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 6, after);
        bool unchanged = true;
        for (int off = 0; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
            if (after[off] != before[off]) unchanged = false;
        }
        expect(unchanged, "AbsentFirstByte_ImageUnchanged_NoAllZeroSector");
    }

    // --- (5) DETERMINISM: two identical slow-first-byte runs write byte-identical
    //     images (pure function of the cycle model). ---
    {
        const WriteOutcome a = run_slow_first_byte(false, 2, 12'345);
        const WriteOutcome b = run_slow_first_byte(false, 2, 12'345);
        expect(a.completed && b.completed && a.byte_exact && b.byte_exact,
               "SlowFirstByte_Deterministic_BothComplete");
    }

    // --- (6) Guard the intent of the window constant: it is a full revolution,
    //     far larger than the old ~1140-cycle window, so any realistic
    //     within-a-rotation setup is covered. ---
    expect(Wd2793::kWriteFirstByteWindowCycles == kRevolutionCycles,
           "WriteWindow_IsOneRevolution");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "wd2793_write_firstbyte_unit_test: all cases passed\n";
    return 0;
}
