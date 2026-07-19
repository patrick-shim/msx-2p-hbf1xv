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
#include <vector>

#include "devices/chipset/ppi_8255.h"
#include "devices/chipset/slot_bus.h"
#include "peripherals/cassette_interface.h"
#include "peripherals/keyboard_matrix.h"

// Suite: Peripherals_CassetteInterface_Unit
//
// Motor/output derivation is a pure, on-demand read of the existing Ppi8255
// (zero edit to Ppi8255 itself); the synthetic-tape input model advances
// READ-ONLY off an injected CassetteClockSource.

namespace {

using sony_msx::devices::chipset::Ppi8255;
using sony_msx::devices::chipset::SlotBus;
using sony_msx::peripherals::CassetteClockSource;
using sony_msx::peripherals::CassetteInterface;
using sony_msx::peripherals::KeyboardMatrix;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// A simple, test-only, directly-settable clock source (no wall-clock, no
// scheduler dependency -- mirrors the machine's real CassetteClock/RtcClock/
// FdcClock adapters but lets the test control the cycle value directly).
class FakeClock final : public CassetteClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycles_; }
    void set_cycles(const std::uint64_t c) { cycles_ = c; }

private:
    std::uint64_t cycles_ = 0;
};

}  // namespace

int main() {
    SlotBus slot_bus;
    KeyboardMatrix kb;
    kb.reset();

    // --- Motor/output track a real Ppi8255's port-C writes LIVE. ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        cassette.reset();

        // Reset default: port C latch 0 -> motor_off()==false -> motor_on()==true;
        // bit5 clear -> output_level()==false.
        expect(cassette.motor_on(), "Reset_PortCZero_MotorOnDefaultTrue");
        expect(!cassette.output_level(), "Reset_PortCZero_OutputLevelFalse");

        ppi.io_write(0xAA, 0x10);  // bit4 = 1 (motor OFF)
        expect(!cassette.motor_on(), "PortCBit4Set_MotorOff_MotorOnFalse");

        ppi.io_write(0xAA, 0x20);  // bit4=0 (motor ON), bit5=1 (output HIGH)
        expect(cassette.motor_on(), "PortCBit4Clear_MotorOn_MotorOnTrue");
        expect(cassette.output_level(), "PortCBit5Set_OutputLevelTrue");

        ppi.io_write(0xAA, 0x00);
        expect(!cassette.output_level(), "PortCBit5Clear_OutputLevelFalse");
    }

    // --- No-tape default is ALWAYS idle-high, with or without an
    //     attached clock source. ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        cassette.reset();
        expect(cassette.cassette_input_high(), "NoTape_NoClock_IdleHigh");

        FakeClock clock;
        cassette.attach_clock_source(&clock);
        clock.set_cycles(123456);
        expect(cassette.cassette_input_high(), "NoTape_WithClock_StillIdleHigh");
    }

    // --- Loading a synthetic tape and advancing the injected clock returns
    //     the expected bit sequence at the expected cycle boundaries. ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        cassette.reset();
        FakeClock clock;
        cassette.attach_clock_source(&clock);

        clock.set_cycles(1000);  // start_cycle_ recorded at load time
        const std::vector<bool> bits{true, false, true, true, false};
        const std::uint64_t cycles_per_bit = 10;
        cassette.load_synthetic_tape(bits, cycles_per_bit);
        expect(cassette.tape_loaded(), "LoadSyntheticTape_TapeLoadedTrue");

        bool all_ok = true;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            // Sample mid-bit (avoids boundary-rounding ambiguity).
            clock.set_cycles(1000 + i * cycles_per_bit + cycles_per_bit / 2);
            if (cassette.cassette_input_high() != bits[i]) {
                all_ok = false;
            }
        }
        expect(all_ok, "SyntheticTape_BitSequence_MatchesAtExpectedCycleBoundaries");
    }

    // --- Past-end-of-tape reverts to idle-high, deterministically. ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        cassette.reset();
        FakeClock clock;
        cassette.attach_clock_source(&clock);
        clock.set_cycles(0);
        cassette.load_synthetic_tape({false, false}, 10);
        clock.set_cycles(1000);  // far past the 2-bit tape
        expect(cassette.cassette_input_high(), "PastEndOfTape_RevertsToIdleHigh");
    }

    // --- reset() ejects the tape (reverts to idle-high default). ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        FakeClock clock;
        cassette.attach_clock_source(&clock);
        clock.set_cycles(0);
        cassette.load_synthetic_tape({false, false, false}, 10);
        clock.set_cycles(5);
        expect(!cassette.cassette_input_high(), "TapeLoaded_ReadsLoadedBitBeforeReset");
        cassette.reset();
        expect(!cassette.tape_loaded(), "Reset_EjectsTape_TapeLoadedFalse");
        expect(cassette.cassette_input_high(), "Reset_EjectsTape_RevertsToIdleHigh");
    }

    // --- eject_tape() also reverts to idle-high without a full reset(). ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        FakeClock clock;
        cassette.attach_clock_source(&clock);
        clock.set_cycles(0);
        cassette.load_synthetic_tape({false}, 10);
        cassette.eject_tape();
        expect(!cassette.tape_loaded(), "EjectTape_TapeLoadedFalse");
        expect(cassette.cassette_input_high(), "EjectTape_RevertsToIdleHigh");
    }

    // --- sample_output() is an explicit, caller-driven utility --
    //     not a production hook; verify it records what the caller asks. ---
    {
        Ppi8255 ppi(slot_bus, kb);
        ppi.reset();
        CassetteInterface cassette(ppi);
        cassette.reset();
        ppi.io_write(0xAA, 0x20);  // motor on, output high
        cassette.sample_output(42);
        expect(cassette.samples().size() == 1, "SampleOutput_RecordsOneSample");
        expect(cassette.samples()[0].at_cycle == 42 && cassette.samples()[0].motor_on &&
                   cassette.samples()[0].output_level,
               "SampleOutput_RecordsExpectedValues");
        cassette.clear_samples();
        expect(cassette.samples().empty(), "ClearSamples_EmptiesLog");
    }

    // --- Two-run determinism: identical cycle sequence -> identical bit
    //     sequence. ---
    {
        auto run = [](std::uint64_t base_cycle) {
            SlotBus sb;
            KeyboardMatrix k;
            k.reset();
            Ppi8255 ppi(sb, k);
            ppi.reset();
            CassetteInterface cassette(ppi);
            cassette.reset();
            FakeClock clock;
            cassette.attach_clock_source(&clock);
            clock.set_cycles(base_cycle);
            cassette.load_synthetic_tape({true, false, true, false, true}, 7);
            std::vector<bool> observed;
            for (int i = 0; i < 10; ++i) {
                clock.set_cycles(base_cycle + static_cast<std::uint64_t>(i) * 7 + 1);
                observed.push_back(cassette.cassette_input_high());
            }
            return observed;
        };
        const std::vector<bool> a = run(500);
        const std::vector<bool> b = run(500);
        expect(a == b, "TwoRunDeterminism_IdenticalCycleSequence_IdenticalBits");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Peripherals_CassetteInterface_Unit cases passed\n";
    return 0;
}
