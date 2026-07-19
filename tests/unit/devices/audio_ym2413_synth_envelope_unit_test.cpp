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

// Suite: Devices_Ym2413SynthEnvelope_Unit
//
// Decay/release oracles are EXACT (fact-sheet §5's own formula): the
// expected native-sample counts are recomputed independently in-test by
// walking §5's global-counter/eg_shift/eg_select mechanism, and each
// measured duration is additionally tied to §5's closed form
// `cycles = (rate<60) ? (1<<(14-(rate/4)))*s[rate&3] : 63`,
// s = {127,102,85,73}, at event-period resolution. The select tables and
// s-values below are §5's own printed values (literals transcribed from
// the fact sheet, not from another emulator's output).
//
// Attack assertions are QUALITATIVE ONLY (monotonicity, termination, rate
// ordering, the 0-3/60-63 endpoint behaviours §5 itself states) -- the
// attack curve is a DISCLOSED APPROXIMATION (see ym2413_synth.h's
// mandatory disclosure block); no hardware-exact attack duration is claimed
// or asserted anywhere.

#include <cstdint>
#include <iostream>

#include "devices/audio/ym2413_opll.h"
#include "devices/audio/ym2413_synth.h"

namespace {

using sony_msx::devices::audio::Ym2413Opll;
using sony_msx::devices::audio::Ym2413Synth;
using EgState = Ym2413Synth::EgState;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

void write_reg(Ym2413Opll& opll, const std::uint8_t addr, const std::uint8_t value) {
    opll.write_address(addr);
    opll.write_data(value);
}

void tick(Ym2413Opll& opll) {
    opll.advance_cycles(Ym2413Synth::kMasterCyclesPerNativeSample);
}

constexpr int kCarrierSlot0 = 1;  // channel 0 carrier

// --- Independent in-test recomputation of §5's decay/release mechanism
// (fact-sheet formula, not implementation code): starting from attenuation
// `start_level` at global counter `start_counter`, returns the tick count
// until attenuation reaches 127. ---
long sim_fall_ticks(const int rate, const std::uint32_t start_counter, const int start_level) {
    // §5's four printed 8-entry select tables (4/8, 5/8, 6/8, 7/8).
    static const int kSelect[4][8] = {
        {0, 1, 0, 1, 0, 1, 0, 1},
        {0, 1, 0, 1, 1, 1, 0, 1},
        {0, 1, 1, 1, 0, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 1, 1},
    };
    long ticks = 0;
    std::uint32_t counter = start_counter;
    int level = start_level;
    while (level < 127) {
        int steps = 0;
        if (rate >= 60) {
            steps = 2;  // §5: 60-63 advance 2 levels per sample
        } else if (rate >= 4) {
            const int r4 = rate / 4;
            const int shift = (r4 <= 13) ? (13 - r4) : 0;
            const int mult = (r4 >= 14) ? (r4 - 13) : 0;
            if ((counter & ((1u << shift) - 1u)) == 0) {
                steps = kSelect[rate & 3][(counter >> shift) & 7] << mult;
            }
        }
        level += steps;
        ++counter;
        ++ticks;
        if (ticks > 3000000) {
            return -1;  // guard (rates 0-3 never complete)
        }
    }
    return ticks;
}

// §5's closed form (the Application Manual Table III-7 reconstruction).
long closed_form_native_samples(const int rate) {
    static const long kS[4] = {127, 102, 85, 73};
    if (rate >= 60) {
        return 63;
    }
    return (1L << (14 - (rate / 4))) * kS[rate & 3];
}

// Programs channel 0's user-patch carrier for an EG experiment and keys on:
// instant attack (AR=15), DR/SL/RR and flags as given. The modulator's
// AR=0 keeps it silent/idle-attacked throughout.
void program_and_key(Ym2413Opll& opll, const std::uint8_t car_reg01, const std::uint8_t car_reg05,
                     const std::uint8_t car_reg07, const std::uint16_t fnum,
                     const std::uint8_t block, const bool sus) {
    opll.reset();
    write_reg(opll, 0x00, 0x00);
    write_reg(opll, 0x01, car_reg01);
    write_reg(opll, 0x02, 0x3F);
    write_reg(opll, 0x03, 0x00);
    write_reg(opll, 0x04, 0x00);
    write_reg(opll, 0x05, car_reg05);
    write_reg(opll, 0x06, 0x00);
    write_reg(opll, 0x07, car_reg07);
    write_reg(opll, 0x10, static_cast<std::uint8_t>(fnum & 0xFF));
    write_reg(opll, 0x30, 0x00);
    write_reg(opll, 0x20, static_cast<std::uint8_t>(0x10 | (sus ? 0x20 : 0x00) |
                                                     ((block & 7) << 1) | ((fnum >> 8) & 1)));
}

void key_off(Ym2413Opll& opll, const std::uint16_t fnum, const std::uint8_t block, const bool sus) {
    write_reg(opll, 0x20, static_cast<std::uint8_t>((sus ? 0x20 : 0x00) | ((block & 7) << 1) |
                                                     ((fnum >> 8) & 1)));
}

long measure_release_ticks(Ym2413Opll& opll, const long limit) {
    long ticks = 0;
    while (opll.synth().eg_state(kCarrierSlot0) != EgState::Idle && ticks < limit) {
        tick(opll);
        ++ticks;
    }
    return (opll.synth().eg_state(kCarrierSlot0) == EgState::Idle) ? ticks : -1;
}

}  // namespace

