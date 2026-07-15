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

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace sony_msx::frontend {

// ---------------------------------------------------------------------------
// M52 (DEC-0079, docs/m52-planner-package.md §2.1): the SDL-free master-volume
// gain + step helper. Header-only and SDL3-independent (the
// machine_audio_mixer.h precedent), so the WHOLE gain law is headlessly
// ctest-provable with NO audio device -- the unit tests target these pure
// functions directly.
//
// PLACEMENT (planner §2.1): the master gain is a PURE POST-MIX SCALAR applied
// strictly AFTER the DEC-0039 machine mix, entirely inside Sdl3AudioPresenter
// (which the headless build never instantiates). It is NOT in MachineAudioMixer
// and is NOT a change to kAmplitudeScale/any mixer amplitude constant -- the
// DEC-0039 PSG:FM:keyclick balance stays byte-identical at full volume.
//
// GAIN LAW (planner DECISION -- linear integer percent):
//     scaled = clamp_int16( (int32)sample * volume_percent / 100 )
//   * volume_percent in [0,100]; 100 = unity (full), 0 = silence.
//   * At volume_percent == 100: sample*100/100 == sample EXACTLY for every
//     int16 (no overflow: |sample*100| <= 3,276,700 < INT32_MAX), so the law is
//     a mathematical identity at full volume -- the byte-identity guarantee.
//   * Because max volume is 100 (attenuation only), |scaled| <= |sample| so the
//     clamp never actually triggers for volume <= 100; it is retained
//     defensively / for documentation symmetry with the mixer.
//   * Disclosed simplification: perceived loudness is not linear in percent (a
//     documented limitation, not a defect) -- matches the disclosed-simplification
//     style of the existing audio path (kAmplitudeScale is itself a fixed linear
//     scale, sdl3_audio_presenter.h:78-82).
// ---------------------------------------------------------------------------

// Clamp a raw volume request into the valid [0,100] percent domain. Attenuation
// only -- there is NO amplification (max is 100 = unity, planner §1.2).
[[nodiscard]] inline int clamp_master_volume(const int volume_percent) {
    return std::clamp(volume_percent, 0, 100);
}

// Apply the linear-integer-percent master gain to ONE interleaved-stereo int16
// sample. At volume 100 this returns `s` unchanged (mathematical identity); at
// 0 it returns 0. Never amplifies.
[[nodiscard]] inline std::int16_t apply_master_gain_sample(const std::int16_t s,
                                                           const int volume_percent) {
    const int v = clamp_master_volume(volume_percent);
    const std::int32_t scaled = static_cast<std::int32_t>(s) * v / 100;
    return static_cast<std::int16_t>(std::clamp<std::int32_t>(scaled, -32768, 32767));
}

// Apply the master gain IN PLACE to an interleaved-stereo int16 PCM buffer.
//   * volume >= 100 (unity): NO-OP -- the buffer is left byte-identical (the
//     byte-identity guarantee at full volume; no per-sample work at all).
//   * volume == 0: the buffer is zeroed (0 is gain-invariant either way).
//   * 0 < volume < 100: each sample is attenuated by s*volume/100.
inline void apply_master_gain(std::vector<std::int16_t>& pcm, const int volume_percent) {
    const int v = clamp_master_volume(volume_percent);
    if (v >= 100) {
        return;  // unity -> byte-identical, no scaling
    }
    for (std::int16_t& s : pcm) {
        s = apply_master_gain_sample(s, v);
    }
}

// Step the master volume by one grid increment in the direction `dir`
// (dir > 0 = up, dir < 0 = down, 0 = no change), landing on the {0,step,...,100}
// grid and CLAMPING at both ends -- it MUST NOT wrap (turning volume down past 0
// must never jump to 100; a dangerous loudness surprise -- planner §2.3, the
// explicit divergence from the persistence stepper which wraps).
//
// A non-grid launch value (e.g. --volume 55) snaps to the adjacent grid point in
// the direction of travel: up from 55 -> 60 (next grid up), down from 55 -> 50
// (next grid down). For an on-grid value this is a plain +/-step (e.g. 70 down ->
// 60), matching the persistence stepper's on-grid feel without its wrap. This
// direction-aware snap satisfies every acceptance value in planner §4.1 S2-1
// (step(70,-1)=60, step(0,-1)=0, step(100,+1)=100, step(55,-1)=50, step(55,+1)=60);
// the planner's prose "round_to_grid + dir*step" would have yielded 70 for the
// off-grid +1 case, inconsistent with its own S2-1 example -- the concrete S2-1
// acceptance values are the oracle followed here.
[[nodiscard]] inline int step_master_volume(const int current, const int dir, const int step) {
    const int cur = clamp_master_volume(current);
    const int s = step > 0 ? step : 1;  // defensive: a non-positive step falls back to 1
    int next = cur;
    if (dir > 0) {
        next = (cur / s + 1) * s;  // next grid strictly above cur (floor-div then +1)
    } else if (dir < 0) {
        next = ((cur + s - 1) / s - 1) * s;  // next grid strictly below cur (ceil-div then -1)
    }
    return std::clamp(next, 0, 100);  // CLAMP, never wrap
}

}  // namespace sony_msx::frontend
