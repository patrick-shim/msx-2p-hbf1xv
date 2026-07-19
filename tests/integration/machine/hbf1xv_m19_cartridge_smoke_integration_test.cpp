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

// Suite: Machine_Hbf1xvM19CartridgeSmoke_Integration (M19 follow-up, backlog B7)
//
// *** MECHANICAL/PLAUSIBILITY SMOKE CHECK -- NOT A BYTE-EXACT PROTOCOL CLAIM ***
//
// ASSET LAYOUT (post-reorg): the game library now lives under games/roms/;
// roms/ holds ONLY the FM-PAC peripheral. This test reads a stealth-action
// cartridge title's ROM at games/roms/action-cartridge.rom, which is
// BYTE-IDENTICAL to the pre-reorg roms/ copy of the same dump (verified: same
// 139,264 bytes, same SHA1 919fa77..., same SHA256 399447d...); every assertion
// below therefore holds unchanged. games/roms/ is untracked, so a machine without
// the asset SKIPS (returns 0) rather than FAILS -- the same skip-when-absent
// discipline the FM-PAC autoload / sdl3_cli_session Case-3 tests use.
//
// Unlike the scroll-shooter ROM (A-M19-10, genuinely unconfirmable identity),
// this ROM's real-world identity as a `Konami`-mapper dump of the
// stealth-action cartridge title (Konami, 1987, MSX2) IS directly confirmed by
// a concrete, citable source already in this repo: its SHA1
// (919fa773e1f239dc90fa47e2770f3f5eca7f9bfe, computed below and asserted)
// matches BYTE-FOR-BYTE one of the `<dump><megarom><type>Konami</type>
// <hash>919fa773e1f239dc90fa47e2770f3f5eca7f9bfe</hash></megarom></dump>`
// entries recorded under that title's `<software>` record
// (`<genmsxid>1028</genmsxid>`, Konami, 1987, MSX2, JP) in
// `references/openmsx-21.0/share/softwaredb.xml` -- an EXACT hash match to a
// specifically `Konami`-typed catalogued dump, not a genre/heuristic
// inference. (That same title also has a few `ASCII8`/`KonamiSCC`/
// `ReproCartridgeV1` dumps recorded under DIFFERENT hashes -- this specific
// file's hash is recorded ONLY under `Konami`.)
//
// Additionally verified live: mounting this exact file in real WSL openMSX
// 19.1 via `-carta <this ROM> -romtype Konami` succeeds (no
// rejection/fatal error), and reading back CPU memory 0x4000/0x4001 (after
// routing primary slot 1 into CPU page 1, exactly as this test does) returns
// 0x41/0x42 ('A'/'B'), matching this emulator's own result byte-for-byte.
// This is a genuinely STRONGER real-world validation than the scroll-shooter ROM's
// disposition, but it is still NOT used for any byte-exact bank-switch/
// protocol correctness claim -- those remain on synthetic, hand-authored
// fixtures only (planner A-M19-10 discipline, unchanged). This test asserts
// ONLY: (a) the file loads without error as `Konami`; (b) its SHA1 matches
// the cited softwaredb.xml entry AND its SHA256 matches
// docs/asset-checksums.txt's recorded value; (c) after cold_boot() +
// routing primary slot 1 into CPU page 1, debug_bus_read(0x4000) == 0x41
// ('A') and debug_bus_read(0x4001) == 0x42 ('B'); (d) the loaded mapper
// reports `Konami`. NO claim is made about actual gameplay correctness or
// about any bank beyond the two header bytes directly observed.

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "machine/hbf1xv_machine.h"

