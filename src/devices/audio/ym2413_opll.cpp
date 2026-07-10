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

#include "devices/audio/ym2413_opll.h"

#include <algorithm>

namespace sony_msx::devices::audio {

namespace {

// ROM instrument patch table (fact-sheet §4, planner package §2.2), REPRODUCED
// VERBATIM (never re-derived). Each row is the raw $00-$07 byte sequence
// (byte i = register $0i): mod $00/$02/$04/$06, car $01/$03/$05/$07.
//
// A-M17-7 caveat (preserved per the planner's explicit instruction): the
// fact-sheet itself notes this is "the community-standard set (originally
// by-ear, later confirmed via the FHB013 die shot / NukeYKT debug-mode
// dump)... treat exact patch bytes as ~99% but not datasheet-certain." The
// HB-F1XV machine XML selects `<ym2413-core>NukeYKT</ym2413-core>`
// (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:193) -- the
// die-shot-derived core openMSX adopted for accuracy
// (references/openmsx-21.0/src/sound/YM2413NukeYKT.hh, "close to how the real
// hardware is implemented"). Reconciling this table against NukeYKT's exact
// decoded per-field patches is deferred to whichever milestone implements DSP
// depth (backlog E1) -- only real audio synthesis makes the byte-exact
// difference observable (M17 is register/channel/rhythm contract only).
constexpr std::array<std::array<std::uint8_t, 8>, 15> kMelodyPatches{{
    {0x71, 0x61, 0x1E, 0x17, 0xD0, 0x78, 0x00, 0x17},  //  1 Violin
    {0x13, 0x41, 0x1A, 0x0D, 0xD8, 0xF7, 0x23, 0x13},  //  2 Guitar
    {0x13, 0x01, 0x99, 0x00, 0xF2, 0xC4, 0x11, 0x23},  //  3 Piano
    {0x31, 0x61, 0x0E, 0x07, 0xA8, 0x64, 0x70, 0x27},  //  4 Flute
    {0x32, 0x21, 0x1E, 0x06, 0xE0, 0x76, 0x00, 0x28},  //  5 Clarinet
    {0x31, 0x22, 0x16, 0x05, 0xE0, 0x71, 0x00, 0x18},  //  6 Oboe
    {0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x10, 0x07},  //  7 Trumpet
    {0x23, 0x21, 0x2D, 0x14, 0xA2, 0x72, 0x00, 0x07},  //  8 Organ
    {0x61, 0x61, 0x1B, 0x06, 0x64, 0x65, 0x10, 0x17},  //  9 Horn
    {0x41, 0x61, 0x0B, 0x18, 0x85, 0xF7, 0x71, 0x07},  // 10 Synthesizer
    {0x13, 0x01, 0x83, 0x11, 0xFA, 0xE4, 0x10, 0x04},  // 11 Harpsichord
    {0x17, 0xC1, 0x24, 0x07, 0xF8, 0xF8, 0x22, 0x12},  // 12 Vibraphone
    {0x61, 0x50, 0x0C, 0x05, 0xC2, 0xF5, 0x20, 0x42},  // 13 Synth Bass
    {0x01, 0x01, 0x55, 0x03, 0xC9, 0x95, 0x03, 0x02},  // 14 Acoustic Bass
    {0x61, 0x41, 0x89, 0x03, 0xF1, 0xE4, 0x40, 0x13},  // 15 Electric Guitar
}};

constexpr std::array<std::uint8_t, 8> kRhythmBd{0x01, 0x01, 0x18, 0x0F, 0xDF, 0xF8, 0x6A, 0x6D};
constexpr std::array<std::uint8_t, 8> kRhythmSdHh{0x01, 0x01, 0x00, 0x00, 0xC8, 0xD8, 0xA7, 0x48};
constexpr std::array<std::uint8_t, 8> kRhythmTomCym{0x05, 0x01, 0x00, 0x00, 0xF8, 0xAA, 0x59, 0x55};

}  // namespace

void Ym2413Opll::reset() {
    // A-M17-4: references/openmsx-21.0/src/sound/YM2413Okazaki.cc:707-720
    // zeroes every register ($00-$3F) and the address latch on reset().
    regs_.fill(0);
    latch_ = 0;
    // E2 (M28-S1): reset the write-timing tracker's device state (the
    // last-write bookkeeping), but not the clock_source_ pointer or the
    // write_timing_enforced_ toggle -- those are wiring/config, mirroring
    // Rp5c01::reset() leaving clock_source_/clock_gate_ untouched
    // (src/devices/rtc/rp5c01.h/.cpp).
    has_last_write_ = false;
    last_write_was_address_ = false;
    last_write_cycle_ = 0;
    // M31 (backlog E1): also restores the synth's documented power-on state
    // (planner §2.2/§2.3); the M17 register contract above is unchanged.
    synth_.reset();
}

void Ym2413Opll::attach_clock_source(Ym2413ClockSource* const source) {
    clock_source_ = source;
}

void Ym2413Opll::set_write_timing_enforced(const bool enforced) {
    write_timing_enforced_ = enforced;
}

bool Ym2413Opll::write_timing_enforced() const {
    return write_timing_enforced_;
}

bool Ym2413Opll::gate_allows_write(const bool is_address_write) {
    if (!write_timing_enforced_ || clock_source_ == nullptr) {
        return true;
    }
    const std::uint64_t now = clock_source_->cpu_cycles();
    if (has_last_write_) {
        // Fact-sheet §8: required minimum spacing is keyed on the PREVIOUS
        // write (address write -> next needs >=12 cycles; data write ->
        // next needs >=84), regardless of which port the CURRENT write
        // targets.
        const std::uint32_t required =
            last_write_was_address_ ? kAddressWriteMinCycles : kDataWriteMinCycles;
        const std::uint64_t elapsed = now - last_write_cycle_;
        if (elapsed < required) {
            // Too-fast write: dropped per real-hardware behaviour. Timing
            // reference is left unchanged (an ignored write does not
            // restart real hardware's busy window).
            return false;
        }
    }
    has_last_write_ = true;
    last_write_was_address_ = is_address_write;
    last_write_cycle_ = now;
    return true;
}

void Ym2413Opll::write_address(const std::uint8_t value) {
    // A-M17-3: the latch stores the value unmasked (YM2413Okazaki.cc:1370-1371).
    // D-M31-2 (DEC-0030 cross-reference, planner M31 §2.8): fMSX masks at
    // latch time instead (YM2413.c:134-137, `Latch = V&0x3F`) --
    // observationally equivalent for all write sequences; M17 settled on
    // mask-at-USE-time (below); recorded, no change.
    if (!gate_allows_write(/*is_address_write=*/true)) {
        return;  // E2: dropped, insufficient spacing since the prior write.
    }
    latch_ = value;
}

void Ym2413Opll::write_data(const std::uint8_t value) {
    // A-M17-3: the 0x3F mask applies at USE time, on the data write
    // (YM2413Okazaki.cc:1372-1373: `writeReg(registerLatch & 0x3f, value)`).
    if (!gate_allows_write(/*is_address_write=*/false)) {
        return;  // E2: dropped, insufficient spacing since the prior write.
    }
    regs_[latch_ & 0x3F] = value;
    // M31 (planner §2.6): every ACCEPTED data write notifies the synth's
    // key-edge detector (key-on/off per channel from $20-$28 bit4, per drum
    // from $0E bits 0-4 gated by bit5). All other synthesis parameters are
    // read live from regs_ at tick time -- no duplicated decode.
    synth_.refresh_keys(*this);
}

void Ym2413Opll::advance_cycles(const std::uint64_t delta_cycles) {
    synth_.advance_cycles(delta_cycles, *this);
}

core::BusData Ym2413Opll::io_read(const core::BusAddress /*port*/) {
    // A-M17-5: open-bus 0xFF -- the real chip has no status/busy register and
    // no readback; the XML declares #7C/#7D write-only (type="O").
    return 0xFF;
}

void Ym2413Opll::io_write(const core::BusAddress port, const core::BusData value) {
    if ((port & 1) == 0) {
        write_address(value);
    } else {
        write_data(value);
    }
}

std::uint8_t Ym2413Opll::register_value(const std::uint8_t addr) const {
    return regs_[addr & 0x3F];
}

std::uint16_t Ym2413Opll::f_number(const int channel) const {
    const std::uint8_t lo = regs_[static_cast<std::size_t>(0x10 + channel)];
    const std::uint8_t hi_bit = regs_[static_cast<std::size_t>(0x20 + channel)] & 0x01;
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi_bit) << 8));
}