int main() {
    // --- 1. RELEASE DURATION ORACLES, all four §5 select columns at rate
    //     base 24 (RR=6) via Rks 0..3, plus a faster rate-40 case and the
    //     rate-55 shift-floor boundary. Exact tick counts from the
    //     independent §5 mechanism recomputation; each also tied to the
    //     closed form at event-period resolution. ---
    {
        struct ReleaseCase {
            std::uint8_t rr;       // 4-bit release rate
            bool ksr;
            std::uint16_t fnum;
            std::uint8_t block;
            int expected_rate;     // 4*RR + Rks
            const char* name;
        };
        const ReleaseCase cases[] = {
            {6, false, 0, 0, 24, "Release_Rate24_SelectColumn0_ExactTicks"},
            {6, true, 256, 0, 25, "Release_Rate25_SelectColumn1_ExactTicks"},
            {6, true, 0, 1, 26, "Release_Rate26_SelectColumn2_ExactTicks"},
            {6, true, 256, 1, 27, "Release_Rate27_SelectColumn3_ExactTicks"},
            {10, false, 0, 0, 40, "Release_Rate40_ExactTicks"},
            {13, true, 256, 1, 55, "Release_Rate55_ShiftFloorBoundary_ExactTicks"},
        };
        for (const ReleaseCase& c : cases) {
            Ym2413Opll opll;
            // Carrier: sustained EG (so DR=0 just parks at 0 dB), AR=15,
            // KSR per case, RR per case.
            const auto reg01 = static_cast<std::uint8_t>(0x20 | (c.ksr ? 0x10 : 0x00) | 0x01);
            program_and_key(opll, reg01, 0xF0, c.rr, c.fnum, c.block, false);
            expect(opll.synth().eg_level(kCarrierSlot0) == 0,
                   "Release_Precondition_InstantAttackReachedZero");
            key_off(opll, c.fnum, c.block, false);
            expect(opll.synth().eg_state(kCarrierSlot0) == EgState::Release,
                   "Release_Precondition_StateIsRelease");
            const long measured = measure_release_ticks(opll, 4000000);
            const long expected = sim_fall_ticks(c.expected_rate, 0, 0);
            expect(measured == expected, c.name);
            // Tie to §5's own closed form: the mechanism total, rounded up
            // to the event period, IS the closed-form sample count.
            const int r4 = c.expected_rate / 4;
            const long period = 1L << ((r4 <= 13) ? (13 - r4) : 0);
            const long rounded_up = ((expected + period - 1) / period) * period;
            expect(rounded_up == closed_form_native_samples(c.expected_rate),
                   "Release_MechanismTotal_MatchesClosedForm");
        }
    }

    // --- 2. Rates 60-63: exactly 2 levels/sample -> 64 ticks from 0 to 127
    //     (the closed form's own 63-with-instant-clamp regime). ---
    {
        Ym2413Opll opll;
        program_and_key(opll, 0x31, 0xF0, 0x0F, 256, 7, false);  // RR=15, KSR=1, block 7
        key_off(opll, 256, 7, false);
        const long measured = measure_release_ticks(opll, 1000);
        expect(measured == 64, "Release_Rate60Plus_TwoLevelsPerSample_64Ticks");
    }

    // --- 3. SUS bit: release rate FORCED to 5 at key-off (§3). RR=1 would
    //     take sim(rate 4) ticks; with SUS=1 it must take sim(rate 20). ---
    {
        Ym2413Opll opll;
        program_and_key(opll, 0x21, 0xF0, 0x01, 0, 0, true);  // RR=1, SUS=1
        key_off(opll, 0, 0, true);
        const long measured = measure_release_ticks(opll, 4000000);
        expect(measured == sim_fall_ticks(20, 0, 0), "Sus_ReleaseRateForcedTo5_Rate20Duration");
        expect(measured != sim_fall_ticks(4, 0, 0), "Sus_NotTheRawRr1Duration");
    }

    // --- 4. FIRST-SEGMENT-SHORTER quirk (§5's global-counter consequence):
    //     the same release started at a different global-counter phase has
    //     a different (deterministic) duration. ---
    {
        const auto run_with_prelude = [](const int prelude_ticks) {
            Ym2413Opll opll;
            program_and_key(opll, 0x21, 0xF0, 0x06, 0, 0, false);  // RR=6 -> rate 24
            for (int i = 0; i < prelude_ticks; ++i) {
                tick(opll);  // counter advances; level parked at 0 dB
            }
            key_off(opll, 0, 0, false);
            return measure_release_ticks(opll, 4000000);
        };
        const long aligned = run_with_prelude(0);
        const long offset = run_with_prelude(100);
        expect(aligned == sim_fall_ticks(24, 0, 0), "GlobalCounterPhase_Aligned_MatchesSim");
        expect(offset == sim_fall_ticks(24, 100, 0), "GlobalCounterPhase_Offset100_MatchesSim");
        expect(aligned != offset, "GlobalCounterPhase_FirstSegmentLengthDiffers");
        expect(offset == run_with_prelude(100), "GlobalCounterPhase_Deterministic_TwoRuns");
    }

    // --- 5. ATTACK (the disclosed approximation -- qualitative
    //     assertions ONLY, per the header's non-cycle-exact disclosure). ---
    {
        // (a) AR=15 (rates 60-63): instant -- already at level 0 / Decay
        //     immediately after the key-on write, zero ticks needed.
        Ym2413Opll instant;
        program_and_key(instant, 0x21, 0xF0, 0x00, 0, 0, false);
        expect(instant.synth().eg_state(kCarrierSlot0) == EgState::Decay &&
                   instant.synth().eg_level(kCarrierSlot0) == 0,
               "Attack_Ar15_InstantToZeroAttenuation");

        // (b) AR=0 (rates 0-3): never advances -- still attacking at max
        //     attenuation (silent) after a long run.
        Ym2413Opll never;
        program_and_key(never, 0x21, 0x00, 0x00, 0, 0, false);  // AR=0
        bool silent = true;
        for (int i = 0; i < 100000; ++i) {
            tick(never);
            if (never.fm_sample() != 0) {
                silent = false;
            }
        }
        expect(never.synth().eg_state(kCarrierSlot0) == EgState::Attack &&
                   never.synth().eg_level(kCarrierSlot0) == 127 && silent,
               "Attack_Ar0_InfiniteNeverCompletes_Silent");

        // (c) Monotonic decrease + termination for a mid AR; (d) higher AR
        //     strictly not slower across the whole 1..14 range.
        long durations[15] = {};
        for (int ar = 1; ar <= 14; ++ar) {
            Ym2413Opll opll;
            program_and_key(opll, 0x21, static_cast<std::uint8_t>(ar << 4), 0x00, 0, 0, false);
            long ticks = 0;
            int last_level = 127;
            bool monotonic = true;
            while (opll.synth().eg_state(kCarrierSlot0) == EgState::Attack && ticks < 3000000) {
                tick(opll);
                const int level = opll.synth().eg_level(kCarrierSlot0);
                if (level > last_level) {
                    monotonic = false;
                }
                last_level = level;
                ++ticks;
            }
            expect(opll.synth().eg_state(kCarrierSlot0) == EgState::Decay,
                   "Attack_Terminates_ReachesDecay");
            if (ar == 4) {
                expect(monotonic, "Attack_MonotonicallyRising_NeverBacktracks");
            }
            durations[ar] = ticks;
        }
        bool ordering = true;
        for (int ar = 2; ar <= 14; ++ar) {
            if (durations[ar] > durations[ar - 1]) {
                ordering = false;
            }
        }
        expect(ordering, "Attack_HigherAr_StrictlyNotSlower");
    }

    // --- 6. EG-TYP: sustained holds at SL; percussive keeps falling at RR
    //     while keyed and eventually idles (§3). SL=2 -> boundary level 16
    //     (3 dB/step from top = 8 EG levels per SL step). ---
    {
        Ym2413Opll sustained;
        program_and_key(sustained, 0x21, 0xF4, 0x26, 0, 0, false);  // EG-TYP=1, DR=4, SL=2, RR=6
        for (int i = 0; i < 400000; ++i) {
            tick(sustained);
        }
        expect(sustained.synth().eg_state(kCarrierSlot0) == EgState::Sustain,
               "EgTyp_Sustained_HoldsInSustain");
        const int held = sustained.synth().eg_level(kCarrierSlot0);
        expect(held >= 16 && held <= 17, "EgTyp_Sustained_LevelAtSlBoundary");
        for (int i = 0; i < 100000; ++i) {
            tick(sustained);
        }
        expect(sustained.synth().eg_level(kCarrierSlot0) == held,
               "EgTyp_Sustained_LevelFrozenWhileKeyed");

        Ym2413Opll percussive;
        program_and_key(percussive, 0x01, 0xF4, 0x26, 0, 0, false);  // EG-TYP=0
        long ticks = 0;
        while (percussive.synth().eg_state(kCarrierSlot0) != EgState::Idle && ticks < 3000000) {
            tick(percussive);
            ++ticks;
        }
        expect(percussive.synth().eg_state(kCarrierSlot0) == EgState::Idle,
               "EgTyp_Percussive_DecaysToSilenceWhileKeyed");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Ym2413SynthEnvelope_Unit cases passed\n";
    return 0;
}
