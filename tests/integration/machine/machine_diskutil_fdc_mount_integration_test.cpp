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

// Suite: Machine_DiskutilFdcMount_Integration  (M53-S5, planner package §6.6)
//
// AC-4 "mount + directory-readable" cross-check (NOT a DOS/BASIC boot -- the M28
// finding, planner §6 R6): the emulator's OWN FDC/DiskDrive headlessly mounts an
// image produced by the standalone tool's build_blank_image() (and by the real
// --format code path), reads LBA 0 back THROUGH the real FDC sector datapath
// (the M16 Restore + Read-Sector CPU probe), and validates the FAT12 structure:
//   - LBA 0 boot sector streams back byte-identical (EB FE 90 ... F9 ... 55 AA).
//   - LBA 1 (FAT copy #1) seed F9 FF FF then zero.
//   - LBA 7 (root directory) all zero => an empty, directory-readable filesystem.
// Determinism: the streamed bytes are identical across two runs.
//
// Links BOTH sony_msx_core AND msx_diskutil (a test is allowed to link both;
// the shipped msx-disk binary never links the core). Mount pattern:
// hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp:104-111; CPU probe:
// hbf1xv_m16_fdc_integration_test.cpp:94-167; temp files:
// hbf1xv_m36_disk_save_persist_integration_test.cpp:46-65.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "utils/cli.h"
#include "utils/msx_disk_format.h"
#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// --- M16 self-contained Z80 assembler + Restore + Read-Sector(LBA0) probe. ---
class Prog {
public:
    void emit(std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            bytes_.push_back(b);
        }
    }
    [[nodiscard]] std::size_t here() const { return bytes_.size(); }
    void emit_jr_back(const std::uint8_t opcode, const std::size_t target) {
        bytes_.push_back(opcode);
        const std::size_t next_pc = bytes_.size() + 1;
        const int disp = static_cast<int>(target) - static_cast<int>(next_pc);
        bytes_.push_back(static_cast<std::uint8_t>(disp & 0xFF));
    }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

constexpr std::uint16_t kBase = 0xC000;
constexpr std::uint16_t kBuffer = 0xC200;

std::vector<std::uint8_t> build_restore_read_sector_probe() {
    Prog p;
    p.emit({0x3E, 0x08});
    p.emit({0x32, 0xFF, 0xFF});  // page1 -> slot3-2 (FDC), page3 stays sub0
    p.emit({0x3E, 0x00});
    p.emit({0x32, 0xFC, 0x7F});  // side = 0
    p.emit({0x3E, 0x80});
    p.emit({0x32, 0xFD, 0x7F});  // drive-select=A, motor on
    p.emit({0x3E, 0x00});
    p.emit({0x32, 0xF8, 0x7F});  // Restore (Type I)
    const std::size_t restore_wait = p.here();
    p.emit({0x3A, 0xF8, 0x7F});
    p.emit({0xE6, 0x01});
    p.emit_jr_back(0x20, restore_wait);
    p.emit({0x3E, 0x00});
    p.emit({0x32, 0xF9, 0x7F});  // TR = 0
    p.emit({0x3E, 0x01});
    p.emit({0x32, 0xFA, 0x7F});  // SR = 1
    p.emit({0x3E, 0x80});
    p.emit({0x32, 0xF8, 0x7F});  // Read Sector (Type II)
    p.emit({0x21, static_cast<std::uint8_t>(kBuffer & 0xFF),
            static_cast<std::uint8_t>(kBuffer >> 8)});
    p.emit({0x01, 0x00, 0x02});  // 512
    const std::size_t read_wait = p.here();
    p.emit({0x3A, 0xFF, 0x7F});
    p.emit({0xE6, 0x80});
    p.emit_jr_back(0x20, read_wait);
    p.emit({0x3A, 0xFB, 0x7F});
    p.emit({0x77});
    p.emit({0x23});
    p.emit({0x0B});
    p.emit({0x78});
    p.emit({0xB1});
    p.emit_jr_back(0x20, read_wait);
    p.emit({0x76});  // HALT
    return p.bytes();
}

struct MountResult {
    bool halted = false;
    std::array<std::uint8_t, 512> lba0_streamed{};  // read through the FDC datapath
    std::array<std::uint8_t, 512> lba1{};           // FAT copy #1 (direct image read oracle)
    std::array<std::uint8_t, 512> lba7{};           // root directory
};