std::uint8_t Ym2413Opll::block(const int channel) const {
    return static_cast<std::uint8_t>((regs_[static_cast<std::size_t>(0x20 + channel)] >> 1) & 0x7);
}

bool Ym2413Opll::key_on(const int channel) const {
    return (regs_[static_cast<std::size_t>(0x20 + channel)] & 0x10) != 0;
}

bool Ym2413Opll::sustain(const int channel) const {
    return (regs_[static_cast<std::size_t>(0x20 + channel)] & 0x20) != 0;
}

std::uint8_t Ym2413Opll::instrument(const int channel) const {
    return static_cast<std::uint8_t>(regs_[static_cast<std::size_t>(0x30 + channel)] >> 4);
}

std::uint8_t Ym2413Opll::volume(const int channel) const {
    return static_cast<std::uint8_t>(regs_[static_cast<std::size_t>(0x30 + channel)] & 0x0F);
}

Ym2413Opll::Patch Ym2413Opll::decode_patch(const std::array<std::uint8_t, 8>& raw) {
    const std::uint8_t b0 = raw[0];
    const std::uint8_t b1 = raw[1];
    const std::uint8_t b2 = raw[2];
    const std::uint8_t b3 = raw[3];
    const std::uint8_t b4 = raw[4];
    const std::uint8_t b5 = raw[5];
    const std::uint8_t b6 = raw[6];
    const std::uint8_t b7 = raw[7];

    Patch patch;

    // $00/$01: AM(bit7) VIB(bit6) EG-TYP(bit5) KSR(bit4) MUL[3:0](bits3-0).
    patch.modulator.am = (b0 & 0x80) != 0;
    patch.modulator.vib = (b0 & 0x40) != 0;
    patch.modulator.sustained_eg = (b0 & 0x20) != 0;
    patch.modulator.ksr = (b0 & 0x10) != 0;
    patch.modulator.multiple = static_cast<std::uint8_t>(b0 & 0x0F);

    patch.carrier.am = (b1 & 0x80) != 0;
    patch.carrier.vib = (b1 & 0x40) != 0;
    patch.carrier.sustained_eg = (b1 & 0x20) != 0;
    patch.carrier.ksr = (b1 & 0x10) != 0;
    patch.carrier.multiple = static_cast<std::uint8_t>(b1 & 0x0F);

    // $02: modulator KSL[1:0](bits7-6) + TL[5:0](bits5-0). Carrier has no TL.
    patch.modulator.ksl = static_cast<std::uint8_t>((b2 >> 6) & 0x3);
    patch.modulator.total_level = static_cast<std::uint8_t>(b2 & 0x3F);

    // $03: carrier KSL[1:0](bits7-6), carrier waveform(bit4), modulator
    // waveform(bit3), modulator FB[2:0](bits2-0) -- grounded in
    // YM2413Okazaki.cc:1439-1444 (more precise than the fact-sheet's
    // simplified per-register text for this shared byte).
    patch.carrier.ksl = static_cast<std::uint8_t>((b3 >> 6) & 0x3);
    patch.carrier.half_sine = (b3 & 0x10) != 0;
    patch.modulator.half_sine = (b3 & 0x08) != 0;
    patch.modulator.feedback = static_cast<std::uint8_t>(b3 & 0x07);

    // $04/$05: AR[3:0](bits7-4) DR[3:0](bits3-0).
    patch.modulator.attack_rate = static_cast<std::uint8_t>((b4 >> 4) & 0xF);
    patch.modulator.decay_rate = static_cast<std::uint8_t>(b4 & 0xF);
    patch.carrier.attack_rate = static_cast<std::uint8_t>((b5 >> 4) & 0xF);
    patch.carrier.decay_rate = static_cast<std::uint8_t>(b5 & 0xF);

    // $06/$07: SL[3:0](bits7-4) RR[3:0](bits3-0).
    patch.modulator.sustain_level = static_cast<std::uint8_t>((b6 >> 4) & 0xF);
    patch.modulator.release_rate = static_cast<std::uint8_t>(b6 & 0xF);
    patch.carrier.sustain_level = static_cast<std::uint8_t>((b7 >> 4) & 0xF);
    patch.carrier.release_rate = static_cast<std::uint8_t>(b7 & 0xF);

    return patch;
}

