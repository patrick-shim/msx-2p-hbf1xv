// Suite: Devices_V9958Registers_Unit
//
// Deterministic unit coverage for the #99 two-write register/address protocol,
// the port-1 write abort on any #98/#99 read, the #9B indirect-register data
// path with auto-increment-inhibit (AII), and the R#14 A16-A14 contribution
// (M14-S2). Grounded in VDP.cc:663-731 / 998 and fact-sheet §4.

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"

namespace {

using sony_msx::devices::video::V9958Vdp;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void set_register(V9958Vdp& vdp, std::uint8_t reg, std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

}  // namespace

int main() {
    // --- Two-write register set: low byte latched, then 0x80|reg writes it. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 7, 0x5A);
        expect(vdp.control_register(7) == 0x5A, "TwoWrite_RegisterSet_StoresLatch");
        set_register(vdp, 19, 0x9C);  // R#19 interrupt line
        expect(vdp.control_register(19) == 0x9C, "TwoWrite_RegisterSet_AltRegister");
    }

    // --- Address setup direction bit: write-setup does NOT prefetch (pointer
    //     stays at the set value); read-setup prefetches (pointer advances). ---
    {
        V9958Vdp vdp;
        // Write direction (bit6=1): pointer := 0x0800, no read-ahead advance.
        vdp.io_write(0x99, 0x00);
        vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | 0x08));  // 0x0800, write
        expect(vdp.vram_pointer() == 0x0800, "AddressSetup_Write_NoPrefetchAdvance");

        // Read direction (bit6=0): pointer := 0x0800, then read-ahead advances +1.
        vdp.io_write(0x99, 0x00);
        vdp.io_write(0x99, 0x08);  // 0x0800, read
        expect(vdp.vram_pointer() == 0x0801, "AddressSetup_Read_PrefetchAdvancesPointer");
    }

    // --- Port-1 write abort: any #98/#99 read clears the half-written latch,
    //     realigning the next #99 write as a fresh first byte (VDP.cc:998). ---
    {
        V9958Vdp vdp;
        vdp.io_write(0x99, 0x3C);   // first byte latched (data=0x3C), stored=true
        (void)vdp.io_read(0x99);    // status read: ABORTS the port-1 write
        vdp.io_write(0x99, 0x81);   // stored was cleared -> this is a FRESH first
                                    // write (data=0x81), NOT a register write
        vdp.io_write(0x99, 0x82);   // second byte: register write R#2 := data(0x81)
        expect(vdp.control_register(2) == 0x81, "Port1Read_AbortsHalfWrite_Realigns");
    }

    // --- #9B indirect sweep: R#17 auto-increments (AII bit7 clear). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 17, 0x02);  // point at R#2, AII off
        vdp.io_write(0x9B, 0x11);     // R#2 = 0x11, R#17 -> 3
        vdp.io_write(0x9B, 0x22);     // R#3 = 0x22, R#17 -> 4
        vdp.io_write(0x9B, 0x33);     // R#4 = 0x33, R#17 -> 5
        expect(vdp.control_register(2) == 0x11 && vdp.control_register(3) == 0x22 &&
                   vdp.control_register(4) == 0x33,
               "Indirect_AutoIncrement_SweepsRegisters");
        expect(vdp.control_register(17) == 0x05, "Indirect_AutoIncrement_AdvancesR17");
    }

    // --- #9B indirect with AII set (R#17 bit7): pointer stays put. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 17, 0x87);  // point at R#7, AII ON (bit7 set)
        vdp.io_write(0x9B, 0x44);     // R#7 = 0x44, R#17 unchanged
        vdp.io_write(0x9B, 0x55);     // R#7 = 0x55 (overwrite), R#17 unchanged
        expect(vdp.control_register(7) == 0x55, "Indirect_AII_OverwritesSameRegister");
        expect(vdp.control_register(17) == 0x87, "Indirect_AII_PointerHeld");
    }

    // --- R#14 supplies A16-A14 of the effective VRAM address. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 14, 0x03);  // A16..A14 = 3
        vdp.io_write(0x99, 0x00);
        vdp.io_write(0x99, 0x40);     // write address 0x0000 -> effective 0xC000
        expect(vdp.vram_address() == 0xC000u, "R14_SuppliesHighAddressBits");
        vdp.io_write(0x98, 0x66);
        expect(vdp.vram().read(0xC000) == 0x66, "R14_EffectiveAddress_WritesHighBank");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
