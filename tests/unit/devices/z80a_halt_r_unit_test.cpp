#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_Z80AHaltR_Unit  (M23-S1, closes backlog C2 in full / DEC-0004)
//
// Proves the Z80 HALT-refetch phantom-M1 fix in isolation, directly against the
// CPU core (bypassing the machine layer's S1985 M1-wait arithmetic entirely).
// Grounding: references/openmsx-21.0/src/cpu/Z80.hh:19-21 (HALT_STATES = 4 +
// WAIT_CYCLES) and CPUCore.cc:2508-2511 (incR(advanceHalt(HALT_STATES,...))) --
// the SAME `halts` computation drives both the R-register increment and the
// clock advance on real silicon; they are not separable mechanisms. This
// milestone's ONLY CPU-core change is that the halted branch now calls the
// EXISTING increment_refresh_register() helper; the branch's own returned
// `tstates` literal stays the bare, unchanged datasheet 4 (A-M23-1's
// architectural invariant -- proven directly below).

namespace {

namespace cpu = sony_msx::devices::cpu;

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
    // --- R increments by exactly 1 per halted step() call. --------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        bus.memory[0x0000] = 0x76;  // HALT
        cpu.step();                // executes HALT itself: R -> 1, halted = true
        const std::uint8_t r_after_halt_opcode = cpu.state().regs().r;

        const std::uint32_t t_idle1 = cpu.step();  // halted idle #1
        const std::uint8_t r_after_idle1 = cpu.state().regs().r;
        const std::uint32_t t_idle2 = cpu.step();  // halted idle #2
        const std::uint8_t r_after_idle2 = cpu.state().regs().r;

        g_failures += expect_true(t_idle1 == 4 && t_idle2 == 4,
                                  "HaltedStep_BareTstates_StaysDatasheetFour")
                          ? 0
                          : 1;
        g_failures += expect_true(
                          static_cast<std::uint8_t>((r_after_idle1 - r_after_halt_opcode) & 0x7F) == 1 &&
                              static_cast<std::uint8_t>((r_after_idle2 - r_after_idle1) & 0x7F) == 1,
                          "HaltedStep_RefreshRegister_IncrementsByExactlyOne")
                          ? 0
                          : 1;
    }

    // --- R wraps 0x7F -> 0x00 while bit 7 is preserved (mirrors the existing ---
    // --- M12 R-register low-7-bit test pattern, z80a_parity_undocumented_-  ---
    // --- unit_test.cpp's RLow7Wrap_Bit7Frozen case). ---------------------------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        bus.memory[0x0000] = 0x76;  // HALT
        cpu.state().regs().r = 0xFF;  // bit7 set, low7 = 0x7F
        cpu.step();                   // HALT opcode's own M1 -> low7 wraps 0x7F->0x00
        g_failures += expect_true(cpu.state().regs().r == 0x80,
                                  "HaltOpcode_RLow7Wrap_Bit7Frozen")
                          ? 0
                          : 1;

        cpu.state().regs().r = 0xFF;  // re-arm: bit7 set, low7 = 0x7F, still halted
        cpu.step();                   // halted idle step -> low7 wraps again
        g_failures += expect_true(cpu.state().regs().r == 0x80,
                                  "HaltedIdleStep_RLow7Wrap_Bit7Frozen")
                          ? 0
                          : 1;
    }

    // --- Z80aCpu::step() called DIRECTLY on an already-halted CPU still returns
    // --- the bare 4 (A-M23-1's invariant: the CPU-core/machine-layer split is --
    // --- preserved, not collapsed -- the machine layer's separate +m1_wait -----
    // --- arithmetic is what turns this into 5T, never the CPU core itself). ---
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        bus.memory[0x0000] = 0x76;  // HALT
        cpu.step();
        const std::uint32_t t = cpu.step();  // already halted
        g_failures += expect_true(t == 4 && cpu.m1_cycles_last_step() == 1,
                                  "HaltedStep_BypassMachine_ReturnsBareFourAndOneM1")
                          ? 0
                          : 1;
    }

    // --- An interrupt arriving while halted still transitions out via the -----
    // --- UNCHANGED, already-QA-verified M12 interrupt-accept path (IM1, bare --
    // --- datasheet 13T ack, unaffected by the HALT-R fix) -- proving zero -----
    // --- coupling between the HALT-R fix and interrupt-accept timing. ---------
    {
        FakeBus bus;
        cpu::CpuBusClient client(bus);
        cpu::Z80aCpu cpu(client);
        cpu.reset();
        cpu.set_interrupt_mode(cpu::InterruptMode::Im1);
        cpu.state().set_iff1(true);
        bus.memory[0x0000] = 0x76;  // HALT
        cpu.step();                 // halted, R -> 1
        cpu.step();                 // one halted idle step, R -> 2 (bare 4T)
        const std::uint8_t r_before_irq = cpu.state().regs().r;

        cpu.request_maskable_interrupt();
        const std::uint32_t t_irq = cpu.step();  // IM1 ack: bare 13T, one more M1
        g_failures += expect_true(t_irq == 13 && !cpu.state().halted() &&
                                      cpu.state().regs().pc == 0x0038,
                                  "HaltedInterruptWake_Im1Ack_BareThirteenTstates")
                          ? 0
                          : 1;
        g_failures += expect_true(
                          static_cast<std::uint8_t>((cpu.state().regs().r - r_before_irq) & 0x7F) == 1,
                          "HaltedInterruptWake_RefreshRegister_OneMoreIncrement")
                          ? 0
                          : 1;
    }

    return g_failures == 0 ? 0 : 1;
}
