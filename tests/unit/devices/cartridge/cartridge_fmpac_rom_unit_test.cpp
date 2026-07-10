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

// Suite: Devices_CartridgeFmPacRom_Unit (M36-S-d, DEC-0050)
//
// 100%-path deterministic coverage of the FM-PAC peripheral cartridge mapper
// grounded verbatim-behavior in references/openmsx-21.0/src/sound/MSXFmPac.cc:
// page-1 window + open bus elsewhere; 4-bank ROM read; 0x5FFE/0x5FFF magic
// unlock (both bytes, re-checked each write); SRAM R/W only while unlocked;
// 0x7FF6 enable (bit0 I/O-enable, bit4 force-reset); 0x7FF7 bank; 0x7FF4/0x7FF5
// memory-mapped OPLL ports; is_valid_image_size.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_fmpac_rom.h"

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

using sony_msx::devices::cartridge::CartridgeFmPacRom;

// A deterministic 64 KB (4-bank) ROM: byte at (bank*0x4000 + 0x0100) == 0xB0+bank
// so bank switching is observable, plus an FM-PAC-shaped header for realism.
std::vector<std::uint8_t> make_rom_64k() {
    std::vector<std::uint8_t> rom(4 * CartridgeFmPacRom::kBankSize, 0x00);
    rom[0] = 'A';
    rom[1] = 'B';
    const char marker[] = "PAC2OPLL";
    for (int i = 0; i < 8; ++i) {
        rom[0x18 + i] = static_cast<std::uint8_t>(marker[i]);
    }
    for (std::size_t bank = 0; bank < 4; ++bank) {
        rom[bank * CartridgeFmPacRom::kBankSize + 0x0100] = static_cast<std::uint8_t>(0xB0 + bank);
    }
    return rom;
}

// Fresh reset cart (CartridgeSlot::load() semantics: construct then reset()).
CartridgeFmPacRom make_cart() {
    CartridgeFmPacRom cart(make_rom_64k());
    cart.reset();
    return cart;
}

void unlock(CartridgeFmPacRom& cart) {
    cart.mem_write(0x5FFE, 0x4D);
    cart.mem_write(0x5FFF, 0x69);
}

}  // namespace