Ym2413Opll::Patch Ym2413Opll::patch(const int channel) const {
    if (instrument(channel) == 0) {
        const std::array<std::uint8_t, 8> raw{regs_[0], regs_[1], regs_[2], regs_[3],
                                              regs_[4], regs_[5], regs_[6], regs_[7]};
        return decode_patch(raw);
    }
    return rom_patch(instrument(channel));
}

bool Ym2413Opll::rhythm_enabled() const {
    return (regs_[0x0E] & 0x20) != 0;
}

bool Ym2413Opll::bd_key() const {
    return (regs_[0x0E] & 0x10) != 0;
}

bool Ym2413Opll::sd_key() const {
    return (regs_[0x0E] & 0x08) != 0;
}

bool Ym2413Opll::tom_key() const {
    return (regs_[0x0E] & 0x04) != 0;
}

bool Ym2413Opll::cym_key() const {
    return (regs_[0x0E] & 0x02) != 0;
}

bool Ym2413Opll::hh_key() const {
    return (regs_[0x0E] & 0x01) != 0;
}

std::uint8_t Ym2413Opll::bd_volume() const {
    return static_cast<std::uint8_t>(regs_[0x36] & 0x0F);
}

std::uint8_t Ym2413Opll::hh_volume() const {
    return static_cast<std::uint8_t>((regs_[0x37] >> 4) & 0x0F);
}

std::uint8_t Ym2413Opll::sd_volume() const {
    return static_cast<std::uint8_t>(regs_[0x37] & 0x0F);
}

std::uint8_t Ym2413Opll::tom_volume() const {
    return static_cast<std::uint8_t>((regs_[0x38] >> 4) & 0x0F);
}

std::uint8_t Ym2413Opll::cym_volume() const {
    return static_cast<std::uint8_t>(regs_[0x38] & 0x0F);
}

Ym2413Opll::Patch Ym2413Opll::rom_patch(const int number) {
    const int clamped = std::clamp(number, 1, 15);
    return decode_patch(kMelodyPatches[static_cast<std::size_t>(clamped - 1)]);
}

Ym2413Opll::Patch Ym2413Opll::rhythm_bd_patch() {
    return decode_patch(kRhythmBd);
}

Ym2413Opll::Patch Ym2413Opll::rhythm_sd_hh_patch() {
    return decode_patch(kRhythmSdHh);
}

Ym2413Opll::Patch Ym2413Opll::rhythm_tom_cym_patch() {
    return decode_patch(kRhythmTomCym);
}

}  // namespace sony_msx::devices::audio
