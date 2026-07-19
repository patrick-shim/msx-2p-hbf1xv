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

// Suite: Diskutils_CliValidation_Unit  (M53-S4, planner package §6.5)
//
// Exit-code contract over parse_args + run (NO subprocess): usage errors (1),
// I/O errors (2), safety refusals (3), and successes (0), including the --force
// overrides. Links ONLY msx_diskutil. Temp files via std::filesystem.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "utils/cli.h"
#include "utils/msx_disk_format.h"

namespace {

int g_failures = 0;

void expect_eq(const int actual, const int expected, const char* case_name) {
    if (actual != expected) {
        std::cerr << "Case failed: " << case_name << " (got exit " << actual << ", expected "
                  << expected << ")\n";
        ++g_failures;
    }
}

// Parse + run a synthetic argv into discarded streams; return the exit code.
int invoke(std::vector<const char*> argv) {
    std::ostringstream out;
    std::ostringstream err;
    const sony_msx::utils::Args a =
        sony_msx::utils::parse_args(static_cast<int>(argv.size()), argv.data());
    return sony_msx::utils::run(a, out, err);
}

void write_sized_file(const std::filesystem::path& p, std::size_t size) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    const std::vector<char> buf(size, 0x00);
    f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
}

}  // namespace

int main() {
    using sony_msx::utils::DiskFormat;
    namespace fs = std::filesystem;

    // --- Usage errors (exit 1). ---
    expect_eq(invoke({"msx-disk"}), 1, "NoMode_Exit1");
    expect_eq(invoke({"msx-disk", "--create", "--read", "x.dsk"}), 1, "DuplicateMode_Exit1");
    expect_eq(invoke({"msx-disk", "--create"}), 1, "CreateWithoutPath_Exit1");
    expect_eq(invoke({"msx-disk", "--frobnicate", "x.dsk"}), 1, "UnknownOption_Exit1");

    // --- --help (exit 0). ---
    expect_eq(invoke({"msx-disk", "--help"}), 0, "Help_Exit0");
    expect_eq(invoke({"msx-disk", "-h"}), 0, "HelpShort_Exit0");

    // --- --create safety refusal + --force override. ---
    {
        const fs::path existing = fs::temp_directory_path() / "m53_cli_existing.dsk";
        write_sized_file(existing, 10);  // any content
        const std::string s = existing.string();
        expect_eq(invoke({"msx-disk", "--create", s.c_str()}), 3, "CreateExisting_NoForce_Exit3");
        expect_eq(invoke({"msx-disk", "--create", s.c_str(), "--force"}), 0,
                  "CreateExisting_Force_Exit0");
        // --force wrote the full blank image over the small file.
        expect_eq(fs::file_size(existing) == DiskFormat::kImageBytes ? 0 : 1, 0,
                  "CreateForce_Wrote737280");
        fs::remove(existing);
    }

    // --- --format: missing target (exit 2), wrong-size refusal (exit 3) + --force. ---
    {
        const fs::path missing = fs::temp_directory_path() / "m53_cli_missing_never.dsk";
        fs::remove(missing);
        const std::string sm = missing.string();
        expect_eq(invoke({"msx-disk", "--format", sm.c_str()}), 2, "FormatMissing_Exit2");

        const fs::path wrong = fs::temp_directory_path() / "m53_cli_wrongsize.dsk";
        write_sized_file(wrong, 4096);  // not 737280
        const std::string sw = wrong.string();
        expect_eq(invoke({"msx-disk", "--format", sw.c_str()}), 3, "FormatWrongSize_NoForce_Exit3");
        expect_eq(invoke({"msx-disk", "--format", sw.c_str(), "--force"}), 0,
                  "FormatWrongSize_Force_Exit0");
        expect_eq(fs::file_size(wrong) == DiskFormat::kImageBytes ? 0 : 1, 0,
                  "FormatForce_Normalized737280");
        fs::remove(wrong);
    }

    // --- --read: missing file (exit 2), bad sector/range (exit 1). ---
    {
        const fs::path missing = fs::temp_directory_path() / "m53_cli_read_missing.dsk";
        fs::remove(missing);
        const std::string sm = missing.string();
        expect_eq(invoke({"msx-disk", "--read", sm.c_str()}), 2, "ReadMissing_Exit2");

        const fs::path present = fs::temp_directory_path() / "m53_cli_read_present.dsk";
        write_sized_file(present, DiskFormat::kImageBytes);
        const std::string sp = present.string();
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--sector", "9999"}), 1,
                  "ReadSectorOutOfRange_Exit1");
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--sector", "abc"}), 1,
                  "ReadSectorNonNumeric_Exit1");
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--range", "10-8"}), 1,
                  "ReadRangeInverted_Exit1");
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--range", "nothex"}), 1,
                  "ReadRangeMalformed_Exit1");
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--sector", "0", "--range", "0-10"}), 1,
                  "ReadSectorAndRange_MutuallyExclusive_Exit1");
        // A valid whole-image read succeeds.
        expect_eq(invoke({"msx-disk", "--read", sp.c_str(), "--sector", "0"}), 0,
                  "ReadSector0_Exit0");
        fs::remove(present);
    }

    // --- --sector/--range rejected on non-read modes (exit 1). ---
    {
        const fs::path p = fs::temp_directory_path() / "m53_cli_sector_on_create.dsk";
        fs::remove(p);
        const std::string s = p.string();
        expect_eq(invoke({"msx-disk", "--create", s.c_str(), "--sector", "0"}), 1,
                  "SectorOnCreate_Exit1");
        fs::remove(p);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
