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

// Suite: Machine_Hbf1xvM34AlesteUltrasonic_System (M34-S3, DEC-0043 Defect
// A, docs/m34-planner-package.md §2.6.4 / test matrix row 10)
//
// THE ALESTE TRANSITION REGRESSION: the human-reported v1.0.33/v1.0.34
// playtest defect ("high pitch beee bbee bip" at the title -> weapon-select
// transition). DEC-0043 coordinator triage: frames 2190-2299 of the recorded
// m32 smoke recipe carry a ~20.6 kHz near-Nyquist tone at RMS up to 17,533,
// 100% PSG (tone period 0 on all three channels, R7=0xB8 tones enabled,
// volumes gated 7-15) -- a real-hardware SILENCE idiom (the ~112 kHz square
// is inaudible and analog-smoothed) that the pre-M34 point sampler folded
// into the audible band.
//
// Recipe (docs/m32-implementation-report.md AC-9, the recorded m32 smoke
// shape): real BIOS cold boot, roms/aleste.rom loaded EXPLICITLY as
// KonamiSCC, real frame loop (step_cpu_instruction() to each 59,736-cycle
// boundary + on_vsync_boundary()), SPACE held for 15 frames at frames
// 600/1500/2100/2700 (cycle stamps (f-1)*59736 -- the exact
// tools/metalgear-evidence-input.script stamp convention; the 2700 hold
// lies beyond this test's 2350-frame budget and is kept for recipe
// fidelity). RUNTIME CHOICE (recorded per §2.6.4): mixed-PCM production is
// window-restricted to frames [2150, 2350) -- 200 frames x 735
// samples/frame through the REAL three-source MachineAudioMixer (PSG + SCC
// + FM), deterministic by construction; generator time before the window is
// not consumed (the chips' phase origin shifts, which changes no metric
// property of the periodic idiom).
//
// HIGH-PITCH-BURST METRIC (§2.6.4, the committed definition): partition
// each channel's PCM into 4,096-sample blocks (full blocks only; 147,000
// samples/channel -> 35 blocks, 3,640-sample remainder discarded); per
// block: integer mean m = floor(sum/4096); x~ = x - m; ZCR = #{i>=1 :
// (x~[i-1]<0) != (x~[i]<0)} / 4095 (zero counts as non-negative); AC_RMS =
// floor(sqrt(sum(x~^2)/4096)). Burst block iff ZCR >= 0.70 (integer form:
// flips >= 2867) AND AC_RMS >= 2,500. Rationale: a 20.7 kHz alias has ZCR
// ~= 2f/fs ~= 0.94 while music sits < ~0.4, and the §2.4 p=0/1 bound
// (peak <= 2,500 => RMS < 2,500) puts the post-fix residual under the RMS
// arm. ORACLE: zero burst blocks on BOTH stereo sides.
//
// DEVIATION FROM THE PACKAGE'S LETTER (recorded, §2.6.4 said left-channel
// only): the S3 pre-fix bring-up measurement showed the burst manifests on
// the RIGHT channel in this trajectory -- during the burst window the game
// holds R10=0 (channel C silent) with A/B volume-gated 11-12, and A/B sit
// phase-OPPOSED, so the left side (A+B) largely cancels to an ~RMS-400
// residual while the right side (A alone) carries the full alias (pre-fix
// measured: ZCR up to 0.937, AC_RMS up to 5,880, multiple burst blocks at
// frames ~2189-2227). Metering BOTH sides is strictly stronger than the
// package's left-only clause and is what makes the pre-fix discrimination
// genuine; the measured baseline is recorded in
// docs/m34-implementation-report.md -- the metric fires pre-fix, proven,
// not assumed.
//
// SKIP DISCIPLINE (DEC-0016 precedent): SKIPs -- never fails -- when the
// ROM/BIOS assets are absent or the ROM is a different dump.
//
// Deterministic oracle: two fully independent sessions produce byte-
// identical window PCM.

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "frontend/machine_audio_mixer.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"
#include "machine/sha1.h"

