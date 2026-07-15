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

#include "diskutils/msx_disk_format.h"

namespace sony_msx::diskutils {

std::vector<std::uint8_t> build_blank_image() {
    // Zero-initialized image: an empty formatted filesystem. Everything not
    // explicitly set below stays 0x00 (boot code region, root directory, data
    // area). Mirrors tools/format-blank-disk.ps1:90-133 byte-for-byte.
    std::vector<std::uint8_t> image(DiskFormat::kImageBytes, 0x00);

    // --- Boot sector + BPB at LBA 0 (standard MSX 720 KB FAT12) ---
    // Multi-byte fields are little-endian. Values verified identical in
    // tools/format-blank-disk.ps1:94-122 AND src/devices/fdc/disk_image.cpp:43-73.
    image[0] = 0xEB;  // JMP short
    image[1] = 0xFE;
    image[2] = 0x90;  // NOP

    // OEM name "SONYMSX " (the M41-A/B-validated prior-art value,
    // format-blank-disk.ps1:98). The BIOS/Disk ROM reads BPB geometry to mount,
    // not this string, so it does not affect mountability (planner §1.3 A3).
    const char oem[8] = {'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' '};
    for (int i = 0; i < 8; ++i) {
        image[3 + i] = static_cast<std::uint8_t>(oem[i]);
    }

    // BPB (offset 11):
    image[11] = 0x00;  // bytes/sector = 512
    image[12] = 0x02;
    image[13] = 0x02;  // sectors/cluster = 2
    image[14] = 0x01;  // reserved sectors = 1
    image[15] = 0x00;
    image[16] = 0x02;  // number of FATs = 2
    image[17] = 0x70;  // root dir entries = 112
    image[18] = 0x00;
    image[19] = 0xA0;  // total sectors = 1440
    image[20] = 0x05;
    image[21] = DiskFormat::kMediaDescriptor;  // media descriptor 0xF9
    image[22] = 0x03;                          // sectors/FAT = 3
    image[23] = 0x00;
    image[24] = 0x09;  // sectors/track = 9
    image[25] = 0x00;
    image[26] = 0x02;  // heads (sides) = 2
    image[27] = 0x00;
    image[28] = 0x00;  // hidden sectors = 0
    image[29] = 0x00;

    // Boot signature.
    image[510] = 0x55;
    image[511] = 0xAA;

    // --- FAT copies at LBA 1 and LBA 4 (1 reserved + 3 sectors/FAT) ---
    // Only the FIRST three bytes of EACH FAT copy are set: the media byte plus
    // the two reserved FAT12 entries (cluster 0/1). The rest of each FAT and the
    // whole root directory + data area stay zero (an empty medium).
    for (const std::uint32_t fat_lba : {1u, 4u}) {
        const std::uint32_t base = fat_lba * DiskFormat::kSectorSize;
        image[base + 0] = DiskFormat::kMediaDescriptor;
        image[base + 1] = 0xFF;
        image[base + 2] = 0xFF;
    }

    // Root directory (LBA 7..13) + data area (LBA 14..1439): already zero.
    return image;
}

}  // namespace sony_msx::diskutils
