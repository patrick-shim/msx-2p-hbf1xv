#pragma once

#include <array>
#include <cstdint>

namespace sony_msx::devices::audio {

// Konami SCC 5-channel wavetable sound generator, plain-SCC ("Real") mode as
// shipped inside Konami SCC MegaROM cartridges (M29-S2, backlog G1; spec
// docs/m29-planner-package.md §2.2; grounding references/fact-sheets/
// "Konami SCC.md" -- cited below as "SCC fact-sheet §N").
//
// NOT a core::IoDevice / core::MemoryDevice: the chip is reached ONLY through
// its owning CartridgeKonamiScc mapper's memory window at 0x9800-0x9FFF (the
// openMSX shape: RomKonamiSCC owns a real `SCC scc;` member --
// references/openmsx-21.0/src/memory/RomKonamiSCC.cc, behaviour reference
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
//   is documented hardware behaviour explicitly NOT emulated (planner §1.3).
//
// Rotation time base (A-M29-6, a DISCLOSED simplification): the rotation
// readback shift advances off an internal master-cycle accumulator fed by
// advance_cycles() ONLY (no machine-clock attachment) -- CPU reads between
// pump batches see rotation frozen at the last advance. Affects only the
// exotic TEST modes, never normal playback. Rotation step rate is the
// fact-sheet's measured 3.58MHz/(period+1) per shift (fact-sheet §6, Pazos
// verbatim); openMSX's own /32 tick granularity here is its resampler
// artifact, not chip semantics ("do not copy it", fact-sheet §10.1) -- a
// deliberate, disclosed divergence from the reference implementation in
// favour of the measured formula. In all-rotate mode (bits7:6 == 01) channel
// 4 rotates at CHANNEL 5's period (fact-sheet §6 bit6 row, SCC.cc:257-262).
//
// reset() = the enen-measured POWER-ON state (SCC fact-sheet §7): wave RAM
// all 0xFF, volumes 15, channel-enable 0, deform 0, periods 0 (i.e. stopped,
// period < 9), positions 0, held outputs refreshed. A-M29-5 DISCLOSURE:
// openMSX splits powerUp (full state) from soft reset (enable/deform only);
// this project's single deterministic reset() collapses both into the
// power-on state, consistent with CartridgeSlot::load()'s reset-once
// contract -- a documented simplification, not an oversight.
//
// Mode-awareness (SCC fact-sheet §10.3, R-M29-9): the dispatch below is
// structured so the SCC-I "Compatible"/"Plus" register maps (backlog row G5)
// can be added later as additional Mode enumerators WITHOUT restructuring --
// but ONLY Mode::Real ships in M29; no Plus/Compatible behaviour exists or
// is claimed anywhere in this file.
class SccWavetable {
public:
    static constexpr int kChannels = 5;
    static constexpr int kWaveLength = 32;
    // De Schrijder DC centre (fact-sheet §5).
    static constexpr std::int32_t kDcCentre = 640;

    // Power-on state per SCC fact-sheet §7 (see class comment / A-M29-5).
    void reset();

    // 256-byte plain-SCC map access (offset = CPU address & 0xFF, supplied by
    // the owning mapper). read() carries the real deform-range side effect
    // (read acts as write 0xFF); peek() never mutates state.
    [[nodiscard]] std::uint8_t read(std::uint8_t offset);
    [[nodiscard]] std::uint8_t peek(std::uint8_t offset) const;
    void write(std::uint8_t offset, std::uint8_t value);

    // Deterministic generator advance in 3.58 MHz MASTER cycles (SCC
    // fact-sheet §10.1). Also advances the internal rotation time base
    // (A-M29-6). Never touches CPU T-state accounting.
    void advance_cycles(std::uint64_t delta_cycles);

    // AC mix sum over the enabled channels (range ~ +-600, fact-sheet §5).
    [[nodiscard]] std::int32_t sample() const;
    // The literal De Schrijder form: 640 + sample() (range +40..+1235).
    [[nodiscard]] std::int32_t amp_out() const;

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
    // SCC-I readiness seam ONLY (G5, fact-sheet §10.3): a single enumerator
    // ships; no Compatible/Plus code path exists in M29 (R-M29-9).
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
    // Internal rotation time base in master cycles (A-M29-6): reset to 0 by
    // deform-register changes and by period writes under deform bit5 (the
    // Artag-confirmed resync, fact-sheet §6 bit5 row).
    std::uint64_t deform_cycles_ = 0;
};

}  // namespace sony_msx::devices::audio
