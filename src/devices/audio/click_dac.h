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
#include <deque>

namespace sony_msx::devices::audio {

// MSX 1-bit "key-click" DAC on PPI port-C bit 7 (M39-A, the digitized-voice
// fix). This is the ONLY audio source on a bare HB-F1XV that games can drive
// at a SUB-FRAME rate to synthesize sampled speech: a game bit-bangs port-C
// bit 7 (0xFF <-> 0x80 on the analog output) hundreds/thousands of times per
// frame in a PWM/PDM pattern whose local duty cycle IS the demodulated PCM
// waveform (a scrolling-shooter title's Japanese copyright voice line, a
// split-screen title's speech). The
// PSG plays MUSIC (envelope/tone) during the voice, so by elimination the
// voice IS this 1-bit DAC -- which had no mixer consumer before M39.
//
// GROUNDING (openMSX, behaviour reference only -- never copied, GPL isolation):
//   - references/openmsx-21.0/src/MSXPPI.cc:117-131 writeC1: an EDGE-triggered
//     `(prevBits ^ value) & 8` on port-C bit 7 calls KeyClick::setClick.
//   - references/openmsx-21.0/src/sound/KeyClick.cc:15-21 setClick: on a level
//     CHANGE, dac.writeDAC(status ? 0xFF : 0x80) -- a DACSound8U.
//   - references/openmsx-21.0/src/sound/DACSound8U.cc:17-20 writeDAC(value):
//     forwards DACSound16S::writeDAC(value - 0x80) -- so idle 0x80 -> 0
//     (silent), click 0xFF -> +0x7F.
//   - references/openmsx-21.0/src/sound/DACSound16S.cc:34-43 writeDAC: a pure
//     DELTA into a band-limited blip buffer (`blip.addDelta(t, value -
//     lastWrittenValue)`); a held level, once its transient settles, is
//     AC-coupled to zero downstream. openMSX's blip is a true band-limited
//     step; this project has no blip buffer, so this class instead reuses the
//     M34 box-average (sinc) reconstruction the PSG/SCC already use, plus an
//     integer DC-blocker to reproduce the "held level -> 0" AC coupling.
//
// SEAM SHAPE (mirrors SccWavetable exactly -- the M29/M34 additive-source
// contract): the machine records cycle-timestamped bit-7 EDGES here via
// record_edge(); the frontend audio mixer then advance_cycles(W) +
// take_integrated_sample(W) once per output sample, box-averaging the edge
// timeline over each ~81-cycle window. The edge stamp is the deterministic
// scheduler cycle at the writing instruction's start (instruction-granular,
// the same precision class as the M32 VDP render-sync seam) -- NEVER a
// wall-clock, so replays are byte-identical.
//
// AC-COUPLED OUTPUT (the openMSX delta/DAC analogue): take_integrated_sample()
// returns the box-averaged level (the local duty cycle, a fixed-point fraction
// in [0, kUnit]) MINUS a slowly-adapting DC estimate (a one-pole integer
// DC-blocker, fc ~= 1.7 Hz at 44.1 kHz). A HELD level therefore decays to a
// contribution of ~0; only TRANSITIONS (the voice) survive. Signed output in
// roughly [-kUnit, +kUnit].
//
// IDLE BYTE-IDENTITY (mandatory, M39): after reset() bit 7 is 0. With bit 7
// never toggled the level is a constant 0, so every integral is 0, the DC
// estimate stays exactly 0, and take_integrated_sample() returns EXACTLY 0 --
// the click term the mixer adds is exactly 0, so every existing audio oracle
// is byte-identical (the M29/M31/M34/M37 null-source pattern). Even with
// capture disabled (the default), record_edge() is a no-op, so headless /
// non-audio runs never allocate an edge and never advance the DC estimate.
//
// DETERMINISM: all integer arithmetic (no floats in the sample path, matching
// the M34 DEC-0043 risk-note-3 discipline); the DC-blocker uses truncating
// integer division (well-defined on every compiler) so a replay is
// byte-identical across runs and platforms.
class ClickDac {
public:
    // Level fixed-point unit: a held-high bit 7 integrates to exactly kUnit
    // per cycle. Exposed so the mixer can normalize the signed AC sample back
    // to [-1, +1] before applying the presentation amplitude. Power of two so
    // the mixer's normalization is an exact shift-class divide.
    static constexpr std::int32_t kUnit = 1 << 15;  // 32768

