#pragma once

#include "core/device_contracts.h"
#include "devices/cartridge/cartridge_mapper_type.h"

namespace sony_msx::devices::cartridge {

// Family-local interface (M19-S2), the same X-pattern already used by
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
    // reset default (planner §2.2 per type). Never touches the immutable
    // loaded image itself -- matches every surveyed openMSX mapper's own
    // reset() (RomGeneric8kB.cc:13-22, RomAscii8kB.cc:24-33,
    // RomAscii16kB.cc:22-28, RomKonami.cc:54-59), none of which re-load or
    // discard the ROM image.
    virtual void reset() = 0;

    [[nodiscard]] virtual CartridgeMapperType mapper_type() const = 0;
};

}  // namespace sony_msx::devices::cartridge
