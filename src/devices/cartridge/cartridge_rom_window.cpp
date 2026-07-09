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

#include "devices/cartridge/cartridge_rom_window.h"

namespace sony_msx::devices::cartridge {

CartridgeRomWindow::CartridgeRomWindow(std::vector<std::uint8_t> image)
    : image_(std::move(image)),
      num_blocks_(static_cast<unsigned>(image_.size() / kBankSize)),
      block_mask_(num_blocks_ > 0 ? num_blocks_ - 1 : 0) {
}

void CartridgeRomWindow::set_bank(const int slot, const unsigned requested_block) {
    if (slot < 0 || slot >= kSlots) {
        return;
    }

    // Byte-exact RomBlocks<BANK_SIZE>::setRom (A-M19-6,
    // references/openmsx-21.0/src/memory/RomBlocks.cc:107-118): the mask is a
    // FALLBACK consulted ONLY when the requested block is already out of
    // range, never an unconditional AND-mask.
    unsigned block = requested_block;
    if (block >= num_blocks_) {
        block &= block_mask_;
    }
    if (block < num_blocks_) {
        slots_[slot] = SlotState{true, block};
    } else {
        slots_[slot] = SlotState{false, 0};
    }
}

void CartridgeRomWindow::set_unmapped(const int slot) {
    if (slot < 0 || slot >= kSlots) {
        return;
    }
    slots_[slot] = SlotState{false, 0};
}

void CartridgeRomWindow::set_block_mask(const unsigned mask) {
    block_mask_ = mask;
}

core::BusData CartridgeRomWindow::read(const core::BusAddress address) const {
    const int slot = (address >> 13) & 0x07;
    const SlotState& state = slots_[slot];
    if (!state.mapped) {
        return kOpenBus;
    }
    const std::size_t offset = static_cast<std::size_t>(state.bank) * kBankSize + (address & (kBankSize - 1));
    return image_[offset];
}

bool CartridgeRomWindow::slot_mapped(const int slot) const {
    if (slot < 0 || slot >= kSlots) {
        return false;
    }
    return slots_[slot].mapped;
}

unsigned CartridgeRomWindow::slot_bank(const int slot) const {
    if (slot < 0 || slot >= kSlots) {
        return 0;
    }
    return slots_[slot].bank;
}

}  // namespace sony_msx::devices::cartridge
