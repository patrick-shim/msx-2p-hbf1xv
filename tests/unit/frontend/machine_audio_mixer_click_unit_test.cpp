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

// Suite: Frontend_MachineAudioMixerClick_Unit (M39-A).
//
// Wires the MSX 1-bit key-click DAC (ClickDac) into MachineAudioMixer as a
// FIFTH additive source. Oracles:
//   1. THE M39 HARD ORACLE: with click == nullptr the new 5-source overload is
//      BYTE-IDENTICAL to the pre-M39 4-source overload for ANY input.
//   2. IDLE BYTE-IDENTITY: an ATTACHED but idle (no bit-7 edges) ClickDac
//      contributes exactly 0 -> also byte-identical (the mandatory "reset bit 7
//      = 0 -> existing oracles unchanged" guarantee).
//   3. CLICK CONTRIBUTES (non-vacuity): a ClickDac fed real edges adds an
//      audible term absent from the idle mix.
//   4. CLAMP: a full-scale click on top of a saturating PSG+SCC mix clamps to
//      the int16 range (the click term is included in the worst case).
//   5. produce_synced_sample (the Fix-B interleaved path): determinism, and a
//      nullptr-everything-but-PSG sample equals the batch's single-sample PSG
//      arithmetic.

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/click_dac.h"
#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
#include "frontend/machine_audio_mixer.h"

namespace {

using sony_msx::devices::audio::ClickDac;
using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
using sony_msx::devices::audio::Ym2413Opll;
using sony_msx::frontend::MachineAudioMixer;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;

void program_psg(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(25);
    psg.write_address(8);
    psg.write_data(12);
    psg.write_address(9);
    psg.write_data(7);
    psg.write_address(7);
    psg.write_data(0x3C);
}

void program_psg_max(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(7);
    psg.write_data(0x3F);  // tone+noise off -> audible at volume
    psg.write_address(8);
    psg.write_data(15);
    psg.write_address(9);
    psg.write_data(15);
    psg.write_address(10);
    psg.write_data(15);
}

void program_scc_max(SccWavetable& scc) {
    scc.reset();
    for (int i = 0; i < 32; ++i) {
        scc.write(static_cast<std::uint8_t>(i), 0x7F);  // max wave
    }
    scc.write(0x8A, 0x0F);
    scc.write(0x8F, 0x01);
    scc.write(0x80, 0x63);
}

}  // namespace

