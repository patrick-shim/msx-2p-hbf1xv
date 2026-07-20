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

// Suite: Devices_Fdc_Wd2793Type3_Unit
//
// WD2793 core Type III (Read Address, Write Track) + Type IV (Force Interrupt)
// interrupting an in-flight Type II transfer.
//
// Grounding (read only, never copied - GPL isolation): openMSX 21.0:
// src/fdc/WD2793.cc startType3Cmd (:820), readAddressCmd, startWriteTrack/
// writeTrackData; startType4Cmd/endCmd (:1035-1073); fact-sheet "FDC for Sony
// HB-F1XV.md" §3 (Type III/IV tables), §5 ("How the WD2793 constructs this" -
// Write Track marker parsing: 0xF5->A1 sync, 0xFE IDAM, 0xFB/0xF8 DAM, 0xF7 CRC).

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

constexpr int kStandardTrackLength = 6250;

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

    void wait_for_drq() {
        int guard = 0;
        while (!fdc.drq() && guard < 8'000'000) {
            ++clock.cycles;
            ++guard;
        }
    }
};

}  // namespace

int main() {
    // --- Read Address: 6 DRQ bytes = Track,Side,Sector,N,CRC1,CRC2; SR<-track. ---
    {
        Fixture f;
        f.drive.set_physical_track(5);
        f.clock.cycles = 0;
        f.fdc.write_command(0xC0);  // Read Address

        std::vector<std::uint8_t> id;
        for (int i = 0; i < 6; ++i) {
            f.wait_for_drq();
            id.push_back(f.fdc.read_data());
        }
        expect(id[0] == 5, "ReadAddress_Byte0_Track");
        expect(id[1] == 0, "ReadAddress_Byte1_Side");
        expect(id[2] == 1, "ReadAddress_Byte2_Sector");
        expect(id[3] == 0x02, "ReadAddress_Byte3_LengthCode2For512");
        expect(f.fdc.sector_register() == 5, "ReadAddress_SectorRegister_CopiedFromTrack");
        expect(f.fdc.intrq(), "ReadAddress_Intrq_SetAfterCompletion");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "ReadAddress_Busy_ClearedAfterCompletion");
    }

    // --- Write Track: reformat a standard 9-sector track (only sector 1's content
    //     is meaningfully exercised here); Read Sector then reads it back. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_command(0xF0);  // Write Track (format)

        std::vector<std::uint8_t> stream;
        stream.reserve(kStandardTrackLength);
        auto put = [&](std::uint8_t v, int n) {
            for (int i = 0; i < n; ++i) stream.push_back(v);
        };
        put(0x4E, 100);                       // Gap4a/Gap1 filler
        put(0xF5, 1);
        stream.push_back(0xFE);               // IDAM
        stream.push_back(0);                  // C = track 0
        stream.push_back(0);                  // H = side 0
        stream.push_back(1);                  // R = sector 1
        stream.push_back(0x02);               // N = 512-byte code
        put(0x4E, 20);                         // Gap2 filler
        put(0xF5, 1);
        stream.push_back(0xFB);               // DAM
        // Deterministic fill that AVOIDS the WD2793 Write-Track reserved escape
        // byte values 0xF5/0xF6/0xF7 (openMSX WD2793.cc:972-992 treats these
        // specially UNCONDITIONALLY wherever they appear in the streamed bytes,
        // matching real hardware -- real format software never embeds them as
        // literal data-field content). `% 0xF5` keeps every byte in [0x00,0xF4],
        // strictly below the first reserved code, so no data byte can be
        // misinterpreted as a sync/CRC escape.
        std::vector<std::uint8_t> pattern(DiskImage::kSectorSize);
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            pattern[j] = static_cast<std::uint8_t>((j * 3 + 7) % 0xF5u);
        }
        for (const std::uint8_t b : pattern) stream.push_back(b);
        stream.push_back(0xF7);               // CRC placeholder -> commits the sector
        // Pad with Gap4b filler to the exact standard track length.
        while (static_cast<int>(stream.size()) < kStandardTrackLength) {
            stream.push_back(0x4E);
        }
        expect(static_cast<int>(stream.size()) == kStandardTrackLength,
               "WriteTrack_StreamLength_MatchesStandardTrack");

        for (const std::uint8_t b : stream) {
            f.wait_for_drq();
            f.fdc.write_data(b);
        }
        expect(f.fdc.intrq(), "WriteTrack_Intrq_SetAfterCompletion");
        const std::uint8_t status = f.fdc.peek_status();
        expect((status & 0x01) == 0, "WriteTrack_Busy_ClearedAfterCompletion");

        std::uint8_t readback[DiskImage::kSectorSize] = {};
        expect(f.image.read_chs(0, 0, 1, readback), "WriteTrack_FormattedSector_ReadableViaImage");
        bool identical = true;
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            if (readback[j] != pattern[j]) {
                identical = false;
                break;
            }
        }
        expect(identical, "WriteTrack_FormattedSector_ContentMatchesStream");

        // Read Sector (Type II) over the same sector now reads back the formatted
        // content through the WD2793 core end-to-end.
        f.fdc.write_track(0);
        f.fdc.write_sector(1);
        f.fdc.write_command(0x80);
        std::vector<std::uint8_t> via_read_sector;
        via_read_sector.reserve(DiskImage::kSectorSize);
        for (std::uint32_t i = 0; i < DiskImage::kSectorSize; ++i) {
            f.wait_for_drq();
            via_read_sector.push_back(f.fdc.read_data());
        }
        bool via_core_identical = true;
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            if (via_read_sector[j] != pattern[j]) {
                via_core_identical = false;
                break;
            }
        }
        expect(via_core_identical, "WriteTrack_ThenReadSector_MatchesFormattedPattern");
    }

    // --- Force Interrupt mid-transfer (Type II Read Sector): terminates the
    //     command, clears Busy immediately, reverts STR to Type-I semantics. ---
    {
        Fixture f;
        f.clock.cycles = 0;
        f.fdc.write_track(0);
        f.fdc.write_sector(1);
        f.fdc.write_command(0x80);  // Read Sector
        // Consume a few bytes so the command is genuinely mid-flight.
        for (int i = 0; i < 10; ++i) {
            f.wait_for_drq();
            (void)f.fdc.read_data();
        }
        expect((f.fdc.peek_status() & 0x01) != 0, "PreFI_MidRead_BusyStillSet");

        f.fdc.write_command(0xD0);  // Force Interrupt, flags=0
        const std::uint8_t status = f.fdc.read_status();
        expect((status & 0x01) == 0, "ForceInterrupt_MidRead_ClearsBusyImmediately");
        // Type-I status layout now active (command_reg hi nibble == 0xD0): Track00
        // reflects the drive (still at track 0 -> bit set), rather than DRQ/Lost-
        // Data/Record-Not-Found bits from the interrupted Type II command.
        expect((status & 0x04) != 0, "ForceInterrupt_MidRead_Track00BitReflectsDrive");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
