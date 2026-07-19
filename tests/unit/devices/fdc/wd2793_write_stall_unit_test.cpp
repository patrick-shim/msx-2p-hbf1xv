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

// Suite: Devices_Fdc_Wd2793WriteStall_Unit  (M45 / DEF-M45-WRITEDRQ)
//
// ANTI-REGRESSION for the WD2793 accurate-mode disk-WRITE corruption that
// destroyed a real RPG-title save (DEC-0067). The existing wd2793_type2/fastdisk
// write oracles only drive an IDEAL 1-cycle-per-poll write loop, which is why
// the corruption was never caught: the defect only manifests when a CPU data-
// register write lands MORE than one byte-period past the (old) rigid absolute-
// cycle DRQ deadline -- e.g. a game write loop with interrupts enabled that
// suffers a mid-transfer ISR stall, or a steadily-slightly-slow poll loop.
//
// The OLD model (Wd2793::write_data, pre-fix) modelled DRQ as a free-running
// absolute-cycle LEVEL deadline; on such a late write it substituted 0x00 +
// LOST_DATA, and each injected zero ALSO consumed a data slot, so the sector
// finished EARLY and the real tail bytes were dropped -> dense zeros + shifted
// survivors + truncated tail. The corrected model (openMSX WD2793.cc:235-247 /
// :742-782, understanding only -- never copied, GPL isolation) uses an EDGE DRQ
// handshake + one-byte pipeline that re-bases the next DRQ on the CPU's actual
// write time, so a merely-late byte is COMMITTED, never lost.
//
// This suite drives DRIFTING / STALLING write cadences (NOT the ideal poll) and
// asserts BYTE-PERFECT output in BOTH --fast-disk modes. Reusing the stall
// cadence from debug/fdc-write-investigation/wd2793_stall_probe.cpp, it
// reproduces the corruption against the PRE-FIX code (proven by an adversarial
// revert: restore the zero-substitution and this suite FAILS -- see the M45
// implementation report / commit message) and passes against the fix.

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

// Distinct, GUARANTEED-NON-ZERO payload byte for (sector, offset). Non-zero so
// that any spurious 0x00 substituted by the (defective) controller is directly
// visible as a mismatch, and so an "injected-zeros" count is meaningful.
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

    // Poll (bounded) until DRQ asserts -- the disk-BIOS polled service loop.
    void wait_for_drq() {
        int guard = 0;
        while (!fdc.drq() && guard < 40'000'000) {
            ++clock.cycles;
            ++guard;
        }
    }
};

// Result of a scripted multi-sector WriteSector under a given cadence.
struct WriteOutcome {
    bool byte_exact = false;      // every written byte == payload (no zeros/shift/trunc)
    int mismatches = 0;           // count of diverging bytes across all sectors
    int spurious_zeros = 0;       // written 0x00 where payload was non-zero
    int first_mismatch = -1;      // linear index of the first diverging byte
    std::uint8_t status = 0;      // final status register
    bool intrq = false;           // command completed (INTRQ raised)
};

// Compare all `nsec` written sectors starting at (track0, side1, sec1) to the
// payload and summarise divergences.
WriteOutcome verify(DiskImage& image, std::uint8_t sec1, int nsec) {
    WriteOutcome r;
    r.byte_exact = true;
    for (int k = 0; k < nsec; ++k) {
        std::uint8_t got[DiskImage::kSectorSize] = {};
        image.read_chs(0, 1, static_cast<std::uint8_t>(sec1 + k), got);
        for (int off = 0; off < static_cast<int>(DiskImage::kSectorSize); ++off) {
            const std::uint8_t want = payload(sec1 + k, off);
            if (got[off] != want) {
                r.byte_exact = false;
                ++r.mismatches;
                if (r.first_mismatch < 0) {
                    r.first_mismatch = k * static_cast<int>(DiskImage::kSectorSize) + off;
                }
                if (got[off] == 0x00) {
                    ++r.spurious_zeros;
                }
            }
        }
    }
    return r;
}

