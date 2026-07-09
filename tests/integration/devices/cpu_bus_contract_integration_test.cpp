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

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"

namespace {

struct BusOp {
    char kind;
    std::uint16_t address;
    std::uint8_t value;
};

class ScriptedBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress address) override {
        const std::uint8_t value = memory[address];
        operations.push_back({'R', address, value});
        return value;
    }

    void write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        memory[address] = value;
        operations.push_back({'W', address, value});
    }

    std::unordered_map<std::uint16_t, std::uint8_t> memory;
    std::vector<BusOp> operations;
};

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Devices_CpuBusContract_Integration
    ScriptedBus bus;
    bus.memory[0x0100] = 0x3E;

    sony_msx::devices::cpu::CpuBusClient cpu_bus(bus);

    const std::uint8_t opcode = cpu_bus.fetch_opcode(0x0100u);
    cpu_bus.write_data(0x0101u, static_cast<std::uint8_t>(opcode + 1));
    const std::uint8_t observed = cpu_bus.read_data(0x0101u);

    if (!expect_true(opcode == 0x3E, "OpcodeFetch_KnownAddress_ExpectedValue")) {
        return 1;
    }

    if (!expect_true(observed == 0x3F, "WriteThenRead_NextByte_ReproducibleValue")) {
        return 1;
    }

    if (!expect_true(bus.operations.size() == 3, "OperationTrace_ExpectedLength")) {
        return 1;
    }

    if (!expect_true(bus.operations[0].kind == 'R' && bus.operations[0].address == 0x0100,
            "Operation0_OpcodeFetch_StableAddress")) {
        return 1;
    }

    if (!expect_true(bus.operations[1].kind == 'W' && bus.operations[1].address == 0x0101 && bus.operations[1].value == 0x3F,
            "Operation1_DataWrite_StableAddressAndValue")) {
        return 1;
    }

    if (!expect_true(bus.operations[2].kind == 'R' && bus.operations[2].address == 0x0101 && bus.operations[2].value == 0x3F,
            "Operation2_DataRead_StableAddressAndValue")) {
        return 1;
    }

    return 0;
}
