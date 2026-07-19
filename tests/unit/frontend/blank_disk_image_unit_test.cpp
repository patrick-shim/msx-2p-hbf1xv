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

// Suite: Frontend_BlankDiskImage_Unit (M56, DEC-0084, planner §7/§9 test 2; slice
// S4).
//
// Byte oracle over sony_msx::frontend::build_blank_disk_image() -- the F5 New
// Blank Disk writer's frontend-local re-expression of the DEC-0080 720 KB
// MSX-DOS FAT12 layout (never links msx_diskutil). Mirrors
// diskutils_blank_layout_unit_test.cpp: size, JMP/OEM, every BPB field, boot
// signature, the all-zero boot-code region, the two F9-FF-FF FAT seeds + zero
// remainder, empty root, zero data area, PLUS a self-contained SHA256 (local to
// this TU) asserting == the DEC-0080 golden, and determinism.

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "frontend/blank_disk_image.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// --- A compact, self-contained SHA-256 (public-domain algorithm; local to this
//     TU -- NOT added to src/). Produces the lowercase hex digest of `data`. ---
class Sha256 {
public:
    Sha256() { reset(); }

    void update(const std::uint8_t* p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            buffer_[buffer_len_++] = p[i];
            if (buffer_len_ == 64) {
                process(buffer_.data());
                bit_len_ += 512;
                buffer_len_ = 0;
            }
        }
    }

    std::string hex_digest() {
        std::uint64_t total_bits = bit_len_ + static_cast<std::uint64_t>(buffer_len_) * 8;
        // Pad: 0x80, then zeros, then the 64-bit big-endian length.
        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) {
                buffer_[buffer_len_++] = 0;
            }
            process(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) {
            buffer_[buffer_len_++] = 0;
        }
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<std::uint8_t>((total_bits >> (i * 8)) & 0xFF);
        }
        process(buffer_.data());

        static const char* kHex = "0123456789abcdef";
        std::string out;
        out.reserve(64);
        for (std::uint32_t h : state_) {
            for (int i = 3; i >= 0; --i) {
                const std::uint8_t byte = static_cast<std::uint8_t>((h >> (i * 8)) & 0xFF);
                out.push_back(kHex[byte >> 4]);
                out.push_back(kHex[byte & 0x0F]);
            }
        }
        return out;
    }

private:
    void reset() {
        state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                  0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        bit_len_ = 0;
        buffer_len_ = 0;
    }

    static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    void process(const std::uint8_t* chunk) {
        static const std::uint32_t k[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
            0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
            0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
            0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
            0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(chunk[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    std::uint64_t bit_len_ = 0;
};

std::string sha256_hex(const std::vector<std::uint8_t>& data) {
    Sha256 h;
    h.update(data.data(), data.size());
    return h.hex_digest();
}

}  // namespace

int main() {
    using sony_msx::frontend::build_blank_disk_image;

    // --- Sanity-check the local SHA256 against the published empty-string vector. ---
    {
        const std::vector<std::uint8_t> empty;
        expect(sha256_hex(empty) ==
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
               "Sha256_SelfCheck_EmptyStringVector");
    }

    const std::vector<std::uint8_t> img = build_blank_disk_image();

    // --- Size + JMP + OEM. ---
    expect(img.size() == 737280, "Size_737280");
    expect(img[0] == 0xEB && img[1] == 0xFE && img[2] == 0x90, "Jmp_EBFE90");
    const char oem[8] = {'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' '};
    bool oem_ok = true;
    for (int i = 0; i < 8; ++i) {
        if (img[3 + i] != static_cast<std::uint8_t>(oem[i])) {
            oem_ok = false;
        }
    }
    expect(oem_ok, "Oem_SONYMSX");

    // --- Every BPB field (offset 0x0B..0x1D). ---
    struct Field {
        std::size_t offset;
        std::uint8_t value;
        const char* name;
    };
    const Field bpb[] = {
        {0x0B, 0x00, "BytesPerSectorLo"}, {0x0C, 0x02, "BytesPerSectorHi_512"},
        {0x0D, 0x02, "SectorsPerCluster_2"},
        {0x0E, 0x01, "ReservedSectorsLo_1"}, {0x0F, 0x00, "ReservedSectorsHi"},
        {0x10, 0x02, "NumberOfFats_2"},
        {0x11, 0x70, "RootDirEntriesLo_112"}, {0x12, 0x00, "RootDirEntriesHi"},
        {0x13, 0xA0, "TotalSectorsLo"}, {0x14, 0x05, "TotalSectorsHi_1440"},
        {0x15, 0xF9, "MediaDescriptor_F9"},
        {0x16, 0x03, "SectorsPerFatLo_3"}, {0x17, 0x00, "SectorsPerFatHi"},
        {0x18, 0x09, "SectorsPerTrackLo_9"}, {0x19, 0x00, "SectorsPerTrackHi"},
        {0x1A, 0x02, "HeadsLo_2"}, {0x1B, 0x00, "HeadsHi"},
        {0x1C, 0x00, "HiddenSectorsLo"}, {0x1D, 0x00, "HiddenSectorsHi"},
    };
    for (const Field& f : bpb) {
        expect(img[f.offset] == f.value, f.name);
    }

    // --- Boot signature + all-zero boot-code region (non-bootable). ---
    expect(img[510] == 0x55 && img[511] == 0xAA, "BootSignature_55AA");
    bool boot_code_zero = true;
    for (std::size_t i = 0x1E; i <= 0x1FD; ++i) {
        if (img[i] != 0x00) {
            boot_code_zero = false;
        }
    }
    expect(boot_code_zero, "BootCodeRegion_0x1E_0x1FD_AllZero");

    // --- FAT copy #1 (LBA 1, offset 512) and #2 (LBA 4, offset 2048). ---
    const std::size_t fat_bases[] = {1u * 512u, 4u * 512u};
    for (const std::size_t base : fat_bases) {
        expect(img[base + 0] == 0xF9 && img[base + 1] == 0xFF && img[base + 2] == 0xFF,
               "FatSeed_F9FFFF");
        bool rest_zero = true;
        for (std::size_t i = 3; i < 3u * 512u; ++i) {
            if (img[base + i] != 0x00) {
                rest_zero = false;
                break;
            }
        }
        expect(rest_zero, "FatRemainder_AllZero");
    }

    // --- Root directory (LBA 7..13) + data area (LBA 14..) all zero. ---
    bool root_zero = true;
    for (std::size_t i = 7u * 512u; i < 14u * 512u; ++i) {
        if (img[i] != 0x00) {
            root_zero = false;
            break;
        }
    }
    expect(root_zero, "RootDirectory_Empty_AllZero");
    bool data_zero = true;
    for (std::size_t i = 14u * 512u; i < 737280u; ++i) {
        if (img[i] != 0x00) {
            data_zero = false;
            break;
        }
    }
    expect(data_zero, "DataArea_AllZero");

    // --- The DEC-0080 golden SHA256 oracle. ---
    const std::string digest = sha256_hex(img);
    const std::string golden = "6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188";
    if (digest != golden) {
        std::cerr << "  digest=" << digest << "\n  golden=" << golden << "\n";
    }
    expect(digest == golden, "GoldenSha256_Matches");

    // --- Determinism: two independent calls are byte-identical. ---
    expect(build_blank_disk_image() == img, "TwoCalls_ByteIdentical");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_BlankDiskImage_Unit cases passed\n";
    return 0;
}
