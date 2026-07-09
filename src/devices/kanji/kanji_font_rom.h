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
#include <vector>

#include "core/bus.h"
#include "core/device_contracts.h"

namespace sony_msx::devices::kanji {

// Kanji font ROM I/O device on ports #D8-#DB (M18-S1, backlog B5).
//
// CRITICAL DEVICE-IDENTITY GROUNDING (A-M18-1/docs/m18-planner-package.md
// §2.7): the HB-F1XV's Kanji font device is openMSX class `Kanji`
// (`MSXKanji`, references/openmsx-21.0/src/MSXKanji.hh/.cc) -- a DIRECTLY
// port-mapped device (`switch (port & 0x03)` dispatch, MSXKanji.cc:34,53,72)
// -- NOT `MSXKanji12` (references/openmsx-21.0/src/MSXKanji12.hh/.cc), a
// switched/expanded-I/O device (MSXSwitchedDevice, device ID 0xF7, ports
// #40-#4F) used by OTHER MSX machines (Hangul/Lascom variants). Confirmed via
// the machine XML directly: references/openmsx-21.0/share/machines/
// Sony_HB-F1XV.xml:29-38 declares `<Kanji id="Kanji ROM">` with four explicit
// DIRECT `<io base=.. num=.. type=..>` entries at #D8/#D9/#DA/#DB -- no
// `<Kanji12>` element, no `<type>` child anywhere in this machine's XML. This
// class therefore implements ONLY the direct-port two-counter address-latch
// protocol; it carries NO switched-I/O dispatch mechanism.
//
// ROM-size code path (A-M18-2): `MSXKanji`'s constructor (MSXKanji.cc:9-24)
// selects `highAddressMask = 0x3F` and `isLascom = false` unless a
// `<type>hangul</type>` / `<type>lascom</type>` XML child is present (absent
// here). The local asset (docs/asset-checksums.txt, 262,144 bytes = 0x40000)
// is exactly the 256 KB, non-Lascom, non-Hangul path this class hardcodes.
//
// Two independent address counters address ONE 256 KB ROM image:
//   adr1_ addresses the JIS1 half [0x00000, 0x1FFFF].
//   adr2_ addresses the JIS2 half [0x20000, 0x3FFFF] -- bit 17 (0x20000) is
//   PRESERVED by every #DB write mask, so adr2_ never leaves the JIS2 half
//   through the normal write path (A-M18-3).
//
// Six behaviors, byte-exact per MSXKanji.cc:32-88 (A-M18-3; re-expressed,
// never copied -- GPL isolation):
//   #D8 write (port&3==0) : adr1_ = (adr1_ & 0x1F800) | ((value & 0x3F) << 5)
//   #D9 write (port&3==1) : adr1_ = (adr1_ & 0x007E0) | ((value & 0x3F) << 11)
//   #DA write (port&3==2) : adr2_ = (adr2_ & 0x3F800) | ((value & 0x3F) << 5)
//   #DB write (port&3==3) : adr2_ = (adr2_ & 0x207E0) | ((value & 0x3F) << 11)
//     -- the mask 0x207E0 preserves bit 17 (0x20000).
//   #D9 read  (port&3==1) : returns rom_[adr1_], THEN
//     adr1_ = (adr1_ & ~0x1F) | ((adr1_ + 1) & 0x1F) -- wraps within a
//     32-byte glyph block.
//   #DB read  (port&3==3) : returns rom_[adr2_] (valid for the 256 KB path
//     this machine always uses -- MSXKanji.cc:82's "temp workaround"), THEN
//     increments adr2_'s low 5 bits identically.
//   #D8/#DA read (port&3==0/2) : always open-bus 0xFF, NO side effect (the
//     isLascom fallthrough that would make #D8 also readable does not apply
//     here -- MSXKanji.cc:54-58, isLascom is false for this machine).
//
// reset() restores adr1_=0x00000, adr2_=0x20000 (MSXKanji.cc:28-29).
//
// Pure combinational device (A-M18-4): no clock dependency, identical to the
// M13 RomDevice / M17 Ym2413Opll no-clock-consumer classification. The image
// is a fixed 256 KB (0x40000) byte store, loaded by the machine via the
// existing M13 RomAssetLoader (bios/f1xvkfn.rom).
class KanjiFontRom final : public core::IoDevice {
public:
    static constexpr std::size_t kImageSize = 0x40000;  // 256 KB (A-M18-2)
    static constexpr std::uint32_t kResetAdr1 = 0x00000;
    static constexpr std::uint32_t kResetAdr2 = 0x20000;

    KanjiFontRom();

    void reset();

    // Replace the backing image. Normalized (truncate / 0xFF-pad) to
    // kImageSize so the device stays deterministic even if a caller supplies
    // a mis-sized image (mirrors devices::memory::RomDevice::set_image's
    // missing-asset 0xFF-fill discipline).
    void set_image(std::vector<std::uint8_t> image);
    [[nodiscard]] const std::vector<std::uint8_t>& image() const { return image_; }

    // core::IoDevice on #D8-#DB (dispatch on port & 0x03).
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // Introspection for deterministic tests (non-perturbing).
    [[nodiscard]] std::uint32_t address1() const { return adr1_; }
    [[nodiscard]] std::uint32_t address2() const { return adr2_; }

private:
    std::vector<std::uint8_t> image_;
    std::uint32_t adr1_ = kResetAdr1;
    std::uint32_t adr2_ = kResetAdr2;
};

}  // namespace sony_msx::devices::kanji
