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

#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_CpuIoSeam_Unit
//
// Verifies the CPU I/O seam (IN/OUT route through core::Bus::io_read/
// io_write with the correct port formation) and the per-step M1-cycle counter
// that the S1985 layer maps to the MSX +1-per-M1 wait. Datasheet T-states are
// asserted unchanged.

namespace {

struct IoOp {
    char kind;  // 'R' or 'W'
    std::uint16_t port;
    std::uint8_t value;
};

class FakeBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress address) override {
        return memory[address];
    }
    void write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        memory[address] = value;
    }
    sony_msx::core::BusData io_read(const sony_msx::core::BusAddress port) override {
        const auto it = io.find(port & 0xFF);
        const std::uint8_t value = (it == io.end()) ? 0xFF : it->second;
        io_ops.push_back({'R', port, value});
        return value;
    }
    void io_write(const sony_msx::core::BusAddress port, const sony_msx::core::BusData value) override {
        io[port & 0xFF] = value;
        io_ops.push_back({'W', port, value});
    }

    std::unordered_map<std::uint16_t, std::uint8_t> memory;
    std::unordered_map<std::uint8_t, std::uint8_t> io;
    std::vector<IoOp> io_ops;
};

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Runs a single instruction from the given program bytes at PC=0 and returns
// the M1-cycle count for that step.
std::uint32_t m1_of(const std::vector<std::uint8_t>& bytes) {
    FakeBus bus;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bus.memory[static_cast<std::uint16_t>(i)] = bytes[i];
    }
    sony_msx::devices::cpu::CpuBusClient client(bus);
    sony_msx::devices::cpu::Z80aCpu cpu(client);
    cpu.reset();
    cpu.step();
    return cpu.m1_cycles_last_step();
}

}  // namespace

int main() {
    // --- OUT (n),A then IN A,(n): port = (A<<8)|n, round-trips through io bus. ---
    {
        FakeBus bus;
        // LD A,0x5A ; OUT (0x99),A ; IN A,(0x99)
        const std::array<std::uint8_t, 6> program{0x3E, 0x5A, 0xD3, 0x99, 0xDB, 0x99};
        for (std::size_t i = 0; i < program.size(); ++i) {
            bus.memory[static_cast<std::uint16_t>(i)] = program[i];
        }
        sony_msx::devices::cpu::CpuBusClient client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(client);
        cpu.reset();

        cpu.step();  // LD A,0x5A
        const std::uint32_t out_t = cpu.step();  // OUT (n),A
        if (!expect_true(out_t == 11, "OutNa_DatasheetTstates_Unchanged")) {
            return 1;
        }
        // Now A is clobbered to prove the read really comes from the bus.
        cpu.state().regs().set_a(0x00);
        const std::uint32_t in_t = cpu.step();  // IN A,(n)
        if (!expect_true(in_t == 11, "InAn_DatasheetTstates_Unchanged")) {
            return 1;
        }
        if (!expect_true(cpu.state().regs().a() == 0x5A, "OutThenIn_SamePort_RoundTripsValue")) {
            return 1;
        }
        // Port formation: OUT used (0x5A<<8)|0x99 = 0x5A99; the second IN used A=0x00
        // so port = 0x0099 — both map to port&0xFF = 0x99 so dispatch is identical.
        if (!expect_true(bus.io_ops.size() == 2 && bus.io_ops[0].kind == 'W' &&
                             bus.io_ops[0].port == 0x5A99 && bus.io_ops[0].value == 0x5A,
                         "OutNa_PortFormation_HighByteIsAccumulator")) {
            return 1;
        }
        if (!expect_true(bus.io_ops[1].kind == 'R' && bus.io_ops[1].port == 0x0099,
                         "InAn_PortFormation_HighByteIsAccumulator")) {
            return 1;
        }
    }

    // --- ED IN r,(C) / OUT (C),r use BC as the port. ---
    {
        FakeBus bus;
        // LD B,0x12 ; LD C,0x34 ; LD D,0x77 ; OUT (C),D ; IN E,(C)
        const std::array<std::uint8_t, 10> program{
            0x06, 0x12, 0x0E, 0x34, 0x16, 0x77, 0xED, 0x51, 0xED, 0x58};
        for (std::size_t i = 0; i < program.size(); ++i) {
            bus.memory[static_cast<std::uint16_t>(i)] = program[i];
        }
        sony_msx::devices::cpu::CpuBusClient client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.step();  // LD B
        cpu.step();  // LD C
        cpu.step();  // LD D
        cpu.step();  // OUT (C),D
        cpu.step();  // IN E,(C)
        if (!expect_true(cpu.state().regs().e() == 0x77, "InOutC_BcPort_RoundTripsValue")) {
            return 1;
        }
        if (!expect_true(bus.io_ops.size() == 2 && bus.io_ops[0].port == 0x1234 &&
                             bus.io_ops[0].value == 0x77 && bus.io_ops[1].port == 0x1234,
                         "InOutC_Port_IsBcRegister")) {
            return 1;
        }
    }

    // --- Unmapped I/O read returns the open-bus value 0xFF (A-1). ---
    {
        FakeBus bus;
        const std::array<std::uint8_t, 2> program{0xDB, 0x40};  // IN A,(0x40)
        for (std::size_t i = 0; i < program.size(); ++i) {
            bus.memory[static_cast<std::uint16_t>(i)] = program[i];
        }
        sony_msx::devices::cpu::CpuBusClient client(bus);
        sony_msx::devices::cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x00);
        cpu.step();
        if (!expect_true(cpu.state().regs().a() == 0xFF, "InAn_UnmappedPort_ReadsOpenBus0xFF")) {
            return 1;
        }
    }

    // --- M1-cycle count oracle (drives the S1985 +1-per-M1 wait): prefixes each add a cycle. ---
    if (!expect_true(m1_of({0x00}) == 1, "M1Count_Nop_IsOne")) {  // NOP
        return 1;
    }
    if (!expect_true(m1_of({0x3E, 0x2A}) == 1, "M1Count_LdAn_IsOne")) {  // LD A,n
        return 1;
    }
    if (!expect_true(m1_of({0xCB, 0x47}) == 2, "M1Count_CbOp_IsTwo")) {  // BIT 0,A
        return 1;
    }
    if (!expect_true(m1_of({0xED, 0x44}) == 2, "M1Count_EdOp_IsTwo")) {  // NEG
        return 1;
    }
    if (!expect_true(m1_of({0xDD, 0x23}) == 2, "M1Count_DdOp_IsTwo")) {  // INC IX
        return 1;
    }
    if (!expect_true(m1_of({0xDD, 0xCB, 0x00, 0x46}) == 2,
                     "M1Count_DdCbOp_IsTwo_DisplacementAndSubOpcodeNotM1")) {  // BIT 0,(IX+0)
        return 1;
    }

    return 0;
}
