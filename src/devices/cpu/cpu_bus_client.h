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

namespace sony_msx::devices::cpu {

class CpuBusClient {
public:
    explicit CpuBusClient(core::Bus& bus);

    core::BusData fetch_opcode(std::uint32_t address);
    core::BusData read_data(std::uint32_t address);
    void write_data(std::uint32_t address, core::BusData value);

    // I/O-space access seam (M11-S1). Port is a full 16-bit value: for
    // IN A,(n)/OUT (n),A the CPU forms (A<<8)|n, for the (C) variants it is BC.
    // The I/O bus keys dispatch on port & 0xFF. Unmapped reads return the
    // open-bus value the underlying core::Bus defines (0xFF).
    core::BusData io_read(std::uint32_t port);
    void io_write(std::uint32_t port, core::BusData value);
    std::uint16_t read_word_le(std::uint32_t address);
    void write_word_le(std::uint32_t address, std::uint16_t value);
    std::uint16_t read_word_be(std::uint32_t address);
    void write_word_be(std::uint32_t address, std::uint16_t value);

private:
    core::Bus& bus_;
};

}  // namespace sony_msx::devices::cpu
