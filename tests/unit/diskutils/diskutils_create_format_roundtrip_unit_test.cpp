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

// Suite: Diskutils_CreateFormatRoundtrip_Unit  (M53-S3, planner package §6.3)
//
// Round-trip determinism through the REAL --create / --format CLI code paths:
//   - --format over a 737,280-byte garbage-filled file yields the blank image.
//   - --create then --format on the same path is idempotent (bytes unchanged).
//   - two --create runs produce byte-identical files.
// Temp-file discipline mirrors hbf1xv_m36_disk_save_persist_integration_test.
// Links ONLY msx_diskutil.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "utils/cli.h"
#include "utils/msx_disk_format.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// Invoke the parse_args + run seam with a synthetic argv, discarding streams.
int invoke(std::vector<const char*> argv) {
    std::ostringstream out;
    std::ostringstream err;
    const sony_msx::utils::Args a =
        sony_msx::utils::parse_args(static_cast<int>(argv.size()), argv.data());
    return sony_msx::utils::run(a, out, err);
}

}  // namespace

int main() {
    using sony_msx::utils::build_blank_image;
    using sony_msx::utils::DiskFormat;
    namespace fs = std::filesystem;

    const std::vector<std::uint8_t> blank = build_blank_image();

    // --- --format over a garbage-filled 737,280-byte file yields the blank. ---
    {
        const fs::path path = fs::temp_directory_path() / "m53_format_garbage.dsk";
        fs::remove(path);

        std::vector<std::uint8_t> garbage(DiskFormat::kImageBytes);
        for (std::size_t i = 0; i < garbage.size(); ++i) {
            garbage[i] = static_cast<std::uint8_t>((i * 31u + 0xC3u) & 0xFFu);
        }
        {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char*>(garbage.data()),
                    static_cast<std::streamsize>(garbage.size()));
        }

        const std::string s = path.string();
        // A correctly-sized target formats without --force (exit 0).
        expect(invoke({"msx-disk", "--format", s.c_str()}) == 0, "Format_CorrectSize_Exit0");
        expect(read_file(path) == blank, "Format_Garbage_YieldsBlank");

        fs::remove(path);
    }

    // --- --create then --format on the same path is idempotent. ---
    {
        const fs::path path = fs::temp_directory_path() / "m53_create_then_format.dsk";
        fs::remove(path);

        const std::string s = path.string();
        expect(invoke({"msx-disk", "--create", s.c_str()}) == 0, "Create_Fresh_Exit0");
        const std::vector<std::uint8_t> after_create = read_file(path);
        expect(after_create == blank, "Create_YieldsBlank");
        expect(after_create.size() == 737280, "Create_Size_737280");

        expect(invoke({"msx-disk", "--format", s.c_str()}) == 0, "Format_AfterCreate_Exit0");
        const std::vector<std::uint8_t> after_format = read_file(path);
        expect(after_format == after_create, "CreateThenFormat_Idempotent");

        fs::remove(path);
    }

    // --- Two --create runs produce byte-identical files (determinism). ---
    {
        const fs::path p1 = fs::temp_directory_path() / "m53_create_a.dsk";
        const fs::path p2 = fs::temp_directory_path() / "m53_create_b.dsk";
        fs::remove(p1);
        fs::remove(p2);
        const std::string s1 = p1.string();
        const std::string s2 = p2.string();
        expect(invoke({"msx-disk", "--create", s1.c_str()}) == 0, "CreateA_Exit0");
        expect(invoke({"msx-disk", "--create", s2.c_str()}) == 0, "CreateB_Exit0");
        expect(read_file(p1) == read_file(p2), "TwoCreates_ByteIdentical");
        fs::remove(p1);
        fs::remove(p2);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
