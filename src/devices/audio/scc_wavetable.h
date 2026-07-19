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

#include <array>
#include <cstdint>

namespace sony_msx::devices::audio {

// Konami SCC 5-channel wavetable sound generator, plain-SCC ("Real") mode as
// shipped inside Konami SCC MegaROM cartridges (grounding: the Konami SCC
// fact sheet -- cited below as "SCC fact-sheet §N").
//
// NOT a core::IoDevice / core::MemoryDevice: the chip is reached ONLY through
// its owning CartridgeKonamiScc mapper's memory window at 0x9800-0x9FFF (the
// openMSX shape: RomKonamiSCC owns a real `SCC scc;` member --
// openMSX 21.0: src/memory/RomKonamiSCC.cc, behaviour reference
// only, never copied; GPL isolation).
//
// Register map (plain SCC, 256-byte map, SCC fact-sheet §3):
//   0x00-0x7F  waveforms ch1..ch4, 32 signed bytes each, R/W. A write to
//              ch4's region (0x60-0x7F) ALSO lands in ch5's playback copy --
//              the plain SCC has no independent ch5 wave RAM (double-grounded,
//              fact-sheet §3/§9 "agreements": SCC.cc:355-372 / fMSX SCC.c:69-71).
//   0x80-0x8F  frequency/volume/enable block (write-only; reads 0xFF):
//              +0/+1..+8/+9 ch1..ch5 12-bit period low/high (high write's
//              bits 4-7 ignored); +0xA..+0xE ch1..ch5 4-bit volume;
//              +0xF channel-enable bits 0-4.
//   0x90-0x9F  x2 mirror of 0x80-0x8F ("region is visible twice", SCC.cc:376).
//   0xA0-0xDF  no function (reads 0xFF, writes ignored).
//   0xE0-0xFF  deformation/test register (§ below). A READ of this range
//              acts as a WRITE of 0xFF and returns 0xFF (Pazos, SCC fact-sheet
//              §3/§6 -- hence read() is non-const; peek() is the
//              side-effect-free variant for dumps/tests).
//
// Generator model (SCC fact-sheet §4/§10.1): per channel, the 5-bit waveform
// position steps once per (period+1) MASTER cycles (3.579545 MHz) via integer
// modular accumulation -- the `+1` divisor is fixed by Pazos's measured
// rotation formula (fact-sheet §9.1 arbitration; fMSX omits it). Writing a
// period value <= 8 stops the channel's counter entirely, holding the current
// output byte (NYYRIKKI, fact-sheet §9.2 arbitration). The held output
// (int8 wave[pos] * vol4) >> 4 is refreshed ONLY at position steps and at
// period writes (NYYRIKKI restart) -- volume and wave-RAM writes therefore
// reach the output at the NEXT step, never retroactively (fact-sheet §4
// latching). Disabled channels contribute 0 to the mix but keep their phase
// counters running (fact-sheet §4).
//
// Mixing (Jon De Schrijder's 2003 scope measurement, SCC fact-sheet §5):
//   AmpOut = 640 + sum of ((SampleValue*VolX) AND #7FF0) div 16
// The signed product's low 4 bits are dropped BEFORE summation (arithmetic
// shift right by 4); amp_out() exposes the literal 640-centred form for
// direct De Schrijder oracles, sample() the AC sum (~+-600) for mixing.
//
// Deformation register (Manuel Pazos 2003, SCC fact-sheet §6):
//   bit0 4-bit frequency mode (period >> 8); bit1 8-bit mode (period & 0xFF),
//   bit1 wins when both set; bits 2-4 no documented function; bit5 waveform
//   restarts from position 0 on frequency writes (also resyncs the rotation
//   time base); bit6 rotate ALL waveforms (wave RAM read-only); bit7 rotate
//   ch4(/5) only (that RAM read-only; plain-SCC only). bits6+7 both: only
//   ch1-3 rotate, ALL wave RAM read-only. The bits-0/1 period masking is
//   applied when a frequency register is WRITTEN (matching openMSX
//   SCC.cc:385-392) -- an already-latched period is not retroactively
//   re-masked by a later deform write. Pazos's bits6+7 data-bus noise anomaly
//   is documented hardware behaviour explicitly NOT emulated.
//
// Rotation time base (a DISCLOSED simplification): the rotation
// readback shift advances off an internal master-cycle accumulator fed by
// advance_cycles() ONLY (no machine-clock attachment) -- CPU reads between
// pump batches see rotation frozen at the last advance. Affects only the
// exotic TEST modes, never normal playback. Rotation step rate is the
// fact-sheet's measured 3.58MHz/(period+1) per shift (fact-sheet §6, Pazos
// verbatim); openMSX's own /32 tick granularity here is its resampler
// artifact, not chip semantics ("do not copy it", fact-sheet §10.1) -- a
// deliberate divergence from the reference implementation, favouring the
// measured formula. In all-rotate mode (bits7:6 == 01) channel 4 rotates at
// CHANNEL 5's period (fact-sheet §6 bit6 row, SCC.cc:257-262).
//
// reset() = the enen-measured POWER-ON state (SCC fact-sheet §7): wave RAM
// all 0xFF, volumes 15, channel-enable 0, deform 0, periods 0 (i.e. stopped,
// period < 9), positions 0, held outputs refreshed. DISCLOSURE:
// openMSX splits powerUp (full state) from soft reset (enable/deform only);
// this project's single deterministic reset() collapses both into the
// power-on state, consistent with CartridgeSlot::load()'s reset-once
// contract -- a documented simplification, not an oversight.
//
// Mode-awareness (SCC fact-sheet §10.3): the dispatch below is
// structured so the SCC-I "Compatible"/"Plus" register maps
// can be added later as additional Mode enumerators WITHOUT restructuring --
// but ONLY Mode::Real is implemented; no Plus/Compatible behaviour exists or
// is claimed anywhere in this file.
class SccWavetable {
public:
    static constexpr int kChannels = 5;
    static constexpr int kWaveLength = 32;
    // De Schrijder DC centre (fact-sheet §5).
    static constexpr std::int32_t kDcCentre = 640;

