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

// Suite: Devices_V9958Ports_Unit
//
// Deterministic unit coverage for the V9958 port decode and the #98 VRAM data
// path (M14-S1). Ports are decoded on `port & 0x03`, so the S1985 straight-alias
// mirror #9C-#9F collapses onto the same four functions as #98-#9B (A-2). The
// #98 path is verified for the shared read/write latch (VDP.cc:789-791), auto-
// increment read/write with the read-ahead buffer, R#14 carry in V9938 modes,
// and the legacy 16 KB wrap in TMS9918 modes (fact-sheet §2:40; VDP.cc:883-887).

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

// --- #99 helpers (two-write protocol). ---
void set_register(V9958Vdp& vdp, std::uint8_t reg, std::uint8_t value) {
    vdp.io_write(0x99, value);                                       // latch data
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));  // register write
}

void set_write_address(V9958Vdp& vdp, std::uint16_t addr) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));      // low byte
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));  // bit6=1 write
}

void set_read_address(V9958Vdp& vdp, std::uint16_t addr) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));      // low byte
    vdp.io_write(0x99, static_cast<std::uint8_t>((addr >> 8) & 0x3F));  // bit6=0 read (read-ahead)
}

}  // namespace

int main() {
    // --- Port decode + #9C-#9F mirror-collapse. ---
    {
        V9958Vdp vdp;
        // #98 and its mirror #9C address the SAME VRAM-data function.
        set_write_address(vdp, 0x0100);
        vdp.io_write(0x9C, 0x77);  // mirror of #98 -> VRAM data write
        expect(vdp.vram().read(0x0100) == 0x77, "Mirror9C_WritesVramLike98");

        // #9B and its mirror #9F address the SAME indirect-register function:
        // point R#17 at R#7 via #99, then write R#7 through the #9F mirror.
        set_register(vdp, 17, 0x07);  // R#17 = 7 (no AII)
        vdp.io_write(0x9F, 0x2A);     // mirror of #9B -> indirect write R#7 = 0x2A
        expect(vdp.control_register(7) == 0x2A, "Mirror9F_IndirectWritesLike9B");

        // Write-only ports read open-bus 0xFF (#9A/#9B and their mirrors).
        expect(vdp.io_read(0x9A) == 0xFF && vdp.io_read(0x9B) == 0xFF,
               "WriteOnlyPorts_ReadOpenBus");
        expect(vdp.io_read(0x9E) == 0xFF && vdp.io_read(0x9F) == 0xFF,
               "WriteOnlyPortMirrors_ReadOpenBus");
    }

    // --- Shared read/write latch: OUT(#98) then IN(#98) returns just-written. ---
    {
        V9958Vdp vdp;
        set_write_address(vdp, 0x0200);
        vdp.io_write(0x98, 0xC3);
        // The next #98 read returns the shared latch (the just-written byte),
        // not VRAM at the post-increment address.
        expect(vdp.io_read(0x98) == 0xC3, "SharedLatch_WriteThenRead_ReturnsWritten");
    }

    // --- Auto-increment write then read-back with the read-ahead buffer. ---
    {
        V9958Vdp vdp;
        set_write_address(vdp, 0x0000);
        for (int i = 0; i < 16; ++i) {
            vdp.io_write(0x98, static_cast<std::uint8_t>(0xA0 + i));
        }
        // The 14-bit pointer advanced 16 times.
        expect(vdp.vram_pointer() == 16, "AutoIncrement_WriteAdvancesPointer");
        // Stored contiguously in VRAM.
        bool stored_ok = true;
        for (int i = 0; i < 16; ++i) {
            if (vdp.vram().read(static_cast<std::size_t>(i)) != 0xA0 + i) {
                stored_ok = false;
            }
        }
        expect(stored_ok, "AutoIncrement_WriteStoresContiguously");

        // Read the block back through #98 (read-ahead prefetch on address setup
        // means the first read returns VRAM[0], not stale data).
        set_read_address(vdp, 0x0000);
        bool readback_ok = true;
        for (int i = 0; i < 16; ++i) {
            if (vdp.io_read(0x98) != 0xA0 + i) {
                readback_ok = false;
            }
        }
        expect(readback_ok, "ReadAhead_AutoIncrement_ReadsBackRamp");
    }

    // --- R#14 carry in a V9938 mode (GRAPHIC4): pointer wraps 0x3FFF -> 0 and
    //     R#14 increments, so the effective address counts past 16 KB. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4 base 0x0C (M3+M4) -> V9938 mode
        set_register(vdp, 14, 0x00);
        set_write_address(vdp, 0x3FFF);
        vdp.io_write(0x98, 0x11);    // writes VRAM[0x3FFF]; ptr 0x3FFF->0, R#14->1
        expect(vdp.control_register(14) == 0x01, "R14Carry_V9938Mode_IncrementsOnWrap");
        vdp.io_write(0x98, 0x22);    // effective addr now (1<<14)|0 = 0x4000
        expect(vdp.vram().read(0x3FFF) == 0x11 && vdp.vram().read(0x4000) == 0x22,
               "R14Carry_V9938Mode_CountsPast16K");
    }

    // --- Legacy 16 KB wrap in a TMS9918 mode (GRAPHIC1): no R#14 carry. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x00);  // GRAPHIC1 base 0x00 -> legacy mode
        set_register(vdp, 14, 0x00);
        set_write_address(vdp, 0x3FFF);
        vdp.io_write(0x98, 0x33);    // writes VRAM[0x3FFF]; ptr wraps to 0
        expect(vdp.control_register(14) == 0x00, "LegacyWrap_NoR14Carry");
        vdp.io_write(0x98, 0x44);    // effective addr wraps back to 0x0000
        expect(vdp.vram().read(0x3FFF) == 0x33 && vdp.vram().read(0x0000) == 0x44,
               "LegacyWrap_WrapsAt16KBoundary");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
