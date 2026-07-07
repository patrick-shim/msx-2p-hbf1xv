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

// M14-S6 VDP parity mode. Runs a flat RAM-only Z80 program (a CPU->VDP driver
// that writes control registers, fills a VRAM block via #98 auto-increment, and
// exercises palette + #9B indirect + the read-ahead path) exactly as the
// parity-trace mode does, then emits a canonical, deterministic dump of the
// externally comparable VDP architectural state: the physical VRAM block, the
// 14-bit VRAM pointer, R#14, and the control-register file. The SAME program
// runs on openMSX's genuine V9958 (tools/openmsx-vdp-parity.ps1) and the two
// dumps are diffed. VRAM is now comparable (it was excluded in M13's diff).
int run_vdp_parity(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                   std::uint32_t vram_bytes, const std::string& out_path) {
    std::ifstream in(bin_path, std::ios::binary);
    if (!in) {
        std::cerr << "vdp-parity: cannot open program: " << bin_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> program((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
    if (program.empty()) {
        std::cerr << "vdp-parity: empty program: " << bin_path << "\n";
        return 2;
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();
    machine.load_memory(base, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = base;

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        ++steps;
    }

    auto hex2 = [](std::uint32_t v) {
        static const char* d = "0123456789ABCDEF";
        std::string s;
        s.push_back(d[(v >> 4) & 0xF]);
        s.push_back(d[v & 0xF]);
        return s;
    };
    auto hex4 = [&](std::uint32_t v) { return hex2((v >> 8) & 0xFF) + hex2(v & 0xFF); };

    const auto& vdp = machine.vdp();
    std::string out;
    out += "[VDP-PARITY]\n";
    out += "VRAMPTR=" + hex4(vdp.vram_pointer()) + "\n";
    out += "R14=" + hex2(vdp.control_register(14)) + "\n";
    // Control-register file R#0..R#27 (decimal-labeled). Only the registers the
    // driver program EXPLICITLY writes are cross-comparable with openMSX (whose
    // BIOS pre-sets the rest); the harness gates on that subset + VRAM.
    auto dec2 = [](int v) {
        std::string s;
        s.push_back(static_cast<char>('0' + (v / 10) % 10));
        s.push_back(static_cast<char>('0' + v % 10));
        return s;
    };
    for (int r = 0; r <= 27; ++r) {
        out += "REG" + dec2(r) + "=" + hex2(vdp.control_register(r)) + "\n";
    }
    // Physical VRAM block (the pass/fail gate: a genuine VRAM read-back).
    out += "VRAM\n";
    for (std::uint32_t off = 0; off < vram_bytes; off += 16) {
        out += hex4(off);
        for (std::uint32_t i = 0; i < 16 && off + i < vram_bytes; ++i) {
            out += " " + hex2(vdp.vram().read(off + i));
        }
        out += "\n";
    }

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        std::cerr << "vdp-parity: cannot write output: " << out_path << "\n";
        return 2;
    }
    out_file.write(out.data(), static_cast<std::streamsize>(out.size()));
    std::cerr << "vdp-parity: steps=" << steps
              << " halted=" << (machine.cpu().state().halted() ? 1 : 0)
              << " vramptr=" << std::hex << vdp.vram_pointer() << std::dec << "\n";
    return 0;
}

// M17-S5 YM2413 (OPLL) register-parity mode. Runs a flat RAM-only Z80 program
// (the SAME `OUT (#7C),reg ; OUT (#7D),value` write sequence assembled by
// tools/gen-m17-ym2413-probe.py) exactly as run_parity_trace does, then emits
// a deterministic dump of the resulting YM2413 register file (all 64 bytes,
// $00-$3F) via the debug-only `register_value(addr)` accessor (A-M17-6). The
// SAME program runs on openMSX's genuine YM2413 (tools/openmsx-ym2413-parity.
// ps1), which reads the equivalent bytes via its "MSX Music regs"
// SimpleDebuggable (references/openmsx-21.0/src/sound/YM2413.hh:40-44); the
// two dumps are diffed per-address.
int run_ym2413_parity(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                      const std::string& out_path) {
    std::ifstream in(bin_path, std::ios::binary);
    if (!in) {
        std::cerr << "ym2413-parity: cannot open program: " << bin_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> program((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
    if (program.empty()) {
        std::cerr << "ym2413-parity: empty program: " << bin_path << "\n";
        return 2;
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();
    machine.load_memory(base, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = base;

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        ++steps;
    }

    auto hex2 = [](std::uint32_t v) {
        static const char* d = "0123456789ABCDEF";
        std::string s;
        s.push_back(d[(v >> 4) & 0xF]);
        s.push_back(d[v & 0xF]);
        return s;
    };

    const auto& ym = machine.ym2413();
    std::string out;
    out += "[YM2413-PARITY]\n";
    for (int addr = 0; addr < 0x40; ++addr) {
        out += "REG addr=" + hex2(static_cast<std::uint32_t>(addr)) +
               " value=" + hex2(ym.register_value(static_cast<std::uint8_t>(addr))) + "\n";
    }

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        std::cerr << "ym2413-parity: cannot write output: " << out_path << "\n";
        return 2;
    }
    out_file.write(out.data(), static_cast<std::streamsize>(out.size()));
    std::cerr << "ym2413-parity: steps=" << steps
              << " halted=" << (machine.cpu().state().halted() ? 1 : 0) << "\n";
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

    if (argc >= 2 && std::string(argv[1]) == "--vdp-parity") {
        if (argc < 7) {
            std::cerr << "usage: " << argv[0]
                      << " --vdp-parity <program.bin> <base_hex> <max_steps> <vram_bytes> <out.txt>\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const auto vram_bytes = static_cast<std::uint32_t>(std::strtoul(argv[5], nullptr, 10));
        const std::string out_path = argv[6];
        return run_vdp_parity(bin_path, base, max_steps, vram_bytes, out_path);
    }

    if (argc >= 2 && std::string(argv[1]) == "--ym2413-parity") {
        if (argc < 6) {
            std::cerr << "usage: " << argv[0]
                      << " --ym2413-parity <program.bin> <base_hex> <max_steps> <out.txt>\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const std::string out_path = argv[5];
        return run_ym2413_parity(bin_path, base, max_steps, out_path);
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
