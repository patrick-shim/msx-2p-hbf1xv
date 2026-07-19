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
#include "core/device_contracts.h"
#include "devices/fdc/disk_drive.h"
#include "devices/fdc/fdc_clock_source.h"
#include "devices/fdc/wd2793.h"
#include "devices/memory/rom_device.h"

namespace sony_msx::devices::fdc {

// Sony connection-style memory decode for the HB-F1XV FDC. A
// core::MemoryDevice attached at slot 3-2 page 1 that WRAPS the DISK-ROM
// RomDevice and decodes the top eight bytes 0x7FF8-0x7FFF to the WD2793 core +
// Sony glue latches; every other page-1 address reads the DISK ROM.
//
// AUTHORITATIVE decode source: openMSX 21.0: src/fdc/PhilipsFDC.cc
// (NOT the fact-sheet's inferred glue table, which the fact-sheet itself flags as
// "verify against PhilipsFDC.cc", Recommendation 4). openMSX decodes address &
// 0x3FFF (ROM based at 0x4000), so CPU 0x7FF8-0x7FFF map to offsets 0x3FF8-0x3FFF:
//   0x3FF8 R=status  W=command   (PhilipsFDC.cc:27-28,133-134)
//   0x3FF9 R/W track             (:29-30,136-137)
//   0x3FFA R/W sector            (:31-32,139-140)
//   0x3FFB R/W data              (:33-34,142-143)
//   0x3FFC R=sideReg  W=side select (bit0)          (:77-80,145-149)
//   0x3FFD R=driveReg&~4, bit2=0 iff disk changed (DSKCHG); READING CLEARS the
//          DSKCHG one-shot (mutating readMem PhilipsFDC.cc:37 -> DiskChanger.cc:95-100);
//          W=drive-select(bits1..0)+motor-on(bit7)   (:35-41,81-94,151-172)
//   0x3FFE R=0xFF (unused)                            (:95-97)
//   0x3FFF R: bit6=!INTRQ, bit7=!DTRQ (ACTIVE-LOW), rest pulled to 1 (:42-55,98-113)
// Reset writes 0x3FFC=0 and 0x3FFD=0 (PhilipsFDC.cc:17-22).
class SonyFdc final : public core::MemoryDevice {
public:
    SonyFdc(memory::RomDevice& rom, Wd2793& fdc, DiskDrive& drive, FdcClockSource& clock);

    // Reset the Sony glue latches (side 0, drive/motor cleared). The wrapped
    // WD2793/DiskDrive are reset by their owners.
    void reset();

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // Introspection for deterministic tests (does not perturb the FDC).
    [[nodiscard]] std::uint8_t side_register() const { return side_reg_; }
    [[nodiscard]] std::uint8_t drive_register() const { return drive_reg_; }

private:
    void write_drive_register(std::uint8_t value);

    memory::RomDevice& rom_;
    Wd2793& fdc_;
    DiskDrive& drive_;
    FdcClockSource& clock_;
    std::uint8_t side_reg_ = 0;
    std::uint8_t drive_reg_ = 0;
};

}  // namespace sony_msx::devices::fdc
