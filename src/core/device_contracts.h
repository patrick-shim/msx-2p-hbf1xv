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

#include <cstdint>

#include "core/bus.h"

// Pure-virtual device *participation contracts* for the slot/I/O decode fabric.
//
// These are abstract interfaces only (allowed in core/ per src/CLAUDE.md: "buses,
// device contracts, orchestration primitives"). They carry NO device behaviour —
// the concrete decode/engine logic lives in src/devices/chipset/ and the concrete
// chips (VDP/PSG/RTC/...) live under src/devices/. A device implements the
// contract(s) matching how it participates on the HB-F1XV bus:
//
//   - MemoryDevice   — occupies a slot/sub-slot page in the CPU address map
//                      (ROM, RAM, mapper RAM). Attached to chipset::SlotBus.
//   - IoDevice       — answers one or more of the 256 I/O ports. Attached to
//                      chipset::IoBus.
//   - SwitchedDevice — an expanded/switched-I/O device (ports #40-#4F) selected
//                      by an 8-bit device ID (S1985 backup RAM is ID 0xFE).
//
// Grounding: S1985 fact-sheet §3/§4/§6/§10; openMSX device model
// (openMSX 21.0: src/MSXDevice.hh, MSXSwitchedDevice.hh) — read for
// understanding only; never copied here (GPL license isolation).

namespace sony_msx::core {

// A device that occupies one or more 16 KB pages of the CPU-addressable memory
// map (ROM, RAM, mapper RAM). The slot fabric resolves *which* device answers an
// access via (primary, sub-slot, page); it then hands the device the full 16-bit
// CPU address, and the device maps that to its own storage (e.g. a 64 KB RAM
// spanning all four pages indexes the address directly; a single-page ROM masks/
// subtracts its page base). Keeping the raw address here lets one device back a
// multi-page span without the fabric needing to know device geometry.
class MemoryDevice {
public:
    virtual ~MemoryDevice() = default;

    virtual BusData mem_read(BusAddress address) = 0;
    virtual void mem_write(BusAddress address, BusData value) = 0;
};

// A device that answers MSX I/O ports. io_read/io_write receive the port
// (callers pass the full port; implementations key on port & 0xFF as needed).
class IoDevice {
public:
    virtual ~IoDevice() = default;

    virtual BusData io_read(BusAddress port) = 0;
    virtual void io_write(BusAddress port, BusData value) = 0;
};

// A device reachable via the MSX expanded/switched-I/O mechanism (ports
// #40-#4F). The controller writes an ID to #40 to select which SwitchedDevice
// receives subsequent #40-#4F traffic; a device matches when id() equals the
// selected ID. Dispatch is on port & 0x0F.
class SwitchedDevice {
public:
    virtual ~SwitchedDevice() = default;

    [[nodiscard]] virtual std::uint8_t id() const = 0;
    virtual BusData switched_read(BusAddress port) = 0;
    virtual void switched_write(BusAddress port, BusData value) = 0;
};

}  // namespace sony_msx::core