    // One-pole DC-blocker (AC-coupling) constants. dc_estimate is held as an
    // int64 scaled by 2^kDcShift; each sample it moves 1/2^kDcLog2Tau of the
    // way toward the current box-averaged level. tau = 2^12 = 4096 samples
    // ~= 93 ms at 44.1 kHz -> fc ~= 1.7 Hz: transparent to the voice band
    // (speech formants 0.2-3 kHz), removes only true DC / the slow keyclick
    // pedestal.
    static constexpr int kDcShift = 16;
    static constexpr int kDcLog2Tau = 12;

    // Safety cap on the pending-edge deque. Under the audio path the mixer
    // drains it every frame (pending stays ~1-2 frames of edges), so this is
    // never hit; it only bounds memory if capture is enabled but NOTHING
    // consumes (advance_cycles never called) -- a configuration that produces
    // no audio anyway, so dropping the oldest edge there is inaudible.
    static constexpr std::size_t kMaxPendingEdges = 1u << 16;  // 65,536

    // Enable/disable edge capture. Default DISABLED: record_edge() is a no-op,
    // so a headless / non-audio run has zero overhead and allocates nothing.
    // The SDL3 frontend and the headless audio-dump path enable it. Not
    // touched by reset() (it is a wiring config, like a clock-source
    // attachment, not per-boot device state).
    void set_capture_enabled(bool enabled) { capture_enabled_ = enabled; }
    [[nodiscard]] bool capture_enabled() const { return capture_enabled_; }

    // Deterministic power-on state: no pending edges, level 0 (silent), the
    // integrator and DC estimate cleared, the consumed-cycle cursor back to 0
    // (aligned with the scheduler's own cold-boot reset to cycle 0). Leaves
    // capture_enabled_ as-is (see above).
    void reset();

    // Record a bit-7 EDGE at absolute scheduler cycle `cycle` to the new level
    // (true = 0xFF click asserted, false = 0x80 idle). Called by the PPI seam
    // ONLY on an actual bit-7 toggle (openMSX's `(prevBits ^ value) & 8`), so
    // consecutive records always alternate level. No-op when capture is
    // disabled. edge_count() counts every recorded edge (diagnostic).
    void record_edge(std::uint64_t cycle, bool level);

    // Advance the reconstruction by `window_cycles` (the mixer's per-sample
    // delta), integrating the piecewise-constant bit-7 level over
    // [consumed_cycle_, consumed_cycle_ + window_cycles) -- a dwell walk over
    // the recorded edges exactly like PsgYm2149/SccWavetable::advance_cycles().
    // Boundary convention (matching the M34 sources): an edge at cycle t takes
    // effect AT t (the dwell before t belongs to the pre-edge level).
    void advance_cycles(std::uint64_t window_cycles);

    // Return the box-averaged level over the advanced window, AC-coupled
    // through the DC-blocker, as a signed fixed-point sample in roughly
    // [-kUnit, +kUnit] (kUnit == "full scale"), then reset the window
    // integral. window_cycles == 0 returns 0 and does NOT perturb the DC
    // estimate (the idle-pump guard, mirroring the M34 take-APIs). PRECONDITION
    // (as for the M34 sources): the caller advanced exactly window_cycles since
    // the previous take.
    [[nodiscard]] std::int32_t take_integrated_sample(std::uint64_t window_cycles);

    // Diagnostic introspection (M39 Step 1 confirmation, and tests): cumulative
    // count of recorded bit-7 edges since the last reset(). The machine reads
    // this per-frame to report the voice-window edge RATE.
    [[nodiscard]] std::uint64_t edge_count() const { return edge_count_; }
    // Pending (not-yet-integrated) edges currently buffered -- for tests.
    [[nodiscard]] std::size_t pending_edges() const { return edges_.size(); }
    // Current DC-blocked-source's held level (false=0x80 idle) -- for tests.
    [[nodiscard]] bool current_level() const { return current_level_; }

private:
    struct Edge {
        std::uint64_t cycle;
        bool level;
    };

    bool capture_enabled_ = false;
    std::deque<Edge> edges_;
    std::uint64_t consumed_cycle_ = 0;  // absolute cycle integrated up to
    bool current_level_ = false;        // bit-7 level as of consumed_cycle_
    std::int64_t level_dwell_integral_ = 0;  // Sigma level*dwell for the window
    std::int64_t dc_acc_ = 0;                // DC estimate * 2^kDcShift
    std::uint64_t edge_count_ = 0;           // cumulative recorded edges
};

}  // namespace sony_msx::devices::audio
