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

// Suite: Devices_V9958PaletteMode_Unit
//
// Deterministic unit coverage for the #9A palette two-write path, the V9938 boot
// palette, the M1-M5/YJK/YAE mode-selection decode, and R#25/26/27 storage
// (M14-S3). Grounded in VDP.cc:298-304/709-714 and fact-sheet §3/§4/§5. The
// palette is 16-of-512 (9-bit GRB); 19,268 is the YJK/YAE on-screen max, NEVER a
// palette dimension (Target Machine Specification COLORS row).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_mode.h"

namespace {

using sony_msx::devices::video::decode_vdp_mode;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpMode;

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

bool decodes(std::uint8_t reg0, std::uint8_t reg1, std::uint8_t reg25, VdpMode expected) {
    return decode_vdp_mode(reg0, reg1, reg25).mode == expected;
}

}  // namespace

int main() {
    // Palette is exactly 16 entries — never a 19,268-sized structure.
    static_assert(V9958Vdp::kNumPaletteEntries == 16, "V9958 palette is 16-of-512");

    // --- V9938 boot palette (VDP.cc:299-302). ---
    {
        V9958Vdp vdp;
        const std::uint16_t expected[16] = {0x000, 0x000, 0x611, 0x733, 0x117, 0x327,
                                            0x151, 0x627, 0x171, 0x373, 0x661, 0x664,
                                            0x411, 0x265, 0x555, 0x777};
        bool ok = true;
        for (int i = 0; i < 16; ++i) {
            if (vdp.palette_entry(i) != expected[i]) {
                ok = false;
            }
        }
        expect(ok, "BootPalette_EqualsV9938Table");
    }

    // --- #9A two-write palette + R#16 pointer auto-increment + 9-bit masking. ---
    {
        V9958Vdp vdp;
        set_register(vdp, 16, 0x00);  // palette pointer -> 0
        // Entry 0: R=7 G=5 B=3 -> byte1 = 0 RRR 0 BBB = 0x73, byte2 = 0000 0 GGG = 0x05.
        vdp.io_write(0x9A, 0x73);
        vdp.io_write(0x9A, 0x05);
        // Entry 1: R=1 G=2 B=4 -> byte1 = 0x14, byte2 = 0x02.
        vdp.io_write(0x9A, 0x14);
        vdp.io_write(0x9A, 0x02);
        expect(vdp.palette_entry(0) == 0x573, "Palette_WritePair0_StoredGRB");
        expect(vdp.palette_entry(1) == 0x214, "Palette_WritePair1_StoredGRB");
        expect(vdp.control_register(16) == 0x02, "Palette_PointerAutoIncrements");

        // Masking: junk high bits are dropped to the 9-bit GRB space (& 0x777).
        set_register(vdp, 16, 0x05);
        vdp.io_write(0x9A, 0xFF);
        vdp.io_write(0x9A, 0xFF);
        expect(vdp.palette_entry(5) == 0x777, "Palette_Write_MasksTo9BitGRB");
    }

    // --- Writing R#16 aborts a half-finished palette load (VDP.cc:1135). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 16, 0x00);
        vdp.io_write(0x9A, 0x73);     // first byte latched for entry 0
        set_register(vdp, 16, 0x03);  // ABORTS the half-load, repoints to entry 3
        vdp.io_write(0x9A, 0x05);     // fresh first byte for entry 3
        vdp.io_write(0x9A, 0x04);     // second byte: entry 3 = ((0x04<<8)|0x05)&0x777
        expect(vdp.palette_entry(0) == 0x000, "Palette_R16Write_AbortsHalfLoad");
        expect(vdp.palette_entry(3) == 0x405, "Palette_AfterAbort_NextPairLandsAtNewIndex");
    }

    // --- Mode-selection decode over the Target-Spec set. ---
    expect(decodes(0x00, 0x00, 0x00, VdpMode::Graphic1), "Mode_Screen1_Graphic1");
    expect(decodes(0x00, 0x10, 0x00, VdpMode::Text1), "Mode_Screen0W40_Text1");
    expect(decodes(0x00, 0x08, 0x00, VdpMode::Multicolor), "Mode_Screen3_Multicolor");
    expect(decodes(0x02, 0x00, 0x00, VdpMode::Graphic2), "Mode_Screen2_Graphic2");
    expect(decodes(0x04, 0x00, 0x00, VdpMode::Graphic3), "Mode_Screen4_Graphic3");
    expect(decodes(0x04, 0x10, 0x00, VdpMode::Text2), "Mode_Screen0W80_Text2");
    expect(decodes(0x06, 0x00, 0x00, VdpMode::Graphic4), "Mode_Screen5_Graphic4");
    expect(decodes(0x08, 0x00, 0x00, VdpMode::Graphic5), "Mode_Screen6_Graphic5");
    expect(decodes(0x0A, 0x00, 0x00, VdpMode::Graphic6), "Mode_Screen7_Graphic6");
    expect(decodes(0x0E, 0x00, 0x00, VdpMode::Graphic7), "Mode_Screen8_Graphic7");
    expect(decodes(0x0E, 0x00, 0x08, VdpMode::ScreenYjk), "Mode_Screen12_Yjk");
    expect(decodes(0x0E, 0x00, 0x18, VdpMode::ScreenYjkYae), "Mode_Screen10_11_YjkYae");
    // YAE without YJK is ignored (stays GRAPHIC7).
    expect(decodes(0x0E, 0x00, 0x10, VdpMode::Graphic7), "Mode_YaeWithoutYjk_Ignored");

    // --- Device tracks the decoded mode across register writes + R#25/26/27
    //     storage (bit-level only, no visual effect). ---
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x0E);   // GRAPHIC7 base
        expect(vdp.mode().mode == VdpMode::Graphic7, "Device_ModeTracksRegisterWrites");
        set_register(vdp, 25, 0x18);  // YJK + YAE
        expect(vdp.mode().mode == VdpMode::ScreenYjkYae && vdp.mode().yjk && vdp.mode().yae,
               "Device_R25_YjkYae_UpdatesModeAndStores");
        expect(vdp.control_register(25) == 0x18, "Device_R25_Stored");
        set_register(vdp, 26, 0x3F);
        set_register(vdp, 27, 0x07);
        expect(vdp.control_register(26) == 0x3F && vdp.control_register(27) == 0x07,
               "Device_R26_R27_HorizontalScroll_Stored");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
