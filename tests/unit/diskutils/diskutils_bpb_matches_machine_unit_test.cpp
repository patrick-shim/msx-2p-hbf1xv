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

// Suite: Diskutils_BpbMatchesMachine_Unit
//
// Belt-and-suspenders machine-parity proof: the ONLY test that
// links BOTH msx_diskutil AND sony_msx_core. Asserts the tool's boot-sector/BPB
// region (offsets 0..29 + {510,511}) and FAT-seed bytes (offsets 512..514 and
// 2048..2050) are byte-identical to the emulator's OWN DiskImage::synthesize()
// expectation -- WITHOUT the shipped msx-disk binary ever linking the core.
// (synthesize() fills the data area with a NON-zero test pattern, so only the
// boot/BPB/FAT-seed regions are compared, never the whole image.)
//
// OPTIONAL geometry cross-check vs the real disks/msxdos23.dsk: skip-when-
// absent (untracked asset) -- pattern from
// disk_boot_investigation_system_test.cpp:150-156.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "utils/msx_disk_format.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    using sony_msx::devices::fdc::DiskImage;
    using sony_msx::utils::build_blank_image;

    const std::vector<std::uint8_t> tool = build_blank_image();
    const std::vector<std::uint8_t> mach = DiskImage::synthesize();

    expect(tool.size() == mach.size() && tool.size() == 737280, "SameSize_737280");

    // --- Boot-sector / BPB region [0..29] byte-identical to the machine. ---
    bool bpb_match = true;
    for (std::size_t i = 0; i <= 29; ++i) {
        if (tool[i] != mach[i]) {
            bpb_match = false;
            std::cerr << "  BPB byte mismatch at 0x" << std::hex << i << ": tool=0x"
                      << static_cast<int>(tool[i]) << " mach=0x" << static_cast<int>(mach[i])
                      << std::dec << "\n";
        }
    }
    expect(bpb_match, "BootBpb_0_29_MatchesMachineSynthesize");

    // --- Boot signature bytes match. ---
    expect(tool[510] == mach[510] && tool[511] == mach[511], "BootSignature_MatchesMachine");

    // --- FAT-seed bytes at both FAT copies (offsets 512..514, 2048..2050). ---
    bool fat_match = true;
    for (const std::size_t base : {std::size_t{512}, std::size_t{2048}}) {
        for (std::size_t j = 0; j < 3; ++j) {
            if (tool[base + j] != mach[base + j]) {
                fat_match = false;
            }
        }
    }
    expect(fat_match, "FatSeed_512_2048_MatchesMachine");

    // The tool's data area is ZERO where synthesize()'s is a test pattern -- this
    // divergence is INTENTIONAL (an empty filesystem vs an FDC read/write test
    // medium). Confirm they genuinely differ there so this test is
    // not silently comparing identical images.
    expect(tool != mach, "ToolAndMachine_DifferInDataArea_AsExpected");

    // --- OPTIONAL: geometry cross-check vs the real MSX-DOS system disk. ---
#ifdef SONY_MSX_DISKS_DIR
    const std::vector<std::uint8_t> dos = read_file(std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk");
    if (dos.empty()) {
        std::cout << "SKIP: disks/msxdos23.dsk absent under " << SONY_MSX_DISKS_DIR
                  << " -- skip-when-absent geometry cross-check (untracked asset, A2)\n";
    } else {
        expect(dos.size() == 737280, "Msxdos23_737280Bytes");
        // Geometry subset only (bytes/sector, media, total sectors, sectors/FAT,
        // sectors/track, heads). OEM string, boot-code region, DOS2 volume-id and
        // the 0x1FE/0x1FF boot-signature LEGITIMATELY differ on a DOS system disk
        // and are deliberately NOT compared.
        const std::size_t geom[] = {0x0B, 0x0C, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B};
        bool geom_match = true;
        for (const std::size_t off : geom) {
            if (dos[off] != tool[off]) {
                geom_match = false;
                std::cerr << "  geometry mismatch at 0x" << std::hex << off << ": dos=0x"
                          << static_cast<int>(dos[off]) << " tool=0x" << static_cast<int>(tool[off])
                          << std::dec << "\n";
            }
        }
        expect(geom_match, "Msxdos23_GeometrySubset_MatchesTool");
    }
#else
    std::cout << "NOTE: SONY_MSX_DISKS_DIR not defined -- msxdos23 geometry cross-check omitted.\n";
#endif

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