#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr const char* kAlesteSha1 = "e93d0840c59c6eba273df546d22148d486a150a6";
constexpr int kTotalFrames = 2350;
constexpr int kWindowStartFrame = 2150;
constexpr std::size_t kSamplesPerFrame = 735;
constexpr std::size_t kBlock = 4096;

// The recorded m32 smoke recipe's input script (see header). 59,736 cycles
// per frame; DOWN at (f-1)*59736, UP at (f+14)*59736 -- 15-frame holds.
constexpr const char* kInputScript =
    "HBF1XV-INPUT-SCRIPT v1\n"
    "T=35781864 KEY=SPACE DOWN\n"    // frame 600:  599*59736 (== the metalgear
    "T=36677904 KEY=SPACE UP\n"      // frame 615:  614*59736  script's own 600-hold stamps)
    "T=89544264 KEY=SPACE DOWN\n"    // frame 1500: 1499*59736
    "T=90440304 KEY=SPACE UP\n"      // frame 1515: 1514*59736
    "T=125385864 KEY=SPACE DOWN\n"   // frame 2100: 2099*59736
    "T=126281904 KEY=SPACE UP\n"     // frame 2115: 2114*59736
    "T=161227464 KEY=SPACE DOWN\n"   // frame 2700: 2699*59736 (beyond budget, recipe fidelity)
    "T=162123504 KEY=SPACE UP\n"     // frame 2715: 2714*59736
    "[END]\n";

struct BlockMetric {
    int zcr_flips = 0;
    long long ac_rms = 0;
    bool burst = false;
};

std::vector<BlockMetric> block_metrics(const std::vector<std::int16_t>& left) {
    std::vector<BlockMetric> out;
    const std::size_t blocks = left.size() / kBlock;
    for (std::size_t b = 0; b < blocks; ++b) {
        const std::int16_t* x = left.data() + b * kBlock;
        std::int64_t sum = 0;
        for (std::size_t i = 0; i < kBlock; ++i) sum += x[i];
        const std::int64_t m = sum / static_cast<std::int64_t>(kBlock);
        BlockMetric bm;
        long double acc = 0.0L;
        bool prev_neg = (static_cast<std::int64_t>(x[0]) - m) < 0;
        for (std::size_t i = 0; i < kBlock; ++i) {
            const std::int64_t d = static_cast<std::int64_t>(x[i]) - m;
            acc += static_cast<long double>(d) * static_cast<long double>(d);
            const bool neg = d < 0;  // zero counts as non-negative
            if (i >= 1 && neg != prev_neg) {
                ++bm.zcr_flips;
            }
            prev_neg = neg;
        }
        bm.ac_rms = static_cast<long long>(
            std::floor(std::sqrt(static_cast<double>(acc / static_cast<long double>(kBlock)))));
        // ZCR >= 0.70 <=> flips/4095 >= 0.70 <=> flips >= 2867 (integer form).
        bm.burst = bm.zcr_flips >= 2867 && bm.ac_rms >= 2500;
        out.push_back(bm);
    }
    return out;
}

struct SessionResult {
    std::vector<std::int16_t> window_pcm;
    bool scc_attached = false;
    int frames_run = 0;
};

SessionResult run_session(const std::vector<std::uint8_t>& image) {
    using sony_msx::frontend::MachineAudioMixer;
    using sony_msx::machine::Hbf1xvMachine;

    SessionResult result;
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    if (machine.load_cartridge(1, sony_msx::devices::cartridge::CartridgeMapperType::KonamiSCC,
                               image) != sony_msx::devices::cartridge::CartridgeLoadResult::Ok) {
        return result;
    }

    sony_msx::machine::InputScriptPlayer script(
        sony_msx::machine::parse_input_script(kInputScript));

    const MachineAudioMixer mixer(81);
    const std::uint64_t target = machine.frame_cycles_per_frame();
    for (int frame = 0; frame < kTotalFrames; ++frame) {
        const std::uint64_t frame_start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - frame_start < target) {
            machine.step_cpu_instruction();
            script.apply_due(machine.elapsed_cycles(), machine.keyboard());
        }
        machine.on_vsync_boundary();
        ++result.frames_run;

        if (frame >= kWindowStartFrame) {
            auto* scc = machine.scc_chip(1);
            result.scc_attached = scc != nullptr;
            const std::vector<std::int16_t> pcm = mixer.mix_interleaved_stereo(
                machine.psg(), MachineAudioMixer::SccSources{scc, nullptr}, &machine.ym2413(),
                kSamplesPerFrame);
            result.window_pcm.insert(result.window_pcm.end(), pcm.begin(), pcm.end());
        }
    }
    return result;
}

}  // namespace

