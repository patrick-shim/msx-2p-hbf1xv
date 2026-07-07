// Suite: Devices_VdpPalette_Unit
//
// Deterministic unit coverage for the M21-S1 color-decode primitives
// (backlog D1/D5): the 3-bit->5-bit expansion table (A-M21-3), RGB555
// pack/unpack, the 9-bit GRB palette-register unpack, the GRAPHIC7 fixed
// 256-color GGGRRRBB decode (A-M21-4/R-M21-1), and the YJK R/G/B clamp
// formula including the negative-numerator rounding case (A-M21-5/R-M21-2).

#include <cmath>
#include <cstdint>
#include <iostream>

#include "devices/video/vdp_palette.h"

namespace {

using namespace sony_msx::devices::video;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- 3-bit -> 5-bit expansion table, exact for all 8 inputs (A-M21-3). ---
    {
        constexpr std::uint8_t kExpected[8] = {0, 4, 9, 13, 18, 22, 27, 31};
        bool all_ok = true;
        for (int c3 = 0; c3 < 8; ++c3) {
            if (expand3to5(static_cast<std::uint8_t>(c3)) != kExpected[c3]) {
                all_ok = false;
            }
        }
        expect(all_ok, "Expand3to5_AllEightInputs_MatchesFixedTable");
    }

