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

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sony_msx::devices::fdc {

// Deterministic 3.5" 720 KB (2DD) sector image for the HB-F1XV built-in drive
// (M16-S1). Geometry: 80 cylinders x 2 sides x 9 sectors x 512 bytes =
// 737,280 bytes, MFM double density, media descriptor 0xF9 (fact-sheet
// "FDC for Sony HB-F1XV.md" §5; openMSX behaviour reference
// references/openmsx-21.0/src/fdc/DSKDiskImage.cc / SectorAccessibleDisk.cc —
// read only, never copied, GPL isolation).
//
// Determinism (hard requirement, planner §5.1 / A-M16-4): the medium is built
// from CONSTANTS by a pure function; there is NO host-filesystem or wall-clock
// input feeding emulation state. `synthesize()` produces a byte-for-byte
// reproducible image (a repository fixture tests/parity/m16_boot.dsk is a
// committed copy of exactly this output). CHS<->LBA per the 9/2/80 geometry.
class DiskImage {
public:
    static constexpr std::uint32_t kSectorSize = 512;
    static constexpr std::uint32_t kSectorsPerTrack = 9;
    static constexpr std::uint32_t kSides = 2;
    static constexpr std::uint32_t kTracks = 80;
    static constexpr std::uint32_t kTotalSectors = kSectorsPerTrack * kSides * kTracks;  // 1440
    static constexpr std::uint32_t kImageBytes = kTotalSectors * kSectorSize;            // 737280
    static constexpr std::uint8_t kMediaDescriptor = 0xF9;

    DiskImage();  // synthesized default medium
    explicit DiskImage(std::vector<std::uint8_t> bytes);

    // Deterministic synthesized medium: a valid FAT12 boot sector + BPB (media
    // 0xF9) at LBA 0, media bytes at the two FAT copies, and a reproducible
    // per-sector fill everywhere else. Pure function of constants only.
    [[nodiscard]] static std::vector<std::uint8_t> synthesize();

    // CHS<->LBA. sector is 1-based (1..9); track 0..79; side 0..1.
    [[nodiscard]] static std::uint32_t chs_to_lba(std::uint32_t track, std::uint32_t side,
                                                  std::uint32_t sector);
    static void lba_to_chs(std::uint32_t lba, std::uint32_t& track, std::uint32_t& side,
                           std::uint32_t& sector);

    [[nodiscard]] bool valid_chs(std::uint32_t track, std::uint32_t side, std::uint32_t sector) const;

    // Sector access by LBA and by CHS. read copies kSectorSize bytes into `out`;
    // write copies kSectorSize bytes from `in` (ignored when write-protected).
    // Returns false for an out-of-range address (or a protected write).
    bool read_lba(std::uint32_t lba, std::uint8_t* out) const;
    bool write_lba(std::uint32_t lba, const std::uint8_t* in);
    bool read_chs(std::uint32_t track, std::uint32_t side, std::uint32_t sector,
                  std::uint8_t* out) const;
    bool write_chs(std::uint32_t track, std::uint32_t side, std::uint32_t sector,
                   const std::uint8_t* in);

    // Raw byte (deterministic tests). Returns 0xFF outside the image.
    [[nodiscard]] std::uint8_t byte_at(std::uint32_t offset) const;

    [[nodiscard]] std::size_t size() const { return data_.size(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return data_; }

    [[nodiscard]] bool write_protected() const { return write_protected_; }
    void set_write_protected(bool protect) { write_protected_ = protect; }

    [[nodiscard]] bool present() const { return present_; }
    void set_present(bool present) { present_ = present; }

private:
    std::vector<std::uint8_t> data_;
    bool write_protected_ = false;
    bool present_ = true;
};

}  // namespace sony_msx::devices::fdc
