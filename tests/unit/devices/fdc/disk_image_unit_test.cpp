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

// Suite: Devices_Fdc_DiskImage_Unit  (M16-S1)
//
// Deterministic 720 KB (2DD) sector-image geometry + access:
//   - size() == 737,280 bytes (80 x 2 x 9 x 512).
//   - LBA<->CHS round-trip at boundary sectors (LBA 0, 8, 9, 17, 1439).
//   - read-after-write determinism.
//   - two independently-synthesized images are byte-identical (A-M16-4).
//   - write-protect blocks writes (read-back unchanged).
//
// Grounding: references/openmsx-21.0/src/fdc/DSKDiskImage.cc / SectorAccessibleDisk.cc
// (behaviour reference, read only, never copied); fact-sheet "FDC for Sony HB-F1XV.md" §5.

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_image.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    using sony_msx::devices::fdc::DiskImage;

    // --- Geometry constants / size. ---
    expect(DiskImage::kImageBytes == 737280, "Geometry_ImageBytes_737280");
    expect(DiskImage::kTotalSectors == 1440, "Geometry_TotalSectors_1440");
    expect(DiskImage::kMediaDescriptor == 0xF9, "Geometry_MediaDescriptor_0xF9");

    {
        DiskImage image;
        expect(image.size() == 737280, "DiskImage_Size_737280");
    }

    // --- LBA<->CHS round-trip at boundary sectors. ---
    {
        const std::uint32_t boundaries[] = {0, 8, 9, 17, 1439};
        for (const std::uint32_t lba : boundaries) {
            std::uint32_t track = 0;
            std::uint32_t side = 0;
            std::uint32_t sector = 0;
            DiskImage::lba_to_chs(lba, track, side, sector);
            const std::uint32_t back = DiskImage::chs_to_lba(track, side, sector);
            expect(back == lba, "LbaChsRoundTrip_Boundary");
            if (back != lba) {
                std::cerr << "  lba=" << lba << " -> C" << track << " S" << side << " R" << sector
                          << " -> lba=" << back << "\n";
            }
        }
        // LBA 0 -> track 0, side 0, sector 1 (first sector on the medium).
        std::uint32_t track = 99, side = 99, sector = 99;
        DiskImage::lba_to_chs(0, track, side, sector);
        expect(track == 0 && side == 0 && sector == 1, "Lba0_IsTrack0Side0Sector1");
        // LBA 9 -> first sector of side 1, track 0 (9 sectors/track, 2 sides interleaved).
        DiskImage::lba_to_chs(9, track, side, sector);
        expect(track == 0 && side == 1 && sector == 1, "Lba9_IsTrack0Side1Sector1");
        // LBA 1439 (last) -> track 79, side 1, sector 9.
        DiskImage::lba_to_chs(1439, track, side, sector);
        expect(track == 79 && side == 1 && sector == 9, "Lba1439_IsLastSector");
    }

    // --- valid_chs bounds. ---
    {
        DiskImage image;
        expect(image.valid_chs(0, 0, 1), "ValidChs_FirstSector");
        expect(image.valid_chs(79, 1, 9), "ValidChs_LastSector");
        expect(!image.valid_chs(80, 0, 1), "ValidChs_TrackOutOfRange");
        expect(!image.valid_chs(0, 2, 1), "ValidChs_SideOutOfRange");
        expect(!image.valid_chs(0, 0, 0), "ValidChs_SectorZeroInvalid");
        expect(!image.valid_chs(0, 0, 10), "ValidChs_SectorTooHigh");
    }

    // --- Read-after-write determinism (by LBA and by CHS). ---
    {
        DiskImage image;
        std::uint8_t pattern[DiskImage::kSectorSize];
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            pattern[j] = static_cast<std::uint8_t>(j ^ 0xA5u);
        }
        expect(image.write_lba(17, pattern), "WriteLba17_Succeeds");
        std::uint8_t readback[DiskImage::kSectorSize] = {};
        expect(image.read_lba(17, readback), "ReadLba17_Succeeds");
        bool identical = true;
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            if (readback[j] != pattern[j]) {
                identical = false;
                break;
            }
        }
        expect(identical, "ReadAfterWrite_ByteIdentical");

        // Same via CHS addressing (LBA 17 -> track 0, side 1, sector 9 per 9/2/80 geometry:
        // lba_to_chs(17): sector=(17%9)+1=9, track_side=17/9=1, side=1%2=1, track=1/2=0).
        std::uint8_t readback_chs[DiskImage::kSectorSize] = {};
        expect(image.read_chs(0, 1, 9, readback_chs), "ReadChs_MatchesLba17");
        bool chs_matches = true;
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            if (readback_chs[j] != pattern[j]) {
                chs_matches = false;
                break;
            }
        }
        expect(chs_matches, "ReadChs_ByteIdenticalToLbaWrite");

        // Out-of-range access fails cleanly.
        expect(!image.read_lba(DiskImage::kTotalSectors, readback), "ReadLba_OutOfRange_Fails");
        expect(!image.write_lba(DiskImage::kTotalSectors, pattern), "WriteLba_OutOfRange_Fails");
    }

    // --- Two independently-synthesized images are byte-identical (A-M16-4). ---
    {
        const std::vector<std::uint8_t> a = DiskImage::synthesize();
        const std::vector<std::uint8_t> b = DiskImage::synthesize();
        expect(a.size() == b.size(), "Synthesize_TwoCalls_SameSize");
        expect(a == b, "Synthesize_TwoCalls_ByteIdentical");

        DiskImage image_a;
        DiskImage image_b;
        expect(image_a.data() == image_b.data(), "DiskImage_TwoDefaultInstances_ByteIdentical");
    }

    // --- Synthesized boot sector: FAT12 BPB signature + media descriptor. ---
    {
        DiskImage image;
        expect(image.byte_at(510) == 0x55 && image.byte_at(511) == 0xAA,
               "Synthesize_BootSignature_55AA");
        expect(image.byte_at(21) == DiskImage::kMediaDescriptor,
               "Synthesize_BpbMediaDescriptor_0xF9");
        expect(image.byte_at(24) == 0x09, "Synthesize_BpbSectorsPerTrack_9");
        expect(image.byte_at(26) == 0x02, "Synthesize_BpbHeads_2");
    }

    // --- byte_at is deterministic and returns 0xFF outside the image. ---
    {
        DiskImage image;
        expect(image.byte_at(DiskImage::kImageBytes) == 0xFF, "ByteAt_OutOfRange_Returns0xFF");
        expect(image.byte_at(0) == 0xEB, "ByteAt_FirstByte_JmpOpcode");
    }

    // --- Write-protect blocks writes; read-back is unchanged. ---
    {
        DiskImage image;
        std::uint8_t before[DiskImage::kSectorSize] = {};
        image.read_lba(3, before);

        image.set_write_protected(true);
        expect(image.write_protected(), "SetWriteProtected_ReadsBack_True");
        std::uint8_t attempted[DiskImage::kSectorSize];
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            attempted[j] = static_cast<std::uint8_t>(0x11);
        }
        expect(!image.write_lba(3, attempted), "WriteLba_WhenProtected_ReturnsFalse");

        std::uint8_t after[DiskImage::kSectorSize] = {};
        image.read_lba(3, after);
        bool unchanged = true;
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            if (after[j] != before[j]) {
                unchanged = false;
                break;
            }
        }
        expect(unchanged, "WriteLba_WhenProtected_MediumUnchanged");

        image.set_write_protected(false);
        expect(image.write_lba(3, attempted), "WriteLba_AfterUnprotect_Succeeds");
    }

    // --- present flag. ---
    {
        DiskImage image;
        expect(image.present(), "DiskImage_DefaultPresent_True");
        image.set_present(false);
        expect(!image.present(), "DiskImage_SetPresent_False_ReadsBack");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
