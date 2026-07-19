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

#include "core/device_contracts.h"
#include "devices/cartridge/cartridge_mapper_type.h"

namespace sony_msx::devices::cartridge {

class CartridgeRomWindow;  // fwd (defined in cartridge_rom_window.h)

// Family-local interface, the same X-pattern already used by
// RtcClockSource/FdcClockSource/CassetteClockSource (each defined inside its
// own device family, not in src/core/ -- src/CLAUDE.md: "Do not place device
// logic in src/core/"). Lets CartridgeSlot hold ONE polymorphic pointer to
// "whichever concrete mapper is currently plugged in," distinct from
// core::device_contracts.h's bus-participation contracts (those are
// machine-bus-wide; this is family-internal).
class CartridgeMapperDevice : public core::MemoryDevice {
public:
    ~CartridgeMapperDevice() override = default;

    // Reinitialize bank-pointer state to this mapper's documented power-on/
    // reset default. Never touches the immutable
    // loaded image itself -- matches every surveyed openMSX mapper's own
    // reset() (RomGeneric8kB.cc:13-22, RomAscii8kB.cc:24-33,
    // RomAscii16kB.cc:22-28, RomKonami.cc:54-59), none of which re-load or
    // discard the ROM image.
    virtual void reset() = 0;

    [[nodiscard]] virtual CartridgeMapperType mapper_type() const = 0;

    // Debug-snapshot seam: the mapper's owned
    // 8-slot ROM window, so the snapshot can dump bank state GENERICALLY
    // without RTTI dispatch. Default nullptr; the window-based mappers
    // (Mirrored/Generic8kB/Generic16kB/Ascii8kB/Ascii16kB/Konami/KonamiSCC)
    // override it to return their owned window. FM-PAC keeps the default (it is
    // dumped via its own [FMPAC.*] sections). Additive read-only, no behavior
    // change.
    [[nodiscard]] virtual const CartridgeRomWindow* rom_window() const { return nullptr; }
};

}  // namespace sony_msx::devices::cartridge
