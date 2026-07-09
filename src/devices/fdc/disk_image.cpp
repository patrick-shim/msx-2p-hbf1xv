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

#include "devices/fdc/disk_image.h"

namespace sony_msx::devices::fdc {

DiskImage::DiskImage() : data_(synthesize()) {}

DiskImage::DiskImage(std::vector<std::uint8_t> bytes) : data_(std::move(bytes)) {
    // Always deterministic: normalize to the exact 2DD size (truncate / 0x00-pad).
    data_.resize(kImageBytes, 0x00);
}

std::vector<std::uint8_t> DiskImage::synthesize() {
    std::vector<std::uint8_t> image(kImageBytes, 0x00);

    // Per-sector deterministic fill: a pure function of (lba, offset). This gives
    // every sector distinct, reproducible content so read/write and boundary
    // tests are meaningful and two builds are byte-identical (A-M16-4).
    for (std::uint32_t lba = 0; lba < kTotalSectors; ++lba) {
        const std::uint32_t base = lba * kSectorSize;
        for (std::uint32_t j = 0; j < kSectorSize; ++j) {
            image[base + j] = static_cast<std::uint8_t>((lba * 13u + j * 7u + 0x5Au) & 0xFFu);
        }
    }

    // Standard MSX 720 KB FAT12 boot sector + BPB at LBA 0 (fact-sheet §5).
    // Values are little-endian where multi-byte.
    std::uint8_t* boot = image.data();
    boot[0] = 0xEB;  // JMP short
    boot[1] = 0xFE;
    boot[2] = 0x90;  // NOP
    const char oem[8] = {'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' '};
    for (int i = 0; i < 8; ++i) {
        boot[3 + i] = static_cast<std::uint8_t>(oem[i]);
    }
    // BPB (offset 11):
    boot[11] = 0x00;  // bytes/sector = 512
    boot[12] = 0x02;
    boot[13] = 0x02;  // sectors/cluster = 2
    boot[14] = 0x01;  // reserved sectors = 1
    boot[15] = 0x00;
    boot[16] = 0x02;  // number of FATs = 2
    boot[17] = 0x70;  // root dir entries = 112
    boot[18] = 0x00;
    boot[19] = 0xA0;  // total sectors = 1440
    boot[20] = 0x05;
    boot[21] = kMediaDescriptor;  // media descriptor 0xF9
    boot[22] = 0x03;              // sectors/FAT = 3
    boot[23] = 0x00;
    boot[24] = 0x09;  // sectors/track = 9
    boot[25] = 0x00;
    boot[26] = 0x02;  // heads = 2
    boot[27] = 0x00;
    boot[28] = 0x00;  // hidden sectors = 0
    boot[29] = 0x00;
    // Boot signature.
    image[510] = 0x55;
    image[511] = 0xAA;

    // FAT copies begin at LBA 1 and LBA 4 (1 reserved + 3 sectors/FAT). The first
    // three bytes of each FAT are the media descriptor + 0xFF 0xFF.
    for (std::uint32_t fat_lba : {1u, 4u}) {
        std::uint8_t* fat = image.data() + fat_lba * kSectorSize;
        fat[0] = kMediaDescriptor;
        fat[1] = 0xFF;
        fat[2] = 0xFF;
        for (std::uint32_t j = 3; j < kSectorSize; ++j) {
            fat[j] = 0x00;
        }
    }

    return image;
}

std::uint32_t DiskImage::chs_to_lba(const std::uint32_t track, const std::uint32_t side,
                                    const std::uint32_t sector) {
    // LBA = (cylinder * heads + head) * sectors_per_track + (sector - 1).
    return (track * kSides + side) * kSectorsPerTrack + (sector - 1);
}

void DiskImage::lba_to_chs(const std::uint32_t lba, std::uint32_t& track, std::uint32_t& side,
                           std::uint32_t& sector) {
    sector = (lba % kSectorsPerTrack) + 1;
    const std::uint32_t track_side = lba / kSectorsPerTrack;
    side = track_side % kSides;
    track = track_side / kSides;
}

bool DiskImage::valid_chs(const std::uint32_t track, const std::uint32_t side,
                          const std::uint32_t sector) const {
    return track < kTracks && side < kSides && sector >= 1 && sector <= kSectorsPerTrack;
}

bool DiskImage::read_lba(const std::uint32_t lba, std::uint8_t* out) const {
    if (lba >= kTotalSectors || out == nullptr) {
        return false;
    }
    const std::uint32_t base = lba * kSectorSize;
    for (std::uint32_t j = 0; j < kSectorSize; ++j) {
        out[j] = data_[base + j];
    }
    return true;
}

bool DiskImage::write_lba(const std::uint32_t lba, const std::uint8_t* in) {
    if (lba >= kTotalSectors || in == nullptr || write_protected_) {
        return false;
    }
    const std::uint32_t base = lba * kSectorSize;
    for (std::uint32_t j = 0; j < kSectorSize; ++j) {
        data_[base + j] = in[j];
    }
    return true;
}

bool DiskImage::read_chs(const std::uint32_t track, const std::uint32_t side,
                         const std::uint32_t sector, std::uint8_t* out) const {
    if (!valid_chs(track, side, sector)) {
        return false;
    }
    return read_lba(chs_to_lba(track, side, sector), out);
}

bool DiskImage::write_chs(const std::uint32_t track, const std::uint32_t side,
                          const std::uint32_t sector, const std::uint8_t* in) {
    if (!valid_chs(track, side, sector)) {
        return false;
    }
    return write_lba(chs_to_lba(track, side, sector), in);
}

std::uint8_t DiskImage::byte_at(const std::uint32_t offset) const {
    if (offset >= data_.size()) {
        return 0xFF;
    }
    return data_[offset];
}

}  // namespace sony_msx::devices::fdc
