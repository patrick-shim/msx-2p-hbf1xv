#pragma once

#include <array>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::audio {

// Deterministic emulated-cycle clock source for the YM2413 register-write
// timing gate (E2, M28-S1/§2.1b, backlog E2). X-pattern of
// rtc::RtcClockSource/fdc::FdcClockSource/peripherals::CassetteClockSource/
// peripherals::RenshaTurboClockSource (src/devices/rtc/rp5c01.h:14-18,
// src/devices/fdc/fdc_clock_source.h): the gate advances READ-ONLY off the
// machine cycle clock (Hbf1xvMachine::elapsed_cycles() == scheduler total
// cycles), never the host wall clock. Consulted PULL-STYLE ONLY from
// write_address()/write_data()/io_write() -- never wired into
// step_cpu_instruction()/run_cycles()/run_frame(), so it cannot perturb the
// M9/M12/M23 zero-tolerance CPU-timing oracles.
class Ym2413ClockSource {
public:
    virtual ~Ym2413ClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

// YM2413 (OPLL) register-accurate model (M17-S1/S2, backlog B3).
//
// CRITICAL DEVICE-IDENTITY GROUNDING (A-M17-1/A-M17-2, DEC-0012): the HB-F1XV's
// slot-3-3 sound device is openMSX class `MSXMusic` -- a FIXED 16 KB ROM +
// YM2413 with I/O-PORT-ONLY register access at #7C/#7D
// (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:180-196,
// `<MSX-MUSIC id="MSX Music">` ... `<io base="0x7C" num="2" type="O"/>`, NO
// `<sramname>` tag) and NO SRAM / bank register / memory-mapped register
// overlay (references/openmsx-21.0/src/sound/MSXMusic.hh:11-32,
// MSXMusic.cc:9-50 `MSXMusicBase`: only `writeIO`/`peekMem` over a plain
// masked ROM image; no bank register, no SRAM member). This is NOT the
// external Panasonic FM-PAC cartridge (`MSXFmPac.hh/.cc`, 4-bank ROM + 8 KB
// SRAM handshake + memory-mapped 0x3FF4-0x3FF7 registers) -- that device is
// NOT installed on this machine. This class therefore models ONLY the
// register file, the two-port write protocol, per-channel/rhythm decode, and
// the ROM instrument-patch table; it carries NO bank-register / SRAM-
// handshake / ID-string-detection logic (that would fabricate hardware this
// machine does not have) and NO audio waveform synthesis / DSP (backlog E1,
// explicitly deferred -- see agent-protocol/state/deferred-backlog.md).
//
// Two-port write protocol (A-M17-3), grounded exactly in
// references/openmsx-21.0/src/sound/YM2413Okazaki.cc:1368-1374
// (`writePort`: `port==0` -> `registerLatch = value` UNMASKED; `port==1` ->
// `writeReg(registerLatch & 0x3f, value)`): port #7C (address, port&1==0)
// LATCHES the written value unmasked; port #7D (data, port&1==1) writes
// `regs_[latch & 0x3F] = value` -- the 0x3F mask is applied at USE time (the
// data write), not at latch time, so a latch value > 0x3F still resolves
// correctly on the following data write (e.g. latch 0xFF -> register 0x3F).
//
// Reset (A-M17-4): `references/openmsx-21.0/src/sound/YM2413Okazaki.cc:696`
// (`std::ranges::fill(reg, 0)` at construction) and `:707-720` (`reset()`
// calls `writeReg(i, 0)` for every register plus `registerLatch = 0`) --
// reset() here zeroes all 64 registers and the address latch.
//
// Read behaviour (A-M17-5): the XML declares #7C/#7D write-only (`type="O"`,
// no `type="I"` entry) and `MSXMusicBase` overrides only `writeIO`
// (MSXMusic.hh:14-15) -- the real chip has no status/busy register and no
// readback (YM2413 fact-sheet §8, "No busy flag / no readback"). `io_read`
// therefore always returns open-bus 0xFF, matching `IoBus`'s unmapped-port
// default (src/devices/chipset/io_bus.h) and making `IN A,(#7C)`/
// `IN A,(#7D)` observably identical to the port being unattached.
//
// Debug-only introspection (A-M17-6): `register_value(addr)` is NOT
// CPU-bus-reachable; it mirrors the `PsgYm2149::register_value` precedent
// (src/devices/audio/psg_ym2149.h) and the openMSX-side
// `YM2413::peekRegs()`/"<id> regs" `SimpleDebuggable`
// (references/openmsx-21.0/src/sound/YM2413.hh:26,40-44, size 0x40) used for
// unit tests and the A/B harness.
class Ym2413Opll final : public core::IoDevice {
public:
    static constexpr std::size_t kRegisterCount = 0x40;  // 64 registers, $00-$3F
    static constexpr int kChannelCount = 9;               // channels 0-8

