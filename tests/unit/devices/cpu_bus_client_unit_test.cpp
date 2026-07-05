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

class FakeBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress address) override {
        operations.push_back({'R', address, memory[address]});
        return memory[address];
    }

    void write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        operations.push_back({'W', address, value});
        memory[address] = value;
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
    // Suite: Devices_CpuBusClient_Unit
    FakeBus bus;
    bus.memory[0x0002] = 0xA5;

    sony_msx::devices::cpu::CpuBusClient client(bus);

    const std::uint8_t opcode = client.fetch_opcode(0x10002u);
    if (!expect_true(opcode == 0xA5, "FetchOpcode_WrappedAddress_ExpectedValue")) {
        return 1;
    }

    client.write_data(0x2FFFFu, 0x33);
    const std::uint8_t data = client.read_data(0x1FFFFu);
    if (!expect_true(data == 0x33, "ReadAfterWrite_WrappedAddress_DeterministicValue")) {
        return 1;
    }

    if (!expect_true(bus.operations.size() == 3, "OperationCount_ExpectedSequenceLength")) {
        return 1;
    }

    if (!expect_true(bus.operations[0].kind == 'R' && bus.operations[0].address == 0x0002,
            "Operation0_FetchOpcode_UsesNormalizedAddress")) {
        return 1;
    }

    if (!expect_true(bus.operations[1].kind == 'W' && bus.operations[1].address == 0xFFFF && bus.operations[1].value == 0x33,
            "Operation1_WriteData_UsesNormalizedAddress")) {
        return 1;
    }

    if (!expect_true(bus.operations[2].kind == 'R' && bus.operations[2].address == 0xFFFF,
            "Operation2_ReadData_UsesNormalizedAddress")) {
        return 1;
    }

    client.write_word_le(0x10010u, 0xBEEF);
    const std::uint16_t word = client.read_word_le(0x10010u);
    if (!expect_true(word == 0xBEEF, "ReadWriteWordLE_NormalizedAddress_RoundTripsValue")) {
        return 1;
    }

    if (!expect_true(bus.operations.size() == 7, "OperationCount_AfterWordOps_ExpectedSequenceLength")) {
        return 1;
    }

    if (!expect_true(bus.operations[3].kind == 'W' && bus.operations[3].address == 0x0010 &&
            bus.operations[3].value == 0xEF, "Operation3_WriteWordLow_ExpectedAddressAndValue")) {
        return 1;
    }

    if (!expect_true(bus.operations[4].kind == 'W' && bus.operations[4].address == 0x0011 &&
            bus.operations[4].value == 0xBE, "Operation4_WriteWordHigh_ExpectedAddressAndValue")) {
        return 1;
    }

    if (!expect_true(bus.operations[5].kind == 'R' && bus.operations[5].address == 0x0010,
            "Operation5_ReadWordLow_ExpectedAddress")) {
        return 1;
    }

    if (!expect_true(bus.operations[6].kind == 'R' && bus.operations[6].address == 0x0011,
            "Operation6_ReadWordHigh_ExpectedAddress")) {
        return 1;
    }

    client.write_word_be(0x20020u, 0x1234);
    const std::uint16_t word_be = client.read_word_be(0x20020u);
    if (!expect_true(word_be == 0x1234, "ReadWriteWordBE_NormalizedAddress_RoundTripsValue")) {
        return 1;
    }

    if (!expect_true(bus.operations.size() == 11, "OperationCount_AfterWordBEOps_ExpectedSequenceLength")) {
        return 1;
    }

    if (!expect_true(bus.operations[7].kind == 'W' && bus.operations[7].address == 0x0020 &&
            bus.operations[7].value == 0x12, "Operation7_WriteWordBEHigh_ExpectedAddressAndValue")) {
        return 1;
    }

    if (!expect_true(bus.operations[8].kind == 'W' && bus.operations[8].address == 0x0021 &&
            bus.operations[8].value == 0x34, "Operation8_WriteWordBELow_ExpectedAddressAndValue")) {
        return 1;
    }

    if (!expect_true(bus.operations[9].kind == 'R' && bus.operations[9].address == 0x0020,
            "Operation9_ReadWordBEHigh_ExpectedAddress")) {
        return 1;
    }

    if (!expect_true(bus.operations[10].kind == 'R' && bus.operations[10].address == 0x0021,
            "Operation10_ReadWordBELow_ExpectedAddress")) {
        return 1;
    }

    return 0;
}
