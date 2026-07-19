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

// Suite: Devices_Ym2413SynthRhythmLfo_Unit
//
// Oracles:
//   - the x2 DOUBLE-OUTPUT quirk as an exact gain law (fact-sheet §6 quirk
//     1 / §7 "output twice ... effectively +6 dB"): a BD note must be
//     sample-for-sample exactly 2x the identical 2-op melody note (channel
//     6, same patch bytes, same registers, same global-counter phase);
//   - §6 keying/commitment semantics ($0E bits, melody ch6-8 suppression,
//     register-file-as-written);
//   - Yamaha's §6 recommended rhythm setup produces non-silent,
//     deterministic output for each of the five drums;
//   - AM depth ~= 4.875 dB (13 x 0.375 dB steps -- §5's 14-level triangle);
//   - the documented VIB +-(F-Num >> 7) law (a DISCLOSED approximation,
//     ym2413_synth.h disclosure item 3).
//
// The 8-byte BD patch literal below is the fact-sheet §4 printed rhythm
// patch row (a permitted transcription from the fact sheet; identical to
// the Ym2413Opll model's kRhythmBd).

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

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

std::vector<std::int32_t> collect(Ym2413Opll& opll, const int count) {
    std::vector<std::int32_t> samples;
    samples.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        tick(opll);
        samples.push_back(opll.fm_sample());
    }
    return samples;
}

// Yamaha's §6 recommended fixed rhythm frequency setup.
void program_recommended_rhythm_freqs(Ym2413Opll& opll) {
    write_reg(opll, 0x16, 0x20);
    write_reg(opll, 0x17, 0x50);
    write_reg(opll, 0x18, 0xC0);
    write_reg(opll, 0x26, 0x05);
    write_reg(opll, 0x27, 0x05);
    write_reg(opll, 0x28, 0x01);
    write_reg(opll, 0x36, 0x00);  // BD volume 0 (loudest)
    write_reg(opll, 0x37, 0x00);  // HH/SD volumes 0
    write_reg(opll, 0x38, 0x00);  // TOM/T-CY volumes 0
}

}  // namespace

