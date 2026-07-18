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

#include "utils/hex_dump.h"

#include <cstddef>

namespace sony_msx::utils {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

// Emit `value` as `digits` lowercase hex characters (zero-padded, big-endian).
void put_hex(std::ostream& out, std::uint64_t value, int digits) {
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        out << kHexDigits[(value >> shift) & 0xFu];
    }
}

constexpr std::size_t kBytesPerLine = 16;

}  // namespace

void write_hex_dump(std::ostream& out, std::span<const std::uint8_t> bytes,
                    std::uint64_t base_offset) {
    const std::size_t n = bytes.size();
    for (std::size_t line = 0; line < n; line += kBytesPerLine) {
        const std::size_t count = (n - line < kBytesPerLine) ? (n - line) : kBytesPerLine;

        put_hex(out, base_offset + line, 8);
        out << ": ";

        // Hex columns: each present byte is "xx " (2-hex + trailing space); an
        // absent column on a short line is "   " (3 spaces) so the gutter aligns.
        // An additional space after the 8th column splits the 8/8 groups.
        for (std::size_t i = 0; i < kBytesPerLine; ++i) {
            if (i < count) {
                put_hex(out, bytes[line + i], 2);
                out << ' ';
            } else {
                out << "   ";
            }
            if (i == 7) {
                out << ' ';
            }
        }

        // The gutter delimiter: one more space then '|' yields the canonical
        // two-space gap before the ASCII gutter.
        out << ' ' << '|';
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t b = bytes[line + i];
            out << static_cast<char>((b >= 0x20 && b <= 0x7E) ? static_cast<char>(b) : '.');
        }
        out << "|\n";
    }
}

}  // namespace sony_msx::utils