    // Power-on state per SCC fact-sheet §7 (see the class-comment disclosure).
    void reset();

    // 256-byte plain-SCC map access (offset = CPU address & 0xFF, supplied by
    // the owning mapper). read() carries the real deform-range side effect
    // (read acts as write 0xFF); peek() never mutates state.
    [[nodiscard]] std::uint8_t read(std::uint8_t offset);
    [[nodiscard]] std::uint8_t peek(std::uint8_t offset) const;
    void write(std::uint8_t offset, std::uint8_t value);

    // Deterministic generator advance in 3.58 MHz MASTER cycles (SCC
    // fact-sheet §10.1). Also advances the internal rotation time base
    // (the disclosed simplification above). Never touches CPU T-state accounting.
    void advance_cycles(std::uint64_t delta_cycles);

    // AC mix sum over the enabled channels (range ~ +-600, fact-sheet §5).
    //
    // POINT-SAMPLE API, deliberately kept byte-identical for existing oracles:
    // the instantaneous held-output sum. The PRODUCTION mix path uses
    // take_integrated_sample() below (point-sampling
    // aliases >Nyquist wavetable stepping into the audible band; DEC-0043).
    [[nodiscard]] std::int32_t sample() const;
    // The literal De Schrijder form: 640 + sample() (range +40..+1235).
    [[nodiscard]] std::int32_t amp_out() const;

