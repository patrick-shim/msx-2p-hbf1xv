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

namespace sony_msx::devices::cartridge {

// Shared 8-slot x 8 KB cartridge-window primitive (M19-S1, backlog B7).
//
// Every MVP external-cartridge mapper type (Mirrored/Generic8kB/Generic16kB/
// Ascii8kB/Ascii16kB/Konami) composes ONE of these: it owns the raw, immutable
// loaded image bytes plus 8 independent "window-slot" descriptors, each either
// unmapped (reads 0xFF) or resolved to a specific 8 KB bank of the image.
// Window-slot `s` (0-7) answers CPU addresses [s*0x2000, (s+1)*0x2000) -- i.e.
// the whole unexpanded, un-sub-slotted 64 KB external primary-slot window
// (planner A-M19-1/A-M19-8: primary slots 1/2 are bare, childless
// `<primary external="true">` XML elements, so there is no narrower `<mem>`
// sub-window to place a ROM inside, unlike RomPlain's general case).
//
// `set_bank` implements openMSX's RomBlocks<BANK_SIZE>::setRom bank-resolution
// algorithm BYTE-EXACT (planner A-M19-6, behaviour reference only, never
// copied -- GPL isolation):
//   references/openmsx-21.0/src/memory/RomBlocks.cc:107-118
//     block = (block < nrBlocks) ? block : block & blockMask;
//     if (block < nrBlocks) { /* real bank */ } else { /* unmapped */ }
// CRITICAL SUBTLETY (deliberately preserved, not "fixed"): blockMask is a
// FALLBACK consulted ONLY when the requested block is already out of range --
// it is NOT an unconditional AND-mask applied to every request. `nrBlocks` is
// computed from the actual loaded image (`image.size() / kBankSize`); the
// DEFAULT `blockMask = nrBlocks - 1` (RomBlocks.cc:47, "wraps at end of ROM
// image"); `set_block_mask` lets a concrete mapper override it (Konami ->31,
// RomKonami.cc:24, "Konami mapper is 256kB in size, even if ROM is smaller").
//
// No clock dependency (planner §2.5): every operation here is a pure,
// combinational function of the stored image + slot state.
class CartridgeRomWindow {
public:
    static constexpr int kSlots = 8;
    static constexpr std::uint32_t kBankSize = 0x2000;  // 8 KB
    static constexpr core::BusData kOpenBus = 0xFF;

    // `image` is the FULL, already-size-validated cartridge image (validation
    // is the concrete mapper type's responsibility, planner A-M19-7 -- this
    // primitive does not pad/truncate/reject). All 8 window-slots start
    // unmapped; the owning mapper's own reset()/constructor establishes the
    // documented initial bank layout.
    explicit CartridgeRomWindow(std::vector<std::uint8_t> image);

    // Resolve `requested_block` against this window's `num_blocks()`/
    // `block_mask()` (A-M19-6 algorithm, exact) and land the result at
    // `slot` (0-7). An out-of-range result (after the mask fallback) leaves
    // the slot unmapped (0xFF reads), matching `setRom`'s "unmapped" branch.
    void set_bank(int slot, unsigned requested_block);

    // Force `slot` unmapped (0xFF reads), matching `setUnmapped`.
    void set_unmapped(int slot);

    // Override the block-resolution fallback mask (default: num_blocks()-1,
    // "wraps at end of ROM image"). Konami overrides this to 31 at
    // construction (RomKonami.cc:24); every other MVP type keeps the default.
    void set_block_mask(unsigned mask);

    // Read a byte at a full 16-bit CPU address inside this window (address
    // implicitly selects window-slot `(address >> 13) & 0x07`). Returns
    // kOpenBus (0xFF) when that slot is unmapped.
    [[nodiscard]] core::BusData read(core::BusAddress address) const;

    [[nodiscard]] bool slot_mapped(int slot) const;
    // The resolved image bank number currently occupying `slot`. Precondition:
    // slot_mapped(slot) -- meaningless (returns 0) when unmapped.
    [[nodiscard]] unsigned slot_bank(int slot) const;

    [[nodiscard]] unsigned num_blocks() const { return num_blocks_; }
    [[nodiscard]] unsigned block_mask() const { return block_mask_; }
    [[nodiscard]] const std::vector<std::uint8_t>& image() const { return image_; }

private:
    struct SlotState {
        bool mapped = false;
        unsigned bank = 0;
    };

    std::vector<std::uint8_t> image_;
    unsigned num_blocks_;
    unsigned block_mask_;
    std::array<SlotState, kSlots> slots_{};
};

}  // namespace sony_msx::devices::cartridge
