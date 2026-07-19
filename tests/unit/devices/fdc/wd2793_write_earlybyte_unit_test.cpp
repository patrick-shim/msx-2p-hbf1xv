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

// Suite: Devices_Fdc_Wd2793WriteEarlyByte_Unit  (M47 / DEF-M47-DISKWRITE, DEC-0072)
//
// ANTI-REGRESSION for the residual WD2793 disk-WRITE corruption that survived
// M45/M45b and destroyed a real RPG-title save (DEC-0072). ROOT CAUSE: the M45
// model committed a Write Sector byte ONLY when transfer_drq(t) was true and
// SILENTLY DROPPED the CPU byte otherwise (data_reg_ updated, but buffer_/
// data_index_/data_available_ NOT advanced). A real WD2793 ALWAYS lays down
// EXACTLY 512 in-order bytes -- an un-serviced slot becomes 0x00 + LOST_DATA
// while the position ALWAYS advances; it NEVER drops (fact-sheet
// "FDC for Sony HB-F1XV.md" §8; openMSX WD2793.cc:742-782; fMSX
// WD1793.c:344-370 -- read only, never copied). Our drop-without-advance shifted
// the fully-committed sector under any non-strictly-polled cadence (an early
// write / a 2-bytes-per-DRQ burst / a fixed cadence running AHEAD of our
// rotational first-DRQ), producing the sporadic coherent-shifted save data.
//
// The M45 wd2793_write_stall suite only drove LATE / stalling cadences (which
// M45 already handled by committing on transfer_drq + re-basing), so the DROP of
// an EARLY / BURST byte was never exercised. This suite drives EARLY and BURST
// cadences (the CPU writes AHEAD of our DRQ deadline), which the M45 else-drop
// corrupts, and asserts EXACTLY-512-in-order byte-perfect output post-fix.
//
// The fix (Wd2793::write_data -> commit_write_sector_byte) LATCHES the CPU byte
// then COMMITS it in-order at the current position ALWAYS -- the byte-POSITION
// is decoupled from the CPU-write TIMING (early OR late), never dropped. The
// un-serviced-slot 0x00 substitution lives in sync() (mid-transfer abandoned
// write) + the first-byte CHECK_WRITE gate. H4: the target CHS is LATCHED at
// begin_write_sector so a mid-transfer side/track change cannot redirect the
// committed sector.
//
// NON-TAUTOLOGY (captured in the M47 implementation report): restoring the M45
// else-drop (`if (phase_ == WriteSector && transfer_drq(t)) { commit } // else
// DROP`) makes Cases A, B and C FAIL (dropped/shifted bytes), and restoring the
// pre-H4 live-CHS commit (`drive_->write_sector(...)` in finish_write_sector)
// makes Case E FAIL (the sector lands on side 1). It passes against the fix.

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
using sony_msx::devices::fdc::FdcSectorWriteObserver;
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

// GUARANTEED-NON-ZERO payload byte keyed on (LBA, offset) so every physical
// sector on the disk carries a UNIQUE stream: a substituted 0x00, a
// dropped/shifted byte, or a byte that leaked onto the wrong CHS is directly
// visible as a mismatch and cannot alias another sector's content.
std::uint8_t payload(std::uint32_t lba, int offset) {
    std::uint32_t x = (lba * 2654435761u) + static_cast<std::uint32_t>(offset) * 40503u + 0x9E3779B9u;
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

    // Poll (bounded, 1 cycle/iteration) until DRQ asserts -- the disk-BIOS polled
    // service loop.
    void wait_for_drq() {
        int guard = 0;
        while (!fdc.drq() && guard < 40'000'000) {
            ++clock.cycles;
            ++guard;
        }
    }
};

const char* mode(bool fast) { return fast ? "fast" : "accurate"; }

