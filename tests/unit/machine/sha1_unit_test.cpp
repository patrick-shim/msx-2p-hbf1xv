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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "machine/sha1.h"

// Suite: Machine_Sha1_Unit (M30-S1, backlog G2, docs/m30-planner-package.md
// §2.1/§4-S1)
//
// Clean-room SHA-1 correctness gated on the PUBLISHED standard test vectors
// (FIPS 180-1 Appendix A/B; RFC 3174 §7.3) plus a streaming==one-shot
// equality proof over a synthetic multi-MB buffer (block-boundary/padding
// edge cases at realistic ROM sizes). Deterministic: pure function of the
// input bytes, no I/O, no clock.
//
// Clean-room discipline (planner R-M30-2): sha1.{h,cpp} were implemented
// solely from FIPS 180-4/RFC 3174; references/fmsx-60/source/EMULib/SHA1.c
// was deliberately never opened.

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> to_bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

}  // namespace

int main() {
    using sony_msx::machine::Sha1;
    using sony_msx::machine::sha1_hex;

    // --- Published vector 1: the empty string. ---
    {
        const std::vector<std::uint8_t> empty;
        expect(sha1_hex(empty) == "da39a3ee5e6b4b0d3255bfef95601890afd80709",
               "EmptyString_PublishedVector_Matches");
    }

    // --- Published vector 2: "abc" (FIPS 180-1 App. A / RFC 3174 test 1). ---
    {
        expect(sha1_hex(to_bytes("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d",
               "Abc_PublishedVector_Matches");
    }

    // --- Published vector 3: the 448-bit two-block message
    //     (FIPS 180-1 App. B / RFC 3174 test 2). ---
    {
        expect(sha1_hex(to_bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
                   "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
               "TwoBlockMessage_PublishedVector_Matches");
    }

    // --- Published vector 4: one million 'a' (FIPS 180-1 App. C / RFC 3174
    //     test 3), exercised BOTH one-shot and in deliberately odd-sized
    //     streaming chunks. ---
    {
        const std::vector<std::uint8_t> million(1'000'000, static_cast<std::uint8_t>('a'));
        const std::string expected = "34aa973cd4c4daa4f61eeb2bdbad27316534016f";
        expect(sha1_hex(million) == expected, "MillionA_PublishedVector_OneShot_Matches");

        Sha1 streaming;
        std::size_t offset = 0;
        // Prime-sized chunks guarantee every block-boundary phase is hit.
        const std::size_t chunk_sizes[] = {1, 63, 64, 65, 127, 4093, 65537};
        std::size_t chunk_index = 0;
        while (offset < million.size()) {
            const std::size_t n =
                std::min(chunk_sizes[chunk_index % 7], million.size() - offset);
            streaming.update(std::span<const std::uint8_t>(million.data() + offset, n));
            offset += n;
            ++chunk_index;
        }
        expect(streaming.hex_digest() == expected, "MillionA_PublishedVector_Streaming_Matches");
    }

    // --- Streaming == one-shot on a synthetic multi-MB patterned buffer
    //     (realistic ROM sizes: 2 MB + a non-power-of-two tail). ---
    {
        std::vector<std::uint8_t> buffer(2 * 1024 * 1024 + 12345);
        std::uint32_t state = 0x12345678u;  // fixed seed: deterministic
        for (std::uint8_t& b : buffer) {
            state = state * 1664525u + 1013904223u;  // LCG, fixed constants
            b = static_cast<std::uint8_t>(state >> 24);
        }
        const std::string one_shot = sha1_hex(buffer);

        Sha1 streaming;
        std::size_t offset = 0;
        std::size_t n = 1;
        while (offset < buffer.size()) {
            const std::size_t take = std::min(n, buffer.size() - offset);
            streaming.update(std::span<const std::uint8_t>(buffer.data() + offset, take));
            offset += take;
            n = (n * 2 > 100'000) ? 7 : n * 2;  // varying chunk sizes
        }
        expect(streaming.hex_digest() == one_shot, "MultiMbBuffer_StreamingEqualsOneShot");
        expect(one_shot.size() == 40, "Digest_Is40LowercaseHexChars_Length");
        bool all_lower_hex = true;
        for (const char c : one_shot) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                all_lower_hex = false;
            }
        }
        expect(all_lower_hex, "Digest_Is40LowercaseHexChars_Alphabet");
    }

    // --- hex_digest() idempotence (documented contract). ---
    {
        Sha1 hasher;
        hasher.update(to_bytes("abc"));
        const std::string first = hasher.hex_digest();
        const std::string second = hasher.hex_digest();
        expect(first == second && first == "a9993e364706816aba3e25717850c26c9cd0d89d",
               "HexDigest_CalledTwice_SameCachedResult");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Sha1_Unit cases passed\n";
    return 0;
}
