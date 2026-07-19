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

// The frontend-local re-expression of the 720 KB MSX-DOS FAT12 blank-disk
// layout used by File > New Blank Disk (F5) -- the same layout that
// msx-diskutil --create produces. (DEC-0084)
//
// HARD BOUNDARY (both ways, DEC-0080): this TU NEVER #includes src/utils/*
// and NEVER links msx_diskutil. The layout facts are re-expressed from the
// documented 720 KB MSX-DOS FAT12 layout spec and pinned by the golden SHA256
// below, not via a code dependency. Compiled into sony_msx_core (SDL-free) so
// its unit test registers OUTSIDE the SONY_MSX_ENABLE_SDL3 guard.
//
// Byte oracle (frontend_blank_disk_image_unit_test): SHA256 of the returned
// image == 6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188.

namespace sony_msx::frontend {

// A fully-formatted, EMPTY, DOS-recognizable, deliberately NON-bootable 720 KB
// (1440 x 512 = 737,280-byte) MSX-DOS FAT12 image. Pure function of constants:
// two independent calls are byte-identical.
[[nodiscard]] std::vector<std::uint8_t> build_blank_disk_image();

}  // namespace sony_msx::frontend
