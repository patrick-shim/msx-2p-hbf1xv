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

// Suite: Machine_Hbf1xvDiskSavePersist_Integration (M36-S-c)
//
// Machine-scope end-to-end disk-save persistence: a sector written through the
// DiskDrive -> DiskImage chain is flushed to a host .dsk and read back
// byte-identical after a fresh reload (AC-c3), two identical write runs produce
// byte-identical output files (AC-c4), and the DEFAULT (no host path) leaves
// the pipeline in-memory-only with no host file created (AC-c2). flush() is
// write-only output and never re-reads the host file into emulation state.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

using sony_msx::devices::fdc::DiskImage;
using sony_msx::machine::Hbf1xvMachine;

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// Write `data` to CHS(track 0, side 0, `sector`) through the machine's drive.
void write_sector_via_drive(Hbf1xvMachine& m, std::uint8_t sector,
                            const std::vector<std::uint8_t>& data) {
    m.disk_drive().write_sector(sector, data.data());
}

}  // namespace

int main() {
    // --- AC-c3: write -> flush -> reload -> read-back byte-identical. ---
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "m36_disksave_roundtrip.dsk";
        std::filesystem::remove(path);

        Hbf1xvMachine machine;
        machine.cold_boot();
        // Bind the mounted image to a host file (the --disk-writable path does
        // exactly this) and write a distinctive sector.
        machine.disk_image().set_host_path(path);

        std::vector<std::uint8_t> written(DiskImage::kSectorSize);
        for (std::size_t i = 0; i < written.size(); ++i) {
            written[i] = static_cast<std::uint8_t>((i * 5u + 0x2Au) & 0xFFu);
        }
        write_sector_via_drive(machine, 5, written);
        expect(machine.disk_image().dirty(), "RoundTrip_DirtyAfterWrite");
        expect(machine.disk_image().flush(), "RoundTrip_FlushOk");
        expect(!machine.disk_image().dirty(), "RoundTrip_CleanAfterFlush");

        // Reload the host file into a fresh drive/image and read the sector back.
        Hbf1xvMachine reloaded;
        reloaded.cold_boot();
        reloaded.disk_image() = DiskImage(read_file(path));
        reloaded.disk_drive().attach_image(&reloaded.disk_image());
        std::vector<std::uint8_t> read_back(DiskImage::kSectorSize, 0);
        expect(reloaded.disk_drive().read_sector(5, read_back.data()), "RoundTrip_ReloadReadOk");
        expect(read_back == written, "RoundTrip_ReadBackByteIdentical");

        std::filesystem::remove(path);
    }

    // --- AC-c4: two identical scripted write runs -> byte-identical files. ---
    {
        const std::filesystem::path pa = std::filesystem::temp_directory_path() / "m36_disksave_a.dsk";
        const std::filesystem::path pb = std::filesystem::temp_directory_path() / "m36_disksave_b.dsk";
        std::filesystem::remove(pa);
        std::filesystem::remove(pb);

        auto run = [](const std::filesystem::path& p) {
            Hbf1xvMachine m;
            m.cold_boot();
            m.disk_image().set_host_path(p);
            for (const std::uint8_t sector : {std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{7}, std::uint8_t{9}}) {
                std::vector<std::uint8_t> s(DiskImage::kSectorSize);
                for (std::size_t i = 0; i < s.size(); ++i) {
                    s[i] = static_cast<std::uint8_t>((sector * 17u + i) & 0xFFu);
                }
                m.disk_drive().write_sector(sector, s.data());
            }
            m.disk_image().flush();
        };
        run(pa);
        run(pb);
        expect(read_file(pa) == read_file(pb), "Determinism_TwoRunsByteIdentical");
        std::filesystem::remove(pa);
        std::filesystem::remove(pb);
    }

    // --- AC-c2: default (no host path) = in-memory only, NO host file. ---
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "m36_disksave_default_none.dsk";
        std::filesystem::remove(path);

        Hbf1xvMachine machine;
        machine.cold_boot();
        std::vector<std::uint8_t> s(DiskImage::kSectorSize, 0x99);
        write_sector_via_drive(machine, 3, s);
        // In-memory write took effect...
        std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
        machine.disk_drive().read_sector(3, back.data());
        expect(back == s, "Default_InMemoryWriteVisible");
        // ...but flush is a no-op and no host file is created.
        expect(!machine.disk_image().flush(), "Default_FlushNoOp");
        expect(!std::filesystem::exists(path), "Default_NoHostFileCreated");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvDiskSavePersist_Integration cases passed\n";
    return 0;
}
