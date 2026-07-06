#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/hbf1xv_machine.h"

namespace {

// M10-S4 parity-trace mode. Loads a flat RAM-only Z80 program at a fixed base,
// forces this emulator's cold_boot reset vector, sets PC to the base, enables
// the deterministic per-instruction trace-export (M10-S1), and single-steps
// until the CPU HALTs (or a bounded step ceiling is hit). The collected trace
// is written to the output path in the exact CpuTraceSink text format so it can
// be diffed line-for-line against the openMSX-side trace produced by
// tools/openmsx-trace-parity.ps1.
int run_parity_trace(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                     const std::string& out_path) {
    std::ifstream in(bin_path, std::ios::binary);
    if (!in) {
        std::cerr << "parity-trace: cannot open program: " << bin_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> program((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
    if (program.empty()) {
        std::cerr << "parity-trace: empty program: " << bin_path << "\n";
        return 2;
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();  // AF=BC=DE=HL=0, SP=FFFF, PC=0, I=R=0, IFF1=IFF2=0, IM1.
    // Authentic reset boots slot-0 BIOS (M13-S4 #A8=0). The parity harness runs a
    // flat RAM-only program, so page the 64 KB mapper RAM into all four pages.
    machine.map_flat_ram();
    machine.load_memory(base, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = base;
    machine.set_cpu_trace_enabled(true);

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        ++steps;
    }

    const std::string trace = machine.cpu_trace().serialize();
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "parity-trace: cannot write output: " << out_path << "\n";
        return 2;
    }
    out.write(trace.data(), static_cast<std::streamsize>(trace.size()));
    if (!out) {
        std::cerr << "parity-trace: write failed: " << out_path << "\n";
        return 2;
    }

    std::cerr << "parity-trace: steps=" << steps
              << " halted=" << (machine.cpu().state().halted() ? 1 : 0)
              << " final_pc=" << std::hex << machine.cpu().state().regs().pc << std::dec << "\n";
    return 0;
}

}  // namespace

// M13-S5 BIOS-boot trace mode. Cold-boots from the authentic reset (#A8 = 0,
// PC = 0x0000, slot-0 BIOS) with the ROM assets loaded from <bios_dir>, then
// single-steps up to <max_steps> instructions, exporting the deterministic
// per-instruction trace so it can be diffed against openMSX's reset-step trace.
// No map_flat_ram: this is the REAL BIOS execution from slot 0.
int run_bios_boot_trace(const std::string& bios_dir, std::uint32_t max_steps,
                        const std::string& out_path) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    for (const std::string& note : machine.rom_diagnostics()) {
        std::cerr << "bios-boot-trace: " << note << "\n";
    }
    machine.set_cpu_trace_enabled(true);

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        ++steps;
    }

    const std::string trace = machine.cpu_trace().serialize();
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "bios-boot-trace: cannot write output: " << out_path << "\n";
        return 2;
    }
    out.write(trace.data(), static_cast<std::streamsize>(trace.size()));
    std::cerr << "bios-boot-trace: steps=" << steps
              << " final_pc=" << std::hex << machine.cpu().state().regs().pc << std::dec << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--bios-boot-trace") {
        if (argc < 5) {
            std::cerr << "usage: " << argv[0]
                      << " --bios-boot-trace <bios_dir> <max_steps> <out.txt>\n";
            return 2;
        }
        return run_bios_boot_trace(argv[2], static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10)),
                                   argv[4]);
    }

    if (argc >= 2 && std::string(argv[1]) == "--parity-trace") {
        if (argc < 6) {
            std::cerr << "usage: " << argv[0]
                      << " --parity-trace <program.bin> <base_hex> <max_steps> <out.txt>\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const std::string out_path = argv[5];
        return run_parity_trace(bin_path, base, max_steps, out_path);
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.run_frame();

    std::cout << "sony-msx-hbf1xv headless scaffold\n";
    std::cout << "elapsed_cycles=" << machine.elapsed_cycles() << "\n";
    std::cout << "frame_count=" << machine.frame_count() << "\n";
    std::cout << "frame_cycles_per_frame=" << machine.frame_cycles_per_frame() << "\n";

    return 0;
}
