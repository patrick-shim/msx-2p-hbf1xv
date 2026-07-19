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

// Suite: Machine_Hbf1xvScc_System
//
// End-to-end: a cold-booted machine with a SYNTHETIC KonamiSCC cartridge
// whose IN-IMAGE Z80 driver program (fetched and executed from the cartridge
// itself over the machine bus) banks, enables the SCC (0x3F at 0x9000), and
// writes a waveform/period/volume/enable set; the frame loop runs via
// step_cpu_instruction() + on_vsync_boundary() -- never run_frame(), which
// carries a double-count hazard here (DEC-0034); then the
// MachineAudioMixer pumps N samples and the PCM sequence matches a
// hand-computed oracle, byte-identical across two runs.
//
// No BIOS asset is needed: the machine cold-boots, page routing is
// established via the debug #A8 seam, and the CPU then runs ONLY cartridge
// code (deterministic with or without bios/ present -- the synthetic-fixture
// discipline).

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_rom_window.h"
#include "frontend/machine_audio_mixer.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::cartridge::CartridgeLoadResult;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeRomWindow;
using sony_msx::frontend::MachineAudioMixer;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// #A8: pages 1+2 -> primary slot 1 (the cartridge), pages 0+3 -> primary
// slot 3 (RAM: stack). SlotBus bit layout, 2 bits per page.
constexpr std::uint8_t kA8Slot1Pages12 = 0xD7;

// Builds the 4-bank synthetic KonamiSCC image. Bank 0 (mapped at CPU 0x4000
// after reset) carries the driver program at its start; banks are otherwise
// filled with their bank-number marker byte.
std::vector<std::uint8_t> make_scc_program_image() {
    std::vector<std::uint8_t> image(4 * CartridgeRomWindow::kBankSize);
    for (unsigned bank = 0; bank < 4; ++bank) {
        const auto marker = static_cast<std::uint8_t>(bank);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[bank * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }

    // The in-cart Z80 driver (assembled by hand; executes at 0x4000):
    //   LD A,0x02 / LD (0x7000),A   ; bank-switch page 3 -> bank 2
    //   LD A,0x3F / LD (0x9000),A   ; enable the SCC
    //   LD HL,0x9800 / LD B,0x20 / LD A,0x40
    //   loop: LD (HL),A / INC HL / DJNZ loop   ; ch1 wave: 32 x +64
    //   LD A,0x63 / LD (0x9880),A   ; ch1 period low  = 0x63 (99)
    //   XOR A     / LD (0x9881),A   ; ch1 period high = 0
    //   LD A,0x0F / LD (0x988A),A   ; ch1 volume 15
    //   LD A,0x01 / LD (0x988F),A   ; enable ch1
    //   HALT
    const std::uint8_t program[] = {
        0x3E, 0x02, 0x32, 0x00, 0x70,        // LD A,2 / LD (0x7000),A
        0x3E, 0x3F, 0x32, 0x00, 0x90,        // LD A,0x3F / LD (0x9000),A
        0x21, 0x00, 0x98,                    // LD HL,0x9800
        0x06, 0x20,                          // LD B,0x20
        0x3E, 0x40,                          // LD A,0x40
        0x77, 0x23, 0x10, 0xFC,              // LD (HL),A / INC HL / DJNZ -4
        0x3E, 0x63, 0x32, 0x80, 0x98,        // LD A,0x63 / LD (0x9880),A
        0xAF, 0x32, 0x81, 0x98,              // XOR A / LD (0x9881),A
        0x3E, 0x0F, 0x32, 0x8A, 0x98,        // LD A,0x0F / LD (0x988A),A
        0x3E, 0x01, 0x32, 0x8F, 0x98,        // LD A,0x01 / LD (0x988F),A
        0x76,                                // HALT
    };
    for (std::size_t i = 0; i < sizeof(program); ++i) {
        image[i] = program[i];
    }
    return image;
}

struct RunResult {
    std::vector<std::int16_t> pcm;
    bool halted = false;
    bool scc_enabled = false;
    std::uint8_t wave0 = 0;
    std::uint8_t page3_marker = 0;
    std::uint64_t frames = 0;
};

RunResult run_session() {
    RunResult result;

    Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();
    const CartridgeLoadResult load = machine.load_cartridge(1, CartridgeMapperType::KonamiSCC,
                                                             make_scc_program_image());
    if (load != CartridgeLoadResult::Ok) {
        return result;
    }

    // Route pages 1/2 at the cartridge, point the CPU at the in-cart driver.
    machine.debug_io_write(0xA8, kA8Slot1Pages12);
    machine.cpu().state().regs().pc = 0x4000;
    machine.cpu().state().regs().sp = 0xF000;  // page 3 RAM

    // The proven frame loop shape: step_cpu_instruction() to each frame
    // boundary, then on_vsync_boundary() -- never run_frame() (DEC-0034).
    for (int frame = 0; frame < 3; ++frame) {
        const std::uint64_t frame_start = machine.elapsed_cycles();
        const std::uint64_t target = machine.frame_cycles_per_frame();
        while (machine.elapsed_cycles() - frame_start < target) {
            machine.step_cpu_instruction();
        }
        machine.on_vsync_boundary();
        ++result.frames;
    }

    result.halted = machine.cpu().state().halted();
    result.page3_marker = machine.debug_bus_read(0x6000);
    auto* scc = machine.scc_chip(1);
    if (scc == nullptr) {
        return result;
    }
    result.scc_enabled = true;
    result.wave0 = static_cast<std::uint8_t>(scc->wave(0, 0));

    // Pump N samples through the REAL mixer (PSG left at its silent
    // cold-boot state; the SCC's held output is (64*15)>>4 = 60).
    const MachineAudioMixer mixer(81);
    result.pcm =
        mixer.mix_interleaved_stereo(machine.psg(), MachineAudioMixer::SccSources{scc, nullptr}, 512);
    return result;
}

}  // namespace

int main() {
    const RunResult first = run_session();

    expect(first.frames == 3, "Session_RanThreeFrames");
    expect(first.halted, "InCartDriver_ReachesHalt");
    expect(first.page3_marker == 2, "InCartDriver_BankSwitchedPage3ToBank2");
    expect(first.scc_enabled, "InCartDriver_EnabledScc_AccessorNonNull");
    expect(first.wave0 == 0x40, "InCartDriver_WaveformLandedInChip");

    // Hand-computed PCM oracle: PSG raw 0 (cold-boot amplitudes 0) + SCC
    // held at (0x40 * 15) >> 4 = 60 -> every value 0*400 + 60*12 = 720 on
    // both channels, for all 512 samples (uniform waveform: position steps
    // never change the held byte).
    bool all_720 = first.pcm.size() == 1024;
    for (const std::int16_t v : first.pcm) {
        if (v != 720) {
            all_720 = false;
        }
    }
    expect(all_720, "PcmSequence_MatchesHandComputedOracle_720Everywhere");

    // Byte-identical across two fully independent runs (determinism).
    const RunResult second = run_session();
    expect(first.pcm == second.pcm, "TwoIndependentRuns_ByteIdenticalPcm");
    expect(second.halted == first.halted && second.wave0 == first.wave0,
           "TwoIndependentRuns_IdenticalMachineOutcome");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvScc_System cases passed\n";
    return 0;
}
