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

// Suite: Diskutils_HexDump_Unit
//
// Deterministic hex-dump formatter oracle: the exact golden string for a
// full 16-byte line, the base_offset column, short-line padding (gutter stays
// aligned), determinism (two dumps byte-identical), and the --read --sector /
// --range slice math against a synthetic full image via the real CLI path.
// Links ONLY msx_diskutil.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "utils/cli.h"
#include "utils/hex_dump.h"
#include "utils/msx_disk_format.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::string dump(const std::vector<std::uint8_t>& bytes, std::uint64_t base) {
    std::ostringstream os;
    sony_msx::utils::write_hex_dump(os, std::span<const std::uint8_t>(bytes), base);
    return os.str();
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using sony_msx::utils::build_blank_image;
    using sony_msx::utils::DiskFormat;

    // --- Exact golden line for the 16-byte boot-sector prefix. ---
    const std::vector<std::uint8_t> boot16 = {
        0xEB, 0xFE, 0x90, 'S', 'O', 'N', 'Y', 'M', 'S', 'X', ' ', 0x00, 0x02, 0x02, 0x01, 0x00};
    const std::string golden =
        "00000000: eb fe 90 53 4f 4e 59 4d  53 58 20 00 02 02 01 00  |...SONYMSX .....|\n";
    const std::string got = dump(boot16, 0);
    expect(got == golden, "FullLine_ExactGoldenString");
    if (got != golden) {
        std::cerr << "  expected: [" << golden << "]\n  got     : [" << got << "]\n";
    }

    // --- base_offset column: same 16 bytes at base 0x200. ---
    const std::string got_off = dump(boot16, 0x200);
    expect(got_off.substr(0, 10) == "00000200: ", "BaseOffset_ColumnRendered");

    // --- Determinism: two dumps byte-identical. ---
    expect(dump(boot16, 0) == dump(boot16, 0), "TwoDumps_ByteIdentical");

    // --- Short-line padding: the '|' aligns with the full line's '|'. ---
    const std::vector<std::uint8_t> three = {0xF9, 0xFF, 0xFF};
    const std::string short_line = dump(three, 0);
    const std::size_t full_pipe = golden.find('|');
    const std::size_t short_pipe = short_line.find('|');
    expect(full_pipe != std::string::npos && short_pipe == full_pipe,
           "ShortLine_GutterAligned_WithFullLine");
    expect(short_line.substr(0, 18) == "00000000: f9 ff ff", "ShortLine_HexPrefix");
    // The gutter shows exactly the 3 present bytes (all non-printable -> '.').
    expect(short_line.find("|...|") != std::string::npos, "ShortLine_Gutter_ThreeDots");
    // No trailing content past the closing pipe + newline.
    expect(short_line.back() == '\n' && short_line[short_line.size() - 2] == '|',
           "ShortLine_EndsWithPipeNewline");

    // --- Empty span emits nothing. ---
    expect(dump({}, 0).empty(), "EmptySpan_NoOutput");

    // --- Slice math via the real --read CLI path on a synthetic full image. ---
    {
        const fs::path path = fs::temp_directory_path() / "m53_hexdump_slice.dsk";
        fs::remove(path);
        const std::vector<std::uint8_t> img = build_blank_image();
        {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char*>(img.data()),
                    static_cast<std::streamsize>(img.size()));
        }
        const std::string s = path.string();

        // --sector 1 == the first FAT copy (offset 0x200, first bytes F9 FF FF).
        {
            std::ostringstream out;
            std::ostringstream err;
            std::vector<const char*> argv = {"msx-disk", "--read", s.c_str(), "--sector", "1"};
            const auto a =
                sony_msx::utils::parse_args(static_cast<int>(argv.size()), argv.data());
            const int rc = sony_msx::utils::run(a, out, err);
            expect(rc == 0, "ReadSector1_Exit0");
            const std::string text = out.str();
            // First line: offset 0x200, F9 FF FF seed. 512 bytes => 32 lines.
            expect(text.rfind("00000200: f9 ff ff", 0) == 0, "ReadSector1_FirstLine_FatSeed");
            std::size_t lines = 0;
            for (const char c : text) {
                if (c == '\n') {
                    ++lines;
                }
            }
            expect(lines == 32, "ReadSector1_Emits_32Lines");
        }

        // --range 200-203 == bytes [0x200,0x203) == F9 FF FF (3 bytes).
        {
            std::ostringstream out;
            std::ostringstream err;
            std::vector<const char*> argv = {"msx-disk", "--read", s.c_str(), "--range", "200-203"};
            const auto a =
                sony_msx::utils::parse_args(static_cast<int>(argv.size()), argv.data());
            const int rc = sony_msx::utils::run(a, out, err);
            expect(rc == 0, "ReadRange_Exit0");
            const std::string text = out.str();
            expect(text.rfind("00000200: f9 ff ff", 0) == 0, "ReadRange_Offset_And_Bytes");
            expect(text.find("|...|") != std::string::npos, "ReadRange_Gutter");
            std::size_t lines = 0;
            for (const char c : text) {
                if (c == '\n') {
                    ++lines;
                }
            }
            expect(lines == 1, "ReadRange_SingleLine");
        }

        fs::remove(path);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
