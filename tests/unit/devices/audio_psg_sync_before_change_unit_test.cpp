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

// Suite: Devices_Audio_PsgSyncBeforeChange_Unit (the CONFIRMED
// digitized-voice fix).
//
// The scroll-shooter / split-screen-title voice is PSG SOFTWARE-PCM: a channel-volume register is
// hammered sub-frame so the volume sequence IS the PCM waveform. The box-
// average is produced once per frame in a batch using END-OF-FRAME register
// state, so every sub-frame volume change collapses (voice silent). The fix is
// a sync-before-change seam: before each register write mutates state, advance
// the box-average integral to the write's cycle using the PRE-write state.
//
// Setup uses tone+noise DISABLED on channel A (R7 bits 0+3), so the channel is
// audible at a CONSTANT level == its resolved amplitude (no tone modulation) --
// the DC-volume-PCM idiom. resolved_amplitude(vol=15) = 2*15+1 = 31,
// resolved_amplitude(vol=0) = 0 (psg_ym2149 fixed-level map: level 0 -> 0).
//
// Oracles:
//   1. DISABLED == plain advance (byte-identity foundation): with sync off,
//      write_register never syncs, so the integral matches a plain
//      advance_cycles() -- every existing PSG/mixer oracle is untouched.
//   2. SUB-FRAME CAPTURE: with sync on, a mid-window volume change is integrated
//      at its TRUE sub-frame position (V1*dwell1 + V2*dwell2), NOT collapsed to
//      the end-of-window value -- the exact arithmetic that makes the voice
//      survive.
//   3. NO-CHANGE EQUIVALENCE: sync on with no intermediate write == plain
//      advance (the fix adds nothing when nothing changes sub-frame).

#include <cstdint>
#include <iostream>

#include "devices/audio/psg_ym2149.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Fake cycle source under the test's control (the machine wires the scheduler).
struct FakeCycle final : public sony_msx::devices::audio::PsgCycleSource {
    std::uint64_t cycle = 0;
    [[nodiscard]] std::uint64_t current_cycle() const override { return cycle; }
};

// Channel A audible at a constant level == its volume (tone A + noise A off).
void setup_dc_channel_a(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(7);
    psg.write_data(0x3F);  // all tone+noise disabled -> channels audible at volume
    psg.write_address(9);
    psg.write_data(0);  // B volume 0
    psg.write_address(10);
    psg.write_data(0);  // C volume 0
}

std::uint64_t integral_a(const PsgYm2149& psg) {
    return psg.generator_snapshot().level_dwell_integral[0];
}

}  // namespace

int main() {
    constexpr std::uint64_t kWindow = 81;

    // --- 1. DISABLED == plain advance (byte-identity foundation). ------------
    {
        PsgYm2149 batch;   // sync OFF (default)
        PsgYm2149 synced;  // sync ON but no cycle source writes drive it here
        FakeCycle clk;
        setup_dc_channel_a(batch);
        setup_dc_channel_a(synced);
        synced.attach_audio_cycle_source(&clk);
        synced.set_audio_sync_enabled(true);
        synced.reset_audio_sync(0);

        batch.write_address(8);
        batch.write_data(15);  // vol 15 -> amp 31
        batch.advance_cycles(kWindow);

        clk.cycle = 0;
        synced.write_address(8);
        synced.write_data(15);            // sync_to_cycle(0) is a no-op
        synced.sync_to_cycle(kWindow);    // finalize the window (the take path)

        expect(integral_a(batch) == integral_a(synced),
               "SyncNoIntermediateChange_EqualsPlainAdvance_ByteIdentity");
        expect(integral_a(batch) == 31u * kWindow, "PlainAdvance_ExactIntegral_31xWindow");
    }

    // --- 2. SUB-FRAME CAPTURE: a mid-window volume change. --------------------
    //     BATCH (sync off): the whole window sees the FINAL volume (0 -> amp 0)
    //         -> integral == 0 (voice collapsed to silence).
    //     SYNC  (sync on):  vol 15 (amp 31) for [0,40), vol 0 (amp 0) for
    //         [40,81) -> integral == 31*40 == 1240 (voice survives).
    {
        PsgYm2149 batch;
        setup_dc_channel_a(batch);
        batch.write_address(8);
        batch.write_data(15);  // vol 15 at "cycle 0"
        batch.write_address(8);
        batch.write_data(0);  // vol 0 -- sync OFF, so no sub-frame capture
        batch.advance_cycles(kWindow);
        expect(integral_a(batch) == 0u, "Batch_SubFrameChangeCollapses_ToFinalValue");

        PsgYm2149 synced;
        FakeCycle clk;
        setup_dc_channel_a(synced);
        synced.attach_audio_cycle_source(&clk);
        synced.set_audio_sync_enabled(true);
        synced.reset_audio_sync(0);
        clk.cycle = 0;
        synced.write_address(8);
        synced.write_data(15);  // sync_to_cycle(0) no-op; vol 15 applies from 0
        clk.cycle = 40;
        synced.write_address(8);
        synced.write_data(0);           // sync_to_cycle(40) captures amp 31 over [0,40)
        synced.sync_to_cycle(kWindow);  // capture amp 0 over [40,81)
        expect(integral_a(synced) == 31u * 40u,
               "Sync_SubFrameChangeCaptured_AtTruePosition_VoiceSurvives");
        expect(integral_a(synced) != integral_a(batch),
               "Sync_DiffersFromBatch_WhenSubFrameChanges_NonVacuous");
    }

    // --- 3. MULTIPLE sub-frame writes accumulate exactly (a mini PCM burst). --
    {
        PsgYm2149 synced;
        FakeCycle clk;
        setup_dc_channel_a(synced);
        synced.attach_audio_cycle_source(&clk);
        synced.set_audio_sync_enabled(true);
        synced.reset_audio_sync(0);
        // amp 31 for [0,20), amp 0 for [20,50), amp 31 for [50,81).
        clk.cycle = 0;
        synced.write_address(8);
        synced.write_data(15);
        clk.cycle = 20;
        synced.write_address(8);
        synced.write_data(0);
        clk.cycle = 50;
        synced.write_address(8);
        synced.write_data(15);
        synced.sync_to_cycle(kWindow);
        expect(integral_a(synced) == 31u * 20u + 0u * 30u + 31u * 31u,
               "Sync_MultipleSubFrameWrites_AccumulateExactly");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Audio_PsgSyncBeforeChange_Unit cases passed\n";
    return 0;
}