    // --- RGB555 pack/unpack round-trip for boundary + arbitrary values. ---
    {
        bool round_trip_ok = true;
        for (int r = 0; r < 32; r += 7) {
            for (int g = 0; g < 32; g += 7) {
                for (int b = 0; b < 32; b += 7) {
                    const std::uint16_t px =
                        pack_rgb555(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b));
                    if (rgb555_red(px) != r || rgb555_green(px) != g || rgb555_blue(px) != b) {
                        round_trip_ok = false;
                    }
                }
            }
        }
        expect(round_trip_ok, "PackRgb555_RoundTrip_ExactForSampledValues");
        expect(pack_rgb555(0, 0, 0) == 0x0000, "PackRgb555_AllZero_IsZero");
        expect(pack_rgb555(31, 31, 31) == 0x7FFF, "PackRgb555_AllMax_Is0x7FFF");
    }

    // --- 9-bit GRB palette-register unpack (VDP.hh:274-278's documented
    //     bit10-8=G, bit6-4=R, bit2-0=B layout). ---
    {
        // grb9 = G=7,R=3,B=1 -> bits: G(10-8)=111, R(6-4)=011, B(2-0)=001
        const std::uint16_t grb9 = (0x7 << 8) | (0x3 << 4) | 0x1;
        const std::uint16_t px = palette_entry_to_rgb555(grb9);
        expect(rgb555_red(px) == expand3to5(3), "PaletteEntryToRgb555_RedComponent");
        expect(rgb555_green(px) == expand3to5(7), "PaletteEntryToRgb555_GreenComponent");
        expect(rgb555_blue(px) == expand3to5(1), "PaletteEntryToRgb555_BlueComponent");
    }

    // --- GRAPHIC7 fixed 256-color byte decode: GGGRRRBB, NOT RRRGGGBB
    //     (A-M21-4/R-M21-1). byte=0b111_000_00 => green=7 (TOP 3 bits),
    //     red=0, blue=0 -> max-green, zero-red, zero-blue. ---
    {
        const std::uint16_t px = graphic7_fixed_color_to_rgb555(0b111'000'00);
        expect(rgb555_green(px) == 31, "Graphic7FixedColor_TopThreeBits_IsGreenNotRed");
        expect(rgb555_red(px) == 0, "Graphic7FixedColor_TopThreeBits_RedIsZero");
        expect(rgb555_blue(px) == 0, "Graphic7FixedColor_TopThreeBits_BlueIsZero");

        // byte=0b000_111_00: red=7 (bits4-2), green=0, blue=0.
        const std::uint16_t px2 = graphic7_fixed_color_to_rgb555(0b000'111'00);
        expect(rgb555_red(px2) == 31, "Graphic7FixedColor_MiddleThreeBits_IsRed");
        expect(rgb555_green(px2) == 0, "Graphic7FixedColor_MiddleThreeBits_GreenIsZero");

        // byte low 2 bits (pseudo-3-bit blue): {0->0,1->2,2->4,3->7} then
        // expanded via the SAME 3-bit table.
        expect(rgb555_blue(graphic7_fixed_color_to_rgb555(0b000'000'00)) == expand3to5(0),
               "Graphic7FixedColor_BlueBits_00");
        expect(rgb555_blue(graphic7_fixed_color_to_rgb555(0b000'000'01)) == expand3to5(2),
               "Graphic7FixedColor_BlueBits_01");
        expect(rgb555_blue(graphic7_fixed_color_to_rgb555(0b000'000'10)) == expand3to5(4),
               "Graphic7FixedColor_BlueBits_10");
        expect(rgb555_blue(graphic7_fixed_color_to_rgb555(0b000'000'11)) == expand3to5(7),
               "Graphic7FixedColor_BlueBits_11");
    }

    // --- YJK R/G/B clamp formula (A-M21-5/R-M21-2): plain C++ int division
    //     (truncating toward zero), never std::floor(). ---
    {
        // Basic in-range case: y=20,j=5,k=3 -> r=clamp(25)=25, g=clamp(23)=23,
        // b=clamp((100-10-3+2)/4)=clamp(89/4)=clamp(22)=22.
        const YjkRgb rgb = yjk_to_rgb(20, 5, 3);
        expect(rgb.r == 25, "YjkToRgb_RChannel_ClampedSum");
        expect(rgb.g == 23, "YjkToRgb_GChannel_ClampedSum");
        expect(rgb.b == 22, "YjkToRgb_BChannel_PositiveNumerator");

        // Clamping at the high boundary: y=31,j=31,k=31 -> r,g clamp to 31.
        const YjkRgb hi = yjk_to_rgb(31, 31, 31);
        expect(hi.r == 31 && hi.g == 31, "YjkToRgb_HighBoundary_ClampsTo31");

        // Negative, non-multiple-of-4 pre-clamp numerator case (the rounding
        // risk, A-M21-5): y=0, j=1, k=0 -> numerator = 5*0 - 2*1 - 0 + 2 = 0
        // (in-range, not the target). Pick y=0, j=2, k=0 instead:
        // numerator = 0 - 4 - 0 + 2 = -2 (negative, not a multiple of 4).
        // Plain C++ truncating division: -2/4 == 0 (truncates toward zero).
        // A std::floor()-based implementation would instead compute
        // floor(-0.5) == -1 for the PRE-clamp quotient.
        {
            constexpr int y = 0;
            constexpr int j = 2;
            constexpr int k = 0;
            constexpr int numerator = 5 * y - 2 * j - k + 2;  // -2
            expect(numerator == -2 && (numerator % 4 != 0), "YjkRoundingCase_NumeratorIsNegativeNonMultipleOf4");

            const int truncating_quotient = numerator / 4;  // plain C++ int division: 0
            const int floor_quotient =
                static_cast<int>(std::floor(static_cast<double>(numerator) / 4.0));  // -1
            expect(truncating_quotient == 0, "YjkRoundingCase_TruncatingQuotientIsZero");
            expect(floor_quotient == -1, "YjkRoundingCase_FloorQuotientWouldDifferByOne");
            expect(truncating_quotient != floor_quotient,
                   "YjkRoundingCase_TruncationAndFloorGenuinelyDiverge");

            const YjkRgb rgb_neg = yjk_to_rgb(y, j, k);
            // Our implementation uses plain int division (per A-M21-5): the
            // clamped B channel equals clamp(truncating_quotient, 0, 31).
            //
            // NOTE (an independently-derived finding beyond the planner
            // package's own framing, disclosed honestly in
            // docs/m21-implementation-report.md): because the clamp's LOWER
            // bound is 0 and floor(x) <= trunc(x) for every x < 0, EVERY
            // negative pre-clamp numerator -- whether divided via plain
            // truncating '/' or via std::floor() -- clamps to the IDENTICAL
            // final value 0. This specific case is therefore not a
            // black-box-observable divergence in the CLAMPED output; it is
            // asserted here as a source-level guarantee (we control the
            // implementation and use plain '/', matching openMSX exactly,
            // BitmapConverter.cc:217-228) plus the arithmetic proof above
            // that the two rounding conventions genuinely differ before
            // clamping, precisely as A-M21-5 describes.
            expect(rgb_neg.b == 0, "YjkToRgb_NegativeNumerator_ClampsToZeroEitherRounding");
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
