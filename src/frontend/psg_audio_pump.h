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

#include <cstdint>
#include <vector>

#include "devices/audio/psg_ym2149.h"

namespace sony_msx::frontend {

// Deterministic, SDL3-independent PSG audio-sample pump (M26-S5, docs/m26-
// planner-package.md §2.6 point 1). Advances a PsgYm2149's generator state by
// a fixed cycles-per-sample delta and collects its StereoSample output, one
// sample at a time -- the exact WIRING pattern Sdl3AudioPresenter uses inside
// its real SDL_AudioStream callback, factored out here with ZERO SDL3
// dependency so the wiring itself (not just the underlying advance_cycles()/
// sample() functions in isolation, already unit-tested since M15) is
// directly, headlessly ctest-provable (R-M26-4's "untested-in-anger" risk).
//
// This is a NEW real-time-driven CONSUMER of PsgYm2149::advance_cycles() --
// confirmed by a repo-wide grep (A-M26-4) to have ZERO call sites anywhere in
// this project before M26. Lives in src/frontend/ (presentation-layer sample
// generation, per src/CLAUDE.md's own explicit boundary) but is intentionally
// SDL3-independent so it stays testable under the default headless
// (-DSONY_MSX_ENABLE_SDL3=OFF) ctest configuration, mirroring the M26-S4
// frame_dump.* headless-buildable precedent. Sdl3AudioPresenter (SDL3-gated)
// is the thin, real SDL_AudioStream wiring layer built ON TOP of this class.
class PsgAudioPump {
public:
    // cycles_per_sample: the 3.58 MHz system-cycle delta advanced before each
    // sample() call (kSystemClockHz / sample_rate_hz, computed by the caller
    // -- this class carries no host audio-device knowledge of its own).
    explicit PsgAudioPump(std::uint64_t cycles_per_sample);

    // Advances `psg` by cycles_per_sample() and returns ONE audio-sample
    // tick's worth of real PSG synthesis.
    //
    // M34 (DEC-0043 Defect A, docs/m34-planner-package.md §2.3.6): the
    // returned sample is the chip's EXACT box average over the advanced
    // window (PsgYm2149::take_integrated_sample()), not the instantaneous
    // point sample() -- point-sampling folded >Nyquist chip content (the
    // Aleste-2 tone-period-0 ~112 kHz silence idiom) into a loud audible
    // alias. The pump's advance-exactly-W-then-take shape satisfies the
    // take-API's documented precondition by construction; W == 0 (the M26
    // idle case) stays exactly silent via the chip's zero-window guard.
    [[nodiscard]] devices::audio::PsgYm2149::StereoSample pump_one_sample(devices::audio::PsgYm2149& psg) const;

    // Convenience: pump `sample_count` samples in sequence, returning the
    // full collected sequence (the deterministic oracle shape the M26-S5
    // numeric test asserts against).
    [[nodiscard]] std::vector<devices::audio::PsgYm2149::StereoSample> pump_samples(
        devices::audio::PsgYm2149& psg, std::size_t sample_count) const;

    [[nodiscard]] std::uint64_t cycles_per_sample() const { return cycles_per_sample_; }

private:
    std::uint64_t cycles_per_sample_;
};

}  // namespace sony_msx::frontend
