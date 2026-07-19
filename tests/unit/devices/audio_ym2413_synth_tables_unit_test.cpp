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

// Suite: Devices_Ym2413SynthTables_Unit
//
// License-safe oracle discipline: every expected
// value below is recomputed INDEPENDENTLY inside this test via
// double-precision math + rounding -- never a golden array copied from
// anywhere. The production tables themselves are computed from the same
// closed forms (fact-sheet §4's structural description of the classic
// Yamaha logarithmic operator); identical numeric OUTPUT from an
// independently-grounded closed form is expected and acceptable
// (DEC-0035).

#include <cmath>
#include <cstdint>
#include <iostream>

#include "devices/audio/ym2413_synth.h"

namespace {

using sony_msx::devices::audio::Ym2413Synth;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr double kPi = 3.14159265358979323846;

}  // namespace

int main() {
    // --- 1. logsin table: independent in-test recomputation of
    //     round(-log2(sin((i+0.5)*pi/512))*256) for every index. ---
    {
        bool all_match = true;
        for (int i = 0; i < 256; ++i) {
            const double s = std::sin((static_cast<double>(i) + 0.5) * kPi / 512.0);
            const auto expected = static_cast<std::uint16_t>(std::lround(-std::log2(s) * 256.0));
            if (Ym2413Synth::logsin_table(i) != expected) {
                all_match = false;
            }
        }
        expect(all_match, "LogSin_AllEntries_MatchIndependentRecomputation");
    }

    // --- 2. logsin properties: 12-bit range, strictly monotonic decreasing
    //     (attenuation falls as the quarter-sine rises), endpoints. ---
    {
        bool monotonic = true;
        bool in_range = true;
        for (int i = 0; i < 256; ++i) {
            const std::uint16_t v = Ym2413Synth::logsin_table(i);
            if (v > 4095) {
                in_range = false;  // 12-bit table (fact-sheet §4)
            }
            if (i > 0 && v > Ym2413Synth::logsin_table(i - 1)) {
                monotonic = false;  // non-increasing toward the sine peak
            }
        }
        expect(in_range, "LogSin_AllEntries_Fit12Bits");
        expect(monotonic, "LogSin_NonIncreasing_TowardSinePeak");
        // Endpoint spot values, recomputed independently here:
        // i=0: sin(0.5*pi/512) ~= 0.0030680; -log2 ~= 8.3482; *256 ~= 2137.
        expect(Ym2413Synth::logsin_table(0) ==
                   static_cast<std::uint16_t>(std::lround(
                       -std::log2(std::sin(0.5 * kPi / 512.0)) * 256.0)),
               "LogSin_Index0_MaxAttenuationEndpoint");
        // i=255: sin ~= 1 -> attenuation ~= 0.
        expect(Ym2413Synth::logsin_table(255) == 0, "LogSin_Index255_ZeroAttenuationEndpoint");
    }

    // --- 3. exp table: independent recomputation of
    //     round(2^(-j/256)*2048); endpoints and monotonicity. ---
    {
        bool all_match = true;
        bool monotonic = true;
        for (int j = 0; j < 256; ++j) {
            const auto expected = static_cast<std::uint16_t>(
                std::lround(std::exp2(-static_cast<double>(j) / 256.0) * 2048.0));
            if (Ym2413Synth::exp2neg_table(j) != expected) {
                all_match = false;
            }
            if (j > 0 && Ym2413Synth::exp2neg_table(j) >= Ym2413Synth::exp2neg_table(j - 1)) {
                monotonic = false;
            }
        }
        expect(all_match, "Exp2Neg_AllEntries_MatchIndependentRecomputation");
        expect(monotonic, "Exp2Neg_StrictlyDecreasing");
        expect(Ym2413Synth::exp2neg_table(0) == 2048, "Exp2Neg_Index0_FullScale2048");
        // j=255: 2^(-255/256)*2048 ~= 1026.8 -> 1027.
        expect(Ym2413Synth::exp2neg_table(255) ==
                   static_cast<std::uint16_t>(std::lround(std::exp2(-255.0 / 256.0) * 2048.0)),
               "Exp2Neg_Index255_Endpoint");
    }

    // --- 4. operator_magnitude: the T = logsin + attenuation composition
    //     halves exactly every 256 units (one factor of 2 = 6.0206 dB). ---
    {
        expect(Ym2413Synth::operator_magnitude(0) == 2048, "Magnitude_T0_FullScale");
        expect(Ym2413Synth::operator_magnitude(256) == 1024, "Magnitude_256Units_ExactlyHalf");
        expect(Ym2413Synth::operator_magnitude(512) == 512, "Magnitude_512Units_ExactlyQuarter");
        // 128 units = one 3 dB volume step: ratio ~ 1/sqrt(2) (within LSB).
        const std::int32_t m0 = Ym2413Synth::operator_magnitude(0);
        const std::int32_t m1 = Ym2413Synth::operator_magnitude(128);
        const double ratio = static_cast<double>(m0) / static_cast<double>(m1);
        expect(ratio > 1.40 && ratio < 1.43, "Magnitude_128Units_IsOne3dBStep");
        // Deep attenuation shifts to exact zero (the silence floor).
        expect(Ym2413Synth::operator_magnitude(16u * 127u + 4000u) == 0,
               "Magnitude_DeepAttenuation_ExactZero");
    }

    // --- 5. exp(logsin) round-trip: the reconstructed quarter sine matches
    //     sin() within quantization bounds (fact-sheet §4 structure). ---
    {
        bool within_bounds = true;
        for (int i = 0; i < 256; ++i) {
            const std::int32_t mag =
                Ym2413Synth::operator_magnitude(Ym2413Synth::logsin_table(i));
            const double expected = std::sin((static_cast<double>(i) + 0.5) * kPi / 512.0) * 2048.0;
            // Two rounding stages (log then exp), each <= 0.5 ULP in the
            // log domain: allow 6 output LSBs of slack.
            if (std::abs(static_cast<double>(mag) - expected) > 6.0) {
                within_bounds = false;
            }
        }
        expect(within_bounds, "ExpLogSinRoundTrip_QuarterSine_WithinQuantizationBounds");
    }

    // --- 6. Symmetry / quadrant conventions carried by the helpers:
    //     phase_increment implements dP = (F-Num << BLOCK) * MUL(/2). ---
    {
        expect(Ym2413Synth::phase_increment(256, 4, 1) == 4096, "PhaseInc_Fnum256Block4Mul1");
        // MUL field 0 -> x1/2 (the fact-sheet §3 table's first entry).
        expect(Ym2413Synth::phase_increment(256, 4, 0) == 2048, "PhaseInc_MulField0_IsHalf");
        // MUL field 0xF -> x15.
        expect(Ym2413Synth::phase_increment(256, 4, 0xF) == 4096 * 15, "PhaseInc_MulFieldF_Is15x");
        // BLOCK doubles per step.
        expect(Ym2413Synth::phase_increment(256, 5, 1) == 8192, "PhaseInc_BlockDoubles");
        // MUL pairs B/A and D/C and F/E repeat (10,10,12,12,15,15).
        expect(Ym2413Synth::phase_increment(100, 0, 0xA) ==
                   Ym2413Synth::phase_increment(100, 0, 0xB),
               "PhaseInc_MulPair_10_10");
        expect(Ym2413Synth::phase_increment(100, 0, 0xC) ==
                   Ym2413Synth::phase_increment(100, 0, 0xD),
               "PhaseInc_MulPair_12_12");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Ym2413SynthTables_Unit cases passed\n";
    return 0;
}
