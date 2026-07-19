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

#include "devices/audio/ym2413_opll.h"

// Suite: Devices_AudioYm2413Opll_Unit  (M17-S1/S2, backlog B3)
//
// YM2413 (OPLL) register-accurate model: two-port address-latch/data write
// protocol (address-latch masking at USE time, not latch time), reset zeroes
// all 64 registers, open-bus read behaviour, per-channel decode (F-Num/Block/
// Key-on/Sustain/Instrument/Volume/Patch), rhythm-mode decode, the 15+3-entry
// ROM instrument patch table, and two-run determinism. Grounding: YM2413
// fact-sheet §3/§4/§6; references/openmsx-21.0/src/sound/YM2413Okazaki.cc
// (behaviour reference only, never copied -- GPL isolation).

namespace {

using sony_msx::devices::audio::Ym2413Opll;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- A-M17-3: two-port write protocol lands in the correct register. ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.io_write(0x7C, 0x10);  // address latch -> register 0x10
        ym.io_write(0x7D, 0x5A);  // data write
        expect(ym.register_value(0x10) == 0x5A, "TwoPortWrite_LandsInLatchedRegister");
    }

    // --- A-M17-3: latch masking applied at USE time, not latch time. ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.write_address(0xFF);  // latch stores 0xFF UNMASKED
        ym.write_data(0x77);     // data write masks: 0xFF & 0x3F == 0x3F
        expect(ym.register_value(0x3F) == 0x77, "LatchMask_0xFF_ResolvesTo0x3F_OnDataWrite");
    }

    // --- Every register in $00-$3F is independently addressable. ---
    {
        Ym2413Opll ym;
        ym.reset();
        for (int reg = 0; reg < 0x40; ++reg) {
            ym.write_address(static_cast<std::uint8_t>(reg));
            ym.write_data(static_cast<std::uint8_t>(reg ^ 0xA5));
        }
        bool all_ok = true;
        for (int reg = 0; reg < 0x40; ++reg) {
            if (ym.register_value(static_cast<std::uint8_t>(reg)) != static_cast<std::uint8_t>(reg ^ 0xA5)) {
                all_ok = false;
            }
        }
        expect(all_ok, "AllRegisters_IndependentlyAddressable");
    }

    // --- A-M17-4: reset zeroes all 64 registers (and the latch). ---
    {
        Ym2413Opll ym;
        ym.reset();
        for (int reg = 0; reg < 0x40; ++reg) {
            ym.write_address(static_cast<std::uint8_t>(reg));
            ym.write_data(0xFF);
        }
        ym.reset();
        bool all_zero = true;
        for (int reg = 0; reg < 0x40; ++reg) {
            if (ym.register_value(static_cast<std::uint8_t>(reg)) != 0) {
                all_zero = false;
            }
        }
        expect(all_zero, "Reset_ZeroesAll64Registers");
        // Latch itself resets too: after reset, writing data with no fresh
        // address write lands at register 0 (latch == 0).
        ym.write_data(0x42);
        expect(ym.register_value(0x00) == 0x42, "Reset_ZeroesLatchToo");
    }

    // --- A-M17-5: io_read always 0xFF regardless of prior writes. ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.io_write(0x7C, 0x10);
        ym.io_write(0x7D, 0x5A);
        expect(ym.io_read(0x7C) == 0xFF, "IoRead_Port7C_AlwaysOpenBus");
        expect(ym.io_read(0x7D) == 0xFF, "IoRead_Port7D_AlwaysOpenBus");
    }

    // --- Per-channel decode: F-Number/Block/Key-on/Sustain/Instrument/Volume. ---
    {
        Ym2413Opll ym;
        ym.reset();
        for (int ch = 0; ch < Ym2413Opll::kChannelCount; ++ch) {
            const std::uint8_t fnum_lo = static_cast<std::uint8_t>(0x34 + ch);
            ym.write_address(static_cast<std::uint8_t>(0x10 + ch));
            ym.write_data(fnum_lo);
            // $20+ch: SUS(bit5)=1 KEY(bit4)=1 BLOCK[2:0](bits3-1)=5 F-Num[8](bit0)=1
            const std::uint8_t reg20 = static_cast<std::uint8_t>(0x20 | 0x10 | (5 << 1) | 0x01);
            ym.write_address(static_cast<std::uint8_t>(0x20 + ch));
            ym.write_data(reg20);
            // $30+ch: INST[3:0]=ch+1 (1-15, wraps within valid range for ch<15) VOL[3:0]=ch
            const std::uint8_t inst = static_cast<std::uint8_t>((ch % 15) + 1);
            const std::uint8_t reg30 = static_cast<std::uint8_t>((inst << 4) | (ch & 0x0F));
            ym.write_address(static_cast<std::uint8_t>(0x30 + ch));
            ym.write_data(reg30);

            const std::uint16_t expected_fnum = static_cast<std::uint16_t>(fnum_lo | 0x100);
            expect(ym.f_number(ch) == expected_fnum, "FNumber_9Bit_DecodedFromLoAndHiBit");
            expect(ym.block(ch) == 5, "Block_3Bit_Decoded");
            expect(ym.key_on(ch), "KeyOn_BitDecoded");
            expect(ym.sustain(ch), "Sustain_BitDecoded");
            expect(ym.instrument(ch) == inst, "Instrument_4Bit_Decoded");
            expect(ym.volume(ch) == (ch & 0x0F), "Volume_4Bit_Decoded");
        }
    }

    // --- Key-on/Sustain false when bits are clear. ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.write_address(0x20);
        ym.write_data(0x00);
        expect(!ym.key_on(0), "KeyOn_ClearWhenBitZero");
        expect(!ym.sustain(0), "Sustain_ClearWhenBitZero");
    }

    // --- User-patch (instrument==0) live decode reflects in-place $00-$07 edits. ---
    {
        Ym2413Opll ym;
        ym.reset();
        ym.write_address(0x30);
        ym.write_data(0x00);  // instrument 0 -> user patch, channel 0
        ym.write_address(0x00);
        ym.write_data(0x71);  // AM=0 VIB=1 EGT=1 KSR=0 MUL=1 (matches Violin mod byte)
        ym.write_address(0x01);
        ym.write_data(0x61);
        ym.write_address(0x02);
        ym.write_data(0x1E);
        ym.write_address(0x03);
        ym.write_data(0x17);
        ym.write_address(0x04);
        ym.write_data(0xD0);
        ym.write_address(0x05);
        ym.write_data(0x78);
        ym.write_address(0x06);
        ym.write_data(0x00);
        ym.write_address(0x07);
        ym.write_data(0x17);

        const auto p = ym.patch(0);
        const auto expected = Ym2413Opll::rom_patch(1);  // Violin byte-identical
        expect(p.modulator.multiple == expected.modulator.multiple, "UserPatch_LiveDecode_ModulatorMul");
        expect(p.modulator.am == expected.modulator.am, "UserPatch_LiveDecode_ModulatorAm");
        expect(p.carrier.release_rate == expected.carrier.release_rate,
               "UserPatch_LiveDecode_CarrierReleaseRate");

        // Edit in place: instrument stays 0 (user), so patch(0) reflects the edit live.
        ym.write_address(0x00);
        ym.write_data(0x00);  // MUL -> 0
        expect(ym.patch(0).modulator.multiple == 0, "UserPatch_LiveDecode_ReflectsInPlaceEdit");
    }

    // --- Rhythm-mode decode: $0E bits + $36-$38 volume nibbles. ---
    {
        Ym2413Opll ym;
        ym.reset();
        // R=1 BD=1 SD=0 TOM=1 T-CY=0 HH=1 -> 0x20 | 0x10 | 0x04 | 0x01 = 0x35
        ym.write_address(0x0E);
        ym.write_data(0x35);
        expect(ym.rhythm_enabled(), "Rhythm_Enabled_Bit5");
        expect(ym.bd_key(), "Rhythm_BdKey_Bit4");
        expect(!ym.sd_key(), "Rhythm_SdKey_Bit3_Clear");
        expect(ym.tom_key(), "Rhythm_TomKey_Bit2");
        expect(!ym.cym_key(), "Rhythm_CymKey_Bit1_Clear");
        expect(ym.hh_key(), "Rhythm_HhKey_Bit0");

        ym.write_address(0x36);
        ym.write_data(0x0A);  // BD volume
        ym.write_address(0x37);
        ym.write_data(0x3C);  // HH=3 SD=C
        ym.write_address(0x38);
        ym.write_data(0x5D);  // TOM=5 CYM=D
        expect(ym.bd_volume() == 0x0A, "Rhythm_BdVolume");
        expect(ym.hh_volume() == 0x03, "Rhythm_HhVolume");
        expect(ym.sd_volume() == 0x0C, "Rhythm_SdVolume");
        expect(ym.tom_volume() == 0x05, "Rhythm_TomVolume");
        expect(ym.cym_volume() == 0x0D, "Rhythm_CymVolume");
    }

    // --- ROM instrument patch table: byte-exact against the fact-sheet §4 table. ---
    {
        struct Row {
            int number;
            std::array<std::uint8_t, 8> bytes;
        };
        const Row rows[] = {
            {1, {0x71, 0x61, 0x1E, 0x17, 0xD0, 0x78, 0x00, 0x17}},
            {2, {0x13, 0x41, 0x1A, 0x0D, 0xD8, 0xF7, 0x23, 0x13}},
            {3, {0x13, 0x01, 0x99, 0x00, 0xF2, 0xC4, 0x11, 0x23}},
            {4, {0x31, 0x61, 0x0E, 0x07, 0xA8, 0x64, 0x70, 0x27}},
            {5, {0x32, 0x21, 0x1E, 0x06, 0xE0, 0x76, 0x00, 0x28}},
            {6, {0x31, 0x22, 0x16, 0x05, 0xE0, 0x71, 0x00, 0x18}},
            {7, {0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x10, 0x07}},
            {8, {0x23, 0x21, 0x2D, 0x14, 0xA2, 0x72, 0x00, 0x07}},
            {9, {0x61, 0x61, 0x1B, 0x06, 0x64, 0x65, 0x10, 0x17}},
            {10, {0x41, 0x61, 0x0B, 0x18, 0x85, 0xF7, 0x71, 0x07}},
            {11, {0x13, 0x01, 0x83, 0x11, 0xFA, 0xE4, 0x10, 0x04}},
            {12, {0x17, 0xC1, 0x24, 0x07, 0xF8, 0xF8, 0x22, 0x12}},
            {13, {0x61, 0x50, 0x0C, 0x05, 0xC2, 0xF5, 0x20, 0x42}},
            {14, {0x01, 0x01, 0x55, 0x03, 0xC9, 0x95, 0x03, 0x02}},
            {15, {0x61, 0x41, 0x89, 0x03, 0xF1, 0xE4, 0x40, 0x13}},
        };
        for (const auto& row : rows) {
            const auto p = Ym2413Opll::rom_patch(row.number);
            const std::uint8_t b0 = row.bytes[0];
            const std::uint8_t b1 = row.bytes[1];
            const std::uint8_t b2 = row.bytes[2];
            const std::uint8_t b3 = row.bytes[3];
            const std::uint8_t b4 = row.bytes[4];
            const std::uint8_t b5 = row.bytes[5];
            const std::uint8_t b6 = row.bytes[6];
            const std::uint8_t b7 = row.bytes[7];
            bool ok = true;
            ok &= p.modulator.am == ((b0 & 0x80) != 0);
            ok &= p.modulator.vib == ((b0 & 0x40) != 0);
            ok &= p.modulator.sustained_eg == ((b0 & 0x20) != 0);
            ok &= p.modulator.ksr == ((b0 & 0x10) != 0);
            ok &= p.modulator.multiple == (b0 & 0x0F);
            ok &= p.carrier.am == ((b1 & 0x80) != 0);
            ok &= p.carrier.vib == ((b1 & 0x40) != 0);
            ok &= p.carrier.sustained_eg == ((b1 & 0x20) != 0);
            ok &= p.carrier.ksr == ((b1 & 0x10) != 0);
            ok &= p.carrier.multiple == (b1 & 0x0F);
            ok &= p.modulator.ksl == ((b2 >> 6) & 0x3);
            ok &= p.modulator.total_level == (b2 & 0x3F);
            ok &= p.carrier.ksl == ((b3 >> 6) & 0x3);
            ok &= p.carrier.half_sine == ((b3 & 0x10) != 0);
            ok &= p.modulator.half_sine == ((b3 & 0x08) != 0);
            ok &= p.modulator.feedback == (b3 & 0x07);
            ok &= p.modulator.attack_rate == ((b4 >> 4) & 0xF);
            ok &= p.modulator.decay_rate == (b4 & 0xF);
            ok &= p.carrier.attack_rate == ((b5 >> 4) & 0xF);
            ok &= p.carrier.decay_rate == (b5 & 0xF);
            ok &= p.modulator.sustain_level == ((b6 >> 4) & 0xF);
            ok &= p.modulator.release_rate == (b6 & 0xF);
            ok &= p.carrier.sustain_level == ((b7 >> 4) & 0xF);
            ok &= p.carrier.release_rate == (b7 & 0xF);
            expect(ok, "RomPatch_ByteExact_AgainstFactSheetTable");
        }

        // Spot-check example from the planner package: rom_patch(3).modulator.mul == 3.
        expect(Ym2413Opll::rom_patch(3).modulator.multiple == 3, "RomPatch3_Piano_ModulatorMul3");

        // Rhythm patches: BD, SD/HH, TOM/T-CY.
        const auto bd = Ym2413Opll::rhythm_bd_patch();
        expect(bd.modulator.multiple == 1 && bd.carrier.multiple == 1, "RhythmBdPatch_ByteExact");
        const auto sd_hh = Ym2413Opll::rhythm_sd_hh_patch();
        expect(sd_hh.modulator.attack_rate == 0x0C, "RhythmSdHhPatch_ByteExact");
        const auto tom_cym = Ym2413Opll::rhythm_tom_cym_patch();
        expect(tom_cym.modulator.multiple == 5, "RhythmTomCymPatch_ByteExact");
    }

    // --- Two-run determinism: identical write sequence -> identical register state. ---
    {
        auto run = []() {
            Ym2413Opll ym;
            ym.reset();
            for (int reg = 0; reg < 0x40; ++reg) {
                ym.write_address(static_cast<std::uint8_t>(reg));
                ym.write_data(static_cast<std::uint8_t>((reg * 7 + 3) & 0xFF));
            }
            return ym;
        };
        const Ym2413Opll a = run();
        const Ym2413Opll b = run();
        bool identical = true;
        for (int reg = 0; reg < 0x40; ++reg) {
            if (a.register_value(static_cast<std::uint8_t>(reg)) != b.register_value(static_cast<std::uint8_t>(reg))) {
                identical = false;
            }
        }
        expect(identical, "TwoRunDeterminism_IdenticalWriteSequence_IdenticalState");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_AudioYm2413Opll_Unit cases passed\n";
    return 0;
}
