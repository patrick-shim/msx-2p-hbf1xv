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

// Suite: Devices_Z80ABlockFlags_Unit  (M12-S1, PRESENT-item regression floor for
// gaps #6 LDI/LDD, #7 CPI/CPD, #8 INI/IND, #9 OUTI/OUTD incl. the NMOS
// OUTI-affects-carry edge). Asserts the FULL flag byte, closing the M9-S3
// unasserted-flag gap. Grounding: fact-sheet §4; openMSX BLOCK_* CPUCore.cc.

namespace {

namespace cpu = sony_msx::devices::cpu;
using State = cpu::Z80aState;

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
        return (it == io.end()) ? 0xFF : it->second;
    }
    void io_write(const sony_msx::core::BusAddress port, const sony_msx::core::BusData value) override {
        io[port & 0xFF] = value;
        last_out_port = static_cast<std::uint16_t>(port);
        last_out_value = value;
    }
    std::array<std::uint8_t, 65536> memory{};
    std::unordered_map<std::uint8_t, std::uint8_t> io;
    std::uint16_t last_out_port = 0;
    std::uint8_t last_out_value = 0;
};

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

int g_failures = 0;

}  // namespace

int main() {
    // --- LDI: n = A + value; Y = bit1(n), X = bit3(n); H=N=0; PV=(BC!=0) ----
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x02);
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().de = 0x5000;
        cpu.state().regs().bc = 0x0003;
        cpu.state().regs().set_f(0x00);
        bus.memory[0x4000] = 0x01;  // n = 0x02+0x01 = 0x03 -> Y(bit1) set, X(bit3) clear
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA0;  // LDI
        const std::uint32_t t = cpu.step();
        // BC -> 0x0002 (!=0) PV set. F = Y|PV = 0x24.
        g_failures += expect_true(t == 16 && bus.memory[0x5000] == 0x01 &&
                                      cpu.state().regs().hl == 0x4001 &&
                                      cpu.state().regs().de == 0x5001 &&
                                      cpu.state().regs().bc == 0x0002 &&
                                      cpu.state().regs().f() == 0x24,
                                  "Ldi_FullFlagByte")
                          ? 0
                          : 1;
    }

    // --- LDD: same flag model, addresses decrement, BC->0 clears PV --------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x02);
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().de = 0x5000;
        cpu.state().regs().bc = 0x0001;  // -> 0 clears PV
        cpu.state().regs().set_f(0x00);
        bus.memory[0x4000] = 0x01;  // n = 0x03 -> Y set
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA8;  // LDD
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 16 && cpu.state().regs().hl == 0x3FFF &&
                                      cpu.state().regs().de == 0x4FFF &&
                                      cpu.state().regs().bc == 0x0000 &&
                                      cpu.state().regs().f() == 0x20,  // Y only
                                  "Ldd_FullFlagByte_BcZeroNoPv")
                          ? 0
                          : 1;
    }

    // --- CPI: S/Z/H per CP; n=A-(HL)-H; Y=bit1,X=bit3; PV=(BC!=0); N=1; C keep
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x05);
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().bc = 0x0002;  // -> 1, PV set
        cpu.state().regs().set_f(State::kFlagCarry);  // C must be preserved
        bus.memory[0x4000] = 0x03;  // result 0x02, no half; n = 0x02 -> Y set
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA1;  // CPI
        const std::uint32_t t = cpu.step();
        // F = C|N|PV|Y = 0x27.
        g_failures += expect_true(t == 16 && cpu.state().regs().hl == 0x4001 &&
                                      cpu.state().regs().bc == 0x0001 &&
                                      cpu.state().regs().f() == 0x27,
                                  "Cpi_FullFlagByte_CarryPreserved")
                          ? 0
                          : 1;
    }

    // --- CPD: half-borrow path affects n; N=1; C preserved (here clear) -----
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0x10);
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().bc = 0x0001;  // -> 0, no PV
        cpu.state().regs().set_f(0x00);
        bus.memory[0x4000] = 0x01;  // 0x10-0x01=0x0F, half borrow -> H set; n=0x0F-1=0x0E
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA9;  // CPD
        const std::uint32_t t = cpu.step();
        // result 0x0F: S no, Z no. H set. n = 0x0E -> X(bit3) set, Y(bit1) set. N set.
        // F = H|X|Y|N = 0x10|0x08|0x20|0x02 = 0x3A.
        g_failures += expect_true(t == 16 && cpu.state().regs().hl == 0x3FFF &&
                                      cpu.state().regs().f() == 0x3A,
                                  "Cpd_FullFlagByte_HalfBorrow")
                          ? 0
                          : 1;
    }

    // --- INI: S/Z/Y/X per DEC B; N=bit7(data); C/H from data+((C+1)&255) -----
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().bc = 0x02C0;  // B->0x01, C=0xC0
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().set_f(0x00);
        bus.io[0xC0] = 0x80;  // data bit7 set -> N; k = 0x80+0xC1 = 0x141 >255 -> H|C
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA2;  // INI
        const std::uint32_t t = cpu.step();
        // b=0x01: no S/Z/XY. N set. H|C set. parity((0x141&7)^1)=parity(0)=even -> PV.
        // F = N|H|C|PV = 0x02|0x10|0x01|0x04 = 0x17.
        g_failures += expect_true(t == 16 && bus.memory[0x4000] == 0x80 &&
                                      cpu.state().regs().b() == 0x01 &&
                                      cpu.state().regs().hl == 0x4001 &&
                                      cpu.state().regs().f() == 0x17,
                                  "Ini_FullFlagByte")
                          ? 0
                          : 1;
    }

    // --- IND: decrement direction; flag model identical -------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().bc = 0x01C0;  // B->0x00 -> Z set
        cpu.state().regs().hl = 0x4000;
        cpu.state().regs().set_f(0x00);
        bus.io[0xC0] = 0x00;  // data 0 -> no N; k = 0 + ((0xC0-1)&0xFF)=0xBF -> no H/C
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xAA;  // IND
        const std::uint32_t t = cpu.step();
        // b=0: Z set. parity((0xBF&7)^0)=parity(7)=3 ones odd -> no PV.
        // F = Z = 0x40.
        g_failures += expect_true(t == 16 && cpu.state().regs().hl == 0x3FFF &&
                                      cpu.state().regs().b() == 0x00 &&
                                      cpu.state().regs().f() == 0x40,
                                  "Ind_FullFlagByte_ZeroB")
                          ? 0
                          : 1;
    }

    // --- OUTI: NMOS carry IS affected; k = L(after inc)+value > 255 -> C|H ---
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().bc = 0x02C0;  // B->0x01, port low byte C=0xC0
        cpu.state().regs().hl = 0x40C0;  // -> 0x40C1, new L=0xC1
        cpu.state().regs().set_f(0x00);
        bus.memory[0x40C0] = 0x80;  // data bit7 set -> N; k=0x80+0xC1=0x141 -> H|C
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xA3;  // OUTI
        const std::uint32_t t = cpu.step();
        // F = N|H|C|PV = 0x17 (parity((0x141&7)^1)=parity(0)=even).
        const bool io_ok = (bus.last_out_port & 0xFF) == 0xC0 && bus.last_out_value == 0x80;
        g_failures += expect_true(t == 16 && io_ok && cpu.state().regs().b() == 0x01 &&
                                      cpu.state().regs().hl == 0x40C1 &&
                                      cpu.state().regs().f() == 0x17,
                                  "Outi_NmosCarryAffected_FullFlagByte")
                          ? 0
                          : 1;
    }

    // --- OUTD: carry NOT set when k <= 255 (proves carry truly varies) ------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().bc = 0x02C0;   // B->0x01
        cpu.state().regs().hl = 0x4002;   // -> 0x4001, new L=0x01
        cpu.state().regs().set_f(State::kFlagCarry);  // start with C set to prove it clears
        bus.memory[0x4002] = 0x01;  // k = 0x01 + 0x01 = 0x02 <= 255 -> no C/H
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0xAB;  // OUTD
        const std::uint32_t t = cpu.step();
        // b=0x01: no S/Z/XY. data bit7=0 -> no N. no H/C. parity((0x02&7)^1)=
        // parity(3)=2 ones even -> PV. F = PV = 0x04 (carry cleared).
        g_failures += expect_true(t == 16 && cpu.state().regs().hl == 0x4001 &&
                                      cpu.state().regs().f() == 0x04,
                                  "Outd_CarryClearedWhenNoOverflow")
                          ? 0
                          : 1;
    }

    return g_failures == 0 ? 0 : 1;
}
