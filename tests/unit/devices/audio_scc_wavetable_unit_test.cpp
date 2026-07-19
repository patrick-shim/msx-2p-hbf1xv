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

#include "devices/audio/scc_wavetable.h"

// Suite: Devices_SccWavetable_Unit (M29-S2, backlog G1)
//
// Every case is hand-computable from the measured formulas in
// references/fact-sheets/Konami SCC.md (De Schrijder amplitude law §5,
// Pazos deformation semantics §6, NYYRIKKI latching/restart/low-period stop
// §4, enen power-on state §7) -- no oracle below is transcribed from
// openMSX/fMSX output (license isolation; both were behaviour references
// only).

namespace {

using sony_msx::devices::audio::SccWavetable;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Fresh, reset chip.
SccWavetable make_chip() {
    SccWavetable scc;
    scc.reset();
    return scc;
}

}  // namespace

int main() {
    // --- reset(): the enen-measured power-on state (fact-sheet §7). ---
    {
        SccWavetable scc = make_chip();
        bool wave_all_ff = true;
        for (int ch = 0; ch < 5; ++ch) {
            for (int i = 0; i < 32; ++i) {
                if (static_cast<std::uint8_t>(scc.wave(ch, i)) != 0xFF) {
                    wave_all_ff = false;
                }
            }
        }
        expect(wave_all_ff, "Reset_WaveRam_AllFF");
        bool state_ok = true;
        for (int ch = 0; ch < 5; ++ch) {
            if (scc.volume(ch) != 15 || scc.period(ch) != 0 || scc.org_period(ch) != 0 ||
                scc.position(ch) != 0) {
                state_ok = false;
            }
            // Held output refreshed from the power-on state:
            // (int8(0xFF) * 15) >> 4 = (-15) >> 4 = -1 (arithmetic shift).
            if (scc.held_output(ch) != -1) {
                state_ok = false;
            }
        }
        expect(state_ok, "Reset_VolumesFull_PeriodsZero_PositionsZero_HeldOutputsRefreshed");
        expect(scc.enable_bits() == 0, "Reset_ChannelEnable_Zero");
        expect(scc.deform() == 0, "Reset_Deform_Zero");
        // enable = 0 => zero AC contribution => amp_out is the bare DC centre.
        expect(scc.sample() == 0 && scc.amp_out() == 640, "Reset_AllDisabled_AmpOutIsDcCentre640");
        // Periods 0 (< 9) => channels stopped: advancing does nothing.
        scc.advance_cycles(100000);
        expect(scc.position(0) == 0, "Reset_PeriodUnder9_CounterStopped");
    }

    // --- Wave write/readback, 0x00-0x7F (read() and peek() agree outside
    //     the deform range). ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i));
        }
        bool readback_ok = true;
        for (int i = 0; i < 32; ++i) {
            if (scc.read(static_cast<std::uint8_t>(i)) != i) {
                readback_ok = false;
            }
            if (scc.peek(static_cast<std::uint8_t>(i)) != i) {
                readback_ok = false;
            }
        }
        expect(readback_ok, "Wave_WriteThenRead_Ch1_RoundTrips");
        // Other channels' regions unaffected (still power-on 0xFF).
        expect(scc.read(0x20) == 0xFF && scc.read(0x40) == 0xFF, "Wave_OtherChannels_Untouched");
    }

    // --- Ch4 write mirrors into ch5's playback copy (fact-sheet §3,
    //     double-grounded: no independent ch5 wave RAM on the plain SCC). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x60, 0x20);  // ch4 wave[0] = +32 -- also ch5's wave[0]
        expect(scc.wave(3, 0) == 0x20 && scc.wave(4, 0) == 0x20, "Ch4Write_LandsInCh5PlaybackCopy");
        // Audible through ch5 alone: period ch5 = 99, vol ch5 = 15, enable
        // bit4 only. The period write refreshes ch5's held output
        // immediately: (32*15)>>4 = 30.
        scc.write(0x8E, 0x0F);  // ch5 volume
        scc.write(0x8F, 0x10);  // enable ch5 only
        scc.write(0x88, 0x63);  // ch5 period low = 99 -> restart + refresh
        expect(scc.sample() == 30, "Ch5PlaysCh4Waveform_SampleMatchesHandComputedValue");
    }

    // --- Freq/vol/enable block x2 mirror at +0x90 (fact-sheet §3). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x90, 0x63);  // mirror of 0x80: ch1 period low
        expect(scc.period(0) == 0x63 && scc.org_period(0) == 0x63, "Mirror90_PeriodWrite_Lands");
        scc.write(0x9A, 0x07);  // mirror of 0x8A: ch1 volume
        expect(scc.volume(0) == 7, "Mirror9A_VolumeWrite_Lands");
        scc.write(0x9F, 0x15);  // mirror of 0x8F: enable
        expect(scc.enable_bits() == 0x15, "Mirror9F_EnableWrite_Lands");
    }

    // --- 12-bit period assembly + high-nibble masking (fact-sheet §3). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x80, 0x34);
        scc.write(0x81, 0xFA);  // bits 4-7 (the 0xF) ignored -> high nibble 0xA
        expect(scc.org_period(0) == 0x0A34 && scc.period(0) == 0x0A34,
               "PeriodAssembly_HighWriteBits47Ignored_12BitValue");
        // Low rewrite preserves the high nibble.
        scc.write(0x80, 0x00);
        expect(scc.org_period(0) == 0x0A00, "PeriodAssembly_LowRewrite_PreservesHighNibble");
    }

    // --- Reads of 0x80-0x9F and 0xA0-0xDF return 0xFF (fact-sheet §3). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x80, 0x63);  // latch something -- must still read 0xFF
        bool all_ff = true;
        for (int off = 0x80; off < 0xE0; ++off) {
            if (scc.read(static_cast<std::uint8_t>(off)) != 0xFF) {
                all_ff = false;
            }
        }
        expect(all_ff, "Reads_0x80To0xDF_AllReturnFF");
        expect(scc.deform() == 0, "Reads_Below0xE0_NeverTouchDeform");
    }

    // --- Period <= 8 freezes stepping (NYYRIKKI, fact-sheet §4/§9.2). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x80, 8);  // period 8 -> stopped
        scc.advance_cycles(1000000);
        expect(scc.position(0) == 0, "PeriodEight_CounterStopped_PositionHeld");
        scc.write(0x80, 9);  // period 9 -> runs: one step per 10 cycles
        scc.advance_cycles(25);
        expect(scc.position(0) == 2, "PeriodNine_Runs_TwoStepsIn25Cycles");
    }

    // --- Period-write restart: counter reset + immediate output refresh
    //     (NYYRIKKI, fact-sheet §4). ---
    {
        SccWavetable scc = make_chip();
        // Wave ch1 all +16; period 99 (step every 100 cycles); vol 15; enable ch1.
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), 0x10);
        }
        scc.write(0x8A, 0x0F);
        scc.write(0x8F, 0x01);
        scc.write(0x80, 0x63);  // period 99; restart; out = (16*15)>>4 = 15
        expect(scc.sample() == 15, "FreqWrite_RefreshesHeldOutputImmediately");

        scc.advance_cycles(150);  // one step at 100; intra-byte count now 50
        expect(scc.position(0) == 1, "Advance150_OneStepTaken");
        scc.write(0x80, 0x63);  // SAME period rewritten -> counter reset to 0
        scc.advance_cycles(99);
        expect(scc.position(0) == 1, "FreqRewrite_ResetsIntraByteCounter_NoStepAt99");
        scc.advance_cycles(1);
        expect(scc.position(0) == 2, "FreqRewrite_StepLandsExactlyAtPeriodPlus1");
    }

    // --- Wave-write latency + volume-latch-at-next-step (fact-sheet §4). ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), 0x10);  // all +16
        }
        scc.write(0x8A, 0x0F);
        scc.write(0x8F, 0x01);
        scc.write(0x80, 0x63);  // out = (16*15)>>4 = 15
        // Overwrite the CURRENTLY-PLAYING byte: not audible yet (held).
        scc.write(0x00, 0x20);  // wave[0] = +32
        expect(scc.sample() == 15, "WaveWriteToPlayingByte_HeldUntilNextEvent");
        // A frequency write refreshes immediately: (32*15)>>4 = 30.
        scc.write(0x80, 0x63);
        expect(scc.sample() == 30, "WaveWrite_BecomesAudibleAtFreqWriteRefresh");
        // Volume write: NOT retroactive...
        scc.write(0x8A, 0x08);
        expect(scc.sample() == 30, "VolumeWrite_NotAppliedRetroactively");
        // ...applied at the next position step: wave[1] = +16, vol 8 ->
        // (16*8)>>4 = 8.
        scc.advance_cycles(100);
        expect(scc.position(0) == 1 && scc.sample() == 8, "VolumeWrite_AppliedAtNextPositionStep");
    }

    // --- Enable bits gate the contribution to 0 while phase keeps running
    //     (fact-sheet §4). ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), 0x10);
        }
        scc.write(0x8A, 0x0F);
        scc.write(0x8F, 0x00);  // ch1 DISABLED
        scc.write(0x80, 0x63);
        expect(scc.sample() == 0, "DisabledChannel_ContributesZero");
        scc.advance_cycles(500);
        expect(scc.position(0) == 5, "DisabledChannel_PhaseKeepsRunning");
        scc.write(0x8F, 0x01);  // re-enable: held output is live again
        expect(scc.sample() == 15, "ReEnabledChannel_HeldOutputContributesAgain");
    }

    // --- The De Schrijder oracle, reproduced literally (fact-sheet §5):
    //     SampleValue=+1, Vol=15, all five channels enabled => AmpOut=640
    //     (each per-channel product 15 truncates to 0 BEFORE summation). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x00, 0x01);  // ch1 wave[0] = +1
        scc.write(0x20, 0x01);  // ch2
        scc.write(0x40, 0x01);  // ch3
        scc.write(0x60, 0x01);  // ch4 (and thereby ch5)
        for (std::uint8_t v = 0x8A; v <= 0x8E; ++v) {
            scc.write(v, 0x0F);  // volumes 15
        }
        scc.write(0x8F, 0x1F);  // all five enabled
        for (std::uint8_t f = 0x80; f <= 0x89; ++f) {
            scc.write(f, 0x00);  // period writes refresh every held output
        }
        expect(scc.amp_out() == 640, "DeSchrijder_FiveChannelsPlusOneVol15_AmpOut640_Literal");

        // Companion positive case: SampleValue=+16, Vol=15 => per-channel
        // (16*15)>>4 = 15, five channels => 640+75 = 715.
        scc.write(0x00, 0x10);
        scc.write(0x20, 0x10);
        scc.write(0x40, 0x10);
        scc.write(0x60, 0x10);
        for (std::uint8_t f = 0x80; f <= 0x89; ++f) {
            scc.write(f, 0x00);
        }
        expect(scc.amp_out() == 715, "DeSchrijder_PlusSixteenVol15_AmpOut715");

        // Companion negative case (signed floor semantics): SampleValue=-1,
        // Vol=15 => (-15)>>4 = -1 per channel, five channels => 640-5 = 635.
        scc.write(0x00, 0xFF);
        scc.write(0x20, 0xFF);
        scc.write(0x40, 0xFF);
        scc.write(0x60, 0xFF);
        for (std::uint8_t f = 0x80; f <= 0x89; ++f) {
            scc.write(f, 0x00);
        }
        expect(scc.amp_out() == 635, "DeSchrijder_MinusOneVol15_AmpOut635_SignedTruncation");
    }

    // --- Deform bits 0/1: 4-bit / 8-bit frequency modes, bit1 wins;
    //     masking applies at frequency-WRITE time (fact-sheet §6). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0xE0, 0x01);  // 4-bit mode
        scc.write(0x80, 0x34);
        scc.write(0x81, 0x0A);  // org 0x0A34 -> effective 0x0A34 >> 8 = 10
        expect(scc.org_period(0) == 0x0A34 && scc.period(0) == 10,
               "DeformBit0_FourBitMode_EffectivePeriodTop4Bits");

        scc.write(0xE0, 0x02);  // 8-bit mode
        scc.write(0x81, 0x0A);  // re-latch -> effective 0x0A34 & 0xFF = 0x34
        expect(scc.period(0) == 0x34, "DeformBit1_EightBitMode_EffectivePeriodLow8Bits");

        scc.write(0xE0, 0x03);  // both set -> bit1 wins (fact-sheet §6 addition)
        scc.write(0x81, 0x0A);
        expect(scc.period(0) == 0x34, "DeformBits0And1_Bit1Wins");

        // An already-latched period is not retroactively re-masked by a
        // deform change alone (openMSX SCC.cc:385-392 shape, disclosed in
        // the header): clearing deform leaves the effective period until the
        // next frequency write.
        scc.write(0xE0, 0x00);
        expect(scc.period(0) == 0x34, "DeformCleared_EffectivePeriodUnchangedUntilNextFreqWrite");
        scc.write(0x81, 0x0A);
        expect(scc.period(0) == 0x0A34, "NextFreqWrite_RelatchesUnmaskedPeriod");
    }

    // --- Deform bit5: waveform restarts from position 0 on frequency
    //     writes (fact-sheet §6). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x80, 0x63);  // period 99
        scc.advance_cycles(500);
        expect(scc.position(0) == 5, "DeformBit5Setup_PositionAdvanced");
        scc.write(0xE0, 0x20);  // bit5 on
        scc.write(0x80, 0x63);  // freq write -> position resets to 0
        expect(scc.position(0) == 0, "DeformBit5_FreqWrite_RestartsWaveformFromPosition0");
    }

    // --- Deform bits 6/7: rotation readback + read-only wave RAM +
    //     the plain-SCC ch4-at-ch5's-period quirk (fact-sheet §6). Rotation
    //     oracles drive advance_cycles() explicitly (A-M29-6 / R-M29-8). ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i));         // ch1: byte i == i
            scc.write(static_cast<std::uint8_t>(0x60 + i), static_cast<std::uint8_t>(i));  // ch4: byte i == i
        }
        scc.write(0x80, 19);  // ch1 period 19 -> one rotation shift per 20 cycles
        scc.write(0x88, 9);   // ch5 period 9  -> one rotation shift per 10 cycles
        scc.write(0xE0, 0x40);  // rotate ALL; wave RAM read-only; timer resync
        scc.advance_cycles(40);
        // ch1 shift = 40 / (19+1) = 2 -> reading offset 0 yields byte 2.
        expect(scc.read(0x00) == 2, "DeformBit6_RotationShiftedRead_MeasuredPazosRate");
        // ch4 quirk: rotates at CHANNEL 5's period (shift = 40/(9+1) = 4),
        // ignoring its own (unset, period 0 -> would be shift 40).
        expect(scc.read(0x60) == 4, "DeformBit6_Ch4RotatesAtCh5Period_PlainSccQuirk");
        // Wave RAM is read-only in rotation mode.
        scc.write(0x05, 0x77);
        expect(scc.wave(0, 5) == 5, "DeformBit6_WaveRamReadOnly_WriteBlocked");
        // Rewriting the SAME deform value is a no-op (no timer resync).
        scc.write(0xE0, 0x40);
        expect(scc.read(0x00) == 2, "DeformSameValueRewrite_NoTimerResync");
        // A CHANGED deform value resyncs the rotation time base to 0.
        scc.write(0xE0, 0xC0);
        expect(scc.read(0x60) == 0, "DeformChange_ResyncsRotationTimeBase");
    }
    {
        // bits6+7 (0xC0): only ch1-3 rotate; ALL wave RAM read-only.
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i));
            scc.write(static_cast<std::uint8_t>(0x60 + i), static_cast<std::uint8_t>(i));
        }
        scc.write(0x80, 19);
        scc.write(0xE0, 0xC0);
        scc.advance_cycles(40);
        expect(scc.read(0x00) == 2, "DeformBits67_Ch1To3StillRotate");
        expect(scc.read(0x60) == 0, "DeformBits67_Ch4DoesNotRotate");
        scc.write(0x65, 0x77);  // ch4 RAM is read-only despite not rotating
        expect(scc.wave(3, 5) == 5, "DeformBits67_AllWaveRamReadOnly");
    }
    {
        // bit7 alone (0x80): rotate ch4(/5) only; ch1-3 stay writable.
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(0x60 + i), static_cast<std::uint8_t>(i));
        }
        scc.write(0x86, 19);    // ch4's OWN period governs outside all-rotate mode
        scc.write(0xE0, 0x80);
        scc.advance_cycles(40);
        expect(scc.read(0x60) == 2, "DeformBit7_Ch4RotatesAtOwnPeriod");
        scc.write(0x00, 0x55);  // ch1 RAM still writable
        expect(scc.wave(0, 0) == 0x55, "DeformBit7_Ch1To3StillWritable");
        scc.write(0x65, 0x77);  // ch4 RAM read-only
        expect(scc.wave(3, 5) == 5, "DeformBit7_Ch4RamReadOnly");
    }

    // --- Deform-range READ acts as write 0xFF and returns 0xFF (Pazos,
    //     fact-sheet §3/§6); peek() has no side effect. ---
    {
        SccWavetable scc = make_chip();
        expect(scc.peek(0xE0) == 0xFF && scc.deform() == 0, "DeformPeek_NoSideEffect");
        const std::uint8_t value = scc.read(0xE7);  // anywhere in 0xE0-0xFF
        expect(value == 0xFF, "DeformRead_ReturnsFF");
        expect(scc.deform() == 0xFF, "DeformRead_ActsAsWriteFF");
        // 0xFF & 0xC0 == 0xC0: ch1-3 rotate, all wave RAM read-only.
        scc.write(0x00, 0x55);
        expect(static_cast<std::uint8_t>(scc.wave(0, 0)) == 0xFF,
               "DeformRead_SideEffectMakesWaveRamReadOnly");
    }

    // --- Two-instance determinism: identical sequences => identical
    //     outputs (tests/CLAUDE.md determinism requirement). ---
    {
        auto run = [] {
            SccWavetable scc;
            scc.reset();
            std::vector<std::int32_t> outputs;
            for (int i = 0; i < 32; ++i) {
                scc.write(static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i * 5));
                scc.write(static_cast<std::uint8_t>(0x60 + i), static_cast<std::uint8_t>(255 - i));
            }
            scc.write(0x8A, 0x0C);
            scc.write(0x8E, 0x09);
            scc.write(0x8F, 0x11);  // ch1 + ch5
            scc.write(0x80, 0x2B);  // ch1 period 43
            scc.write(0x88, 0x17);  // ch5 period 23
            for (int i = 0; i < 200; ++i) {
                scc.advance_cycles(81);
                outputs.push_back(scc.sample());
            }
            outputs.push_back(scc.amp_out());
            outputs.push_back(scc.read(0xF0));  // deform read side effect included
            outputs.push_back(scc.read(0x00));
            return outputs;
        };
        expect(run() == run(), "TwoInstanceDeterminism_IdenticalSequences_IdenticalOutputs");
    }

    // =====================================================================
    // M34 additive take_integrated_sample() cases (DEC-0043 Defect A,
    // docs/m34-planner-package.md §2.6.5). Every oracle below is dwell
    // arithmetic authored by hand BEFORE execution (R-M34-9). Boundary
    // convention (§2.3.3): a position step completing at cycle t changes
    // the held output effective AFTER cycle t.
    // =====================================================================

    // --- M34: a STOPPED channel (period <= 8) holds its output constant ->
    //     integrates to EXACTLY the held constant (negative fixed point:
    //     power-on held output is (int8(0xFF)*15)>>4 = -1). ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x8F, 0x01);  // enable ch1 (period still 0 -> stopped)
        scc.advance_cycles(81);
        expect(scc.take_integrated_sample(81) == -1,
               "M34_StoppedChannel_IntegratesToExactHeldConstant_Neg1");
        // A stopped DISABLED channel contributes exactly 0.
        scc.write(0x8F, 0x00);
        scc.advance_cycles(81);
        expect(scc.take_integrated_sample(81) == 0,
               "M34_StoppedDisabledChannel_ContributesExactlyZero");
    }

    // --- M34: fast-stepping channel, period 9 (8 whole steps per 81-cycle
    //     window, §2.6.5 hand oracle). Ch1 wave = ramp v[p] = 8p (p 0..15),
    //     volume 15 -> held out(p) = (8p*15)>>4 = floor(7.5p):
    //     out(0..8) = 0,7,15,22,30,37,45,52,60. Period 9 -> a position step
    //     every 10 master cycles; the period write restarts count and
    //     refreshes out at pos 0. Window cycles [1..81]: dwell 10 each at
    //     out(0..7) (steps complete at cycles 10,20,...,80) + 1 cycle at
    //     out(8): integral = 10*(0+7+15+22+30+37+45+52) + 60 = 2080+60 =
    //     2140 -> round(2140/81) = round(26.42) = 26. Final phase: pos 8,
    //     count 1. ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i),
                      static_cast<std::uint8_t>((i < 16 ? 8 * i : 0)));
        }
        scc.write(0x8A, 0x0F);  // ch1 volume 15
        scc.write(0x8F, 0x01);  // enable ch1
        scc.write(0x80, 0x09);  // ch1 period 9 (running; restart + refresh)
        scc.write(0x81, 0x00);
        scc.advance_cycles(81);
        expect(scc.take_integrated_sample(81) == 26,
               "M34_Period9_EightStepsPerWindow_HandComputedDwellAverage26");
        expect(scc.position(0) == 8 && scc.held_output(0) == 60,
               "M34_Period9_PhaseStateMatchesBulkAdvanceSemantics");
    }

    // --- M34: enable gating inside the integral matches sample() -- the
    //     SAME running ramp DISABLED integrates to 0 while the phase keeps
    //     stepping. ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i),
                      static_cast<std::uint8_t>((i < 16 ? 8 * i : 0)));
        }
        scc.write(0x8A, 0x0F);
        scc.write(0x8F, 0x00);  // ch1 NOT enabled
        scc.write(0x80, 0x09);
        scc.write(0x81, 0x00);
        scc.advance_cycles(81);
        expect(scc.take_integrated_sample(81) == 0 && scc.position(0) == 8,
               "M34_DisabledRunningChannel_IntegralZero_PhaseStillRuns");
    }

    // --- M34: constant-wave running channel is a fixed point: uniform wave
    //     0x40 -> held (64*15)>>4 = 60 at every position; period 99 steps
    //     never change the level -> integrated == point == 60 exactly. ---
    {
        SccWavetable scc = make_chip();
        for (int i = 0; i < 32; ++i) {
            scc.write(static_cast<std::uint8_t>(i), 0x40);
        }
        scc.write(0x8A, 0x0F);
        scc.write(0x8F, 0x01);
        scc.write(0x80, 0x63);  // period 99
        scc.write(0x81, 0x00);
        scc.advance_cycles(81);
        const std::int32_t integrated = scc.take_integrated_sample(81);
        expect(integrated == 60 && integrated == scc.sample(),
               "M34_ConstantWaveRunning_FixedPoint_IntegratedEqualsPoint60");
    }

    // --- M34: W=0 guard (§2.3.5) + discard semantics. ---
    {
        SccWavetable scc = make_chip();
        scc.write(0x8F, 0x01);  // enabled stopped ch1, held -1
        expect(scc.take_integrated_sample(0) == 0, "M34_ZeroWindow_ReturnsZero");
        scc.advance_cycles(40);
        expect(scc.take_integrated_sample(0) == 0, "M34_ZeroWindowTake_DiscardsAccumulation");
        scc.advance_cycles(81);
        expect(scc.take_integrated_sample(81) == -1, "M34_AfterDiscard_NextWindowClean");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_SccWavetable_Unit cases passed\n";
    return 0;
}
