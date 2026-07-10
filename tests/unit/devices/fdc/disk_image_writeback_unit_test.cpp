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

// Suite: Devices_DiskImageWriteback_Unit (M36-S-c)
//
// Deterministic coverage of the opt-in host-file disk-save persistence added
// to DiskImage: flush round-trip (write -> flush -> reload -> read-back
// byte-identical), the write-protect gate, the no-host-path no-op, the dirty
// flag transitions, and determinism (two identical write runs produce
// byte-identical output files). flush() is write-only output and never
// re-reads the host file into emulation state.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_image.h"

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

using sony_msx::devices::fdc::DiskImage;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

std::filesystem::path temp_path(const char* leaf) {
    return std::filesystem::temp_directory_path() / leaf;
}

}  // namespace

int main() {
    // --- No host path: flush() is a no-op returning false; default in-memory. ---
    {
        DiskImage img;  // synthesized default medium
        std::vector<std::uint8_t> sector(DiskImage::kSectorSize, 0xAA);
        expect(!img.dirty(), "NoPath_InitiallyClean");
        expect(img.write_lba(100, sector.data()), "NoPath_WriteSucceedsInMemory");
        expect(img.dirty(), "NoPath_DirtyAfterWrite");
        expect(!img.flush(), "NoPath_FlushReturnsFalse");
    }

    // --- Flush round-trip: write -> flush -> reload -> read-back identical. ---
    {
        const std::filesystem::path path = temp_path("m36_diskwb_roundtrip.dsk");
        std::filesystem::remove(path);

        DiskImage img;
        img.set_host_path(path);

        // Write a distinctive pattern into sector LBA 200.
        std::vector<std::uint8_t> written(DiskImage::kSectorSize);
        for (std::size_t i = 0; i < written.size(); ++i) {
            written[i] = static_cast<std::uint8_t>((i * 3u + 0x11u) & 0xFFu);
        }
        expect(img.write_lba(200, written.data()), "RoundTrip_WriteSucceeds");
        expect(img.dirty(), "RoundTrip_DirtyBeforeFlush");
        expect(img.flush(), "RoundTrip_FlushSucceeds");
        expect(!img.dirty(), "RoundTrip_CleanAfterFlush");

        // The host file is exactly the in-memory image (size + bytes).
        const std::vector<std::uint8_t> on_disk = read_file(path);
        expect(on_disk.size() == DiskImage::kImageBytes, "RoundTrip_FileSizeExact");
        expect(on_disk == img.data(), "RoundTrip_FileMatchesMemory");

        // Reload the host file into a fresh image and read the sector back.
        DiskImage reloaded(read_file(path));
        std::vector<std::uint8_t> read_back(DiskImage::kSectorSize, 0);
        expect(reloaded.read_lba(200, read_back.data()), "RoundTrip_ReloadReadSucceeds");
        expect(read_back == written, "RoundTrip_ReadBackByteIdentical");

        std::filesystem::remove(path);
    }

    // --- Write-protect gate: protected image rejects writes, stays clean. ---
    {
        const std::filesystem::path path = temp_path("m36_diskwb_protected.dsk");
        std::filesystem::remove(path);

        DiskImage img;
        img.set_host_path(path);
        img.set_write_protected(true);
        std::vector<std::uint8_t> sector(DiskImage::kSectorSize, 0x5C);
        expect(!img.write_lba(10, sector.data()), "Protected_WriteRejected");
        expect(!img.dirty(), "Protected_StaysClean");
        // Flush still writes the (unmodified) image out; the point is no write
        // corrupted it. But a protected image is not the save target; verify
        // the persisted bytes equal a fresh synthesized medium (unchanged).
        expect(img.flush(), "Protected_FlushSucceeds");
        const std::vector<std::uint8_t> on_disk = read_file(path);
        expect(on_disk == DiskImage().data(), "Protected_PersistedUnchanged");
        std::filesystem::remove(path);
    }

    // --- Determinism: two identical scripted write runs -> byte-identical files. ---
    {
        const std::filesystem::path path_a = temp_path("m36_diskwb_det_a.dsk");
        const std::filesystem::path path_b = temp_path("m36_diskwb_det_b.dsk");
        std::filesystem::remove(path_a);
        std::filesystem::remove(path_b);

        auto run = [](const std::filesystem::path& p) {
            DiskImage img;
            img.set_host_path(p);
            for (std::uint32_t lba : {5u, 300u, 900u, 1439u}) {
                std::vector<std::uint8_t> s(DiskImage::kSectorSize);
                for (std::size_t i = 0; i < s.size(); ++i) {
                    s[i] = static_cast<std::uint8_t>((lba + i) & 0xFFu);
                }
                img.write_lba(lba, s.data());
            }
            img.flush();
        };
        run(path_a);
        run(path_b);
        expect(read_file(path_a) == read_file(path_b), "Determinism_TwoRunsByteIdentical");
        std::filesystem::remove(path_a);
        std::filesystem::remove(path_b);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_DiskImageWriteback_Unit cases passed\n";
    return 0;
}