// Mount `disk_bytes` in the machine's OWN FDC, CPU-read LBA 0 through the real
// sector datapath, and read back LBA 1/7 for the FAT12-structure oracle.
MountResult mount_and_read(const std::vector<std::uint8_t>& disk_bytes) {
    using sony_msx::machine::Hbf1xvMachine;

    Hbf1xvMachine machine;
    machine.cold_boot();
    machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk_bytes);
    machine.disk_drive().attach_image(&machine.disk_image());
    machine.map_flat_ram();

    const std::vector<std::uint8_t> program = build_restore_read_sector_probe();
    machine.load_memory(kBase, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = kBase;

    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 2'000'000) {
        machine.step_cpu_instruction();
        ++guard;
    }

    MountResult r;
    r.halted = machine.cpu().state().halted();
    for (std::size_t i = 0; i < 512; ++i) {
        r.lba0_streamed[i] = machine.read_memory(static_cast<std::uint16_t>(kBuffer + i));
    }
    // LBA 1 = CHS(track 0, side 0, sector 2); LBA 7 = CHS(0, 0, 8).
    machine.disk_image().read_chs(0, 0, 2, r.lba1.data());
    machine.disk_image().read_chs(0, 0, 8, r.lba7.data());
    return r;
}

void validate(const MountResult& r, const std::vector<std::uint8_t>& disk_bytes, const char* tag) {
    std::string t(tag);
    expect(r.halted, (t + "_ProbeReachesHalt").c_str());

    // LBA 0 streamed through the FDC == the mounted image's boot sector.
    bool lba0_ok = true;
    for (std::size_t i = 0; i < 512; ++i) {
        if (r.lba0_streamed[i] != disk_bytes[i]) {
            lba0_ok = false;
            break;
        }
    }
    expect(lba0_ok, (t + "_Lba0_StreamedThroughFdc_MatchesImage").c_str());
    expect(r.lba0_streamed[0] == 0xEB && r.lba0_streamed[1] == 0xFE && r.lba0_streamed[2] == 0x90,
           (t + "_Lba0_JmpEBFE90").c_str());
    expect(r.lba0_streamed[21] == 0xF9, (t + "_Lba0_Media_F9").c_str());
    expect(r.lba0_streamed[510] == 0x55 && r.lba0_streamed[511] == 0xAA,
           (t + "_Lba0_BootSig_55AA").c_str());

    // LBA 1: FAT copy #1 seed F9 FF FF then all zero.
    expect(r.lba1[0] == 0xF9 && r.lba1[1] == 0xFF && r.lba1[2] == 0xFF,
           (t + "_Lba1_FatSeed_F9FFFF").c_str());
    bool fat_rest_zero = true;
    for (std::size_t i = 3; i < 512; ++i) {
        if (r.lba1[i] != 0x00) {
            fat_rest_zero = false;
            break;
        }
    }
    expect(fat_rest_zero, (t + "_Lba1_FatRemainder_Zero").c_str());

    // LBA 7: root directory all zero => empty, directory-readable filesystem.
    bool root_zero = true;
    for (std::size_t i = 0; i < 512; ++i) {
        if (r.lba7[i] != 0x00) {
            root_zero = false;
            break;
        }
    }
    expect(root_zero, (t + "_Lba7_RootDirectory_Empty").c_str());
}

}  // namespace

int main() {
    using sony_msx::utils::build_blank_image;
    using sony_msx::utils::DiskFormat;
    namespace fs = std::filesystem;

    // --- A tool-CREATED (in-memory build_blank_image) image mounts + reads back. ---
    const std::vector<std::uint8_t> created = build_blank_image();
    const MountResult created_r = mount_and_read(created);
    validate(created_r, created, "Created");

    // --- A tool-FORMATTED image (real --format code path over a temp file). ---
    {
        const fs::path path = fs::temp_directory_path() / "m53_fdc_mount_format.dsk";
        fs::remove(path);
        // Pre-fill with a 737,280-byte non-zero pattern so --format really rewrites.
        {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            const std::vector<char> junk(DiskFormat::kImageBytes, static_cast<char>(0xA5));
            f.write(junk.data(), static_cast<std::streamsize>(junk.size()));
        }
        std::ostringstream out;
        std::ostringstream err;
        const std::string s = path.string();
        std::vector<const char*> argv = {"msx-disk", "--format", s.c_str()};
        const auto a = sony_msx::utils::parse_args(static_cast<int>(argv.size()), argv.data());
        expect(sony_msx::utils::run(a, out, err) == 0, "FormatPath_Exit0");

        std::ifstream in(path, std::ios::binary);
        const std::vector<std::uint8_t> formatted((std::istreambuf_iterator<char>(in)),
                                                  std::istreambuf_iterator<char>());
        in.close();
        expect(formatted.size() == 737280, "Formatted_Size_737280");
        expect(formatted == created, "Formatted_EqualsCreated");

        const MountResult formatted_r = mount_and_read(formatted);
        validate(formatted_r, formatted, "Formatted");
        fs::remove(path);
    }

    // --- Determinism: mount + FDC read twice, streamed LBA 0 byte-identical. ---
    {
        const MountResult a = mount_and_read(created);
        const MountResult b = mount_and_read(created);
        expect(a.lba0_streamed == b.lba0_streamed, "Mount_TwoRuns_Lba0_ByteIdentical");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
