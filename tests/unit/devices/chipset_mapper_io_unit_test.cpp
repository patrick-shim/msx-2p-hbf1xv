#include <cstdint>
#include <iostream>

#include "devices/chipset/mapper_io.h"
#include "devices/chipset/s1985_engine.h"

// Suite: Devices_ChipsetMapperIo_Unit
//
// M11-S4: mapper ports #FC-#FF store a per-page segment; readback is the S1985
// `100xxxxx` pattern = 0x80 | (segment & 0x1F) (fact-sheet §4).

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    using sony_msx::devices::chipset::MapperIo;
    using sony_msx::devices::chipset::S1985Engine;

    MapperIo mapper;
    // The S1985 engine configures the readback base/mask (0x80 / 0x1F).
    S1985Engine engine;
    engine.configure_mapper(mapper);

    // Representative segments -> readback pattern.
    mapper.io_write(0xFC, 0x00);  // page 0
    if (!expect_true(mapper.io_read(0xFC) == 0x80, "Segment00_Readback_Is80")) {
        return 1;
    }
    mapper.io_write(0xFD, 0x1F);  // page 1
    if (!expect_true(mapper.io_read(0xFD) == 0x9F, "Segment1F_Readback_Is9F")) {
        return 1;
    }
    // 0x25 & 0x1F = 0x05 -> 0x85 (upper bits masked off = the `100xxxxx` pattern).
    mapper.io_write(0xFE, 0x25);  // page 2
    if (!expect_true(mapper.io_read(0xFE) == 0x85, "Segment25_Readback_Is85_UpperBitsMasked")) {
        return 1;
    }
    mapper.io_write(0xFF, 0xFF);  // page 3 -> 0xFF & 0x1F = 0x1F -> 0x9F
    if (!expect_true(mapper.io_read(0xFF) == 0x9F, "SegmentFF_Readback_Is9F")) {
        return 1;
    }

    // Segments are independent per page and stored verbatim.
    if (!expect_true(mapper.segment(0) == 0x00 && mapper.segment(1) == 0x1F &&
                         mapper.segment(2) == 0x25 && mapper.segment(3) == 0xFF,
                     "Segments_StoredPerPage_Independently")) {
        return 1;
    }

    return 0;
}