int main() {
    // --- 1. THE M39 HARD ORACLE: nullptr click == the 4-source overload. -----
    {
        PsgYm2149 psg_a;
        PsgYm2149 psg_b;
        program_psg(psg_a);
        program_psg(psg_b);
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> four = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr, 2000);
        const std::vector<std::int16_t> five = mixer.mix_interleaved_stereo(
            psg_b, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, static_cast<ClickDac*>(nullptr), 2000);
        expect(four == five, "NullClick_ByteIdenticalToFourSourceOverload_HardOracle");
        bool any_nonzero = false;
        for (const std::int16_t v : four) {
            if (v != 0) {
                any_nonzero = true;
                break;
            }
        }
        expect(any_nonzero, "NullClick_OracleInputNonSilent_NonVacuous");
    }

    // --- 2. IDLE BYTE-IDENTITY: an attached but idle ClickDac -> exactly 0. ---
    {
        PsgYm2149 psg_a;
        PsgYm2149 psg_b;
        program_psg(psg_a);
        program_psg(psg_b);
        ClickDac idle;
        idle.set_capture_enabled(true);
        idle.reset();  // bit 7 = 0, never toggled
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> no_click = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, static_cast<ClickDac*>(nullptr), 2000);
        const std::vector<std::int16_t> idle_click = mixer.mix_interleaved_stereo(
            psg_b, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, &idle, 2000);
        expect(no_click == idle_click, "IdleAttachedClick_ContributesExactlyZero_ByteIdentity");
    }

    // --- 3. CLICK CONTRIBUTES (non-vacuity): edges add an audible term. ------
    {
        PsgYm2149 psg_a;
        PsgYm2149 psg_b;
        psg_a.reset();  // silent
        psg_b.reset();
        ClickDac click;
        click.set_capture_enabled(true);
        click.reset();
        // ~50% duty square: toggle every ~160 cycles across the mixed window.
        bool level = true;
        for (std::uint64_t c = 0; c < 2000ull * kCyclesPerSample; c += 160) {
            click.record_edge(c, level);
            level = !level;
        }
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> without = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, static_cast<ClickDac*>(nullptr), 2000);
        const std::vector<std::int16_t> with = mixer.mix_interleaved_stereo(
            psg_b, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, &click, 2000);
        expect(without != with, "ClickWithEdges_ChangesTheMix_NonVacuous");
        std::int32_t max_abs = 0;
        for (const std::int16_t v : with) {
            const std::int32_t a = v < 0 ? -v : v;
            if (a > max_abs) {
                max_abs = a;
            }
        }
        expect(max_abs > 1000, "ClickWithEdges_AudiblyNonZero");
    }

    // --- 4. CLAMP: full-scale click + saturating PSG + two SCCs -> int16. -----
    {
        PsgYm2149 psg;
        program_psg_max(psg);
        SccWavetable scc1;
        SccWavetable scc2;
        program_scc_max(scc1);
        program_scc_max(scc2);
        ClickDac click;
        click.set_capture_enabled(true);
        click.reset();
        click.record_edge(0, true);  // rise -> the first sample is full-scale +kUnit
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> pcm = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{&scc1, &scc2}, nullptr,
            MachineAudioMixer::FmSources{nullptr, nullptr}, &click, 8);
        bool in_range = true;
        for (const std::int16_t v : pcm) {
            if (v < -32768 || v > 32767) {
                in_range = false;
            }
        }
        expect(in_range, "FullScaleClickPlusSaturatingMix_ClampedToInt16");
        // The first sample should be pinned at the positive rail (well past int16
        // without the clamp): PSG 24,800 + 2 SCC + click 9,374 >> 32,767.
        expect(pcm[0] == 32767, "FullScaleClickWorstCase_ClampsToPositiveRail");
    }

    // --- 5. produce_synced_sample: determinism + (sync on, no intra-window
    //     writes) equals the batch advance+take arithmetic. produce_synced_
    //     sample drives the PSG via sync_to_cycle (the Fix-B path), so sync MUST
    //     be enabled; with no register writes during production it advances by
    //     `window` per sample exactly like the batch advance_cycles path. -------
    {
        const auto run = [] {
            PsgYm2149 psg;
            program_psg(psg);
            psg.set_audio_sync_enabled(true);
            psg.reset_audio_sync(0);
            const MachineAudioMixer mixer(kCyclesPerSample);
            std::vector<std::int16_t> out;
            std::uint64_t boundary = 0;
            for (int i = 0; i < 500; ++i) {
                const std::uint64_t next = boundary + kCyclesPerSample;
                const std::array<std::int16_t, 2> s = mixer.produce_synced_sample(
                    psg, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
                    MachineAudioMixer::FmSources{nullptr, nullptr}, nullptr, next, kCyclesPerSample);
                out.push_back(s[0]);
                out.push_back(s[1]);
                boundary = next;
            }
            return out;
        };
        const std::vector<std::int16_t> a = run();
        const std::vector<std::int16_t> b = run();
        expect(a == b, "ProduceSyncedSample_TwoRuns_ByteIdentical");
        // Sync-on + no intra-window write: sync_to_cycle advances by `window`
        // (== advance_cycles), so produce_synced_sample matches the batch
        // single-sample arithmetic exactly.
        PsgYm2149 psg_batch;
        program_psg(psg_batch);
        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> batch = mixer.mix_interleaved_stereo(
            psg_batch, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr, 500);
        bool prefix_matches = true;
        for (std::size_t i = 0; i < 40; ++i) {
            if (a[i] != batch[i]) {
                prefix_matches = false;
            }
        }
        expect(prefix_matches, "ProduceSyncedSample_SyncOnNoWrites_EqualsBatchArithmetic");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerClick_Unit cases passed\n";
    return 0;
}
