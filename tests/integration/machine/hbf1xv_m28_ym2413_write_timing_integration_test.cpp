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

// Suite: Machine_Hbf1xvM28Ym2413WriteTiming_Integration  (M28-S1, backlog E2)
//
// E2: YM2413 register-write minimum-spacing gate, exercised over the REAL M11
// CPU bus (real Z80A `OUT (n),A` execution via step_cpu_instruction(), never
// Hbf1xvMachine::debug_io_write()'s zero-cycle-advance raw poke) at varying
// instruction spacing, confirming register writes land/drop per the cited
// constants (references/fact-sheets/Yamaha YM2413 FM Chip.md §8).
//
// Cases:
//   1. Regression parity (gate at its DEFAULT -- OFF): a tight, back-to-back
//      OUT (#7C)/OUT (#7D) register-write sequence over the real bus lands
//      EVERY write, byte-identical to the pre-M28 (M17) baseline behaviour --
//      confirms wiring the E2 clock source into Hbf1xvMachine (M28-S1)
//      changed nothing by default.
//   2. Gate ENABLED + tight sequence: at least the first write lands (no
//      prior reference -> always accepted), but STRICTLY FEWER registers
//      land than the generously-spaced case below -- proves the gate
//      genuinely drops real, CPU-driven, too-fast writes.
//   3. Gate ENABLED + generously NOP-padded sequence (>=84 cycles between
//      every data write and the following address write): every register
//      lands -- proves sufficient real spacing is accepted.
//   4. Two-machine determinism: the identical gated program run on two
//      independent machine instances produces byte-identical register state
//      (including which writes were dropped).
//
// (Zero-regression across the full M1-M27 suite is confirmed separately by
// the full `ctest` run captured in docs/m28-implementation-report.md, not
// re-asserted here.)

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

struct RegVal {
    std::uint8_t reg;
    std::uint8_t value;
};

const std::array<RegVal, 4> kProbe{{
    {0x01, 0xAA},
    {0x02, 0xBB},
    {0x03, 0xCC},
    {0x04, 0xDD},
}};

// Assembles: for each (reg,value): LD A,reg ; OUT (0x7C),A ; LD A,value ;
// OUT (0x7D),A ; [nops_between_pairs x NOP]. HALT at the end. The NOP run
// lands BETWEEN one pair's data write and the next pair's address write --
// exactly the E2 84-cycle data-write "wait before the next write" gap.
std::vector<std::uint8_t> build_probe(const int nops_between_pairs) {
    std::vector<std::uint8_t> bytes;
    for (const auto& rv : kProbe) {
        bytes.insert(bytes.end(), {0x3E, rv.reg, 0xD3, 0x7C});    // LD A,reg  ; OUT (0x7C),A
        bytes.insert(bytes.end(), {0x3E, rv.value, 0xD3, 0x7D});  // LD A,val  ; OUT (0x7D),A
        for (int i = 0; i < nops_between_pairs; ++i) {
            bytes.push_back(0x00);  // NOP
        }
    }
    bytes.push_back(0x76);  // HALT
    return bytes;
}

int landed_count(sony_msx::machine::Hbf1xvMachine& machine) {
    int count = 0;
    for (const auto& rv : kProbe) {
        if (machine.ym2413().register_value(rv.reg) == rv.value) {
            ++count;
        }
    }
    return count;
}

void run_to_halt(sony_msx::machine::Hbf1xvMachine& machine) {
    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 10000) {
        machine.step_cpu_instruction();
        ++guard;
    }
    expect(machine.cpu().state().halted(), "Probe_ReachesHalt");
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Case 1: default (gate OFF) -- tight sequence, every write lands. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        expect(!machine.ym2413().write_timing_enforced(), "Default_GateOff_OverRealMachine");

        const std::vector<std::uint8_t> program = build_probe(/*nops_between_pairs=*/0);
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;
        run_to_halt(machine);

        expect(landed_count(machine) == static_cast<int>(kProbe.size()),
               "GateOff_TightSequence_AllWritesLand_M17BaselineParity");
    }

    // --- Case 2 vs 3: gate ENABLED, tight vs generously-padded spacing. ---
    int tight_landed = 0;
    int padded_landed = 0;
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        // The Ym2413Clock adapter is already wired by wire_bus() (X-pattern);
        // enabling the gate here is the only opt-in needed.
        machine.ym2413().set_write_timing_enforced(true);

        const std::vector<std::uint8_t> program = build_probe(/*nops_between_pairs=*/0);
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;
        run_to_halt(machine);

        tight_landed = landed_count(machine);
        expect(tight_landed >= 1,
               "GateOn_TightSequence_FirstWriteAlwaysAccepted_AtLeastOneLands");
    }
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.ym2413().set_write_timing_enforced(true);

        // 50 NOPs @ >=2 real cycles each comfortably exceeds the cited 84
        // master-cycle data-write minimum spacing under any plausible NOP
        // timing (the real Z80 NOP is 4 T-states; even a pessimistic 2T/NOP
        // model clears 84 with this many).
        const std::vector<std::uint8_t> program = build_probe(/*nops_between_pairs=*/50);
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;
        run_to_halt(machine);

        padded_landed = landed_count(machine);
        expect(padded_landed == static_cast<int>(kProbe.size()),
               "GateOn_GenerouslyPaddedSequence_AllWritesLand");
    }
    expect(tight_landed < padded_landed,
           "GateOn_TightSequence_DropsStrictlyMoreThanPaddedSequence_RealBusEvidence");
    std::cerr << "[evidence] gate-enabled landed counts: tight=" << tight_landed
              << " padded=" << padded_landed << " (of " << kProbe.size() << ")\n";

    // --- Case 4: two-machine determinism under the gated, tight program. ---
    {
        auto run = []() {
            Hbf1xvMachine machine;
            machine.cold_boot();
            machine.map_flat_ram();
            machine.ym2413().set_write_timing_enforced(true);
            const std::vector<std::uint8_t> program = build_probe(/*nops_between_pairs=*/0);
            machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
            machine.cpu().state().regs().pc = 0x0000;
            int guard = 0;
            while (!machine.cpu().state().halted() && guard < 10000) {
                machine.step_cpu_instruction();
                ++guard;
            }
            return machine;
        };
        Hbf1xvMachine a = run();
        Hbf1xvMachine b = run();
        bool identical = true;
        for (int reg = 0; reg < 0x40; ++reg) {
            if (a.ym2413().register_value(static_cast<std::uint8_t>(reg)) !=
                b.ym2413().register_value(static_cast<std::uint8_t>(reg))) {
                identical = false;
            }
        }
        expect(identical, "TwoMachineDeterminism_GatedProgram_IdenticalRegisterState");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM28Ym2413WriteTiming_Integration cases passed\n";
    return 0;
}
