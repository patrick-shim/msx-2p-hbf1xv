#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "devices/cpu/z80a_state.h"

namespace sony_msx::machine::debug_format {

// Shared, deterministic, locale-independent ASCII formatting primitives used by
// the M10 debug/trace serializers (S1 CPU trace sink, S3 full-state dump and
// execution-event log). Hand-rolled so output is byte-identical across runs and
// environments: fixed field order, fixed-width uppercase hex, base-10 decimals,
// no locale/stream state, no floating point, no wall-clock.

// Uppercase-hex, zero-padded to at least `digits`, big-endian.
inline std::string to_hex(const std::uint64_t value, const std::size_t digits) {
    static const char kHexDigits[] = "0123456789ABCDEF";
    std::string out;
    std::uint64_t v = value;
    do {
        out.push_back(kHexDigits[v & 0x0F]);
        v >>= 4;
    } while (v != 0);
    while (out.size() < digits) {
        out.push_back('0');
    }
    for (std::size_t i = 0, j = out.size() - 1; i < j; ++i, --j) {
        const char tmp = out[i];
        out[i] = out[j];
        out[j] = tmp;
    }
    return out;
}

// Base-10, no locale involvement.
inline std::string to_dec(const std::uint64_t value) {
    if (value == 0) {
        return "0";
    }
    std::string out;
    std::uint64_t v = value;
    while (v != 0) {
        out.push_back(static_cast<char>('0' + static_cast<char>(v % 10)));
        v /= 10;
    }
    for (std::size_t i = 0, j = out.size() - 1; i < j; ++i, --j) {
        const char tmp = out[i];
        out[i] = out[j];
        out[j] = tmp;
    }
    return out;
}

// Decomposed 8-character flag string from an F register value: each set flag is
// its letter, each clear flag is '.', in fixed S,Z,Y,H,X,P/V,N,C order.
inline std::string flag_string(const std::uint8_t f) {
    using devices::cpu::Z80aState;
    std::string s(8, '.');
    if ((f & Z80aState::kFlagSign) != 0) {
        s[0] = 'S';
    }
    if ((f & Z80aState::kFlagZero) != 0) {
        s[1] = 'Z';
    }
    if ((f & Z80aState::kFlagY) != 0) {
        s[2] = 'Y';
    }
    if ((f & Z80aState::kFlagHalfCarry) != 0) {
        s[3] = 'H';
    }
    if ((f & Z80aState::kFlagX) != 0) {
        s[4] = 'X';
    }
    if ((f & Z80aState::kFlagParityOverflow) != 0) {
        s[5] = 'P';
    }
    if ((f & Z80aState::kFlagSubtract) != 0) {
        s[6] = 'N';
    }
    if ((f & Z80aState::kFlagCarry) != 0) {
        s[7] = 'C';
    }
    return s;
}

}  // namespace sony_msx::machine::debug_format