// Compare `sector` at (track, side) against payload(lba, .).
bool sector_byte_exact(DiskImage& image, std::uint8_t track, std::uint8_t side,
                       std::uint8_t sector, int* first_mismatch = nullptr) {
    const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);
    std::uint8_t got[DiskImage::kSectorSize] = {};
    image.read_chs(track, side, sector, got);
    for (int off = 0; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
        if (got[off] != payload(lba, off)) {
            if (first_mismatch != nullptr) *first_mismatch = off;
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    // --- (A) BURST cadence: after the rotational first DRQ, the CPU streams all
    //     512 bytes AHEAD of the per-byte DRQ deadline (inter-byte < cycles_per_
    //     byte). The M45 else-drop discards every byte that lands before its DRQ
    //     window -> a densely-corrupted sector; the fix commits each byte
    //     in-order regardless of timing -> BYTE-PERFECT, in BOTH --fast-disk
    //     modes. (Adversarial-revert of the else-drop FAILS this case.) ---
    for (bool fast : {false, true}) {
        Fixture f(fast);
        const std::uint8_t track = 0;
        const std::uint8_t side = 1;
        const std::uint8_t sector = 3;
        const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);
        f.drive.set_side(side);
        f.drive.set_physical_track(track);
        f.clock.cycles = 1000;
        f.fdc.write_track(track);
        f.fdc.write_sector(sector);
        f.fdc.write_command(0xA0);  // Write Sector, single
        // Reach the rotational first-DRQ, then burst ahead of the DRQ cadence.
        const std::uint64_t inter_byte = fast ? 3u : 40u;  // < cycles_per_byte (8 / 114)
        f.wait_for_drq();
        for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
            f.fdc.write_data(payload(lba, i));
            f.clock.cycles += inter_byte;
        }
        f.clock.cycles += 2'000'000;  // settle
        int fm = -1;
        const bool ok = sector_byte_exact(f.image, track, side, sector, &fm);
        expect(ok, "Burst_ByteExact");
        expect((f.fdc.peek_status() & Wd2793::kBusy) == 0, "Burst_BusyCleared");
        expect(f.fdc.intrq(), "Burst_Intrq");
        if (!ok) std::cerr << "  [" << mode(fast) << "] first_mismatch=" << fm << "\n";
    }

    // --- (B) SINGLE INJECTED EARLY write at byte 100: a well-behaved poll loop
    //     that writes byte 100 WITHOUT waiting for its DRQ (one byte lands early).
    //     The M45 else-drop discards byte 100 and every later byte shifts down one
    //     (truncated tail); the fix commits byte 100 in place -> position
    //     preserved, BYTE-PERFECT, in BOTH modes. ---
    for (bool fast : {false, true}) {
        Fixture f(fast);
        const std::uint8_t track = 0;
        const std::uint8_t side = 1;
        const std::uint8_t sector = 5;
        const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);
        const int early_at = 100;
        f.drive.set_side(side);
        f.drive.set_physical_track(track);
        f.clock.cycles = 1000;
        f.fdc.write_track(track);
        f.fdc.write_sector(sector);
        f.fdc.write_command(0xA0);
        for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
            if (i != early_at) {
                f.wait_for_drq();  // byte early_at: write immediately (ahead of DRQ)
            }
            f.fdc.write_data(payload(lba, i));
        }
        f.clock.cycles += 2'000'000;
        int fm = -1;
        const bool ok = sector_byte_exact(f.image, track, side, sector, &fm);
        expect(ok, "EarlyInject_ByteExact");
        expect((f.fdc.peek_status() & Wd2793::kLostData) == 0, "EarlyInject_NoLostData");
        expect(f.fdc.intrq(), "EarlyInject_Intrq");
        if (!ok) std::cerr << "  [" << mode(fast) << "] first_mismatch=" << fm << "\n";
    }

    // --- (C) MULTI-SECTOR (0xB0) at the RPG-title save geometry: track 0, side 1,
    //     sectors 7 & 8 == LBA 15 & 16 (the two adjacent corrupt save-slot
    //     records of DEC-0072). Bursting straight through the sector boundary
    //     under the adversarial early cadence, EACH sector lands byte-exact on
    //     ITS OWN LBA with no cross-sector shift, in BOTH modes. ---
    for (bool fast : {false, true}) {
        Fixture f(fast);
        const std::uint8_t track = 0;
        const std::uint8_t side = 1;
        const std::uint8_t sec1 = 7;
        const int nsec = 2;  // sectors 7,8 -> LBA 15,16
        f.drive.set_side(side);
        f.drive.set_physical_track(track);
        f.clock.cycles = 1000;
        f.fdc.write_track(track);
        f.fdc.write_sector(sec1);
        f.fdc.write_command(0xB0);  // Write Sector, MULTI
        const std::uint64_t inter_byte = fast ? 3u : 40u;
        f.clock.cycles = f.fdc.drq_deadline();  // sector 7 first DRQ
        for (int i = 0; i < nsec * static_cast<int>(DiskImage::kSectorSize); ++i) {
            const std::uint8_t sector =
                static_cast<std::uint8_t>(sec1 + i / static_cast<int>(DiskImage::kSectorSize));
            const int off = i % static_cast<int>(DiskImage::kSectorSize);
            f.fdc.write_data(payload(DiskImage::chs_to_lba(track, side, sector), off));
            f.clock.cycles += inter_byte;
        }
        f.clock.cycles += 2'000'000;  // settle (trailing auto-advanced sector 9 aborts via gate)
        expect(DiskImage::chs_to_lba(track, side, 7) == 15, "Multi_Sec7_IsLba15");
        expect(DiskImage::chs_to_lba(track, side, 8) == 16, "Multi_Sec8_IsLba16");
        int fm7 = -1;
        int fm8 = -1;
        const bool ok7 = sector_byte_exact(f.image, track, side, 7, &fm7);
        const bool ok8 = sector_byte_exact(f.image, track, side, 8, &fm8);
        expect(ok7, "Multi_Sec7_Lba15_ByteExact");
        expect(ok8, "Multi_Sec8_Lba16_ByteExact");
        if (!ok7 || !ok8) {
            std::cerr << "  [" << mode(fast) << "] fm7=" << fm7 << " fm8=" << fm8 << "\n";
        }
    }

    // --- (D) ALL-CHS SWEEP: every (track 0..79, side 0..1, sector 1..9) written
    //     under the adversarial early/burst cadence and read back BYTE-PERFECT
    //     (DEC-0072 "byte-perfect across ALL CHS"). One shared image + drive so a
    //     leak onto the wrong CHS surfaces as a later mismatch. Fast-disk mode
    //     keeps the 1440-sector sweep quick (rotational latency collapses); the
    //     write model is identical in both modes (Cases A-C prove accurate). ---
    {
        Fixture f(true);  // fast-disk: collapse rotational latency for the 1440-sector sweep
        f.clock.cycles = 1000;
        for (std::uint8_t track = 0; track < DiskImage::kTracks; ++track) {
            for (std::uint8_t side = 0; side < DiskImage::kSides; ++side) {
                for (std::uint8_t sector = 1; sector <= DiskImage::kSectorsPerTrack; ++sector) {
                    const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);
                    f.drive.set_side(side);
                    f.drive.set_physical_track(track);
                    f.fdc.write_track(track);
                    f.fdc.write_sector(sector);
                    f.fdc.write_command(0xA0);
                    f.clock.cycles = f.fdc.drq_deadline();  // jump to first DRQ (no per-cycle spin)
                    for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
                        f.fdc.write_data(payload(lba, i));
                        f.clock.cycles += 3;  // burst: inter-byte < fast cycles_per_byte (8)
                    }
                    f.clock.cycles += 1000;  // brief inter-command settle
                    (void)f.fdc.intrq();
                }
            }
        }
        // Read back EVERY sector and confirm byte-perfect + no cross-CHS leakage.
        int bad = 0;
        std::uint32_t first_bad_lba = 0xFFFFFFFFu;
        for (std::uint8_t track = 0; track < DiskImage::kTracks; ++track) {
            for (std::uint8_t side = 0; side < DiskImage::kSides; ++side) {
                for (std::uint8_t sector = 1; sector <= DiskImage::kSectorsPerTrack; ++sector) {
                    if (!sector_byte_exact(f.image, track, side, sector)) {
                        ++bad;
                        if (first_bad_lba == 0xFFFFFFFFu) {
                            first_bad_lba = DiskImage::chs_to_lba(track, side, sector);
                        }
                    }
                }
            }
        }
        expect(bad == 0, "AllChsSweep_EverySectorByteExact");
        if (bad != 0) {
            std::cerr << "  AllChsSweep: " << bad << " sector(s) wrong, first_bad_lba="
                      << first_bad_lba << "\n";
        }
    }

    // --- (E) H4 CHS LATCH: a WriteSector begun on SIDE 0 is committed to side 0
    //     even when a mid-transfer glue-register side latch flips the drive to
    //     side 1 (the openMSX/hardware rule: the head can't move / the CHS is
    //     fixed while BUSY). All 3 DEC-0072 corrupt sectors were side 1, redirected
    //     by exactly such a mid-write side change. Pre-H4 (live-CHS commit) this
    //     lands on side 1; post-fix it lands on side 0 and side 1 is untouched. ---
    for (bool fast : {false, true}) {
        Fixture f(fast);
        const std::uint8_t track = 0;
        const std::uint8_t sector = 4;
        const std::uint32_t lba0 = DiskImage::chs_to_lba(track, 0, sector);
        // Capture side 1's pre-write content to prove it is NOT touched.
        std::uint8_t side1_before[DiskImage::kSectorSize] = {};
        f.image.read_chs(track, 1, sector, side1_before);

        f.drive.set_side(0);
        f.drive.set_physical_track(track);
        f.clock.cycles = 1000;
        f.fdc.write_track(track);
        f.fdc.write_sector(sector);
        f.fdc.write_command(0xA0);  // Write Sector begun on side 0 (latched)
        for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
            f.wait_for_drq();
            if (i == 200) {
                f.drive.set_side(1);  // mid-transfer 0x7FFC side latch -> side 1
            }
            f.fdc.write_data(payload(lba0, i));
        }
        f.clock.cycles += 2'000'000;
        // Committed to the LATCHED side 0.
        int fm = -1;
        const bool ok0 = sector_byte_exact(f.image, track, 0, sector, &fm);
        expect(ok0, "H4_CommittedToLatchedSide0");
        // Side 1 same sector is UNCHANGED (the mid-write side flip did NOT redirect it).
        std::uint8_t side1_after[DiskImage::kSectorSize] = {};
        f.image.read_chs(track, 1, sector, side1_after);
        bool side1_unchanged = true;
        for (int off = 0; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
            if (side1_after[off] != side1_before[off]) side1_unchanged = false;
        }
        expect(side1_unchanged, "H4_Side1_NotRedirected");
        if (!ok0) std::cerr << "  [" << mode(fast) << "] H4 first_mismatch=" << fm << "\n";
    }

    // --- (F) MID-TRANSFER ABANDONED write (exercises the sync() un-serviced-slot
    //     0x00 substitution + advance). The CPU writes 200 real bytes then STOPS;
    //     after a full disk revolution the WD2793 fills the remaining slots with
    //     0x00 + LOST_DATA (never dropping/shifting the 200 real bytes, never
    //     hanging BUSY) and completes the sector. This is DISTINCT from the
    //     first-byte gate (which aborts an ABSENT first byte with no write). ---
    {
        Fixture f(false);
        const std::uint8_t track = 0;
        const std::uint8_t side = 0;
        const std::uint8_t sector = 2;
        const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);
        f.drive.set_side(side);
        f.drive.set_physical_track(track);
        f.clock.cycles = 1000;
        f.fdc.write_track(track);
        f.fdc.write_sector(sector);
        f.fdc.write_command(0xA0);
        const int supplied = 200;
        for (int i = 0; i < supplied; ++i) {
            f.wait_for_drq();
            f.fdc.write_data(payload(lba, i));
        }
        // CPU abandons the write. Advance well past a full revolution and sync.
        f.clock.cycles += 4'000'000;
        (void)f.fdc.intrq();  // force sync -> fill the tail with 0x00 + LOST_DATA
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & Wd2793::kBusy) == 0, "Abandon_BusyCleared_NoHang");
        expect((status & Wd2793::kLostData) != 0, "Abandon_LostDataSet");
        expect(f.fdc.intrq(), "Abandon_Intrq");
        std::uint8_t got[DiskImage::kSectorSize] = {};
        f.image.read_chs(track, side, sector, got);
        bool head_real = true;
        for (int off = 0; off < supplied; ++off) {
            if (got[off] != payload(lba, off)) head_real = false;  // 200 real bytes NOT shifted
        }
        bool tail_zero = true;
        for (int off = supplied; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
            if (got[off] != 0x00) tail_zero = false;  // un-serviced slots -> 0x00
        }
        expect(head_real, "Abandon_HeadBytesRealAndInPosition");
        expect(tail_zero, "Abandon_TailZeroSubstituted");
    }

    // --- (G) WRITE-TRACE OBSERVER: non-perturbing per-byte + finish trace. It
    //     observes 512 COMMITTED (never substituted) bytes for a normal write, a
    //     single finish at the correct LBA, and installing it does NOT change the
    //     written image (byte-identical vs the same write with no observer). ---
    {
        struct TraceObserver final : FdcSectorWriteObserver {
            int committed = 0;
            int substituted = 0;
            int finishes = 0;
            std::uint32_t finish_lba = 0xFFFFFFFFu;
            std::uint16_t finish_crc = 0;
            std::vector<std::uint8_t> seen;
            void on_write_byte(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t,
                               std::uint32_t, int, std::uint8_t value, bool subst,
                               std::uint64_t) override {
                if (subst) {
                    ++substituted;
                } else {
                    ++committed;
                }
                seen.push_back(value);
            }
            void on_sector_write(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t,
                                 std::uint32_t lba, const std::uint8_t*, std::size_t,
                                 std::uint16_t crc) override {
                ++finishes;
                finish_lba = lba;
                finish_crc = crc;
            }
        };

        const std::uint8_t track = 0;
        const std::uint8_t side = 1;
        const std::uint8_t sector = 6;
        const std::uint32_t lba = DiskImage::chs_to_lba(track, side, sector);

        auto run_normal_write = [&](FdcSectorWriteObserver* obs,
                                    std::vector<std::uint8_t>& out_image) {
            Fixture f(false);
            f.fdc.set_sector_write_observer(obs);
            f.drive.set_side(side);
            f.drive.set_physical_track(track);
            f.clock.cycles = 1000;
            f.fdc.write_track(track);
            f.fdc.write_sector(sector);
            f.fdc.write_command(0xA0);
            for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
                f.wait_for_drq();
                f.fdc.write_data(payload(lba, i));
            }
            f.clock.cycles += 2'000'000;
            (void)f.fdc.intrq();
            out_image.assign(DiskImage::kSectorSize, 0);
            f.image.read_chs(track, side, sector, out_image.data());
        };

        TraceObserver obs;
        std::vector<std::uint8_t> with_obs;
        std::vector<std::uint8_t> without_obs;
        run_normal_write(&obs, with_obs);
        run_normal_write(nullptr, without_obs);

        expect(obs.committed == static_cast<int>(DiskImage::kSectorSize),
               "Observer_All512Committed");
        expect(obs.substituted == 0, "Observer_NoneSubstituted");
        expect(obs.finishes == 1, "Observer_OneFinish");
        expect(obs.finish_lba == lba, "Observer_FinishLbaCorrect");
        bool trace_matches_payload = obs.seen.size() == DiskImage::kSectorSize;
        for (std::size_t i = 0; i < obs.seen.size() && trace_matches_payload; ++i) {
            if (obs.seen[i] != payload(lba, static_cast<int>(i))) trace_matches_payload = false;
        }
        expect(trace_matches_payload, "Observer_TracedBytesMatchPayload");
        expect(with_obs == without_obs, "Observer_NonPerturbing_ImageIdentical");
        bool image_exact = with_obs.size() == DiskImage::kSectorSize;
        for (std::size_t i = 0; i < with_obs.size() && image_exact; ++i) {
            if (with_obs[i] != payload(lba, static_cast<int>(i))) image_exact = false;
        }
        expect(image_exact, "Observer_ImageByteExact");
    }

    // --- (H) DETERMINISM: two identical burst runs write byte-identical images
    //     (pure function of the cycle model, no wall clock). ---
    {
        auto burst_once = []() {
            Fixture f(false);
            const std::uint32_t lba = DiskImage::chs_to_lba(0, 1, 3);
            f.drive.set_side(1);
            f.drive.set_physical_track(0);
            f.clock.cycles = 1000;
            f.fdc.write_track(0);
            f.fdc.write_sector(3);
            f.fdc.write_command(0xA0);
            f.clock.cycles = f.fdc.drq_deadline();
            for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
                f.fdc.write_data(payload(lba, i));
                f.clock.cycles += 40;
            }
            f.clock.cycles += 2'000'000;
            (void)f.fdc.intrq();
            std::vector<std::uint8_t> out(DiskImage::kSectorSize);
            f.image.read_chs(0, 1, 3, out.data());
            return out;
        };
        expect(burst_once() == burst_once(), "Burst_Deterministic_TwoRunsByteIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "wd2793_write_earlybyte_unit_test: all cases passed\n";
    return 0;
}
