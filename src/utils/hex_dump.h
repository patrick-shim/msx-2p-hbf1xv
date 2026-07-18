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
#include <ostream>
#include <span>

namespace sony_msx::utils {

// Deterministic xxd/hexdump-C hybrid formatter (planner package §2.3). Emits one
// 16-byte line per row:
//   0000fe00: eb fe 90 53 4f 4e 59 4d  53 58 20 00 02 02 01 00  |...SONYMSX .....|
// - 8-hex-digit zero-padded byte offset (base_offset + row index), ": ".
// - 16 bytes as lowercase 2-hex, space separated, an extra space after the 8th.
// - "  |" + 16-char ASCII gutter (printable 0x20..0x7E verbatim, else '.') + "|".
// A trailing short row pads the hex columns with spaces so the gutter aligns.
// PURE FUNCTION of the bytes: no timestamps, so two dumps of identical bytes are
// byte-identical. An empty span emits nothing.
void write_hex_dump(std::ostream& out, std::span<const std::uint8_t> bytes,
                    std::uint64_t base_offset);

}  // namespace sony_msx::utils
