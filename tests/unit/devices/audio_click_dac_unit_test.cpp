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

// Suite: Devices_Audio_ClickDac_Unit (the MSX 1-bit key-click DAC).
//
// The ClickDac is the additive band-limited source for PPI port-C bit-7 edges
// (openMSX KeyClick -> DACSound; ClickDac.h). Oracles:
//   1. IDLE BYTE-IDENTITY (mandatory): after reset(), with bit 7 never toggled,
//      every take is EXACTLY 0 -- so the mixer's click term is 0 and every
//      existing audio oracle stays byte-identical.
//   2. CAPTURE-DISABLED: record_edge() is a no-op -> also exactly 0.
//   3. BOX AVERAGE: a rising edge held for a full window integrates to kUnit
//      (full scale), hand-checkable.
//   4. AC-COUPLING (held -> 0): a held level decays to ~0 (the DC-blocker),
//      mirroring openMSX's blip-delta AC coupling; a rapid toggle stays loud.
//   5. DETERMINISM: identical edge sequences -> identical samples.

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/click_dac.h"

namespace {

using sony_msx::devices::audio::ClickDac;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kWindow = 81;

}  // namespace

int main() {
    // --- 1. IDLE BYTE-IDENTITY: reset, capture on, NO edges -> exactly 0. -----
    {
        ClickDac dac;
        dac.set_capture_enabled(true);
        dac.reset();
        bool all_zero = true;
        for (int i = 0; i < 4000; ++i) {
            dac.advance_cycles(kWindow);
            if (dac.take_integrated_sample(kWindow) != 0) {
                all_zero = false;
                break;
            }
        }
        expect(all_zero, "IdleNoEdges_EveryTakeExactlyZero_ByteIdentity");
        expect(dac.edge_count() == 0, "IdleNoEdges_EdgeCountZero");
    }

    // --- 2. CAPTURE DISABLED: record_edge is a no-op -> exactly 0. ------------
    {
        ClickDac dac;
        dac.reset();  // capture disabled by default
        dac.record_edge(10, true);
        dac.record_edge(50, false);
        expect(dac.edge_count() == 0, "CaptureDisabled_RecordEdgeNoOp_NoEdges");
        bool all_zero = true;
        for (int i = 0; i < 100; ++i) {
            dac.advance_cycles(kWindow);
            if (dac.take_integrated_sample(kWindow) != 0) {
                all_zero = false;
            }
        }
        expect(all_zero, "CaptureDisabled_EveryTakeExactlyZero");
    }

    // --- 3. BOX AVERAGE: a rising edge at cycle 0, held for a full 100-cycle
    //     window, integrates to EXACTLY kUnit (full scale). Hand-checkable. ---
    {
        ClickDac dac;
        dac.set_capture_enabled(true);
        dac.reset();
        dac.record_edge(0, true);  // rising to 0xFF at cycle 0
        dac.advance_cycles(100);
        const std::int32_t s0 = dac.take_integrated_sample(100);
        expect(s0 == ClickDac::kUnit, "RisingEdgeHeldFullWindow_IntegratesToKUnit");
        expect(dac.edge_count() == 1, "RisingEdge_EdgeCountOne");
    }

    // --- 4. AC-COUPLING: hold the level HIGH forever -> the DC-blocker decays
    //     the contribution to ~0 (a held level is silent, mirroring openMSX);
    //     the first sample is full-scale, a far-later sample is ~0. ----------
    {
        ClickDac dac;
        dac.set_capture_enabled(true);
        dac.reset();
        dac.record_edge(0, true);  // rise once, then hold high forever
        dac.advance_cycles(100);
        const std::int32_t first = dac.take_integrated_sample(100);
        std::int32_t last = 0;
        for (int i = 0; i < 40000; ++i) {
            dac.advance_cycles(kWindow);
            last = dac.take_integrated_sample(kWindow);
        }
        expect(first == ClickDac::kUnit, "HeldHigh_FirstSampleFullScale");
        expect(last >= -2 && last <= 2, "HeldHigh_DecaysToApproxZero_AcCoupled");
    }

    // --- 4b. A RAPID square-wave toggle stays audibly non-zero (voice). ------
    {
        ClickDac dac;
        dac.set_capture_enabled(true);
        dac.reset();
        // ~50% duty square at ~11 kHz (toggle every ~160 cycles).
        bool level = true;
        std::uint64_t c = 0;
        for (int e = 0; e < 4000; ++e) {
            dac.record_edge(c, level);
            level = !level;
            c += 160;
        }
        std::int32_t max_abs = 0;
        for (int i = 0; i < 3000; ++i) {
            dac.advance_cycles(kWindow);
            const std::int32_t s = dac.take_integrated_sample(kWindow);
            const std::int32_t a = s < 0 ? -s : s;
            if (a > max_abs) {
                max_abs = a;
            }
        }
        // A ~50% duty toggle at kHz rates keeps substantial AC content.
        expect(max_abs > ClickDac::kUnit / 8, "RapidToggle_StaysAudiblyNonZero");
    }

    // --- 5. DETERMINISM: two identical edge sequences -> identical samples. ---
    {
        const auto run = [] {
            ClickDac dac;
            dac.set_capture_enabled(true);
            dac.reset();
            bool level = true;
            std::uint64_t c = 5;
            for (int e = 0; e < 1000; ++e) {
                dac.record_edge(c, level);
                level = !level;
                c += 137;  // irregular
            }
            std::vector<std::int32_t> out;
            for (int i = 0; i < 2000; ++i) {
                dac.advance_cycles(kWindow);
                out.push_back(dac.take_integrated_sample(kWindow));
            }
            return out;
        };
        expect(run() == run(), "TwoRuns_ByteIdenticalSamples");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Audio_ClickDac_Unit cases passed\n";
    return 0;
}
