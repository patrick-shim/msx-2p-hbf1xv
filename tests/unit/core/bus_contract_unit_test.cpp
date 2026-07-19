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

#include <cstdint>
#include <iostream>

#include "core/bus.h"

namespace {

bool expect_equal(const std::uint16_t lhs, const std::uint16_t rhs, const char* case_name) {
    if (lhs == rhs) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << ", expected " << rhs << ", got " << lhs << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Core_BusContract_Unit
    if (!expect_equal(sony_msx::core::normalize_bus_address(0x00001234u), 0x1234u,
            "NormalizeAddress_InRange_Unchanged")) {
        return 1;
    }

    if (!expect_equal(sony_msx::core::normalize_bus_address(0x0001FFFFu), 0xFFFFu,
            "NormalizeAddress_OutOfRange_WrapsTo16Bit")) {
        return 1;
    }

    if (!expect_equal(sony_msx::core::normalize_bus_address(0xABCDEF12u), 0xEF12u,
            "NormalizeAddress_HighBitsSet_DeterministicMask")) {
        return 1;
    }

    return 0;
}
