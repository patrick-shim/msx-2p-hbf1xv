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

#include "devices/chipset/s1985_engine.h"
#include "devices/chipset/switched_io.h"

// Suite: Devices_ChipsetBackupRam_Unit
//
// M11-S4: the S1985 16-byte backup RAM on switched-I/O device ID 0xFE, reached
// through the #40-#4F controller. Register semantics per fact-sheet §6 /
// openMSX MSXS1985.cc:44-91.

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
    using sony_msx::devices::chipset::S1985Engine;
    using sony_msx::devices::chipset::SwitchedIoController;

    S1985Engine engine;
    engine.reset();
    SwitchedIoController controller;
    controller.attach(&engine);
    controller.reset();

    // Before selecting ID 0xFE, the controller's default selected=0 has no device
    // (engine is at ID 0xFE), so reads are open-bus.
    if (!expect_true(controller.io_read(0x42) == 0xFF, "BeforeSelect_Read_OpenBus")) {
        return 1;
    }

    // Select device ID 0xFE via #40.
    controller.io_write(0x40, 0xFE);
    if (!expect_true(controller.selected() == 0xFE, "WriteBase40_SelectsDeviceId")) {
        return 1;
    }

    // index 0 read = ~ID = 0x01 (the S1985/S3527 discriminator).
    if (!expect_true(controller.io_read(0x40) == 0x01, "Index0_Read_IsComplementOfId")) {
        return 1;
    }

    // Backup RAM round-trip: index1 sets address, index2 read/writes sram[address].
    controller.io_write(0x41, 0x05);  // address = 0x05
    controller.io_write(0x42, 0xA3);  // sram[5] = 0xA3
    controller.io_write(0x41, 0x0A);  // address = 0x0A
    controller.io_write(0x42, 0x5C);  // sram[10] = 0x5C
    controller.io_write(0x41, 0x05);  // address = 0x05
    if (!expect_true(controller.io_read(0x42) == 0xA3, "Index2_ReadsBackWrittenSramByte")) {
        return 1;
    }
    // Address masks to 0x0F: writing 0x1A selects index 0x0A.
    controller.io_write(0x41, 0x1A);
    if (!expect_true(controller.io_read(0x42) == 0x5C, "Index1_AddressMaskedToLowNibble")) {
        return 1;
    }

    // Colour + pattern registers. index6 shifts color2<-color1, color1<-value.
    controller.io_write(0x46, 0x11);  // color1 = 0x11 (color2 = old color1 = 0)
    controller.io_write(0x46, 0x22);  // color2 = 0x11, color1 = 0x22
    // index7 write sets pattern; index7 read returns color2 (if bit7) else color1
    // and THEN rotates pattern left.
    controller.io_write(0x47, 0x80);  // pattern = 0x80 (bit7 set)
    if (!expect_true(controller.io_read(0x47) == 0x11, "Index7_Bit7Set_ReturnsColor2")) {
        return 1;
    }
    // After the read, pattern rotated: 0x80 -> 0x01 (bit7 clear) -> next read color1.
    if (!expect_true(controller.io_read(0x47) == 0x22, "Index7_AfterRotate_ReturnsColor1")) {
        return 1;
    }

    // Rotation is a left-rotate: set 0x01, read -> color1, pattern -> 0x02.
    controller.io_write(0x47, 0x01);
    (void)controller.io_read(0x47);  // pattern 0x01 -> 0x02
    // Set pattern to 0x40 then read three times to observe wrap 0x40->0x80(color2)->...
    controller.io_write(0x47, 0x40);
    if (!expect_true(controller.io_read(0x47) == 0x22, "Index7_Pattern40_Bit7Clear_Color1")) {
        return 1;  // 0x40 bit7 clear -> color1 (0x22); rotate -> 0x80
    }
    if (!expect_true(controller.io_read(0x47) == 0x11, "Index7_AfterRotateTo80_Color2")) {
        return 1;  // now 0x80 bit7 set -> color2 (0x11)
    }

    // Other indices read 0xFF.
    if (!expect_true(controller.io_read(0x43) == 0xFF && controller.io_read(0x4F) == 0xFF,
                     "OtherIndices_Read_ReturnFF")) {
        return 1;
    }

    // reset() clears address/pattern/colours and the volatile store.
    controller.io_write(0x41, 0x05);
    controller.io_write(0x42, 0x99);
    engine.reset();
    controller.io_write(0x41, 0x05);
    if (!expect_true(controller.io_read(0x42) == 0x00, "Reset_ClearsBackupRam")) {
        return 1;
    }

    return 0;
}