int main() {
    // --- is_valid_image_size ---
    expect(CartridgeFmPacRom::is_valid_image_size(0x4000), "ValidSize_16KB");
    expect(CartridgeFmPacRom::is_valid_image_size(0x10000), "ValidSize_64KB");
    expect(CartridgeFmPacRom::is_valid_image_size(0x8000), "ValidSize_32KB");
    expect(!CartridgeFmPacRom::is_valid_image_size(0), "InvalidSize_Zero");
    expect(!CartridgeFmPacRom::is_valid_image_size(0x2000), "InvalidSize_8KB");
    expect(!CartridgeFmPacRom::is_valid_image_size(0x4001), "InvalidSize_NotBankMultiple");
    expect(!CartridgeFmPacRom::is_valid_image_size(0x14000), "InvalidSize_TooBig80KB");

    // --- Power-on/reset default: locked, bank 0, enable 0, ROM visible. ---
    {
        CartridgeFmPacRom cart = make_cart();
        expect(!cart.sram_enabled(), "Reset_Locked");
        expect(cart.bank() == 0, "Reset_Bank0");
        expect(cart.enable_register() == 0, "Reset_Enable0");
        expect(cart.mem_read(0x4100) == 0xB0, "Reset_RomBank0Visible");
        // 0x7FF6/0x7FF7 register readback.
        expect(cart.mem_read(0x7FF6) == 0x00, "Reset_ReadEnableReg");
        expect(cart.mem_read(0x7FF7) == 0x00, "Reset_ReadBankReg");
    }

    // --- Open bus on pages 0/2/3 (page-1 device only). ---
    {
        CartridgeFmPacRom cart = make_cart();
        expect(cart.mem_read(0x0000) == 0xFF, "OpenBus_Page0");
        expect(cart.mem_read(0x2000) == 0xFF, "OpenBus_Page0b");
        expect(cart.mem_read(0x8000) == 0xFF, "OpenBus_Page2");
        expect(cart.mem_read(0xC000) == 0xFF, "OpenBus_Page3");
        // Writes elsewhere are ignored (no crash, no state change).
        cart.mem_write(0x0000, 0x11);
        cart.mem_write(0x9000, 0x22);
        expect(cart.mem_read(0x4100) == 0xB0, "OpenBus_WritesElsewhereIgnored");
    }

    // --- Magic unlock: both bytes required, re-checked each write. ---
    {
        CartridgeFmPacRom cart = make_cart();
        cart.mem_write(0x5FFE, 0x4D);
        expect(!cart.sram_enabled(), "Unlock_OnlyFirstByte_StillLocked");
        cart.mem_write(0x5FFF, 0x69);
        expect(cart.sram_enabled(), "Unlock_BothBytes_Unlocked");
        // While unlocked, magic addresses read back the constants.
        expect(cart.mem_read(0x5FFE) == 0x4D, "Unlock_Read1FFE_Is4D");
        expect(cart.mem_read(0x5FFF) == 0x69, "Unlock_Read1FFF_Is69");
        // Wrong magic re-locks.
        cart.mem_write(0x5FFF, 0x00);
        expect(!cart.sram_enabled(), "Unlock_WrongSecondByte_Relocks");
        expect(cart.mem_read(0x4100) == 0xB0, "Relock_RomVisibleAgain");
    }

    // --- SRAM R/W while unlocked; blocked while locked. ---
    {
        CartridgeFmPacRom cart = make_cart();
        // Locked: writes to the SRAM window are ignored.
        cart.mem_write(0x4000, 0x5A);
        unlock(cart);
        expect(cart.mem_read(0x4000) == 0x00, "Sram_WriteWhileLocked_Ignored");
        // Unlocked: write first + last addressable SRAM bytes.
        cart.mem_write(0x4000, 0x11);              // rel 0x0000
        cart.mem_write(0x5FFD, 0x22);              // rel 0x1FFD (last addressable)
        expect(cart.mem_read(0x4000) == 0x11, "Sram_WriteFirst_ReadsBack");
        expect(cart.mem_read(0x5FFD) == 0x22, "Sram_WriteLast_ReadsBack");
        // The underlying store carries the same bytes.
        expect(cart.sram().read(0x0000) == 0x11, "Sram_StoreFirst_Matches");
        expect(cart.sram().read(0x1FFD) == 0x22, "Sram_StoreLast_Matches");
    }

    // --- enable bit0 does NOT gate memory-mapped SRAM writes (MSXFmPac.cc:80). ---
    {
        CartridgeFmPacRom cart = make_cart();
        unlock(cart);
        // enable stays 0 (bit0 clear): SRAM writes still land.
        cart.mem_write(0x4200, 0x77);
        expect(cart.mem_read(0x4200) == 0x77, "Sram_WriteWithIoDisabled_StillLands");
    }

    // --- Bank register selects ROM banks; SRAM window unchanged when unlocked. ---
    {
        CartridgeFmPacRom cart = make_cart();
        cart.mem_write(0x7FF7, 0x02);  // bank 2
        expect(cart.bank() == 0x02, "Bank_Set2");
        expect(cart.mem_read(0x4100) == 0xB2, "Bank_RomReadUsesBank2");
        cart.mem_write(0x7FF7, 0x03);
        expect(cart.mem_read(0x4100) == 0xB3, "Bank_RomReadUsesBank3");
        // Bank is masked to 2 bits.
        cart.mem_write(0x7FF7, 0xFF);
        expect(cart.bank() == 0x03, "Bank_MaskedTo2Bits");
        // Unlock: the SRAM window is bank-independent.
        cart.mem_write(0x7FF7, 0x01);
        unlock(cart);
        cart.mem_write(0x4300, 0x9A);
        cart.mem_write(0x7FF7, 0x02);  // change ROM bank
        expect(cart.mem_read(0x4300) == 0x9A, "Bank_SramWindowUnaffectedByBank");
    }

    // --- enable bit4 force-reset clears the magic latches (re-locks). ---
    {
        CartridgeFmPacRom cart = make_cart();
        unlock(cart);
        expect(cart.sram_enabled(), "ForceReset_PreUnlocked");
        cart.mem_write(0x7FF6, 0x10);  // bit4 set
        expect(!cart.sram_enabled(), "ForceReset_RelocksSram");
        expect(cart.enable_register() == 0x10, "ForceReset_EnableReg");
        // While bit4 is set, the magic latches are gated (cannot re-arm).
        cart.mem_write(0x5FFE, 0x4D);
        cart.mem_write(0x5FFF, 0x69);
        expect(!cart.sram_enabled(), "ForceReset_MagicGatedWhileBit4Set");
    }

    // --- enable readback + bit masking (value & 0x11). ---
    {
        CartridgeFmPacRom cart = make_cart();
        cart.mem_write(0x7FF6, 0x11);
        expect(cart.mem_read(0x7FF6) == 0x11, "Enable_Readback11");
        cart.mem_write(0x7FF6, 0xEE);  // only bits 0 and 4 kept -> 0x00
        expect(cart.enable_register() == 0x00, "Enable_MaskedTo0x11");
    }

    // --- Memory-mapped OPLL ports 0x7FF4/0x7FF5 forward to the owned YM2413. ---
    {
        CartridgeFmPacRom cart = make_cart();
        cart.mem_write(0x7FF4, 0x30);  // OPLL address latch
        cart.mem_write(0x7FF5, 0xAB);  // OPLL data -> register 0x30
        expect(cart.opll().register_value(0x30) == 0xAB, "Opll_MemoryMappedWrite");
    }

    // --- 16 KB single-bank image: bank register wraps within the one bank. ---
    {
        std::vector<std::uint8_t> rom16(CartridgeFmPacRom::kBankSize, 0x00);
        rom16[0x0100] = 0x5C;
        CartridgeFmPacRom cart(std::move(rom16));
        cart.reset();
        expect(cart.mem_read(0x4100) == 0x5C, "Rom16_Bank0");
        cart.mem_write(0x7FF7, 0x03);  // bank 3 wraps to the only bank
        expect(cart.mem_read(0x4100) == 0x5C, "Rom16_BankWrapsToOnlyBank");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeFmPacRom_Unit cases passed\n";
    return 0;
}