int main() {
    // --- 1. THE x2 DOUBLE-OUTPUT GAIN LAW: BD (rhythm ch6) vs the byte-
    //     identical user-patch melody note on the SAME channel 6 -- exactly
    //     2x, sample for sample. ---
    {
        // Fact-sheet §4 rhythm patch row "BD" (permitted literal).
        const std::uint8_t kBdPatch[8] = {0x01, 0x01, 0x18, 0x0F, 0xDF, 0xF8, 0x6A, 0x6D};

        const auto run_melody = [&] {
            Ym2413Opll opll;
            opll.reset();
            for (std::uint8_t i = 0; i < 8; ++i) {
                write_reg(opll, i, kBdPatch[i]);
            }
            write_reg(opll, 0x16, 0x90);  // ch6 fnum low
            write_reg(opll, 0x36, 0x00);  // ch6: instrument 0 (user), volume 0
            write_reg(opll, 0x26, 0x10 | (3 << 1));  // key-on, block 3
            return collect(opll, 3000);
        };
        const auto run_bd = [&] {
            Ym2413Opll opll;
            opll.reset();
            write_reg(opll, 0x0E, 0x20);  // rhythm mode ON first (§6)
            write_reg(opll, 0x16, 0x90);
            write_reg(opll, 0x36, 0x00);  // BD volume 0
            write_reg(opll, 0x26, 3 << 1);  // block 3, key bit 0 (§6 quirk 3)
            write_reg(opll, 0x0E, 0x30);  // BD key via $0E bit4
            return collect(opll, 3000);
        };

        const std::vector<std::int32_t> melody = run_melody();
        const std::vector<std::int32_t> bd = run_bd();
        bool exactly_double = melody.size() == bd.size();
        bool non_silent = false;
        for (std::size_t i = 0; i < melody.size() && exactly_double; ++i) {
            if (bd[i] != 2 * melody[i]) {
                exactly_double = false;
            }
            if (melody[i] != 0) {
                non_silent = true;
            }
        }
        expect(non_silent, "RhythmX2_ControlMelodyNote_NonSilent");
        expect(exactly_double, "RhythmX2_BdExactlyDoubleOfIdenticalMelodyNote");
    }

    // --- 2. Recommended §6 setup: each drum, keyed alone, produces
    //     non-silent deterministic output; unkeyed drums stay idle. ---
    {
        struct DrumCase {
            std::uint8_t key_bit;
            int slot_a;  // the drum's slot (checked active)
            int idle_slot;  // an unrelated drum slot (checked idle)
            const char* name;
        };
        const DrumCase drums[] = {
            {0x10, 13, 15, "Rhythm_BD_KeyedAlone_NonSilent"},
            {0x08, 15, 13, "Rhythm_SD_KeyedAlone_NonSilent"},
            {0x04, 16, 13, "Rhythm_TOM_KeyedAlone_NonSilent"},
            {0x02, 17, 13, "Rhythm_TCY_KeyedAlone_NonSilent"},
            {0x01, 14, 13, "Rhythm_HH_KeyedAlone_NonSilent"},
        };
        for (const DrumCase& d : drums) {
            const auto run = [&] {
                Ym2413Opll opll;
                opll.reset();
                write_reg(opll, 0x0E, 0x20);
                program_recommended_rhythm_freqs(opll);
                write_reg(opll, 0x0E, static_cast<std::uint8_t>(0x20 | d.key_bit));
                return opll;
            };
            Ym2413Opll opll = run();
            const std::vector<std::int32_t> samples = collect(opll, 2000);
            bool non_silent = false;
            for (const std::int32_t s : samples) {
                if (s != 0) {
                    non_silent = true;
                }
            }
            expect(non_silent, d.name);
            expect(opll.synth().eg_state(d.idle_slot) == EgState::Idle,
                   "Rhythm_UnkeyedDrumSlot_StaysIdle");
            // Determinism: an identical fresh run yields the identical
            // stream.
            Ym2413Opll opll2 = run();
            expect(collect(opll2, 2000) == samples, "Rhythm_Drum_TwoRuns_ByteIdentical");
        }
    }

    // --- 3. Melody ch6-8 suppression while rhythm mode is on (§6 quirk 2:
    //     the register file keeps what was written; the key bit
    //     simply stops having an effect). ---
    {
        Ym2413Opll opll;
        opll.reset();
        write_reg(opll, 0x17, 0x80);  // ch7 fnum
        write_reg(opll, 0x37, 0x10);  // ch7: instrument 1, volume 0
        write_reg(opll, 0x27, 0x10 | (4 << 1));  // melody key-on ch7
        const std::vector<std::int32_t> before = collect(opll, 2000);
        bool melody_sounds = false;
        for (const std::int32_t s : before) {
            if (s != 0) {
                melody_sounds = true;
            }
        }
        expect(melody_sounds, "MelodyCh7_RhythmOff_Sounds");

        write_reg(opll, 0x0E, 0x20);  // rhythm ON, no drums keyed
        // The melody key edge releases ch7's operators; the register file
        // itself is untouched.
        expect(opll.register_value(0x27) == static_cast<std::uint8_t>(0x10 | (4 << 1)),
               "MelodyCh7_RegisterFileKeepsKeyBit_DM31_3");
        (void)collect(opll, 120000);  // let the release run out
        const std::vector<std::int32_t> after = collect(opll, 500);
        bool suppressed = true;
        for (const std::int32_t s : after) {
            if (s != 0) {
                suppressed = false;
            }
        }
        expect(suppressed, "MelodyCh7_RhythmOn_SuppressedToExactSilence");
    }

    // --- 4. AM depth: a held carrier with AM=1 swings ~4.875 dB between
    //     the LFO's 0-step and 13-step windows (§5's 14-level triangle of
    //     0.375 dB steps; the 512-sample step counter is the documented
    //     disclosure-item-4 choice). ---
    {
        Ym2413Opll opll;
        opll.reset();
        write_reg(opll, 0x00, 0x00);
        write_reg(opll, 0x01, 0xA1);  // car: AM=1, EG-TYP=1, MUL=1
        write_reg(opll, 0x02, 0x3F);
        write_reg(opll, 0x03, 0x00);
        write_reg(opll, 0x04, 0x00);
        write_reg(opll, 0x05, 0xF0);
        write_reg(opll, 0x06, 0x00);
        write_reg(opll, 0x07, 0x00);
        write_reg(opll, 0x10, 0x00);  // fnum 256 via $20 bit0
        write_reg(opll, 0x30, 0x00);
        write_reg(opll, 0x20, 0x19);  // key-on, block 4, fnum8=1

        // Two full AM cycles: 2 x 26 steps x 512 samples.
        const std::vector<std::int32_t> samples = collect(opll, 2 * 26 * 512);
        std::int32_t max_window_peak = 0;
        std::int32_t min_window_peak = 0x7FFFFFFF;
        for (std::size_t w = 0; w < samples.size() / 512; ++w) {
            std::int32_t peak = 0;
            for (std::size_t i = w * 512; i < (w + 1) * 512; ++i) {
                peak = std::max(peak, std::abs(samples[i]));
            }
            max_window_peak = std::max(max_window_peak, peak);
            min_window_peak = std::min(min_window_peak, peak);
        }
        // Loudest window: AM step 0 -> full scale 2048 >> 3 = 256. Softest:
        // step 13 -> T = 208 units -> round(2^(-208/256)*2048) >> 3 = 145.
        expect(max_window_peak == 256, "AmDepth_LoudestWindow_FullScale");
        expect(min_window_peak >= 143 && min_window_peak <= 147,
               "AmDepth_SoftestWindow_About4_875dBDown");
        const double ratio =
            static_cast<double>(max_window_peak) / static_cast<double>(min_window_peak);
        expect(ratio > 1.70 && ratio < 1.85, "AmDepth_PeakToTrough_Approx4_8dB");
    }

    // --- 5. VIB: the documented +-(F-Num >> 7) law (disclosure item 3),
    //     zero-mean over a full 8-step cycle. ---
    {
        bool law_holds = true;
        for (const std::uint16_t fnum : {std::uint16_t{128}, std::uint16_t{256},
                                          std::uint16_t{511}, std::uint16_t{100}}) {
            const std::int32_t p = fnum >> 7;
            std::int32_t sum = 0;
            std::int32_t peak = 0;
            for (int step = 0; step < 8; ++step) {
                const std::int32_t off = Ym2413Synth::vibrato_fnum_offset(step, fnum);
                sum += off;
                peak = std::max(peak, std::abs(off));
            }
            if (sum != 0 || peak != p) {
                law_holds = false;
            }
            // Triangle symmetry: second half mirrors the first.
            for (int step = 0; step < 4; ++step) {
                if (Ym2413Synth::vibrato_fnum_offset(step + 4, fnum) !=
                    -Ym2413Synth::vibrato_fnum_offset(step, fnum)) {
                    law_holds = false;
                }
            }
        }
        expect(law_holds, "Vib_PeakIsFnumOver128_ZeroMean_Triangle");

        // End-to-end: over one full VIB cycle (8 x 1024 ticks) the total
        // phase advance equals the unmodulated total exactly (zero-mean) --
        // the 19-bit accumulator returns to its start value.
        Ym2413Opll opll;
        opll.reset();
        write_reg(opll, 0x00, 0x00);
        write_reg(opll, 0x01, 0x61);  // car: VIB=1, EG-TYP=1, MUL=1
        write_reg(opll, 0x02, 0x3F);
        write_reg(opll, 0x05, 0xF0);
        write_reg(opll, 0x10, 0x00);
        write_reg(opll, 0x30, 0x00);
        write_reg(opll, 0x20, 0x19);  // key-on, block 4, fnum 256
        for (int i = 0; i < 8192; ++i) {
            tick(opll);
        }
        // 8192 ticks x 4096 base increment = 2^25 == 0 (mod 2^19); the VIB
        // offsets cancel exactly over the complete cycle.
        expect(opll.synth().phase(1) == 0, "Vib_ZeroMean_PhaseReturnsAfterFullLfoCycle");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_Ym2413SynthRhythmLfo_Unit cases passed\n";
    return 0;
}
