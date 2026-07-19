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

namespace sony_msx::devices::memory {

// Reusable, parametric-size, deterministic battery-backed byte store
// (DEC-0012).
//
// A device-agnostic generalization of
// `S1985Engine::load_backup_ram`/`save_backup_ram`
// (src/devices/chipset/s1985_engine.h/.cpp):
// an absent/short/unreadable file leaves the store
// UNTOUCHED (deterministic default: all-zero after construction/clear()),
// never fabricated; `load()`/`save()` round-trip byte-identical; no
// wall-clock or host-filesystem nondeterminism beyond the one fixed
// load-at-setup read.
//
// Sizing precedent (why 16 KB / 0x4000 is the unit-tested size): the
// REAL 16 KB MSX-JE SRAM belongs to the Halnote-mapped MSX-JE firmware ROM at
// slot 0-3 -- NOT the built-in YM2413/MSX-MUSIC device at
// slot 3-3, which the machine XML shows has NO `<sramname>` tag at all
// (openMSX 21.0: share/machines/Sony_HB-F1XV.xml:105-115,179-197).
// The exact size is grounded in
// openMSX 21.0: src/memory/RomHalnote.cc:37-46:
// `sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config);` ==
// 16384 bytes. Wiring one into slot 3-3 would fabricate a chip-select
// overlay this machine does not have. Current consumers compose their own
// instance: halnote::HalnoteRom (16 KB) and cartridge::CartridgeFmPacRom
// (8 KB); it is additionally unit-tested standalone at 16 KB.
class BatteryBackedSram {
public:
    // Construct a store of `byte_count` bytes, zero-initialized (the
    // deterministic power-on default).
    explicit BatteryBackedSram(std::size_t byte_count);

    [[nodiscard]] std::size_t size() const;

    // Contiguous read-only view of the store (for deterministic dumps, e.g.
    // the machine state dump's SRAM section). Never null for a non-empty store.
    [[nodiscard]] const std::uint8_t* data() const;

    [[nodiscard]] std::uint8_t read(std::size_t offset) const;
    void write(std::size_t offset, std::uint8_t value);

    // Reset the store to its deterministic power-on default (all-zero).
    void clear();

    // Load exactly size() bytes from `path` into the store. Absent/short/
    // unreadable file -> the store is left UNTOUCHED (still whatever it was
    // before the call -- callers that want the deterministic zero default on
    // a failed load should clear() first); returns false. A successful
    // full-size read overwrites the store and returns true.
    bool load(const std::filesystem::path& path);

    // Write the current store contents to `path`. Returns false on any I/O
    // failure; save() never mutates the store either way.
    [[nodiscard]] bool save(const std::filesystem::path& path) const;

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace sony_msx::devices::memory
