// Suite: Devices_ResetStatusRegister_Unit  (boot-logo fix, targeted defect cycle)
//
// #F4 reset-status latch, non-inverted variant (Sony_HB-F1XV.xml
// `<ResetStatusRegister>` `<inverted>false</inverted>`): power-up reads 0x00
// (bit 7 clear = cold boot -> the MSX2+ BIOS runs the animated logo), the
// BIOS then writes bit 7 set so warm restarts skip it. Write mask: only bits
// 7/5 storable, bit 5 additionally preserved from the previous value
// (references/openmsx-21.0/src/MSXResetStatusRegister.cc:28-35 -- behavior
// reference, never copied).

#include <cstdint>
#include <iostream>

#include "devices/chipset/reset_status_register.h"

namespace {

using sony_msx::devices::chipset::ResetStatusRegister;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Power-up: status reads 0x00 (cold-boot flag clear). ---
    {
        ResetStatusRegister reg;
        expect(reg.io_read(0xF4) == 0x00, "PowerOn_ReadsZero_ColdBootFlagClear");
        reg.power_on_reset();
        expect(reg.io_read(0xF4) == 0x00, "PowerOnReset_ReadsZero");
    }

    // --- Write mask: status = (status & 0x20) | (value & 0xA0). ---
    {
        ResetStatusRegister reg;
        reg.io_write(0xF4, 0xFF);  // BIOS-style "set everything" write
        expect(reg.io_read(0xF4) == 0xA0, "WriteFF_OnlyBits7And5Stored");

        reg.io_write(0xF4, 0x00);  // bit 5 of the stored value is preserved
        expect(reg.io_read(0xF4) == 0x20, "WriteZero_Bit5Preserved_Bit7Cleared");

        reg.io_write(0xF4, 0x80);  // the real BIOS post-logo write
        expect(reg.io_read(0xF4) == 0xA0, "Write80_Bit7Set_Bit5StillPreserved");
    }

    // --- The boot-logo protocol shape: cold 0x00 -> BIOS writes 0x80 ->
    //     reads 0x80 (warm restarts would skip the logo) -> next cold
    //     power-up clears it again. ---
    {
        ResetStatusRegister reg;
        expect(reg.io_read(0xF4) == 0x00, "BootProtocol_ColdReadsZero");
        reg.io_write(0xF4, 0x80);
        expect(reg.io_read(0xF4) == 0x80, "BootProtocol_BiosMarksBooted");
        reg.power_on_reset();
        expect(reg.io_read(0xF4) == 0x00, "BootProtocol_NextPowerCycleClearsAgain");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_ResetStatusRegister_Unit cases passed\n";
    return 0;
}
