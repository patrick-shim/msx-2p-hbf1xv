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

// Suite: Devices_Fdc_Wd2793FastDisk_Unit
//
// Fast-disk (FDC turbo) quality-of-life mode: an OPT-IN toggle (default OFF)
// that collapses the WD2793/floppy read+seek timing WAITS so disk loads finish
// near-instantly, while default-off timing stays 100% cycle-accurate (byte-
// identical). This suite proves, deterministically:
//   1. Default OFF is unchanged (fast_disk() == false; the accurate first-DRQ
//      deadline is byte-identical to the pre-change constant).
//   2. Fast mode transfers the SAME 512 sector bytes correctly (read + write),
//      with NO Lost Data and INTRQ set -- data integrity preserved.
//   3. Fast mode completes in FAR fewer emulated cycles (the QoL speedup).
//   4. Fast mode is DETERMINISTIC (two runs -> identical bytes AND identical
//      completion cycle).
//   5. Fast mode preserves integrity even under a deliberate large read
//      overshoot (the Lost-Data catch-up is suppressed, so a "late" CPU never
//      skips/corrupts bytes -- the whole point of the turbo-disk gating model).
//   6. The rotational latency (the dominant cost) collapses to 0.
//   7. Machine-level set_fast_disk() propagates to BOTH the WD2793 and DiskDrive.
//
// Fast mode remains fully deterministic (cycle-based, just smaller offsets); it
// is NOT a wall-clock or randomized fast-forward.

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_drive.h"
#include "devices/fdc/disk_image.h"
#include "devices/fdc/fdc_clock_source.h"
#include "devices/fdc/wd2793.h"
#include "machine/hbf1xv_machine.h"

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

// Accurate-timing reference constants (Wd2793 header).
constexpr std::uint64_t kCyclesPerByte = 114;
constexpr std::uint64_t kReadSectorHeaderCycles = 47 * kCyclesPerByte;  // 5358

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
        // Set the turbo flag on BOTH devices (each owns its own delays), mirroring
        // Hbf1xvMachine::set_fast_disk().
        fdc.set_fast_disk(fast);
        drive.set_fast_disk(fast);
    }

    // Poll 1 emulated cycle at a time until DRQ asserts (mirrors the disk-BIOS
    // polled service loop). Never overshoots the exact DRQ-arm cycle, so in
    // ACCURATE mode it cannot spuriously trip the Lost-Data catch-up.
    void wait_for_drq() {
        int guard = 0;
        while (!fdc.drq() && guard < 8'000'000) {
            ++clock.cycles;
            ++guard;
        }
    }
};

struct ReadResult {
    std::vector<std::uint8_t> bytes;
    std::uint64_t elapsed = 0;  // cycles from command issue to completion
    std::uint8_t status = 0;
    bool intrq = false;
};

// Full single-sector Read Sector via the polled loop; returns the transferred
// bytes + the total emulated cycles the transfer took.
ReadResult run_read(bool fast, std::uint8_t sector, std::uint64_t issue_cycle) {
    Fixture f(fast);
    f.clock.cycles = issue_cycle;
    f.fdc.write_track(0);
    f.fdc.write_sector(sector);
    f.fdc.write_command(0x80);  // Read Sector, single, no flags
    ReadResult r;
    r.bytes.reserve(DiskImage::kSectorSize);
    for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
        f.wait_for_drq();
        r.bytes.push_back(f.fdc.read_data());
    }
    r.elapsed = f.clock.cycles - issue_cycle;
    r.status = f.fdc.peek_status();
    r.intrq = f.fdc.intrq();
    return r;
}

