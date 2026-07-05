#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/bus.h"
#include "devices/chipset/s1985_engine.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"

// Suite: Devices_ChipsetM1Wait_Unit
//
// M11-S4: the +1-per-M1 wait helper, cross-checked against the S1985 fact-sheet
// §8 examples by combining the CPU's datasheet T-states + M1-cycle count with
// S1985Engine::m1_wait_tstates.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

class SimpleBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress a) override { return memory[a]; }
    void write(const sony_msx::core::BusAddress a, const sony_msx::core::BusData v) override { memory[a] = v; }
    std::array<std::uint8_t, 0x10000> memory{};
};

// Executes one instruction, returns {datasheet_tstates, m1_cycles}.
struct StepResult {
    std::uint32_t datasheet;
    std::uint32_t m1;
};

StepResult run_one(const std::vector<std::uint8_t>& bytes) {
    SimpleBus bus;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bus.memory[i] = bytes[i];
    }
    sony_msx::devices::cpu::CpuBusClient client(bus);
    sony_msx::devices::cpu::Z80aCpu cpu(client);
    cpu.reset();
    const std::uint32_t t = cpu.step();
    return {t, cpu.m1_cycles_last_step()};
}

}  // namespace

int main() {
    using sony_msx::devices::chipset::S1985Engine;
    S1985Engine engine;

    // Pure helper: +1 T per M1 cycle.
    if (!expect_true(engine.m1_wait_tstates(0) == 0 && engine.m1_wait_tstates(1) == 1 &&
                         engine.m1_wait_tstates(2) == 2 && engine.m1_wait_tstates(5) == 5,
                     "M1WaitHelper_LinearInM1Cycles")) {
        return 1;
    }

    // §8 example: XOR A (opcode AF) documented 4T -> MSX 5T (one M1).
    {
        const StepResult r = run_one({0xAF});
        const std::uint32_t msx = r.datasheet + engine.m1_wait_tstates(r.m1);
        if (!expect_true(r.datasheet == 4 && r.m1 == 1 && msx == 5,
                         "XorA_DatasheetPlusM1Wait_Is5")) {
            std::cerr << "  datasheet=" << r.datasheet << " m1=" << r.m1 << " msx=" << msx << "\n";
            return 1;
        }
    }

    // §8 example: BIT 0,(HL) (CB 46) documented 12T -> MSX 14T (two M1: CB + 46).
    {
        const StepResult r = run_one({0xCB, 0x46});
        const std::uint32_t msx = r.datasheet + engine.m1_wait_tstates(r.m1);
        if (!expect_true(r.datasheet == 12 && r.m1 == 2 && msx == 14,
                         "Bit0Hl_DatasheetPlusM1Wait_Is14")) {
            std::cerr << "  datasheet=" << r.datasheet << " m1=" << r.m1 << " msx=" << msx << "\n";
            return 1;
        }
    }

    // §8 example: OUT (n),A (opcode D3) documented 11T -> MSX 12T (one M1).
    {
        const StepResult r = run_one({0xD3, 0x00});
        const std::uint32_t msx = r.datasheet + engine.m1_wait_tstates(r.m1);
        if (!expect_true(r.datasheet == 11 && r.m1 == 1 && msx == 12,
                         "OutNa_DatasheetPlusM1Wait_Is12")) {
            std::cerr << "  datasheet=" << r.datasheet << " m1=" << r.m1 << " msx=" << msx << "\n";
            return 1;
        }
    }

    return 0;
}
