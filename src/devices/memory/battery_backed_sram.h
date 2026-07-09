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

// Reusable, parametric-size, deterministic battery-backed byte store (M17-S4,
// backlog B4).
//
// STANDALONE PRIMITIVE ONLY -- NOT wired to any slot this milestone (§3.3 of
// docs/m17-planner-package.md; DEC-0012). Generalizes
// `S1985Engine::load_backup_ram`/`save_backup_ram`
// (src/devices/chipset/s1985_engine.h:63-64, s1985_engine.cpp:79-103) into a
// device-agnostic store: absent/short/unreadable file -> the store is left
// UNTOUCHED (deterministic default: all-zero after construction/clear()),
// never fabricated; `load()`/`save()` round-trip byte-identical; no wall-clock
// or host-filesystem nondeterminism beyond the one fixed load-at-setup read.
//
// Sizing precedent (why 16 KB / 0x4000 is the unit-tested size, §3.3): the
// REAL 16 KB MSX-JE SRAM belongs to the Halnote-mapped MSX-JE firmware ROM at
// slot 0-3 (backlog B6) -- NOT this milestone's YM2413/MSX-MUSIC device at
// slot 3-3, which the machine XML shows has NO `<sramname>` tag at all
// (references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:105-115,179-197).
// The exact size is grounded in
// references/openmsx-21.0/src/memory/RomHalnote.cc:37-46:
// `sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config);` ==
// 16384 bytes. This class is deliberately NOT instantiated in `Hbf1xvMachine`
// and NOT wired to any slot in M17 -- no real consumer exists yet (Halnote/B6
// is still out of scope); wiring it into slot 3-3 would fabricate a
// chip-select overlay this specific machine does not have (A-M17-1/A-M17-2).
// It is built here, standalone and unit-tested at 16 KB, so a future Halnote
// milestone (backlog B6) can attach it directly with no redesign.
class BatteryBackedSram {
public:
    // Construct a store of `byte_count` bytes, zero-initialized (the
    // deterministic power-on default).
    explicit BatteryBackedSram(std::size_t byte_count);

    [[nodiscard]] std::size_t size() const;

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