bool bytes_match_image(const std::vector<std::uint8_t>& got, DiskImage& image, std::uint8_t track,
                       std::uint8_t side, std::uint8_t sector) {
    std::uint8_t expected[DiskImage::kSectorSize] = {};
    image.read_chs(track, side, sector, expected);
    if (got.size() != DiskImage::kSectorSize) {
        return false;
    }
    for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
        if (got[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    // --- (1) Default OFF: fast_disk() is false and timing is byte-identical.
    //     A fresh controller reports fast_disk()==false, and the accurate first-
    //     DRQ deadline for sector 1 at phase 0 is exactly kReadSectorHeaderCycles
    //     (unchanged from the pre-turbo constant). ---
    {
        Fixture f(false);
        expect(!f.fdc.fast_disk(), "DefaultOff_FdcFlagFalse");
        expect(!f.drive.fast_disk(), "DefaultOff_DriveFlagFalse");
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(1);
        f.fdc.write_command(0x80);
        expect(f.fdc.drq_deadline() == kReadSectorHeaderCycles,
               "DefaultOff_FirstDrqDeadline_ByteIdenticalToAccurateConstant");
    }

    // --- (2)+(3)+(4) Fast-mode read: SAME bytes, FAR fewer cycles, deterministic.
    //     Sector 1 at phase 0 -- a clean case with zero rotational component even
    //     in accurate mode, so the speedup here is purely the per-byte/header
    //     collapse (the rotational win is proven separately in (6)). ---
    {
        Fixture ref(false);  // for the golden image bytes
        ReadResult accurate = run_read(false, 1, 0);
        ReadResult fast = run_read(true, 1, 0);

        expect(bytes_match_image(accurate.bytes, ref.image, 0, 0, 1),
               "AccurateRead_512Bytes_MatchImage");
        expect(bytes_match_image(fast.bytes, ref.image, 0, 0, 1),
               "FastRead_512Bytes_MatchImage_DataIntegrityPreserved");
        expect(fast.bytes == accurate.bytes, "FastRead_SameBytesAsAccurateRead");

        expect((fast.status & Wd2793::kBusy) == 0, "FastRead_Busy_Cleared");
        expect((fast.status & Wd2793::kLostData) == 0, "FastRead_LostData_NotSet");
        expect((fast.status & Wd2793::kRecordNotFound) == 0, "FastRead_RecordNotFound_NotSet");
        expect(fast.intrq, "FastRead_Intrq_Set");

        // The QoL speedup: fast completes in far fewer cycles. Conservative bound
        // (actual ratio ~15x for this case): fast*10 < accurate.
        expect(fast.elapsed * 10 < accurate.elapsed, "FastRead_FarFewerCycles_Than_Accurate");

        // Determinism: an independent second fast run yields identical bytes AND
        // an identical completion-cycle count (pure function of the cycle model).
        ReadResult fast2 = run_read(true, 1, 0);
        expect(fast2.bytes == fast.bytes, "FastRead_Deterministic_SameBytes");
        expect(fast2.elapsed == fast.elapsed, "FastRead_Deterministic_SameElapsedCycles");
    }

    // --- (5) Fast-mode integrity under a deliberate LARGE overshoot. A slow CPU
    //     that lets many byte-periods elapse before each service would, in
    //     ACCURATE mode, trip Lost Data and drop bytes. In FAST mode the catch-up
    //     is suppressed (the disk gates on CPU reads), so every byte is delivered
    //     sequentially and correctly, with NO Lost Data -- proving the collapse
    //     never corrupts a transfer. ---
    {
        Fixture f(true);
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(5);
        f.fdc.write_command(0x80);
        std::vector<std::uint8_t> got;
        got.reserve(DiskImage::kSectorSize);
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            // Jump the clock far past any plausible DRQ window before every read.
            f.clock.cycles += 100'000;
            got.push_back(f.fdc.read_data());
        }
        expect(bytes_match_image(got, f.image, 0, 0, 5),
               "FastRead_LargeOvershoot_BytesStillMatch_NoCorruption");
        expect((f.fdc.peek_status() & Wd2793::kLostData) == 0,
               "FastRead_LargeOvershoot_NoLostData");
        expect(f.fdc.intrq(), "FastRead_LargeOvershoot_Intrq_Set");
    }

    // --- (2b) Fast-mode WRITE: 512 streamed bytes land correctly in the image,
    //     no Lost Data, INTRQ set (the write path speeds up too). ---
    {
        Fixture f(true);
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(2);
        f.fdc.write_command(0xA0);  // Write Sector, single
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            f.fdc.write_data(static_cast<std::uint8_t>((i * 7 + 3) & 0xFFu));
        }
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & Wd2793::kBusy) == 0, "FastWrite_Busy_Cleared");
        expect((status & Wd2793::kLostData) == 0, "FastWrite_LostData_NotSet");
        expect(f.fdc.intrq(), "FastWrite_Intrq_Set");
        std::uint8_t written[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 2, written);
        bool ok = true;
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            if (written[i] != static_cast<std::uint8_t>((i * 7 + 3) & 0xFFu)) {
                ok = false;
                break;
            }
        }
        expect(ok, "FastWrite_ImageContainsWrittenBytes");
    }

    // --- (6) Rotational latency (the dominant load cost) collapses to 0 in fast
    //     mode. Accurate: sector index 8 (SR 9) at phase 0 sits ~8/9 of a
    //     revolution away (a large wait). Fast: 0 (treated as already under the
    //     head). Both are DETERMINISTIC pure functions of `now`. ---
    {
        DiskDrive accurate_drive;
        DiskDrive fast_drive;
        fast_drive.set_fast_disk(true);
        const std::uint64_t accurate_wait = accurate_drive.cycles_until_sector_id(8u, 0);
        const std::uint64_t fast_wait = fast_drive.cycles_until_sector_id(8u, 0);
        expect(accurate_wait > 500'000, "RotLatency_Accurate_LargeWait");
        expect(fast_wait == 0, "RotLatency_Fast_CollapsedToZero");
        // Fast is the same (0) at every rotational phase -- deterministic + phase-
        // independent (no rotational component left).
        expect(fast_drive.cycles_until_sector_id(8u, 400'000) == 0,
               "RotLatency_Fast_ZeroAtEveryPhase");
    }

    // --- (7) Machine-level set_fast_disk() propagates to BOTH owned devices, and
    //     the machine accessor reflects it. Toggling back to false restores the
    //     accurate default on both. ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        expect(!machine.fast_disk(), "Machine_DefaultOff");
        expect(!machine.fdc().fast_disk() && !machine.disk_drive().fast_disk(),
               "Machine_DefaultOff_BothDevices");
        machine.set_fast_disk(true);
        expect(machine.fast_disk(), "Machine_SetTrue_Reflected");
        expect(machine.fdc().fast_disk() && machine.disk_drive().fast_disk(),
               "Machine_SetTrue_PropagatedToBothDevices");
        machine.set_fast_disk(false);
        expect(!machine.fast_disk(), "Machine_SetFalse_Reflected");
        expect(!machine.fdc().fast_disk() && !machine.disk_drive().fast_disk(),
               "Machine_SetFalse_PropagatedToBothDevices");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "wd2793_fastdisk_unit_test: all cases passed\n";
    return 0;
}
