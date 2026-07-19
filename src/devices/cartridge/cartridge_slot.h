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

#include <memory>
#include <vector>

#include "devices/cartridge/cartridge_mapper_device.h"
#include "devices/cartridge/cartridge_mapper_type.h"

namespace sony_msx::devices::cartridge {

// Outcome of a cartridge load request. Never silently swallowed:
// a failed load leaves the slot's PRIOR state completely untouched.
enum class CartridgeLoadResult {
    Ok,
    // Hbf1xvMachine::load_cartridge only -- CartridgeSlot itself never
    // returns this (a CartridgeSlot instance always represents an
    // already-selected, valid slot; slot-number routing is a machine-level
    // concern).
    InvalidSlotNumber,
    ImageSizeInvalidForMapperType,
};

// The ONE device actually attached to slot_bus_ for an external cartridge bay.
// Wraps `std::unique_ptr<CartridgeMapperDevice>`:
// nullptr = empty slot = byte-for-byte identical to the original "reserved
// open-bus" default (a strong, built-in regression guard for anyone not using
// --cart1/--cart2) -- mem_read returns 0xFF, mem_write is a no-op.
//
// `load()` validates the image size against `type`'s documented requirement
// BEFORE constructing anything; on success it constructs the
// concrete mapper, calls ITS reset() once (establishing a well-defined
// power-up bank layout for every type uniformly -- including Konami, whose
// own constructor deliberately does not self-reset, RomKonami.cc:33-35), and
// only THEN replaces the active mapper. On failure the prior state (loaded or
// empty) is left completely untouched -- never partially applied.
//
// `reset()` calls the active mapper's reset() (if loaded) and is a no-op when
// empty; it NEVER unloads -- matches real hardware power-cycle-with-
// cartridge-still-inserted semantics.
class CartridgeSlot final : public core::MemoryDevice {
public:
    // `primary_slot_number` (1 or 2 for this machine) is a diagnostic-only
    // label; routing itself is entirely slot_bus_'s job and this
    // class carries zero slot-bus-attachment knowledge of its own.
    explicit CartridgeSlot(int primary_slot_number);

    [[nodiscard]] CartridgeLoadResult load(CartridgeMapperType type, std::vector<std::uint8_t> image);
    void unload();
    void reset();

    [[nodiscard]] bool loaded() const { return mapper_ != nullptr; }
    // Precondition: loaded().
    [[nodiscard]] CartridgeMapperType mapper_type() const;
    [[nodiscard]] int primary_slot_number() const { return primary_slot_number_; }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // Debug/test seam onto the active concrete mapper (nullptr when empty).
    [[nodiscard]] const CartridgeMapperDevice* mapper() const { return mapper_.get(); }
    CartridgeMapperDevice* mapper() { return mapper_.get(); }

private:
    static constexpr core::BusData kOpenBus = 0xFF;

    int primary_slot_number_;
    std::unique_ptr<CartridgeMapperDevice> mapper_;
};

}  // namespace sony_msx::devices::cartridge