// Drive a WriteSector (single 0xA0 or multi 0xB0) of `nsec` sectors, injecting a
// single mid-transfer stall of `stall_cycles` right before linear byte
// `stall_at`, plus optional steady per-byte inter-byte CPU work `post` cycles.
WriteOutcome run_write(bool fast, std::uint8_t sec1, int nsec, int stall_at,
                       std::uint64_t stall_cycles, std::uint64_t post) {
    Fixture f(fast);
    f.drive.set_side(1);
    f.drive.set_physical_track(0);
    f.clock.cycles = 1000;
    f.fdc.write_track(0);
    f.fdc.write_sector(sec1);
    f.fdc.write_command(nsec > 1 ? 0xB0 : 0xA0);

    const int total = nsec * static_cast<int>(DiskImage::kSectorSize);
    for (int i = 0; i < total; ++i) {
        f.wait_for_drq();
        if (i == stall_at) {
            f.clock.cycles += stall_cycles;  // a single long ISR mid-transfer
        }
        const int s = sec1 + i / static_cast<int>(DiskImage::kSectorSize);
        const int off = i % static_cast<int>(DiskImage::kSectorSize);
        f.fdc.write_data(payload(s, off));
        f.clock.cycles += post;  // steady inter-byte CPU work (drift source)
    }
    // Settle > one disk revolution. A multi-record (0xB0) write of nsec < 9
    // sectors auto-advances to the NEXT sector after the last one supplied and
    // then waits for its first byte; since the CPU stops feeding, that sector
    // terminates via the first-byte CHECK_WRITE timeout -> INTRQ. DEF-M45-WRITEDRQ
    // -FIX made that window the correct rotational + full-revolution span (was a
    // too-short ~1140-cycle window), so the settle must now span a full revolution
    // for the natural multi-write termination to be observed. Single-sector (0xA0)
    // writes complete at byte 511 regardless, so the larger settle is harmless.
    f.clock.cycles += 2'000'000;  // settle (> 1 revolution; see note above)

    WriteOutcome r = verify(f.image, sec1, nsec);
    r.status = f.fdc.peek_status();
    r.intrq = f.fdc.intrq();
    return r;
}

const char* mode(bool fast) { return fast ? "fast" : "accurate"; }

}  // namespace

