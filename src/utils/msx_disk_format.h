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

#include <cstdint>
#include <vector>

// Standalone HOST-side MSX disk-format writer (M53, DEC-0080). This translation
// unit is BUILD-ISOLATED: it links NOTHING from sony_msx_core and includes NO
// emulator header. The 720 KB FAT12 layout facts below are tiny public
// FAT12/hardware constants re-expressed from the documented spec (planner
// package docs/m53-planner-package.md §2.2), NOT a code dependency on the
// emulator. Sources of truth cross-checked: tools/format-blank-disk.ps1:82-133
// (the A/B-validated M41 blank), src/devices/fdc/disk_image.cpp:43-85 (boot
// sector/BPB/FAT seed, machine's OWN expectation).
namespace sony_msx::utils {

// Geometry of the HB-F1XV built-in 3.5" 720 KB (2DD) medium: 80 cylinders x
// 2 sides x 9 sectors x 512 bytes = 737,280 bytes, media descriptor 0xF9.
struct DiskFormat {
    static constexpr std::uint32_t kSectorSize = 512;
    static constexpr std::uint32_t kSectorsPerTrack = 9;
    static constexpr std::uint32_t kSides = 2;
    static constexpr std::uint32_t kTracks = 80;
    static constexpr std::uint32_t kTotalSectors = kSectorsPerTrack * kSides * kTracks;  // 1440
    static constexpr std::uint32_t kImageBytes = kTotalSectors * kSectorSize;            // 737280
    static constexpr std::uint32_t kMaxSectorIndex = kTotalSectors - 1;                  // 1439
    static constexpr std::uint8_t kMediaDescriptor = 0xF9;
};

// Build a byte-exact, deterministic, EMPTY formatted 720 KB MSX-DOS FAT12
// blank image (737,280 bytes). A pure function of the constants above: no
// wall-clock, no host input, no volume serial -- so two calls are byte-identical
// and the SHA256 equals the tools/format-blank-disk.ps1 default blank. Boot code
// region (offset 0x1E..0x1FD) is ALL ZERO (a genuinely empty, non-bootable
// data/files medium), the two FAT copies are seeded F9 FF FF, and the root
// directory + data area are ALL ZERO (an empty filesystem => zero files).
[[nodiscard]] std::vector<std::uint8_t> build_blank_image();

}  // namespace sony_msx::utils
