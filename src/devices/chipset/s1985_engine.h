#pragma once

#include <array>
#include <cstdint>

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/mapper_io.h"

namespace sony_msx::devices::chipset {

// Thin S1985 "MSX-ENGINE" residual layer (M11-S4).
//
// Per DEC-0002 and fact-sheet §10, the S1985 is NOT a monolith: PPI slot-select,
// PSG, RTC and the mapper registers are independent modules; this class models
// only the residual engine-specific behaviour:
//   (a) the 16-byte battery-backed backup RAM on switched-I/O device ID 0xFE,
//       with the address/data/rotating-pattern/color1/color2 registers
//       (fact-sheet §6; openMSX MSXS1985.cc:44-91). This class IS that
//       SwitchedDevice (attach it to the SwitchedIoController).
//   (b) the mapper readback base 0x80 / mask 0x1F (`100xxxxx`) — apply via
//       configure_mapper (fact-sheet §4; MSXS1985.cc:31-34).
//   (c) the +1 M1 opcode-fetch wait state helper (fact-sheet §8; A-4). The Z80
//       core publishes datasheet T-states + an M1-cycle count; this maps the
//       count to +1 T-state per M1 cycle. The machine applies it to the
//       scheduler.
//
// SRAM persistence (openMSX saves 16 bytes to a .sram file) is out of M11 scope
// (A-5 / R-6): storage here is volatile and cleared on reset().
class S1985Engine final : public core::SwitchedDevice {
public:
    static constexpr std::uint8_t kId = 0xFE;                 // switched-I/O device ID
    static constexpr std::size_t kBackupRamBytes = 0x10;      // 16 bytes
    static constexpr std::uint32_t kM1WaitTStates = 1;        // +1 T per M1 cycle

    void reset();

    // Push the S1985 mapper-readback pattern (base 0x80, mask 0x1F) into a
    // MapperIo instance.
    void configure_mapper(MapperIo& mapper) const;

    // MSX wait-state contribution for a step that performed `m1_cycles` M1
    // opcode-fetch cycles: +kM1WaitTStates each.
    [[nodiscard]] std::uint32_t m1_wait_tstates(std::uint32_t m1_cycles) const;

    // core::SwitchedDevice — the 16-byte backup RAM (ID 0xFE).
    [[nodiscard]] std::uint8_t id() const override;
    core::BusData switched_read(core::BusAddress port) override;
    void switched_write(core::BusAddress port, core::BusData value) override;

    // Test/debug view of the backup-RAM store.
    [[nodiscard]] std::uint8_t backup_byte(std::uint8_t index) const;

private:
    [[nodiscard]] core::BusData peek_switched(core::BusAddress port) const;

    std::array<std::uint8_t, kBackupRamBytes> sram_{};
    std::uint8_t address_ = 0;
    std::uint8_t pattern_ = 0;
    std::uint8_t color1_ = 0;
    std::uint8_t color2_ = 0;
};

}  // namespace sony_msx::devices::chipset