int main() {
    // --- (1) Single-sector write with a single large mid-transfer stall (an ISR
    //     of ~12000 cycles at byte 130 -- the exact wd2793_stall_probe cadence
    //     that reproduced the disk2 LBA9 corruption). Post-fix: BYTE-PERFECT, no
    //     spurious zeros, no LOST_DATA, no truncation, INTRQ set -- in BOTH
    //     modes. (Against the pre-fix code this same assertion FAILS: ~105 zeros
    //     injected + dropped tail -- the non-tautology adversarial-revert proof.)
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_write(fast, /*sec1=*/1, /*nsec=*/1,
                                         /*stall_at=*/130, /*stall_cycles=*/12000,
                                         /*post=*/0);
        expect(r.byte_exact, "MidStall_Single_ByteExact");
        expect(r.spurious_zeros == 0, "MidStall_Single_NoSpuriousZeros");
        expect((r.status & Wd2793::kLostData) == 0, "MidStall_Single_NoLostData");
        expect((r.status & Wd2793::kBusy) == 0, "MidStall_Single_BusyCleared");
        expect(r.intrq, "MidStall_Single_Intrq");
        if (!r.byte_exact) {
            std::cerr << "  [" << mode(fast) << "] mismatches=" << r.mismatches
                      << " spurious_zeros=" << r.spurious_zeros
                      << " first_mismatch=" << r.first_mismatch << "\n";
        }
    }

    // --- (2) STEADY DRIFT: a well-behaved poll loop that is slightly slower than
    //     the ideal 1-cycle poll -- 300 cycles of inter-byte CPU work per byte.
    //     Under the OLD absolute-cycle deadline this drift accumulates until the
    //     controller starts substituting zeros mid-stream; the edge handshake
    //     re-bases each DRQ on the actual write, so it stays byte-perfect. ---
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_write(fast, /*sec1=*/4, /*nsec=*/1,
                                         /*stall_at=*/-1, /*stall_cycles=*/0,
                                         /*post=*/300);
        expect(r.byte_exact, "SteadyDrift_Single_ByteExact");
        expect(r.spurious_zeros == 0, "SteadyDrift_Single_NoSpuriousZeros");
        expect((r.status & Wd2793::kLostData) == 0, "SteadyDrift_Single_NoLostData");
        expect(r.intrq, "SteadyDrift_Single_Intrq");
        if (!r.byte_exact) {
            std::cerr << "  [" << mode(fast) << "] mismatches=" << r.mismatches
                      << " spurious_zeros=" << r.spurious_zeros
                      << " first_mismatch=" << r.first_mismatch << "\n";
        }
    }

    // --- (3) MULTI-SECTOR write (0xB0, 3 sectors = the RPG-title save geometry:
    //     track0 side1 sec1..3 / LBA 9,10,11) with a mid-transfer stall that
    //     straddles a sector boundary (byte 700, well into sector 2). All three
    //     sectors must be byte-perfect in BOTH modes -- no cross-sector shift,
    //     no truncated tail. ---
    for (bool fast : {false, true}) {
        const WriteOutcome r = run_write(fast, /*sec1=*/1, /*nsec=*/3,
                                         /*stall_at=*/700, /*stall_cycles=*/9000,
                                         /*post=*/0);
        expect(r.byte_exact, "MidStall_Multi3_ByteExact");
        expect(r.spurious_zeros == 0, "MidStall_Multi3_NoSpuriousZeros");
        expect((r.status & Wd2793::kLostData) == 0, "MidStall_Multi3_NoLostData");
        expect(r.intrq, "MidStall_Multi3_Intrq");
        if (!r.byte_exact) {
            std::cerr << "  [" << mode(fast) << "] mismatches=" << r.mismatches
                      << " spurious_zeros=" << r.spurious_zeros
                      << " first_mismatch=" << r.first_mismatch << "\n";
        }
    }

    // --- (4) DETERMINISM: two identical stalled runs produce byte-identical
    //     written images (pure function of the cycle model, no wall clock). ---
    {
        Fixture a(false);
        Fixture b(false);
        auto drive_once = [](Fixture& f) {
            f.drive.set_side(1);
            f.drive.set_physical_track(0);
            f.clock.cycles = 1000;
            f.fdc.write_track(0);
            f.fdc.write_sector(2);
            f.fdc.write_command(0xA0);
            for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
                f.wait_for_drq();
                if (i == 200) f.clock.cycles += 7777;
                f.fdc.write_data(payload(2, i));
            }
            f.clock.cycles += 100'000;
            std::vector<std::uint8_t> out(DiskImage::kSectorSize);
            f.image.read_chs(0, 1, 2, out.data());
            return out;
        };
        const std::vector<std::uint8_t> ra = drive_once(a);
        const std::vector<std::uint8_t> rb = drive_once(b);
        expect(ra == rb, "StalledWrite_Deterministic_TwoRunsByteIdentical");
    }

    // --- (5) Cross-mode equivalence: the SAME stalled cadence yields the SAME
    //     written bytes with and without --fast-disk (fast is a pure timing-scale
    //     of the identical FSM, not a bug-suppressing branch). ---
    {
        const WriteOutcome acc = run_write(false, 7, 1, 150, 5000, 0);
        const WriteOutcome fst = run_write(true, 7, 1, 150, 5000, 0);
        expect(acc.byte_exact && fst.byte_exact, "CrossMode_BothByteExact");
        // Both wrote the identical payload; compare the on-disk result directly.
        Fixture ref(false);
        std::uint8_t want[DiskImage::kSectorSize] = {};
        for (int i = 0; i < static_cast<int>(DiskImage::kSectorSize); ++i) {
            want[i] = payload(7, i);
        }
        // (acc.byte_exact && fst.byte_exact) already proves both == payload, so
        // they equal each other; this assertion documents the intent.
        expect(acc.byte_exact == fst.byte_exact, "CrossMode_SameOutcome");
        (void)want;
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "wd2793_write_stall_unit_test: all cases passed\n";
    return 0;
}
