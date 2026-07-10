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

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace sony_msx::machine {

// Clean-room SHA-1 (M30-S1, backlog G2, docs/m30-planner-package.md §2.1).
//
// Implemented solely from the published standard: FIPS 180-4 (equivalently
// RFC 3174) -- 512-bit blocks, 80-round compression, standard 0x80/length
// padding, 160-bit digest. Correctness is gated on the standard's own
// published test vectors (tests/unit/machine/sha1_unit_test.cpp).
//
// Clean-room discipline (planner R-M30-2): this file was written without
// opening `references/fmsx-60/source/EMULib/SHA1.c` (or any other emulator's
// SHA-1); the algorithm is a public NIST standard and this is an independent
// from-spec implementation, exactly like the from-spec SHA-256 precedent in
// tests/integration/machine/hbf1xv_m19_aleste_smoke_integration_test.cpp.
//
// Placement note (planner §2.1): src/machine/ -- the cartridge identifier
// (cartridge_identifier.h) needs it in-process; src/core/ is under the M30
// zero-touch constraint and tools/ scripts cannot be linked.
class Sha1 {
public:
    Sha1();

    // Streaming input; callable any number of times before hex_digest().
    // Calling update() after hex_digest() is not supported (the digest is
    // finalized); construct a fresh Sha1 instead.
    void update(std::span<const std::uint8_t> data);

    // Finalizes (pads + processes the trailing block(s)) on first call and
    // returns the 40-char lowercase-hex digest. Idempotent: later calls
    // return the same cached digest.
    [[nodiscard]] std::string hex_digest();

private:
    void process_block(const std::uint8_t* block);

    std::array<std::uint32_t, 5> h_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    std::uint64_t total_bytes_ = 0;
    bool finalized_ = false;
};

// One-shot convenience: lowercase hex, 40 chars.
[[nodiscard]] std::string sha1_hex(std::span<const std::uint8_t> data);

}  // namespace sony_msx::machine
