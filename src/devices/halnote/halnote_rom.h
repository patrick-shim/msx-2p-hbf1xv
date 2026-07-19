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
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/cartridge/cartridge_rom_window.h"
#include "devices/memory/battery_backed_sram.h"

namespace sony_msx::devices::halnote {

// HalnoteRom -- the Halnote-mapped MSX-JE firmware ROM at primary slot 0,
// secondary slot 3.
//
// A byte-exact port of openMSX 21.0: src/memory/RomHalnote.{hh,cc}
// (behaviour reference only -- GPL isolation, never copied into this file):
// a 1 MB image organized as 128x8 KB main banks (window-slots 2-5 switchable
// via four write-triggered registers; slots 0/1 SRAM-gated; slots 6/7
// permanently unmapped), with the last 512 KB additionally addressable as
// 256x2 KB sub-banks that can shadow part of window-slot 3 (CPU
// 0x7000-0x7FFF) when a sub-mapper-enable bit is set. There is also a real
// 16 KB battery-backed SRAM at CPU 0x0000-0x3FFF, gated by a separate enable
// bit (RomHalnote.cc:1-24 header comment).
//
// Composition (both primitives REUSED verbatim, neither
// reimplemented):
//   - `devices::cartridge::CartridgeRomWindow window_` -- the shared 8-slot x
//     8 KB primitive, for the main bank-switch window. Halnote relies on the
//     window's DEFAULT block mask (num_blocks()-1 == 127 for a 1 MB/128-bank
//     image); unlike Konami's `set_block_mask(31)` override, NO override is
//     made here (RomHalnote.cc has no setBlockMask call at all).
//   - `devices::memory::BatteryBackedSram sram_{0x4000}` -- the reusable 16 KB
//     primitive (RomHalnote.cc:44, `make_unique<SRAM>(..., 0x4000, ...)`),
//     wired as the real store behind the SRAM-enable gate.
//
// SRAM access (0x0000-0x3FFF) is a direct address-range branch rather than
// openMSX's pointer-indirection into window-slots 0/1 (RomHalnote.cc:107-112)
// -- proven behaviourally IDENTICAL
// (CartridgeRomWindow has no mechanism to point a slot at an external
// buffer); window-slots 0/1 are consequently left permanently
// unmapped/unused for this device's whole lifetime.
//
// Register numbering note: RomHalnote.cc's header comment labels
// the four main bank-switch registers "bank 0".."bank 3" (0-based,
// comment-only), but the actual code (`writeMem:100`, `auto bank = address
// >> 13;`) computes bank in {2,3,4,5} for these same four regions, matching
// `CartridgeRomWindow`'s own slot-index convention. This implementation
// follows the code's numbering (2-5), not the header comment's prose.
//
// Determinism: a pure, combinational device -- mem_read/
// mem_write are functions of stored bytes only, never elapsed_cycles(). No
// clock consumer. SRAM persistence (load/save) is a one-time, explicit
// setup/flush action performed by the owning machine, never from inside
// mem_read/mem_write.
class HalnoteRom final : public core::MemoryDevice {
public:
    static constexpr std::size_t kImageBytes = 0x100000;  // 1 MB (RomHalnote.cc:40-43)
    static constexpr std::size_t kSramBytes = 0x4000;      // 16 KB (RomHalnote.cc:44)
    static constexpr core::BusAddress kSramLimit = 0x4000;       // SRAM region [0x0000,0x4000)
    static constexpr core::BusAddress kBankSwitchLimit = 0xC000; // never a write target >= here
    static constexpr core::BusData kOpenBus = 0xFF;

    // Builds an all-0xFF 1 MB placeholder window and a zero-initialized 16 KB
    // SRAM, already in the documented reset() bank/flag layout (mirrors
    // `RomDevice`'s placeholder-then-later-set_image pattern).
    HalnoteRom();

    // RomHalnote.cc:48-61, byte-exact: clears sub_banks_/sram_enabled_/
    // sub_mapper_enabled_, re-establishes the window bank/unmapped layout,
    // AND clears sram_ content (a disclosed simplification beyond literal
    // openMSX, mirroring the S1985Engine::reset() precedent exactly
    // (s1985_engine.cpp) -- real battery-backed SRAM survives a reset in
    // reality, but this emulator's cold_boot() models a fresh, deterministic
    // power-on; persistence is modeled entirely through the file load/save
    // the owning machine performs around this call).
    void reset();

    // Replace the backing ROM image (e.g. asset (re)load at cold_boot).
    // Normalizes to exactly kImageBytes (truncate/0xFF-pad, never throws,
    // mirrors RomDevice::normalize_image); reconstructs the internal window;
    // re-applies the bank/flag portion of reset() WITHOUT touching sram_
    // (SRAM content is independent of the ROM image).
    void set_image(std::vector<std::uint8_t> image);

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    [[nodiscard]] const devices::cartridge::CartridgeRomWindow& window() const { return window_; }

    // Exposed directly so the OWNING MACHINE can call .load()/.save() on the
    // real store (no redundant pass-through wrapper here,
    // the primitive's own public API is already the right shape).
    [[nodiscard]] const devices::memory::BatteryBackedSram& sram() const { return sram_; }
    devices::memory::BatteryBackedSram& sram() { return sram_; }

    [[nodiscard]] bool sram_enabled() const { return sram_enabled_; }
    [[nodiscard]] bool sub_mapper_enabled() const { return sub_mapper_enabled_; }
    [[nodiscard]] std::uint8_t sub_bank(int index) const {
        return sub_banks_[static_cast<std::size_t>(index)];
    }

private:
    // Shared by reset() and set_image(): sub_banks_/enable flags + the
    // window bank/unmapped layout. Deliberately does NOT touch sram_ (the
    // caller decides: reset() clears it explicitly right after; set_image()
    // never does).
    void reset_bank_state();

    devices::cartridge::CartridgeRomWindow window_;
    devices::memory::BatteryBackedSram sram_{kSramBytes};
    std::array<std::uint8_t, 2> sub_banks_{};
    bool sram_enabled_ = false;
    bool sub_mapper_enabled_ = false;
};

}  // namespace sony_msx::devices::halnote
