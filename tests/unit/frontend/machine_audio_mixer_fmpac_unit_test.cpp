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

// Suite: Frontend_MachineAudioMixerFmPac_Unit (M37 Slice B, DEC-0055, R-D)
//
// Wires the inserted external FM-PAC cartridge's OWN YM2413 (OPLL) into the
// MachineAudioMixer as an ADDITIVE source, alongside PSG + SCC + the built-in
// MSX-MUSIC OPLL, reusing the built-in OPLL's exact kFmAmplitudeScale and
// per-sample advance cadence (no new DSP, no new scaling -- the planner's
// "wholly-additive follow-on", cartridge_fmpac_rom.h:61-62).
//
// THE M37 HARD REGRESSION ORACLE lives here (case 1): with NO FM-PAC inserted
// (an all-null FmSources) the new 5-arg overload's PCM is BYTE-IDENTICAL to
// the pre-M37 4-arg overload for ANY input -- proven against twin PSG + keyed
// built-in FM + SCC. The built-in-OPLL + PSG + SCC mix is untouched.
//
// Non-vacuity (cases 2-4): an FM-PAC cart keyed a note THROUGH the memory-
// mapped 0x7FF4/0x7FF5 register ports (the genuine end-to-end path a game
// drives) contributes an audible, hand-checkable term to the mix, matched
// against an independently-programmed lock-step twin OPLL; two FM-PACs (both
// external bays) sum; and the built-in OPLL and an FM-PAC coexist additively.
// Case 5 is two-run determinism with all four source classes active.

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
#include "devices/cartridge/cartridge_fmpac_rom.h"
#include "frontend/machine_audio_mixer.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::devices::audio::SccWavetable;
using sony_msx::devices::audio::Ym2413Opll;
using sony_msx::devices::cartridge::CartridgeFmPacRom;
using sony_msx::frontend::MachineAudioMixer;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::uint64_t kCyclesPerSample = 81;  // the presenter's own integer step

// FM-PAC CPU-visible memory-mapped OPLL register ports (page-1 absolute
// addresses, cartridge_fmpac_rom.h:44-45): 0x7FF4 = address, 0x7FF5 = data.
constexpr std::uint16_t kFmPacOpllAddrPort = 0x7FF4;
constexpr std::uint16_t kFmPacOpllDataPort = 0x7FF5;

void write_reg(Ym2413Opll& opll, const std::uint8_t addr, const std::uint8_t value) {
    opll.write_address(addr);
    opll.write_data(value);
}

// Same register traffic as write_reg(), but driven THROUGH the FM-PAC cart's
// CPU-visible memory-mapped ports -- the genuine end-to-end path (game writes
// 0x7FF4/0x7FF5, cart forwards to its owned OPLL, cartridge_fmpac_rom.cpp:
// 109-114). Proves the mixed contribution is reachable the way software keys.
void write_reg_via_cart(CartridgeFmPacRom& cart, const std::uint8_t addr, const std::uint8_t value) {
    cart.mem_write(kFmPacOpllAddrPort, addr);
    cart.mem_write(kFmPacOpllDataPort, value);
}

// The M29/M31 unit tests' non-trivial PSG program (varying output).
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

// A held carrier-only FM tone on `channel` (user patch, instant attack,
// sustained at 0 dB): per-channel peak 2048 >> 3 = 256 -- the M31 idiom.
template <typename WriteFn>
void program_fm_tone(WriteFn&& write, const int channel) {
    write(0x00, 0x00);  // mod: AR=0 -> silent forever
    write(0x01, 0x21);  // car: EG-TYP=1, MUL=1
    write(0x02, 0x3F);
    write(0x03, 0x00);
    write(0x04, 0x00);
    write(0x05, 0xF0);  // car AR=15 (instant)
    write(0x06, 0x00);
    write(0x07, 0x00);
    write(static_cast<std::uint8_t>(0x10 + channel), 0x00);
    write(static_cast<std::uint8_t>(0x30 + channel), 0x00);  // user patch, volume 0
    write(static_cast<std::uint8_t>(0x20 + channel), 0x19);  // key-on, block 4, fnum 256
}

void program_fm_tone_direct(Ym2413Opll& opll, const int channel) {
    program_fm_tone([&](std::uint8_t a, std::uint8_t v) { write_reg(opll, a, v); }, channel);
}

void program_fm_tone_via_cart(CartridgeFmPacRom& cart, const int channel) {
    program_fm_tone([&](std::uint8_t a, std::uint8_t v) { write_reg_via_cart(cart, a, v); }, channel);
}

