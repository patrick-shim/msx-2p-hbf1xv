#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "devices/audio/psg_ym2149.h"

namespace sony_msx::frontend {

// Deterministic, decoded-PSG-audio dump serializer (M27-S5, "Production
// Hardening + Debug/Test Tooling" item 2, docs/m27-planner-package.md §2.3).
//
// Genuinely reuses M26's real PSG sample-generation wiring
// (frontend::PsgAudioPump / devices::audio::PsgYm2149::advance_cycles()/
// sample()) -- NOT a parallel/duplicate synthesis implementation -- and
// mirrors machine/frame_dump.h's exact reuse-of-debug_dump::serialize_region()
// discipline (src/frontend/ MAY depend on src/machine/, A-M27-8). Headless-
// buildable, no SDL3 dependency (A-M27-7): added to the SAME unguarded
// `sony_msx_core` source list `psg_audio_pump.cpp`/`sdl3_cli.cpp` already use.
//
// Format (mirrors frame_dump.h's exact doc-comment/format discipline):
//   HBF1XV-AUDIO-DUMP v1
//   [AUDIO] sample_rate=<hz> channels=2 bits=16 samples=<N>
//   [PCM] size=<N*4>            (folded canonical hex, REUSING
//                                 machine::debug_dump::serialize_region())
//   [END]
//
// PCM payload layout: `N` little-endian signed-16-bit stereo sample pairs
// (left, right interleaved) -- 4 bytes per sample. Host-native little-endian
// x86/x64 assumption (the SAME assumption frame_dump.h's own doc comment
// already discloses for its pixel buffer).
inline constexpr const char* kAudioDumpFormatTag = "HBF1XV-AUDIO-DUMP v1";

// Documented, linear, deterministic PCM-scaling formula (R-M27-3), grounded
// in the REAL `PsgYm2149::sample()` numeric range: each of
// StereoSample::left/right sums two 5-bit `resolved_amplitude()` values
// (each in [0,31]; psg_ym2149.cpp `sample()`/`resolved_amplitude()`,
// re-confirmed this cycle, A-M27-4), so `raw_sum` is in [0, 62]. Linear map:
// 0 -> INT16_MIN (-32768), 62 -> INT16_MAX (32767). Out-of-range input
// (should not occur given the grounded [0,62] range, but the device layer is
// not itself modified by this milestone) is clamped, never overflowed.
[[nodiscard]] std::int16_t psg_raw_sum_to_pcm16(std::int32_t raw_sum);

// Pumps `sample_count` real StereoSample values from `psg` via a NEW
// PsgAudioPump(kSystemClockHz / sample_rate_hz) (genuine reuse of the M26
// wiring pattern; A-M27-5's already-shipped Sdl3AudioPresenter::kSampleRateHz
// = 44100 is the natural default a caller should pass for consistency, but
// this function takes the rate explicitly so it stays independent of the
// SDL3-gated Sdl3AudioPresenter type), converts each channel to signed
// 16-bit PCM via psg_raw_sum_to_pcm16(), and serializes via the EXISTING
// machine::debug_dump::serialize_region() folded-hex routine (genuine reuse,
// mirrors frame_dump.cpp's own reuse precedent exactly). Advances `psg`'s
// OWN generator state by `sample_count * cycles_per_sample` cycles as a
// side effect (the same "pumping consumes deterministic emulated time"
// contract PsgAudioPump::pump_samples() already documents).
[[nodiscard]] std::string serialize_psg_audio_dump(devices::audio::PsgYm2149& psg,
                                                    std::uint64_t sample_rate_hz,
                                                    std::size_t sample_count);

// Writes to <debug_root>/sounds/<filename> (a NEW debug/ subfolder,
// matching the existing traces//logs//frames/ convention -- CLAUDE.md's own
// explicit "organize sub-directories if needed" license), creating the
// directory if absent. Takes debug_root explicitly (not a Hbf1xvMachine
// method) so this stays a pure src/frontend/ utility with zero new
// machine-level surface (A-M27-9's risk-reduction goal -- item 1 already
// needed no new machine method; item 2 needs none either). Returns true on
// success.
bool write_psg_audio_dump(const std::filesystem::path& debug_root, const std::string& filename,
                           devices::audio::PsgYm2149& psg, std::uint64_t sample_rate_hz,
                           std::size_t sample_count);

}  // namespace sony_msx::frontend
