// Suite: Devices_Fdc_Wd2793Type2_Unit  (M16-S3)
//
// WD2793 core Type II (Read Sector / Write Sector): DRQ cadence, Lost Data (read
// vs write semantics differ), Record Not Found, multi-record track-boundary stop.
//
// Grounding (read only, never copied - GPL isolation): references/openmsx-21.0/
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

constexpr std::uint64_t kReadStartCycles = 2 * 114;
constexpr std::uint64_t kCyclesPerByte = 114;

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
        // Let 5 whole byte-periods elapse past the first DRQ window before the
        // first read_data() call (deliberate overshoot - the point of this case).
        f.clock.cycles += kReadStartCycles + 5 * kCyclesPerByte;
        f.fdc.read_data();
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x04) != 0, "ReadSector_MissedDrq_LostDataSet");
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

    // --- Missed DRQ on write: a 0x00 byte is substituted (command continues,
    //     does not abort), and Lost Data is set. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(3);
        f.fdc.write_command(0xA0);
        // Let 3 whole byte-periods elapse before the first write_data() call
        // (deliberate overshoot).
        f.clock.cycles += kReadStartCycles + 3 * kCyclesPerByte;
        f.fdc.write_data(0xAB);  // first serviced byte (earlier ones are lost)
        const std::uint8_t status_after_first = f.fdc.peek_status();
        expect((status_after_first & 0x04) != 0, "WriteSector_MissedDrq_LostDataSet");
        // Finish the rest of the sector promptly (no further loss) so the command
        // completes and the image write actually lands (proves it "continues").
        for (std::uint32_t i = 1; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            f.fdc.write_data(0x00);
        }
        expect(f.fdc.intrq(), "WriteSector_AfterLostData_StillCompletes");
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