    // E2 (M28-S1, backlog E2) register-write minimum-spacing constants,
    // grounded in references/fact-sheets/Yamaha YM2413 FM Chip.md §8
    // ("Register write timing (real hardware constraint): after an address
    // write, wait >=12 master cycles (~3.36 us); after a data write, wait
    // >=84 master cycles (~23.52 us) before the next write ... Violating the
    // 84-cycle rule causes dropped/wrong register writes on real hardware.")
    // -- independently sourced from the Yamaha Application Manual + andete's
    // hardware measurements (fact-sheet §2), NOT transcribed from any
    // openMSX table (there is no numeric-table risk here, only two scalar
    // constants; see docs/m28-planner-package.md §2.1b).
    static constexpr std::uint32_t kAddressWriteMinCycles = 12;
    static constexpr std::uint32_t kDataWriteMinCycles = 84;

    void reset();

    // Bus-independent register access mirroring the two-port protocol.
    void write_address(std::uint8_t value);  // #7C: latch (unmasked at write time)
    void write_data(std::uint8_t value);      // #7D: regs_[latch & 0x3F] = value

    // E2 write-timing gate (M28-S1). attach_clock_source() supplies the
    // read-only cycle source (mirrors Rp5c01::attach_clock_source,
    // src/devices/rtc/rp5c01.h:58); set_write_timing_enforced() toggles the
    // gate. DEFAULT OFF: matches openMSX's own documented default-disabled
    // stance (fact-sheet §8, "openMSX (Nuked-OPLL core) currently has the
    // too-fast-access-timing emulation disabled") AND the M28-S1 mandatory
    // regression pre-check finding -- the EXISTING M17 tests
    // (tests/unit/devices/audio_ym2413_opll_unit_test.cpp,
    // tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp) issue
    // back-to-back register writes with zero/near-zero cycle spacing (the
    // unit test never attaches a clock source, so it is unaffected either
    // way; the integration test's Hbf1xvMachine::debug_io_write() helper is a
    // zero-cycle-advance raw bus poke that WOULD spuriously drop writes if
    // this gate defaulted on -- see docs/m28-implementation-report.md for the
    // full pre-check trace). A caller must opt in explicitly.
    void attach_clock_source(Ym2413ClockSource* source);
    void set_write_timing_enforced(bool enforced);
    [[nodiscard]] bool write_timing_enforced() const;

    // core::IoDevice on #7C/#7D (keyed on port & 1: 0 = address, 1 = data).
    // io_read always returns open-bus 0xFF (A-M17-5) regardless of prior writes.
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // Debug-only register readback (A-M17-6), NOT CPU-bus-reachable.
    [[nodiscard]] std::uint8_t register_value(std::uint8_t addr) const;

    // Decoded 2-operator parameter set (modulator/carrier), the resolved
    // per-channel patch shape whether derived live from the user patch
    // ($00-$07) or a fixed ROM instrument entry (planner package §2.2).
    struct OperatorPatch {
        bool am = false;               // AM (tremolo enable), reg bit7
        bool vib = false;              // VIB (vibrato enable), reg bit6
        bool sustained_eg = false;     // EG-TYP: true=sustained, false=percussive, bit5
        bool ksr = false;              // KSR (key-scale-of-rate), bit4
        std::uint8_t multiple = 0;     // MUL[3:0], bits3-0
        std::uint8_t ksl = 0;          // KSL[1:0] key-scale level attenuation
        std::uint8_t total_level = 0;  // TL[5:0] -- modulator only; 0 for carrier
        bool half_sine = false;        // waveform select (DC/DM): 0=full sine, 1=half
        std::uint8_t feedback = 0;     // FB[2:0] -- modulator only; 0 for carrier
        std::uint8_t attack_rate = 0;  // AR[3:0]
        std::uint8_t decay_rate = 0;   // DR[3:0]
        std::uint8_t sustain_level = 0;  // SL[3:0]
        std::uint8_t release_rate = 0;   // RR[3:0]
    };
    struct Patch {
        OperatorPatch modulator;
        OperatorPatch carrier;
    };

