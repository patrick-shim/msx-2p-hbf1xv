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

// Suite: Diskutils_BlankLayout_Unit
//
// Byte oracle over build_blank_image(): size, JMP/OEM, every BPB field
// (0x0B..0x1D), boot signature, the all-zero boot-code region, the two
// F9-FF-FF-seeded FAT copies + their zero remainder, the empty root directory,
// and the zero data area. Determinism: two independent calls are byte-identical.
// Links ONLY msx_diskutil -- NO emulator header, NO sony_msx_core (isolation).

#include <cstdint>
#include <iostream>
#include <vector>

#include "utils/msx_disk_format.h"

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
    using sony_msx::utils::build_blank_image;
    using sony_msx::utils::DiskFormat;

    const std::vector<std::uint8_t> img = build_blank_image();

    // --- Size + JMP + OEM. ---
    expect(img.size() == 737280, "Size_737280");
    expect(DiskFormat::kImageBytes == 737280, "Constant_ImageBytes_737280");
    expect(img[0] == 0xEB && img[1] == 0xFE && img[2] == 0x90, "Jmp_EBFE90");
    const char oem[8] = {'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' '};
    bool oem_ok = true;
    for (int i = 0; i < 8; ++i) {
        if (img[3 + i] != static_cast<std::uint8_t>(oem[i])) {
            oem_ok = false;
        }
    }
    expect(oem_ok, "Oem_SONYMSX");

    // --- Every BPB field (offset 0x0B..0x1D). ---
    struct Field {
        std::size_t offset;
        std::uint8_t value;
        const char* name;
    };
    const Field bpb[] = {
        {0x0B, 0x00, "BytesPerSectorLo"}, {0x0C, 0x02, "BytesPerSectorHi_512"},
        {0x0D, 0x02, "SectorsPerCluster_2"},
        {0x0E, 0x01, "ReservedSectorsLo_1"}, {0x0F, 0x00, "ReservedSectorsHi"},
        {0x10, 0x02, "NumberOfFats_2"},
        {0x11, 0x70, "RootDirEntriesLo_112"}, {0x12, 0x00, "RootDirEntriesHi"},
        {0x13, 0xA0, "TotalSectorsLo"}, {0x14, 0x05, "TotalSectorsHi_1440"},
        {0x15, 0xF9, "MediaDescriptor_F9"},
        {0x16, 0x03, "SectorsPerFatLo_3"}, {0x17, 0x00, "SectorsPerFatHi"},
        {0x18, 0x09, "SectorsPerTrackLo_9"}, {0x19, 0x00, "SectorsPerTrackHi"},
        {0x1A, 0x02, "HeadsLo_2"}, {0x1B, 0x00, "HeadsHi"},
        {0x1C, 0x00, "HiddenSectorsLo"}, {0x1D, 0x00, "HiddenSectorsHi"},
    };
    for (const Field& f : bpb) {
        expect(img[f.offset] == f.value, f.name);
    }
    expect(img[0x15] == DiskFormat::kMediaDescriptor, "MediaDescriptor_MatchesConstant");

    // --- Boot signature + all-zero boot-code region (non-bootable). ---
    expect(img[510] == 0x55 && img[511] == 0xAA, "BootSignature_55AA");
    bool boot_code_zero = true;
    for (std::size_t i = 0x1E; i <= 0x1FD; ++i) {
        if (img[i] != 0x00) {
            boot_code_zero = false;
        }
    }
    expect(boot_code_zero, "BootCodeRegion_0x1E_0x1FD_AllZero");

    // --- FAT copy #1 (LBA 1, offset 512) and #2 (LBA 4, offset 2048). ---
    const std::size_t fat_bases[] = {1u * DiskFormat::kSectorSize, 4u * DiskFormat::kSectorSize};
    for (const std::size_t base : fat_bases) {
        expect(img[base + 0] == 0xF9 && img[base + 1] == 0xFF && img[base + 2] == 0xFF,
               "FatSeed_F9FFFF");
        bool rest_zero = true;
        // Each FAT copy is 3 sectors (3 * 512 = 1536 bytes); only bytes 0..2 set.
        for (std::size_t i = 3; i < 3u * DiskFormat::kSectorSize; ++i) {
            if (img[base + i] != 0x00) {
                rest_zero = false;
                break;
            }
        }
        expect(rest_zero, "FatRemainder_AllZero");
    }

    // --- Root directory (LBA 7..13, offsets 3584..7167) all zero. ---
    bool root_zero = true;
    for (std::size_t i = 7u * DiskFormat::kSectorSize; i < 14u * DiskFormat::kSectorSize; ++i) {
        if (img[i] != 0x00) {
            root_zero = false;
            break;
        }
    }
    expect(root_zero, "RootDirectory_Empty_AllZero");

    // --- Data area (LBA 14.. offset 7168..737279) all zero. ---
    bool data_zero = true;
    for (std::size_t i = 14u * DiskFormat::kSectorSize; i < DiskFormat::kImageBytes; ++i) {
        if (img[i] != 0x00) {
            data_zero = false;
            break;
        }
    }
    expect(data_zero, "DataArea_AllZero");

    // --- Determinism: two independent calls are byte-identical. ---
    const std::vector<std::uint8_t> img2 = build_blank_image();
    expect(img == img2, "TwoCalls_ByteIdentical");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
