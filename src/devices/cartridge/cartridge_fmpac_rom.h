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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "devices/audio/ym2413_opll.h"
#include "devices/cartridge/cartridge_mapper_device.h"
#include "devices/memory/battery_backed_sram.h"

namespace sony_msx::devices::cartridge {

// `FmPac` mapper -- the external Panasonic FM-PAC peripheral cartridge (M36,
// DEC-0050). Grounds references/openmsx-21.0/src/sound/MSXFmPac.cc
// (behaviour reference only, never copied -- GPL isolation). The FM-PAC adds
// the 8 KB battery-backed SRAM that the HB-F1XV's *built-in* MSX-MUSIC (OPLL +
// APRLOPLL BIOS, no SRAM) deliberately lacks: inserting this cartridge is what
// lets an SRAM-save game (e.g. YS II) find "S-RAM AVAILABLE".
//
// The device is a PAGE-1 device: it occupies CPU 0x4000-0x7FFF of its slot and
// presents OPEN BUS (0xFF) on pages 0/2/3 (matching a bare FM-PAC cart, which
// only answers page 1). openMSX masks every access `address &= 0x3FFF`; the
// CPU-visible ABSOLUTE addresses are therefore page-1-relative + 0x4000:
//
//   CPU addr     rel      function                                MSXFmPac.cc
//   0x4000-5FFD  0x0000-1FFD  8 KB SRAM window (when unlocked)     43-45,115
//                             else ROM: rom[bank*0x4000 + rel]     53-54
//   0x5FFE       0x1FFE   r1ffe magic; write 0x4D (gated bit4)     44,84-89
//   0x5FFF       0x1FFF   r1fff magic; write 0x69 (gated bit4)     44,90-95
//   0x7FF4       0x3FF4   YM2413 (OPLL) address port               96-99
//   0x7FF5       0x3FF5   YM2413 (OPLL) data port                  96-99
//   0x7FF6       0x3FF6   enable: bit0 I/O-enable, bit4 force-rst  38,100-106
//   0x7FF7       0x3FF7   bank: value & 0x03 (16 KB ROM banks)     40,107-113
//
// SRAM unlock (MSXFmPac.cc:137-144): sramEnabled = (r1ffe==0x4D) &&
// (r1fff==0x69), re-evaluated on EVERY write to 0x5FFE/0x5FFF and on the
// enable bit-4 force-reset. BOTH bytes are required (AND). While unlocked, the
// magic-register addresses read back the 0x4D/0x69 constants (the top two
// bytes of the 8 KB chip are shadowed by these registers, so the *addressable*
// SRAM is 0x1FFE = 8190 bytes; MSXFmPac.cc:11,46-49).
//
// OPLL (0x7FF4/0x7FF5): the FM-PAC carries its OWN YM2413. This device owns one
// and forwards the memory-mapped address/data writes to it (faithful register
// interface). It is NOT attached to the machine I/O bus #7C/#7D -- those belong
// to the built-in MSX-MUSIC, which stays SRAM-less and unaffected (DEC-0050,
// AC-d4). Mixing this cartridge OPLL's audio into the machine output is a
// wholly-additive follow-on (planner §1.2 "not new DSP"); the accessor opll()
// is the ready wiring seam, mirroring cartridge_konami_scc_rom's scc().
//
// Universal FM-PAC mapper -- never keyed to any specific game
// (feedback_universal_fixes_only). Like the M19/M29 mappers the constructor
// deliberately does NOT self-reset; CartridgeSlot::load() resets once before
// installing.
class CartridgeFmPacRom final : public CartridgeMapperDevice {
public:
    static constexpr std::size_t kBankSize = 0x4000;    // 16 KB ROM bank
    static constexpr std::size_t kSramBytes = 0x2000;   // 8 KB battery SRAM chip
    static constexpr std::uint16_t kSramWindow = 0x1FFE;  // addressable rel 0..0x1FFD

    // 1..4 whole 16 KB banks (the real BIOS is one 16 KB bank; 64 KB variants
    // carry 4). A bank register value is masked against the actual bank count
    // so a bank>available never reads past the image (planner §2.1).
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()). Does NOT call reset().
    explicit CartridgeFmPacRom(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::FmPac;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // Battery-SRAM persistence (M17 BatteryBackedSram primitive; M20 Halnote
    // .sram pattern). load(): absent/short file -> store left at its
    // deterministic zero default (never fabricated). Both round-trip
    // byte-identical.
    [[nodiscard]] devices::memory::BatteryBackedSram& sram() { return sram_; }
    [[nodiscard]] const devices::memory::BatteryBackedSram& sram() const { return sram_; }
    [[nodiscard]] bool load_sram(const std::filesystem::path& path) { return sram_.load(path); }
    [[nodiscard]] bool save_sram(const std::filesystem::path& path) const { return sram_.save(path); }

    // The owned FM-PAC OPLL (wiring seam for a future audio-mix follow-on).
    [[nodiscard]] devices::audio::Ym2413Opll& opll() { return opll_; }
    [[nodiscard]] const devices::audio::Ym2413Opll& opll() const { return opll_; }

    // Debug/test seams.
    [[nodiscard]] bool sram_enabled() const { return sram_enabled_; }
    [[nodiscard]] std::uint8_t bank() const { return bank_; }
    [[nodiscard]] std::uint8_t enable_register() const { return enable_; }
    // M36 Phase 3 snapshot: the SRAM-unlock magic latch (planner §2.4 item 7).
    [[nodiscard]] std::uint8_t r1ffe() const { return r1ffe_; }
    [[nodiscard]] std::uint8_t r1fff() const { return r1fff_; }

private:
    void check_sram_enable();
    [[nodiscard]] std::uint8_t rom_read(std::uint16_t rel) const;

    std::vector<std::uint8_t> rom_;
    std::size_t num_banks_ = 1;
    devices::memory::BatteryBackedSram sram_{kSramBytes};
    devices::audio::Ym2413Opll opll_;

    std::uint8_t enable_ = 0;  // 0x3FF6 latch (value & 0x11)
    std::uint8_t bank_ = 0;    // 0x3FF7 latch (value & 0x03)
    std::uint8_t r1ffe_ = 0;   // 0x5FFE magic
    std::uint8_t r1fff_ = 0;   // 0x5FFF magic
    bool sram_enabled_ = false;
};

}  // namespace sony_msx::devices::cartridge
