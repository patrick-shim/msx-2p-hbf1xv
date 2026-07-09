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

// Suite: Devices_Z80AParityUndocumented_Unit  (M12-S1, PRESENT-item regression
// floor for gaps #13 SLL, #14 IXH/IXL, #15 prefix chaining/NONI, #16 ED-hole
// 2-NOP, #17 R rules, #18 LD R,A bit7, #19 OUT (C),0=0).
//
// These lock behavior already implemented in M9 so later fixes cannot regress
// them. Deterministic opcode buffers + a fake bus with I/O tracking; exact
// register/flag/T-state/R assertions. Grounding: fact-sheet §4/§7/§8.

namespace {

namespace cpu = sony_msx::devices::cpu;
using State = cpu::Z80aState;

struct IoOp {
    char kind;
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
        io_ops.push_back({'R', static_cast<std::uint16_t>(port), value});
        return value;
    }
    void io_write(const sony_msx::core::BusAddress port, const sony_msx::core::BusData value) override {
        io[port & 0xFF] = value;
        io_ops.push_back({'W', static_cast<std::uint16_t>(port), value});
    }
    std::array<std::uint8_t, 65536> memory{};
    std::unordered_map<std::uint8_t, std::uint8_t> io;
    std::vector<IoOp> io_ops;
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
    // --- SLL (undocumented CB 30-37): r = (r<<1)|1 ------------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_b(0xFF);
        bus.memory[0x0000] = 0xCB;
        bus.memory[0x0001] = 0x30;  // SLL B
        const std::uint32_t t = cpu.step();
        // 0xFF -> (0xFF<<1)|1 = 0xFF, carry out 1. F: S|Y|X|PV|C = 0xAD.
        g_failures += expect_true(t == 8 && cpu.state().regs().b() == 0xFF &&
                                      cpu.state().regs().f() == 0xAD,
                                  "SllB_SetsBitZero_FullFlags")
                          ? 0
                          : 1;
    }
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().hl = 0x4000;
        bus.memory[0x4000] = 0x80;
        bus.memory[0x0000] = 0xCB;
        bus.memory[0x0001] = 0x36;  // SLL (HL)
        const std::uint32_t t = cpu.step();
        // 0x80 -> (0x80<<1)|1 = 0x01, carry 1. F = C only.
        g_failures += expect_true(t == 15 && bus.memory[0x4000] == 0x01 &&
                                      cpu.state().regs().f() == State::kFlagCarry,
                                  "SllMemHl_SetsBitZero_CarryOut")
                          ? 0
                          : 1;
    }

    // --- IXH/IXL/IYH/IYL halves -------------------------------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().ix = 0x12FF;
        bus.memory[0x0000] = 0xDD;
        bus.memory[0x0001] = 0x24;  // INC IXH
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().ix == 0x13FF,
                                  "IncIxh_HighHalfIncremented")
                          ? 0
                          : 1;
    }
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().iy = 0x00AB;
        bus.memory[0x0000] = 0xFD;
        bus.memory[0x0001] = 0x2D;  // DEC IYL
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().iy == 0x00AA,
                                  "DecIyl_LowHalfDecremented")
                          ? 0
                          : 1;
    }
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().ix = 0x2010;
        cpu.state().regs().set_a(0x01);
        bus.memory[0x0000] = 0xDD;
        bus.memory[0x0001] = 0x84;  // ADD A,IXH (IXH=0x20)
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().a() == 0x21,
                                  "AddAIxh_UsesHighHalf")
                          ? 0
                          : 1;
    }
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().iy = 0x00CD;
        bus.memory[0x0000] = 0xFD;
        bus.memory[0x0001] = 0x45;  // LD B,IYL
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().b() == 0xCD,
                                  "LdBIyl_ReadsLowHalf")
                          ? 0
                          : 1;
    }

    // --- DD/FD prefix chaining: only the last prefix wins -----------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().ix = 0x1200;
        cpu.state().regs().iy = 0x3400;
        bus.memory[0x0000] = 0xDD;
        bus.memory[0x0001] = 0xFD;
        bus.memory[0x0002] = 0x24;  // DD FD 24 -> INC IYH (last prefix FD wins)
        const std::uint32_t t = cpu.step();
        // IX untouched, IY high incremented; three M1 fetches -> R = 3.
        g_failures += expect_true(t == 12 && cpu.state().regs().ix == 0x1200 &&
                                      cpu.state().regs().iy == 0x3500 && cpu.state().regs().r == 0x03,
                                  "DdFd_LastPrefixWins_IyhIncremented")
                          ? 0
                          : 1;
    }
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().ix = 0x00FF;
        bus.memory[0x0000] = 0xDD;
        bus.memory[0x0001] = 0x00;  // DD then NOP: prefix has no effect, still 4+4
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().ix == 0x00FF &&
                                      cpu.state().regs().r == 0x02,
                                  "DdNop_NoEffect_StillCostsTimeAndR")
                          ? 0
                          : 1;
    }

    // --- ED-hole opcode = two NOPs (8T, R+2) ------------------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x00;  // ED hole (NONI)
        const std::uint32_t t = cpu.step();
        g_failures += expect_true(t == 8 && cpu.state().regs().pc == 0x0002 &&
                                      cpu.state().regs().r == 0x02,
                                  "EdHole_TwoByteNop_RPlusTwo")
                          ? 0
                          : 1;
    }

    // --- OUT (C),0 = 0 on NMOS (ED 71) ------------------------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().bc = 0x00A5;
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x71;  // OUT (C),0
        const std::uint32_t t = cpu.step();
        const bool wrote_zero = bus.io_ops.size() == 1 && bus.io_ops[0].kind == 'W' &&
                                (bus.io_ops[0].port & 0xFF) == 0xA5 && bus.io_ops[0].value == 0x00;
        g_failures += expect_true(t == 12 && wrote_zero, "OutC0_NmosOutputsZero") ? 0 : 1;
    }

    // --- LD R,A writes the full 8 bits (bit 7 included) -------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().set_a(0xC3);  // bit7 set
        bus.memory[0x0000] = 0xED;
        bus.memory[0x0001] = 0x4F;  // LD R,A
        cpu.step();
        // R written to 0xC3, but the LD R,A M1 fetch already ticked R once more?
        // No: R is set AFTER the opcode fetches. The two fetches (ED,4F) happen
        // before ld r,a writes A. So final R == 0xC3 exactly.
        g_failures += expect_true(cpu.state().regs().r == 0xC3, "LdRA_WritesBit7") ? 0 : 1;
    }

    // --- R low-7 wrap 0x7F -> 0x00 with bit 7 frozen ----------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().r = 0xFF;  // bit7 set, low7 = 0x7F
        bus.memory[0x0000] = 0x00;    // NOP -> one M1 -> low7 wraps to 0, bit7 preserved
        cpu.step();
        g_failures += expect_true(cpu.state().regs().r == 0x80, "RLow7Wrap_Bit7Frozen") ? 0 : 1;
    }

    // --- DDCB post-CB sub-opcode is not an M1 (no extra R tick) ------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.state().regs().ix = 0x4000;
        bus.memory[0x4001] = 0x01;
        bus.memory[0x0000] = 0xDD;
        bus.memory[0x0001] = 0xCB;
        bus.memory[0x0002] = 0x01;  // displacement d = 1
        bus.memory[0x0003] = 0x06;  // RLC (IX+1)
        cpu.step();
        // Only DD and CB are M1 fetches -> R = 2 (displacement + sub-opcode are not).
        g_failures += expect_true(cpu.state().regs().r == 0x02 && bus.memory[0x4001] == 0x02,
                                  "DdCb_SubOpcodeNotM1_RPlusTwo")
                          ? 0
                          : 1;
    }

    return g_failures == 0 ? 0 : 1;
}
