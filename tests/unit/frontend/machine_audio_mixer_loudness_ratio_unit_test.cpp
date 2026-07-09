// Suite: Frontend_MachineAudioMixerLoudnessRatio_Unit (M32-S4, Defect B of
// DEC-0039, docs/m32-planner-package.md §2.8 oracle 2 / test matrix row 11)
//
// THE LOUDNESS-RATIO ORACLE: one full-volume FM carrier (the M31 S2
// full-volume patch, per-channel peak ~255) and one full-volume PSG channel
// (amplitude 31 square), each mixed ALONE through MachineAudioMixer, must
// sit within +-15% of the reference per-channel loudness ratio 9000/21000
// (openMSX references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:63/:191,
// normalized per channel per AY8910.cc:64-93/977-980 and
// YM2413Okazaki.cc:48/154-165/1051-1054). The +-15% bound absorbs waveform
// shape, EG settle, and zero-order hold -- it is derived from the reference
// ratio, NOT tuned to the implementation.
//
// Deterministic: fixed register programs, fixed sample counts, no wall
// clock; two-run identity asserted.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/ym2413_opll.h"
#include "frontend/machine_audio_mixer.h"

namespace {

using sony_msx::devices::audio::PsgYm2149;
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

void write_reg(Ym2413Opll& opll, const std::uint8_t addr, const std::uint8_t value) {
    opll.write_address(addr);
    opll.write_data(value);
}

// One full-volume FM carrier (the M31 S2 full-volume idiom: user patch,
// modulator silent, carrier AR=15 instant, sustained at volume 0).
void program_single_fm_carrier(Ym2413Opll& opll) {
    write_reg(opll, 0x00, 0x00);  // mod: AR=0 -> silent forever
    write_reg(opll, 0x01, 0x21);  // car: EG-TYP=1, MUL=1
    write_reg(opll, 0x02, 0x3F);
    write_reg(opll, 0x03, 0x00);
    write_reg(opll, 0x04, 0x00);
    write_reg(opll, 0x05, 0xF0);  // car AR=15 (instant)
    write_reg(opll, 0x06, 0x00);
    write_reg(opll, 0x07, 0x00);
    write_reg(opll, 0x10, 0x00);
    write_reg(opll, 0x30, 0x00);  // user patch, volume 0
    write_reg(opll, 0x20, 0x19);  // key-on, block 4, fnum 256
}

// One full-volume PSG channel: channel A only (tone enabled, volume 15 ->
// resolved amplitude peaks at 31; channel A is the CENTER channel of the B1
// stereo law, contributing equally to both sides).
void program_single_psg_channel(PsgYm2149& psg) {
    psg.reset();
    psg.write_address(0);
    psg.write_data(100);  // channel A period low (audible tone)
    psg.write_address(1);
    psg.write_data(0);
    psg.write_address(8);
    psg.write_data(15);   // channel A volume 15 (fixed amplitude, max)
    psg.write_address(7);
    psg.write_data(0x3E);  // mixer: tone A on, everything else off
}

std::int32_t peak_abs(const std::vector<std::int16_t>& pcm, const std::size_t skip_pairs) {
    std::int32_t peak = 0;
    for (std::size_t i = skip_pairs * 2; i < pcm.size(); ++i) {
        peak = std::max<std::int32_t>(peak, std::abs(static_cast<std::int32_t>(pcm[i])));
    }
    return peak;
}

}  // namespace

int main() {
    constexpr std::size_t kSamples = 6000;      // ~136 ms
    constexpr std::size_t kSettleSkip = 1000;   // skip attack/settle region

    const MachineAudioMixer mixer(kCyclesPerSample);

    // --- FM alone. ---
    PsgYm2149 silent_psg;
    silent_psg.reset();
    Ym2413Opll fm;
    fm.reset();
    program_single_fm_carrier(fm);
    const std::vector<std::int16_t> fm_pcm = mixer.mix_interleaved_stereo(
        silent_psg, MachineAudioMixer::SccSources{nullptr, nullptr}, &fm, kSamples);
    const std::int32_t fm_peak = peak_abs(fm_pcm, kSettleSkip);

    // --- PSG alone. ---
    PsgYm2149 psg;
    program_single_psg_channel(psg);
    const std::vector<std::int16_t> psg_pcm = mixer.mix_interleaved_stereo(
        psg, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr, kSamples);
    const std::int32_t psg_peak = peak_abs(psg_pcm, kSettleSkip);

    std::cout << "loudness-ratio probe: fm_peak=" << fm_peak << " psg_peak=" << psg_peak
              << " ratio=" << (psg_peak != 0 ? static_cast<double>(fm_peak) / psg_peak : -1.0)
              << " reference=" << (9000.0 / 21000.0) << "\n";

    expect(fm_peak > 0, "FmCarrier_ProducesNonSilentOutput");
    expect(psg_peak > 0, "PsgChannel_ProducesNonSilentOutput");

    // --- The oracle: peak_FM / peak_PSG within +-15% of 9000/21000. ---
    {
        const double reference = 9000.0 / 21000.0;  // 0.428571...
        const double ratio = static_cast<double>(fm_peak) / static_cast<double>(psg_peak);
        expect(ratio >= reference * 0.85 && ratio <= reference * 1.15,
               "LoudnessRatio_FmOverPsg_Within15PercentOfOpenMsxReference");
    }

    // --- Grounding cross-checks: the peaks sit at their derived scales
    //     (PSG 31*400 = 12,400 exactly for a full-volume square; FM within
    //     the 255-peak * 21 neighborhood, <= 256*21). ---
    expect(psg_peak == 31 * MachineAudioMixer::kPsgAmplitudeScale,
           "PsgPeak_Exactly31TimesScale_FullVolumeSquare");
    expect(fm_peak <= 256 * MachineAudioMixer::kFmAmplitudeScale &&
               fm_peak >= 240 * MachineAudioMixer::kFmAmplitudeScale,
           "FmPeak_WithinFullScaleCarrierNeighborhood");

    // --- Two-run determinism. ---
    {
        PsgYm2149 psg2;
        program_single_psg_channel(psg2);
        Ym2413Opll fm2;
        fm2.reset();
        program_single_fm_carrier(fm2);
        PsgYm2149 silent2;
        silent2.reset();
        const std::vector<std::int16_t> fm_pcm2 = mixer.mix_interleaved_stereo(
            silent2, MachineAudioMixer::SccSources{nullptr, nullptr}, &fm2, kSamples);
        const std::vector<std::int16_t> psg_pcm2 = mixer.mix_interleaved_stereo(
            psg2, MachineAudioMixer::SccSources{nullptr, nullptr}, nullptr, kSamples);
        expect(fm_pcm == fm_pcm2 && psg_pcm == psg_pcm2, "TwoRuns_ByteIdenticalPcm");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MachineAudioMixerLoudnessRatio_Unit cases passed\n";
    return 0;
}