    // Per-channel decode (channel 0-8), computed on demand from regs_
    // (fact-sheet §3 master register-map table).
    [[nodiscard]] std::uint16_t f_number(int channel) const;  // 9-bit
    [[nodiscard]] std::uint8_t block(int channel) const;      // 3-bit
    [[nodiscard]] bool key_on(int channel) const;
    [[nodiscard]] bool sustain(int channel) const;
    [[nodiscard]] std::uint8_t instrument(int channel) const;  // 4-bit, 0=user,1-15=ROM
    [[nodiscard]] std::uint8_t volume(int channel) const;      // 4-bit attenuation
    // Resolved patch: live-decoded from $00-$07 when instrument(c)==0, else the
    // fixed ROM patch entry instrument(c) (1-15).
    [[nodiscard]] Patch patch(int channel) const;

    // Rhythm-mode decode (register $0E + rhythm volumes $36-$38, fact-sheet
    // §3/§6). Pure register views: callers select melody vs rhythm channel-6-8
    // interpretation based on rhythm_enabled(), exactly as real firmware would.
    [[nodiscard]] bool rhythm_enabled() const;  // $0E bit5 (R)
    [[nodiscard]] bool bd_key() const;          // $0E bit4
    [[nodiscard]] bool sd_key() const;          // $0E bit3
    [[nodiscard]] bool tom_key() const;         // $0E bit2
    [[nodiscard]] bool cym_key() const;         // $0E bit1
    [[nodiscard]] bool hh_key() const;          // $0E bit0
    [[nodiscard]] std::uint8_t bd_volume() const;   // $36 & 0xF
    [[nodiscard]] std::uint8_t hh_volume() const;   // ($37 >> 4) & 0xF
    [[nodiscard]] std::uint8_t sd_volume() const;   // $37 & 0xF
    [[nodiscard]] std::uint8_t tom_volume() const;  // ($38 >> 4) & 0xF
    [[nodiscard]] std::uint8_t cym_volume() const;  // $38 & 0xF

    // ROM instrument patch table (15 melody entries, 1-indexed, + 3 rhythm
    // patches), reproduced verbatim from the YM2413 fact-sheet §4 / planner
    // package §2.2 (A-M17-7: "community-standard... ~99% but not
    // datasheet-certain" -- see the caveat comment at the table definition in
    // the .cpp). number is clamped to [1,15] for determinism.
    [[nodiscard]] static Patch rom_patch(int number);
    [[nodiscard]] static Patch rhythm_bd_patch();
    [[nodiscard]] static Patch rhythm_sd_hh_patch();
    [[nodiscard]] static Patch rhythm_tom_cym_patch();

private:
    // Decodes a raw 8-byte $00-$07-shaped patch row (byte i = register $0i)
    // into the modulator/carrier field split. Grounded in the ACTUAL register
    // bit layout per references/openmsx-21.0/src/sound/YM2413Okazaki.cc:
    // 1388-1500 (writeReg cases 0x00-0x07) -- more precise than the fact-
    // sheet's simplified per-register text for register $03's shared
    // modulator/carrier bit-packing (carrier KSL bits7-6, carrier waveform
    // bit4, modulator waveform bit3, modulator feedback bits2-0).
    [[nodiscard]] static Patch decode_patch(const std::array<std::uint8_t, 8>& raw);

    // E2 gate check (M28-S1): returns true (and records this write as the
    // new "last write") when the write is allowed to land; returns false
    // (state left untouched, mirroring real-hardware "busy window ignores
    // the write" behaviour -- the drop does NOT reset the timing reference)
    // when a too-fast write must be dropped. A no-op passthrough (always
    // true) when disabled or no clock source is attached.
    [[nodiscard]] bool gate_allows_write(bool is_address_write);

    std::array<std::uint8_t, kRegisterCount> regs_{};
    std::uint8_t latch_ = 0;

    Ym2413ClockSource* clock_source_ = nullptr;
    bool write_timing_enforced_ = false;
    bool has_last_write_ = false;
    bool last_write_was_address_ = false;
    std::uint64_t last_write_cycle_ = 0;
};

}  // namespace sony_msx::devices::audio
