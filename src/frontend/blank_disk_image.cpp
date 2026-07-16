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

#include "frontend/blank_disk_image.h"

namespace sony_msx::frontend {

namespace {

// The 720 KB (2DD) MSX-DOS FAT12 geometry (re-expressed from the documented
// tools/format-blank-disk.ps1:82-133 spec -- NOT a code dependency on
// src/diskutils, DEC-0080 both-ways build isolation).
constexpr std::uint32_t kSectorSize = 512;
constexpr std::uint32_t kSectorsPerTrack = 9;
constexpr std::uint32_t kSides = 2;
constexpr std::uint32_t kTracks = 80;
constexpr std::uint32_t kTotalSectors = kSectorsPerTrack * kSides * kTracks;  // 1440
constexpr std::uint32_t kImageBytes = kTotalSectors * kSectorSize;            // 737280
constexpr std::uint8_t kMediaDescriptor = 0xF9;

}  // namespace

std::vector<std::uint8_t> build_blank_disk_image() {
    // Zero-initialized image (empty formatted filesystem); only the BPB + the two
    // FAT seeds are written, so the root directory and data area stay all-zero
    // (empty, DOS-recognizable, deliberately NON-bootable -- no proprietary DOS
    // system files, DEC-0080).
    std::vector<std::uint8_t> image(kImageBytes, 0x00);

    // --- Boot sector + BPB at LBA 0 (standard MSX 720 KB FAT12). ---
    image[0] = 0xEB;  // JMP short
    image[1] = 0xFE;
    image[2] = 0x90;  // NOP
    // OEM name "SONYMSX " (offset 3..10).
    const char oem[8] = {'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' '};
    for (int i = 0; i < 8; ++i) {
        image[3 + i] = static_cast<std::uint8_t>(oem[i]);
    }
    // BPB (offset 0x0B..0x1D).
    image[0x0B] = 0x00;  // bytes/sector = 512
    image[0x0C] = 0x02;
    image[0x0D] = 0x02;  // sectors/cluster = 2
    image[0x0E] = 0x01;  // reserved sectors = 1
    image[0x0F] = 0x00;
    image[0x10] = 0x02;  // number of FATs = 2
    image[0x11] = 0x70;  // root dir entries = 112
    image[0x12] = 0x00;
    image[0x13] = 0xA0;  // total sectors = 1440
    image[0x14] = 0x05;
    image[0x15] = kMediaDescriptor;
    image[0x16] = 0x03;  // sectors/FAT = 3
    image[0x17] = 0x00;
    image[0x18] = 0x09;  // sectors/track = 9
    image[0x19] = 0x00;
    image[0x1A] = 0x02;  // heads = 2
    image[0x1B] = 0x00;
    image[0x1C] = 0x00;  // hidden sectors = 0
    image[0x1D] = 0x00;
    // Boot signature.
    image[510] = 0x55;
    image[511] = 0xAA;
    // The boot-code region 0x1E..0x1FD stays all-zero (non-bootable).

    // --- FAT copies at LBA 1 and LBA 4 (1 reserved + 3 sectors/FAT). ---
    for (const std::uint32_t fat_lba : {1u, 4u}) {
        const std::size_t base = static_cast<std::size_t>(fat_lba) * kSectorSize;
        image[base + 0] = kMediaDescriptor;  // F9
        image[base + 1] = 0xFF;
        image[base + 2] = 0xFF;
        // The rest of each FAT stays zero (empty).
    }

    // Root directory (LBA 7..13) + data area (LBA 14..) are already zero.
    return image;
}

}  // namespace sony_msx::frontend
