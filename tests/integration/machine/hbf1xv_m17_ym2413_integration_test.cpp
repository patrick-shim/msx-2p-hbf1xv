// Suite: Machine_Hbf1xvM17Ym2413_Integration  (M17-S3, backlog B3)
//
// Machine-level wiring of the YM2413 (OPLL) register-accurate device: attached
// on io_bus_ #7C/#7D (M11 SystemBus), alongside the UNCHANGED M13 fmmusic_rom_
// at slot 3-3 page 1 (A-M17-2 hard regression guard -- this machine's real
// sound device is the internal MSXMusic pattern: fixed ROM + I/O-port-only
// OPLL, no memory-space register overlay, so the ROM attach needed no wrap).
//
// Cases:
//   1. A real CPU program drives OUT (#7C),A / OUT (#7D),A over the full M11
//      bus for a representative set of registers; machine.ym2413().
//      register_value(...) reflects every write.
//   2. fmmusic_rom_ at slot 3-3 page 1 returns IDENTICAL bytes before and
//      after the OUT sequence (A-M17-2 regression guard).
//   3. IN A,(#7C) / IN A,(#7D) over the bus read 0xFF (A-M17-5 open-bus),
//      regardless of the prior writes.
//   4. Two-machine determinism: the identical program run on two independent
//      machine instances produces byte-identical register state.
//
// (Zero-regression across the full M1-M16 suite is confirmed separately by the
// full `ctest` run captured in the M17 implementation report, not re-asserted
// here.)

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

// Representative register/value pairs covering user-patch ($00-$07), F-Num/
// Block/Key-on ($10-$18/$20-$28), instrument/volume ($30-$38), and rhythm
// ($0E, $36-$38) -- the same address families the M17-S5 A/B probe exercises.
struct RegVal {
    std::uint8_t reg;
    std::uint8_t value;
};

const std::array<RegVal, 12> kRepresentative{{
    {0x00, 0x71},  // user-patch modulator (Violin mod byte)
    {0x03, 0x17},
    {0x07, 0x23},
    {0x10, 0x34},  // channel 0 F-Num low
    {0x18, 0x2A},  // channel 8 F-Num low
    {0x20, 0x31},  // channel 0: SUS=1 KEY=1 BLOCK=0 F-Num[8]=1
    {0x28, 0x00},  // channel 8: idle
    {0x30, 0x1A},  // channel 0: instrument 1, volume A
    {0x38, 0xF3},  // channel 8: instrument F, volume 3 (also rhythm TOM/CYM volume when rhythm on)
    {0x0E, 0x35},  // rhythm enable + BD/TOM/HH keys
    {0x36, 0x0A},  // BD volume
    {0x37, 0x3C},  // HH/SD volume
}};

// Assembles: for each (reg,value): LD A,reg ; OUT (0x7C),A ; LD A,value ; OUT
// (0x7D),A. HALT at the end.
std::vector<std::uint8_t> build_ym2413_write_probe() {
    std::vector<std::uint8_t> bytes;
    for (const auto& rv : kRepresentative) {
        bytes.insert(bytes.end(), {0x3E, rv.reg, 0xD3, 0x7C});     // LD A,reg  ; OUT (0x7C),A
        bytes.insert(bytes.end(), {0x3E, rv.value, 0xD3, 0x7D});  // LD A,val  ; OUT (0x7D),A
    }
    bytes.push_back(0x76);  // HALT
    return bytes;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Case 1: real CPU program drives OUT (#7C)/OUT (#7D); register_value reflects it. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        const std::vector<std::uint8_t> program = build_ym2413_write_probe();
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;

        int guard = 0;
        while (!machine.cpu().state().halted() && guard < 10000) {
            machine.step_cpu_instruction();
            ++guard;
        }
        expect(machine.cpu().state().halted(), "WriteProbe_ReachesHalt");

        bool all_ok = true;
        for (const auto& rv : kRepresentative) {
            if (machine.ym2413().register_value(rv.reg) != rv.value) {
                all_ok = false;
            }
        }
        expect(all_ok, "WriteProbe_RegisterValue_ReflectsEveryOutWrite");
    }

    // --- Case 2: fmmusic_rom_ regression guard (A-M17-2). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();

        auto read_fmmusic_rom = [&machine]() {
            // Route page 1 -> primary slot 3, sub-slot 3 (MSX-MUSIC ROM), while
            // page 3 stays primary slot 0 (untouched; no CPU program runs here).
            machine.debug_io_write(0xA8, 0x0C);      // page1 field bits[3:2] = 11 (slot 3)
            machine.debug_bus_write(0xFFFF, 0x0C);   // slot-3 sub-slot reg: page1 field = 11 (sub 3)
            std::array<std::uint8_t, 64> sample{};
            for (std::size_t i = 0; i < sample.size(); ++i) {
                sample[i] = machine.debug_bus_read(static_cast<std::uint16_t>(0x4000 + i));
            }
            machine.debug_io_write(0xA8, 0x00);  // restore the authentic reset default
            return sample;
        };

        const std::array<std::uint8_t, 64> before = read_fmmusic_rom();

        // Drive the SAME OUT (#7C)/(#7D) write sequence as Case 1, directly over
        // the bus (no CPU navigation needed -- #7C/#7D are pure I/O ports).
        for (const auto& rv : kRepresentative) {
            machine.debug_io_write(0x7C, rv.reg);
            machine.debug_io_write(0x7D, rv.value);
        }

        const std::array<std::uint8_t, 64> after = read_fmmusic_rom();
        expect(before == after, "FmMusicRom_Slot33Page1_UnchangedByYm2413Writes");
    }

    // --- Case 3: IN A,(#7C) / IN A,(#7D) over the bus read 0xFF (A-M17-5). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0x7C, 0x10);
        machine.debug_io_write(0x7D, 0x5A);
        expect(machine.debug_io_read(0x7C) == 0xFF, "IoRead_Port7C_OpenBus_OverBus");
        expect(machine.debug_io_read(0x7D) == 0xFF, "IoRead_Port7D_OpenBus_OverBus");
    }

    // --- Case 4: two-machine determinism. ---
    {
        auto run = []() {
            Hbf1xvMachine machine;
            machine.cold_boot();
            machine.map_flat_ram();
            const std::vector<std::uint8_t> program = build_ym2413_write_probe();
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
        expect(identical, "TwoMachineDeterminism_IdenticalProgram_IdenticalRegisterState");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM17Ym2413_Integration cases passed\n";
    return 0;
}
