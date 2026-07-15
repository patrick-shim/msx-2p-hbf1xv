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

// Thin entry point for the standalone host-side msx-disk utility (M53,
// DEC-0080). Links ONLY msx_diskutil -- no sony_msx_core, no emulator header --
// so the shipped binary is provably isolated from the emulator (one-way build
// isolation). All logic lives in the unit-testable cli/run seam.

#include <iostream>

#include "diskutils/cli.h"

int main(int argc, char** argv) {
    const sony_msx::diskutils::Args args = sony_msx::diskutils::parse_args(argc, argv);
    return sony_msx::diskutils::run(args, std::cout, std::cerr);
}