    // ------------------------------------------------------------------
    // Box-average integration API -- the anti-aliasing production sample
    // path (DEC-0043), the same paired-API shape
    // as PsgYm2149::take_integrated_sample().
    //
    // During advance_cycles(), one mono int64 integral accumulates
    // Σ level(t) × dwell_cycles over the enabled channels, walked at each
    // channel's OWN (period+1)-master-cycle position-step boundaries: a
    // stopped channel (period <= 8) holds out_ constant and contributes
    // exactly out × delta; a running channel contributes its held output per
    // dwell segment, refreshed at every position step (which is also where
    // pending volume/wave writes become audible -- fact-sheet §4 latching,
    // unchanged). Channel-enable gating matches sample() exactly (enable_
    // bit clear => the channel contributes 0 to the integral while its phase
    // keeps running).
    //
    // BOUNDARY CONVENTION: a position step completing at cycle t
    // changes the level effective AFTER cycle t -- the completing cycle's
    // dwell belongs to the pre-step held output.
    //
    // take_integrated_sample(W) returns round(integral, W) via the shared
    // round-half-away-from-zero helper (dwell_rounding.h), then
    // resets the integral. W == 0 returns 0. PRECONDITION:
    // the caller advances exactly window_cycles between takes
    // (MachineAudioMixer guarantees this by construction). FIXED-POINT
    // PROPERTY: a constant summed level L integrates to exactly L.
    //
    // WHAT THE BOX FILTER HONESTLY IS (a disclosed
    // simplification -- the SAME response as the PSG side, T = W/3,579,545 s):
    // |H(f)| = |sin(pi f T)/(pi f T)| -- a sinc, NOT a brickwall. For a
    // full-volume PSG-style square of half-period H cycles and W = 81 the
    // exact worst-case AC bound is A*B/W with B = H/2 (H <= W/2) else
    // H - W/2; the per-period consequence table (p = 0..5 rows, partial
    // suppression at p=2/4, passband droop -0.007 dB @1 kHz .. -1.72 dB
    // @15 kHz) is recorded in full in psg_ym2149.h's contract block and
    // applies unchanged to SCC content at the same step rates: ultrasonic
    // stepping (period+1 <= ~40 cycles) is strongly suppressed but NOT
    // erased; audible-band wavetable playback sees only sinc rolloff. This
    // is a disclosed simplification vs openMSX's true band-limited
    // resampling (openMSX 21.0: src/sound/ResampledSoundDevice.hh:
    // 23,29,46-48, BlipBuffer.hh:1-28 -- behaviour reference only, never
    // copied); genuine band-limited resampling remains deferred future
    // work.
    // ------------------------------------------------------------------
    [[nodiscard]] std::int32_t take_integrated_sample(std::uint64_t window_cycles);

    // Introspection for deterministic tests (side-effect free).
    [[nodiscard]] std::int8_t wave(int channel, int index) const;
    [[nodiscard]] std::uint16_t period(int channel) const;      // effective (deform-masked)
    [[nodiscard]] std::uint16_t org_period(int channel) const;  // as-written 12-bit
    [[nodiscard]] std::uint8_t volume(int channel) const;
    [[nodiscard]] std::uint8_t enable_bits() const { return enable_; }
    [[nodiscard]] std::uint8_t deform() const { return deform_; }
    [[nodiscard]] std::uint8_t position(int channel) const;
    [[nodiscard]] std::int32_t held_output(int channel) const;

private:
    // SCC-I readiness seam ONLY (fact-sheet §10.3): a single enumerator
    // ships; no Compatible/Plus code path exists.
    enum class Mode { Real };

    // (int8 wave * vol4) >> 4 -- the measured AND-#7FF0-div-16 (fact-sheet §5).
    [[nodiscard]] std::int32_t adjust(int channel) const;
    [[nodiscard]] std::uint8_t read_wave(int channel, std::uint8_t offset) const;
    void write_wave(int channel, std::uint8_t offset, std::uint8_t value);
    void set_freq_vol(std::uint8_t address, std::uint8_t value);
    void set_deform(std::uint8_t value);
    void apply_deform(std::uint8_t value);

    Mode mode_ = Mode::Real;
    std::array<std::array<std::int8_t, kWaveLength>, kChannels> wave_{};
    std::array<std::uint16_t, kChannels> org_period_{};
    std::array<std::uint16_t, kChannels> period_{};
    std::array<std::uint8_t, kChannels> volume_{};
    std::array<std::uint8_t, kChannels> pos_{};
    std::array<std::uint64_t, kChannels> count_{};
    std::array<std::int32_t, kChannels> out_{};
    std::array<bool, kChannels> rotate_{};
    std::array<bool, kChannels> read_only_{};
    std::uint8_t enable_ = 0;
    std::uint8_t deform_ = 0;
    // Internal rotation time base in master cycles: reset to 0 by
    // deform-register changes and by period writes under deform bit5 (the
    // Artag-confirmed resync, fact-sheet §6 bit5 row).
    std::uint64_t deform_cycles_ = 0;
    // Mono Σ level×dwell accumulator for take_integrated_sample()
    // (contract above). |level| <= 600, so int64 never overflows for
    // any realistic window.
    std::int64_t level_dwell_integral_ = 0;
};

}  // namespace sony_msx::devices::audio