#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Minimal, self-contained SHA-256 (FIPS 180-4), implemented from the public
// specification for this test's own verification need only (mirrors
// hbf1xv_m19_scroll_shooter_smoke_integration_test.cpp's own copy -- each test
// executable is self-contained, no shared test-only library exists in this
// project).
class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        h_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        buffer_.clear();
        total_len_ = 0;
    }

    void update(const std::uint8_t* data, std::size_t len) {
        total_len_ += len;
        buffer_.insert(buffer_.end(), data, data + len);
        std::size_t offset = 0;
        while (buffer_.size() - offset >= 64) {
            process_block(&buffer_[offset]);
            offset += 64;
        }
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    [[nodiscard]] std::string finalize_hex() {
        const std::uint64_t bit_len = total_len_ * 8;
        std::vector<std::uint8_t> pad;
        pad.push_back(0x80);
        while ((buffer_.size() + pad.size()) % 64 != 56) {
            pad.push_back(0x00);
        }
        for (int i = 7; i >= 0; --i) {
            pad.push_back(static_cast<std::uint8_t>((bit_len >> (8 * i)) & 0xFF));
        }
        buffer_.insert(buffer_.end(), pad.begin(), pad.end());
        for (std::size_t offset = 0; offset < buffer_.size(); offset += 64) {
            process_block(&buffer_[offset]);
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const std::uint32_t word : h_) {
            oss << std::setw(8) << word;
        }
        return oss.str();
    }

private:
    static std::uint32_t rotr(const std::uint32_t x, const unsigned n) {
        return (x >> n) | (x << (32 - n));
    }

    void process_block(const std::uint8_t* block) {
        static constexpr std::uint32_t kK[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
            0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
            0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
            0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
            0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
            0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
            0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
            0xc67178f2u};

        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24) |
                   (static_cast<std::uint32_t>(block[4 * i + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[4 * i + 2]) << 8) |
                   static_cast<std::uint32_t>(block[4 * i + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        std::uint32_t e = h_[4], f = h_[5], g = h_[6], hh = h_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = hh + s1 + ch + kK[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += hh;
    }

    std::array<std::uint32_t, 8> h_{};
    std::vector<std::uint8_t> buffer_;
    std::uint64_t total_len_ = 0;
};

std::string sha256_hex(const std::vector<std::uint8_t>& data) {
    Sha256 hasher;
    hasher.update(data.data(), data.size());
    return hasher.finalize_hex();
}

// Minimal, self-contained SHA-1 (FIPS 180-4), implemented from the public
// specification for this test's own verification need only -- solely to
// cross-check this file's hash against the softwaredb.xml-cited value (SHA1
// is the hash convention that database uses; docs/asset-checksums.txt uses
// SHA256, checked separately via Sha256 above).
std::string sha1_hex(const std::vector<std::uint8_t>& data) {
    std::uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu, h3 = 0x10325476u, h4 = 0xC3D2E1F0u;

    std::vector<std::uint8_t> msg(data);
    const std::uint64_t bit_len = static_cast<std::uint64_t>(msg.size()) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<std::uint8_t>((bit_len >> (8 * i)) & 0xFF));
    }

    auto rotl = [](const std::uint32_t x, const unsigned n) { return (x << n) | (x >> (32 - n)); };

    for (std::size_t offset = 0; offset < msg.size(); offset += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            const std::uint8_t* b = &msg[offset + static_cast<std::size_t>(4 * i)];
            w[i] = (static_cast<std::uint32_t>(b[0]) << 24) | (static_cast<std::uint32_t>(b[1]) << 16) |
                   (static_cast<std::uint32_t>(b[2]) << 8) | static_cast<std::uint32_t>(b[3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const std::uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << h0 << std::setw(8) << h1 << std::setw(8) << h2
        << std::setw(8) << h3 << std::setw(8) << h4;
    return oss.str();
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::devices::cartridge::CartridgeLoadResult;
    using sony_msx::devices::cartridge::CartridgeMapperType;

    // SONY_MSX_ROMS_DIR = <source>/roms; the game library sits beside it at
    // <source>/games/roms (untracked). Derive it rather than adding a new build
    // macro, so this local test stays self-contained.
    const std::filesystem::path path =
        std::filesystem::path(SONY_MSX_ROMS_DIR).parent_path() / "games" / "roms" / "action-cartridge.rom";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cout << "SKIP: games/roms/action-cartridge.rom not present (path: " << path
                  << ") -- skip-when-absent asset guard\n";
        return 0;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());

    // --- File size: 139,264 bytes = 17 x 8 KB banks. ---
    expect(image.size() == 139'264, "CartridgeRom_FileSize_Is139264Bytes");

    // --- SHA1 matches the softwaredb.xml-cited (Konami, 1987, MSX2,
    //     genmsxid 1028) `Konami`-typed dump entry EXACTLY. ---
    const std::string sha1_digest = sha1_hex(image);
    const std::string expected_sha1 = "919fa773e1f239dc90fa47e2770f3f5eca7f9bfe";
    expect(sha1_digest == expected_sha1,
           "CartridgeRom_Sha1_MatchesSoftwareDbKonamiTypedDump");

    // --- SHA256 matches docs/asset-checksums.txt's recorded value. ---
    const std::string sha256_digest = sha256_hex(image);
    const std::string expected_sha256 =
        "399447d8012035b5a97dd3aec235a8e7d03b8da499196b6f047e1c7290a35760";
    expect(sha256_digest == expected_sha256, "CartridgeRom_Sha256_MatchesRecordedChecksum");

    Hbf1xvMachine machine;
    machine.cold_boot();
    const CartridgeLoadResult load_result = machine.load_cartridge(1, CartridgeMapperType::Konami, image);
    expect(load_result == CartridgeLoadResult::Ok, "CartridgeRom_LoadsAsKonami_NoError");
    expect(machine.cartridge_slot1().loaded() &&
               machine.cartridge_slot1().mapper_type() == CartridgeMapperType::Konami,
           "CartridgeRom_ActiveMapperReportsKonami");

    // --- Route primary slot 1 into CPU page 1 (0xF7: page0=slot3, page1=
    //     slot1, page2=slot3, page3=slot3 -- this project's own established
    //     SlotBus bit layout, slot_bus.cpp:47-49) and read back the two
    //     header-signature bytes. ---
    machine.map_flat_ram();
    machine.debug_io_write(0xA8, 0xF7);
    expect(machine.debug_bus_read(0x4000) == 0x41, "CartridgeRom_Page1_FirstByte_Is_A_0x41");
    expect(machine.debug_bus_read(0x4001) == 0x42, "CartridgeRom_Page1_SecondByte_Is_B_0x42");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
