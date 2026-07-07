// Suite: Machine_Hbf1xvM19AlesteSmoke_Integration (M19-S4, backlog B7)
//
// *** MECHANICAL SMOKE CHECK ONLY -- NO GAMEPLAY/HARDWARE-IDENTITY CLAIM ***
//
// roms/aleste.rom's real-world mapper/game identity is NOT confirmable from
// the artifacts on hand (planner A-M19-10, §2.6 track 2): its 2,097,152-byte
// size is incompatible with a straight Mirrored dump (64 KB cap, A-M19-7) or
// a genuine 256 KB-class Konami-mapper dump (RomKonami.cc:27-31's own
// warning threshold), and no SHA1 is recorded to cross-reference openMSX's
// software database. This test asserts ONLY three concrete, directly
// observable, non-fabricated mechanical facts:
//   (a) the file loads without error as `Generic8kB` (openMSX's own label:
//       "Generic ROM types that don't exist in real ROMs", RomInfo.cc:17-19 --
//       the single most honest choice for a file whose real identity is
//       unconfirmed);
//   (b) its SHA256 matches docs/asset-checksums.txt's recorded value;
//   (c) after cold_boot() + routing primary slot 1 into CPU page 1,
//       debug_bus_read(0x4000) == 0x41 ('A') and debug_bus_read(0x4001) ==
//       0x42 ('B') -- the MSX cartridge-ROM header signature bytes directly
//       observed during grounding.
// NO claim is made anywhere in this file (or anywhere else in this
// milestone) that this proves the real Aleste game runs/plays correctly, nor
// that Generic8kB is this file's genuine hardware mapper type -- only that
// the cartridge-loading PIPELINE (file -> image -> slot -> CPU-bus-readable
// bytes) functions end-to-end with a real, sizeable, pre-existing file.

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
// specification for this test's own verification need only -- not vendored
// from any external/GPL source (no license-isolation concern; the algorithm
// itself is a public NIST standard).
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

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::devices::cartridge::CartridgeLoadResult;
    using sony_msx::devices::cartridge::CartridgeMapperType;

    const std::filesystem::path path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "aleste.rom";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Case failed: AlesteRom_FileOpens (path: " << path << ")\n";
        return 1;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());

    // --- (a) File size + mechanical Generic8kB load succeeds. ---
    expect(image.size() == 2'097'152, "AlesteRom_FileSize_Is2097152Bytes");

    // --- (b) SHA256 matches docs/asset-checksums.txt's recorded value. ---
    const std::string digest = sha256_hex(image);
    // docs/asset-checksums.txt records this exact SHA256 for roms/aleste.rom;
    // re-verified independently here (an own from-spec SHA-256 implementation
    // above), not merely re-asserted.
    const std::string expected_digest =
        "4ea1e183467abe094df88901fc5dc70f0df5c1fc424de19f212692001bd5ad43";
    expect(digest == expected_digest, "AlesteRom_Sha256_MatchesRecordedChecksum");

    Hbf1xvMachine machine;
    machine.cold_boot();
    const CartridgeLoadResult load_result = machine.load_cartridge(1, CartridgeMapperType::Generic8kB, image);
    expect(load_result == CartridgeLoadResult::Ok, "AlesteRom_LoadsAsGeneric8kB_MechanicalOnly_NoError");

    // --- (c) Route primary slot 1 into CPU page 1 (mirrors map_flat_ram()'s
    //     #A8 pattern: page0=slot3, page1=slot1, page2=slot3, page3=slot3 ->
    //     0xF7, this project's own established SlotBus bit layout,
    //     slot_bus.cpp:47-49) and read back the two header-signature bytes
    //     directly observed during grounding. ---
    machine.map_flat_ram();
    machine.debug_io_write(0xA8, 0xF7);
    expect(machine.debug_bus_read(0x4000) == 0x41, "AlesteRom_Page1_FirstByte_Is_A_0x41");
    expect(machine.debug_bus_read(0x4001) == 0x42, "AlesteRom_Page1_SecondByte_Is_B_0x42");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