// A minimal valid 16 KB (one-bank) FM-PAC image, reset (the constructor does
// NOT self-reset -- CartridgeSlot::load() does; cartridge_fmpac_rom.h:66-67).
CartridgeFmPacRom make_reset_fmpac() {
    std::vector<std::uint8_t> image(CartridgeFmPacRom::kBankSize, 0x00);
    CartridgeFmPacRom cart(std::move(image));
    cart.reset();
    return cart;
}

void program_scc(SccWavetable& scc) {
    scc.reset();
    for (int i = 0; i < 32; ++i) {
        scc.write(static_cast<std::uint8_t>(i), 0x40);
    }
    scc.write(0x8A, 0x0F);  // channel 1 volume 15
    scc.write(0x8F, 0x01);  // channel 1 enable
    scc.write(0x80, 0x63);  // channel 1 period
}

}  // namespace

int main() {
    // --- 1. THE M37 HARD ORACLE: no FM-PAC inserted (all-null FmSources) =>
    //     BYTE-IDENTICAL to the pre-M37 4-arg overload for a rich input
    //     (varying PSG + keyed built-in OPLL + one SCC). The built-in-OPLL +
    //     PSG + SCC mix is provably untouched. ---
    {
        // Independent twins on both sides (the mixer mutates generator state).
        PsgYm2149 psg_a;
        PsgYm2149 psg_b;
        program_psg(psg_a);
        program_psg(psg_b);
        Ym2413Opll builtin_a;
        Ym2413Opll builtin_b;
        builtin_a.reset();
        builtin_b.reset();
        program_fm_tone_direct(builtin_a, 3);
        program_fm_tone_direct(builtin_b, 3);
        SccWavetable scc_a;
        SccWavetable scc_b;
        program_scc(scc_a);
        program_scc(scc_b);

        const MachineAudioMixer mixer(kCyclesPerSample);
        // Pre-M37 path (4-arg overload -- its output is M31/M34-locked).
        const std::vector<std::int16_t> pre_m37 = mixer.mix_interleaved_stereo(
            psg_a, MachineAudioMixer::SccSources{&scc_a, nullptr}, &builtin_a, 2000);
        // New path, NO FM-PAC (all-null FmSources).
        const std::vector<std::int16_t> no_fmpac = mixer.mix_interleaved_stereo(
            psg_b, MachineAudioMixer::SccSources{&scc_b, nullptr}, &builtin_b,
            MachineAudioMixer::FmSources{nullptr, nullptr}, 2000);

        expect(pre_m37 == no_fmpac, "ZeroFmPac_ByteIdenticalToPreM37Mix_HardOracle");
        // Non-vacuity guard: the shared input is genuinely non-silent, so the
        // oracle above is comparing real audio, not two zero buffers.
        bool any_nonzero = false;
        for (const std::int16_t v : pre_m37) {
            if (v != 0) {
                any_nonzero = true;
                break;
            }
        }
        expect(any_nonzero, "ZeroFmPac_OracleInputIsNonSilent_NonVacuous");
    }

    // --- 2. FM-PAC CONTRIBUTES: an inserted FM-PAC cart, keyed a note THROUGH
    //     its memory-mapped 0x7FF4/0x7FF5 ports, appears in the mix. Silent
    //     PSG, no SCC, no built-in FM => pcm == clamp(fmpac_sample * scale) on
    //     both channels, matched against an independently-programmed lock-step
    //     twin, and audibly non-zero. ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent
        CartridgeFmPacRom cart = make_reset_fmpac();
        program_fm_tone_via_cart(cart, 0);  // key via the CPU-visible ports
        Ym2413Opll twin;
        twin.reset();
        program_fm_tone_direct(twin, 0);  // identical register state

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{&cart.opll(), nullptr}, 600);

        bool law_holds = mixed.size() == 1200;
        bool fmpac_audible = false;
        for (std::size_t i = 0; i < 600 && law_holds; ++i) {
            twin.advance_cycles(kCyclesPerSample);
            const std::int32_t expected = std::clamp(
                twin.fm_sample() * MachineAudioMixer::kFmAmplitudeScale, -32768, 32767);
            if (mixed[i * 2] != expected || mixed[i * 2 + 1] != expected) {
                law_holds = false;
            }
            if (expected != 0) {
                fmpac_audible = true;
            }
        }
        expect(law_holds, "FmPacContributes_SampleTimesScale_MonoBothChannels_LockStepTwin");
        expect(fmpac_audible, "FmPacContributes_KeyedViaMemMappedPorts_ActuallyNonSilent");
    }

    // --- 3. BOTH EXTERNAL BAYS: two inserted FM-PAC carts (bay 1 + bay 2),
    //     keyed on different channels, SUM additively -- real hardware mixes
    //     both bays' sound-out lines (feedback_universal_fixes_only /
    //     _system_wide_review_first). pcm == clamp((s1 + s2) * scale). ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent
        CartridgeFmPacRom cart1 = make_reset_fmpac();
        CartridgeFmPacRom cart2 = make_reset_fmpac();
        program_fm_tone_via_cart(cart1, 0);
        program_fm_tone_via_cart(cart2, 5);
        Ym2413Opll twin1;
        Ym2413Opll twin2;
        twin1.reset();
        twin2.reset();
        program_fm_tone_direct(twin1, 0);
        program_fm_tone_direct(twin2, 5);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr,
            MachineAudioMixer::FmSources{&cart1.opll(), &cart2.opll()}, 600);

        bool law_holds = mixed.size() == 1200;
        bool both_contributed = false;
        for (std::size_t i = 0; i < 600 && law_holds; ++i) {
            twin1.advance_cycles(kCyclesPerSample);
            twin2.advance_cycles(kCyclesPerSample);
            const std::int32_t sum = twin1.fm_sample() + twin2.fm_sample();
            const std::int32_t expected =
                std::clamp(sum * MachineAudioMixer::kFmAmplitudeScale, -32768, 32767);
            if (mixed[i * 2] != expected || mixed[i * 2 + 1] != expected) {
                law_holds = false;
            }
            // Genuinely two summed tones (not one masking the other).
            if (twin1.fm_sample() != 0 && twin2.fm_sample() != 0) {
                both_contributed = true;
            }
        }
        expect(law_holds, "TwoFmPacs_BothBaysSumAdditively_LockStepTwins");
        expect(both_contributed, "TwoFmPacs_BothActuallyContribute_NonVacuous");
    }

    // --- 4. BUILT-IN OPLL + FM-PAC COEXIST: the machine's own MSX-MUSIC OPLL
    //     (the `fm` term) and an inserted FM-PAC OPLL are INDEPENDENT, additive
    //     sources -- a real HB-F1XV + FM-PAC has two whole OPLLs. Silent PSG,
    //     no SCC => pcm == clamp((builtin + fmpac) * scale). ---
    {
        PsgYm2149 psg;
        psg.reset();  // silent
        Ym2413Opll builtin;
        builtin.reset();
        program_fm_tone_direct(builtin, 1);
        Ym2413Opll builtin_twin;
        builtin_twin.reset();
        program_fm_tone_direct(builtin_twin, 1);

        CartridgeFmPacRom cart = make_reset_fmpac();
        program_fm_tone_via_cart(cart, 4);
        Ym2413Opll fmpac_twin;
        fmpac_twin.reset();
        program_fm_tone_direct(fmpac_twin, 4);

        const MachineAudioMixer mixer(kCyclesPerSample);
        const std::vector<std::int16_t> mixed = mixer.mix_interleaved_stereo(
            psg, MachineAudioMixer::SccSources{nullptr, nullptr}, &builtin,
            MachineAudioMixer::FmSources{&cart.opll(), nullptr}, 600);

        bool law_holds = mixed.size() == 1200;
        bool both_paths_audible = false;
        for (std::size_t i = 0; i < 600 && law_holds; ++i) {
            builtin_twin.advance_cycles(kCyclesPerSample);
            fmpac_twin.advance_cycles(kCyclesPerSample);
            const std::int32_t sum = builtin_twin.fm_sample() + fmpac_twin.fm_sample();
            const std::int32_t expected =
                std::clamp(sum * MachineAudioMixer::kFmAmplitudeScale, -32768, 32767);
            if (mixed[i * 2] != expected || mixed[i * 2 + 1] != expected) {
                law_holds = false;
            }
            if (builtin_twin.fm_sample() != 0 && fmpac_twin.fm_sample() != 0) {
                both_paths_audible = true;
            }
        }
        expect(law_holds, "BuiltInPlusFmPac_IndependentAdditiveOpllS_LockStepTwins");
        expect(both_paths_audible, "BuiltInPlusFmPac_BothOpllSContribute_NonVacuous");
    }

    // --- 5. Two-run determinism with ALL FOUR source classes active
    //     (PSG + SCC + built-in OPLL + FM-PAC OPLL). ---
    {
        const auto run = [] {
            PsgYm2149 psg;
            program_psg(psg);
            SccWavetable scc;
            program_scc(scc);
            Ym2413Opll builtin;
            builtin.reset();
            program_fm_tone_direct(builtin, 2);
            CartridgeFmPacRom cart = make_reset_fmpac();
            program_fm_tone_via_cart(cart, 6);
            const MachineAudioMixer mixer(kCyclesPerSample);
            return mixer.mix_interleaved_stereo(
                psg, MachineAudioMixer::SccSources{&scc, nullptr}, &builtin,
                MachineAudioMixer::FmSources{&cart.opll(), nullptr}, 1500);
        };
        expect(run() == run(), "AllFourSources_TwoRuns_ByteIdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerFmPac_Unit cases passed\n";
    return 0;
}
