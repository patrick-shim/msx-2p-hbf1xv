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
#include <vector>

#include "frontend/master_volume.h"

// Suite: Frontend_MasterVolume_Unit (M52, DEC-0079, docs/m52-planner-package.md
// §2.1/§4.1 S1-1/S2-1).
//
// Pure, SDL-free tests for the master-gain + step helper. The deterministic
// oracle: a fixed input always maps to a fixed output. The load-bearing property
// is IDENTITY AT FULL VOLUME (apply_master_gain_sample(s,100)==s for EVERY int16,
// and apply_master_gain(buf,100) leaves the buffer byte-identical) -- the
// byte-identity guarantee that keeps the DEC-0039 mix unchanged at the default.

namespace {

using sony_msx::frontend::apply_master_gain;
using sony_msx::frontend::apply_master_gain_sample;
using sony_msx::frontend::clamp_master_volume;
using sony_msx::frontend::step_master_volume;

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // === S1-1: identity at 100 for EVERY int16 (the byte-identity oracle). ===
    {
        bool identity_all = true;
        for (int s = -32768; s <= 32767; ++s) {
            if (apply_master_gain_sample(static_cast<std::int16_t>(s), 100) !=
                static_cast<std::int16_t>(s)) {
                identity_all = false;
                break;
            }
        }
        expect(identity_all, "Sample_Identity_At100_ForEveryInt16");
    }

    // === Silence at 0: every sample -> 0. ===
    {
        expect(apply_master_gain_sample(32767, 0) == 0, "Sample_Silence_At0_MaxPos");
        expect(apply_master_gain_sample(-32768, 0) == 0, "Sample_Silence_At0_MaxNeg");
        expect(apply_master_gain_sample(12345, 0) == 0, "Sample_Silence_At0_Mid");
    }

    // === Mid-scale (50%): s*50/100 with truncation toward zero, no overflow. ===
    {
        expect(apply_master_gain_sample(1000, 50) == 500, "Sample_Mid50_1000to500");
        expect(apply_master_gain_sample(-1000, 50) == -500, "Sample_Mid50_Neg1000toNeg500");
        // Truncation (integer division toward zero): 101*50/100 = 5050/100 = 50.
        expect(apply_master_gain_sample(101, 50) == 50, "Sample_Mid50_101to50_Truncates");
        // Extreme magnitudes: no int32 overflow (|s*50| <= 1,638,400).
        expect(apply_master_gain_sample(32767, 50) == 16383, "Sample_Mid50_MaxPos_NoOverflow");
        expect(apply_master_gain_sample(-32768, 50) == -16384, "Sample_Mid50_MaxNeg_NoOverflow");
    }

    // === No overflow at extreme ±32767 across a range of volumes (attenuation
    //     only, so |scaled| <= |s| and the clamp never actually triggers). ===
    {
        bool bounded = true;
        for (int v = 0; v <= 100; ++v) {
            const std::int16_t hi = apply_master_gain_sample(32767, v);
            const std::int16_t lo = apply_master_gain_sample(-32768, v);
            if (hi > 32767 || hi < 0 || lo < -32768 || lo > 0) {
                bounded = false;
                break;
            }
        }
        expect(bounded, "Sample_Extremes_NeverOverflowOrAmplify_AllVolumes");
        // At 100 the extremes are exactly themselves (identity).
        expect(apply_master_gain_sample(32767, 100) == 32767 &&
                   apply_master_gain_sample(-32768, 100) == -32768,
               "Sample_Extremes_Identity_At100");
    }

    // === Volume clamp: out-of-domain requests are clamped to [0,100]. ===
    {
        expect(clamp_master_volume(-5) == 0 && clamp_master_volume(150) == 100 &&
                   clamp_master_volume(37) == 37,
               "ClampVolume_ToRange");
        // A >100 request behaves as unity (no amplification).
        expect(apply_master_gain_sample(20000, 150) == 20000, "Sample_Above100_ClampsToUnity");
    }

    // === Buffer path: no-op@100 (byte-identical), zero@0, attenuate@mid. ===
    {
        const std::vector<std::int16_t> original{-32768, -1000, -1, 0, 1, 1000, 32767, 12345};

        std::vector<std::int16_t> at100 = original;
        apply_master_gain(at100, 100);
        expect(at100 == original, "Buffer_At100_ByteIdentical_NoOp");

        std::vector<std::int16_t> at0 = original;
        apply_master_gain(at0, 0);
        expect(std::vector<std::int16_t>(original.size(), 0) == at0, "Buffer_At0_AllZero");

        std::vector<std::int16_t> at50 = original;
        apply_master_gain(at50, 50);
        std::vector<std::int16_t> expected50;
        expected50.reserve(original.size());
        for (const std::int16_t s : original) {
            expected50.push_back(apply_master_gain_sample(s, 50));
        }
        expect(at50 == expected50, "Buffer_At50_MatchesPerSampleGain");

        // Empty buffer is a safe no-op at any volume.
        std::vector<std::int16_t> empty;
        apply_master_gain(empty, 50);
        expect(empty.empty(), "Buffer_Empty_SafeNoOp");
    }

    // === S2-1: step_master_volume -- CLAMP not wrap, grid-snap toward direction. ===
    {
        expect(step_master_volume(70, -1, 10) == 60, "Step_70_Down_60");
        expect(step_master_volume(0, -1, 10) == 0, "Step_0_Down_ClampsAt0_NoWrapTo100");
        expect(step_master_volume(100, +1, 10) == 100, "Step_100_Up_ClampsAt100");
        expect(step_master_volume(55, -1, 10) == 50, "Step_55_Down_GridSnap_50");
        expect(step_master_volume(55, +1, 10) == 60, "Step_55_Up_GridSnap_60");
        // Additional on-grid + boundary behavior (no wrap at either end).
        expect(step_master_volume(60, +1, 10) == 70, "Step_60_Up_70");
        expect(step_master_volume(60, -1, 10) == 50, "Step_60_Down_50");
        expect(step_master_volume(95, +1, 10) == 100, "Step_95_Up_ClampsAt100");
        expect(step_master_volume(5, -1, 10) == 0, "Step_5_Down_ClampsAt0");
        expect(step_master_volume(0, +1, 10) == 10, "Step_0_Up_10");
        expect(step_master_volume(100, -1, 10) == 90, "Step_100_Down_90");
        // dir == 0 is a no-op (clamped into range).
        expect(step_master_volume(55, 0, 10) == 55, "Step_Dir0_NoChange");
        // A full down-then-up round trip from an over-held key saturates, never wraps.
        int v = 50;
        for (int i = 0; i < 20; ++i) {
            v = step_master_volume(v, -1, 10);
        }
        expect(v == 0, "Step_HeldDown_SaturatesAt0_NeverWraps");
        for (int i = 0; i < 20; ++i) {
            v = step_master_volume(v, +1, 10);
        }
        expect(v == 100, "Step_HeldUp_SaturatesAt100_NeverWraps");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MasterVolume_Unit cases passed\n";
    return 0;
}
