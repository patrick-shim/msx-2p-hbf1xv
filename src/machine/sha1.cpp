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

#include "machine/sha1.h"

namespace sony_msx::machine {

namespace {

// FIPS 180-4 §3.2: circular left rotate on a 32-bit word.
constexpr std::uint32_t rotl(const std::uint32_t x, const unsigned n) {
    return (x << n) | (x >> (32u - n));
}

}  // namespace

Sha1::Sha1() {
    // FIPS 180-4 §5.3.1: SHA-1 initial hash value.
    h_ = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
}

void Sha1::update(std::span<const std::uint8_t> data) {
    total_bytes_ += data.size();
    std::size_t offset = 0;

    // Top up a partially-filled block first.
    if (buffer_len_ > 0) {
        while (buffer_len_ < 64 && offset < data.size()) {
            buffer_[buffer_len_++] = data[offset++];
        }
        if (buffer_len_ == 64) {
            process_block(buffer_.data());
            buffer_len_ = 0;
        }
    }

    // Whole blocks straight from the input span.
    while (data.size() - offset >= 64) {
        process_block(data.data() + offset);
        offset += 64;
    }

    // Stash the tail.
    while (offset < data.size()) {
        buffer_[buffer_len_++] = data[offset++];
    }
}

std::string Sha1::hex_digest() {
    if (!finalized_) {
        // FIPS 180-4 §5.1.1 padding: a single 0x80 byte, then zeros until the
        // message length is congruent to 56 (mod 64), then the ORIGINAL
        // message length in bits as a 64-bit big-endian integer.
        const std::uint64_t bit_len = total_bytes_ * 8u;

        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) {
                buffer_[buffer_len_++] = 0x00;
            }
            process_block(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) {
            buffer_[buffer_len_++] = 0x00;
        }
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<std::uint8_t>((bit_len >> (8 * i)) & 0xFFu);
        }
        process_block(buffer_.data());
        buffer_len_ = 0;
        finalized_ = true;
    }

    static const char* kHexDigits = "0123456789abcdef";
    std::string hex;
    hex.reserve(40);
    for (const std::uint32_t word : h_) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex.push_back(kHexDigits[(word >> shift) & 0xFu]);
        }
    }
    return hex;
}

void Sha1::process_block(const std::uint8_t* block) {
    // FIPS 180-4 §6.1.2: message schedule W[0..79].
    std::uint32_t w[80];
    for (int t = 0; t < 16; ++t) {
        w[t] = (static_cast<std::uint32_t>(block[4 * t]) << 24) |
               (static_cast<std::uint32_t>(block[4 * t + 1]) << 16) |
               (static_cast<std::uint32_t>(block[4 * t + 2]) << 8) |
               static_cast<std::uint32_t>(block[4 * t + 3]);
    }
    for (int t = 16; t < 80; ++t) {
        w[t] = rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }

    std::uint32_t a = h_[0];
    std::uint32_t b = h_[1];
    std::uint32_t c = h_[2];
    std::uint32_t d = h_[3];
    std::uint32_t e = h_[4];

    // FIPS 180-4 §4.1.1 (functions f_t) + §4.2.1 (constants K_t), 80 rounds.
    for (int t = 0; t < 80; ++t) {
        std::uint32_t f;
        std::uint32_t k;
        if (t < 20) {
            f = (b & c) | ((~b) & d);  // Ch
            k = 0x5A827999u;
        } else if (t < 40) {
            f = b ^ c ^ d;  // Parity
            k = 0x6ED9EBA1u;
        } else if (t < 60) {
            f = (b & c) | (b & d) | (c & d);  // Maj
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;  // Parity
            k = 0xCA62C1D6u;
        }
        const std::uint32_t temp = rotl(a, 5) + f + e + k + w[t];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = temp;
    }

    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
}

std::string sha1_hex(std::span<const std::uint8_t> data) {
    Sha1 hasher;
    hasher.update(data);
    return hasher.hex_digest();
}

}  // namespace sony_msx::machine