int main() {
    // --- Skip gates. ---
    const std::filesystem::path rom_path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "aleste.rom";
    std::ifstream rom_in(rom_path, std::ios::binary);
    if (!rom_in) {
        std::cout << "SKIP: " << rom_path << " not present (local dev asset)\n";
        return 0;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(rom_in)),
                                          std::istreambuf_iterator<char>());
    const std::string actual_sha1 = sony_msx::machine::sha1_hex(image);
    if (actual_sha1 != kAlesteSha1) {
        std::cout << "SKIP: aleste.rom sha1=" << actual_sha1
                  << " differs from the specified dump " << kAlesteSha1 << "\n";
        return 0;
    }
    if (!std::filesystem::exists(std::filesystem::path(SONY_MSX_BIOS_DIR) / "f1xvbios.rom")) {
        std::cout << "SKIP: BIOS assets not present\n";
        return 0;
    }

    // --- Session 1 + the burst metric. ---
    const SessionResult first = run_session(image);
    expect(first.frames_run == kTotalFrames, "Session_RanAllFrames");
    expect(first.scc_attached, "Aleste_SccChipAttachedInBay1");
    expect(first.window_pcm.size() == 2ull * kSamplesPerFrame * (kTotalFrames - kWindowStartFrame),
           "WindowPcm_ExpectedSampleCount");

    for (int side = 0; side < 2; ++side) {
        std::vector<std::int16_t> channel;
        channel.reserve(first.window_pcm.size() / 2);
        for (std::size_t i = static_cast<std::size_t>(side); i < first.window_pcm.size(); i += 2) {
            channel.push_back(first.window_pcm[i]);
        }
        const auto metrics = block_metrics(channel);
        int burst_blocks = 0;
        int max_flips = 0;
        long long max_rms = 0;
        for (std::size_t b = 0; b < metrics.size(); ++b) {
            if (metrics[b].burst) {
                ++burst_blocks;
            }
            if (metrics[b].zcr_flips > max_flips) max_flips = metrics[b].zcr_flips;
            if (metrics[b].ac_rms > max_rms) max_rms = metrics[b].ac_rms;
            std::cout << (side == 0 ? "L" : "R") << " block " << b
                      << ": zcr_flips=" << metrics[b].zcr_flips
                      << " (zcr=" << static_cast<double>(metrics[b].zcr_flips) / 4095.0
                      << ") ac_rms=" << metrics[b].ac_rms << (metrics[b].burst ? "  BURST" : "")
                      << "\n";
        }
        std::cout << (side == 0 ? "LEFT" : "RIGHT") << " summary: blocks=" << metrics.size()
                  << " burst_blocks=" << burst_blocks << " max_zcr_flips=" << max_flips
                  << " max_ac_rms=" << max_rms << "\n";
        expect(metrics.size() == 35, "Metric_35FullBlocksOverTheWindow");
        expect(burst_blocks == 0, "ZeroBurstBlocks_UltrasonicIdiomRendersSilent_TheM34Oracle");
    }

    // --- Session 2: two-run byte identity. ---
    const SessionResult second = run_session(image);
    expect(second.window_pcm == first.window_pcm, "TwoSessions_ByteIdenticalWindowPcm");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM34AlesteUltrasonic_System cases passed\n";
    return 0;
}
