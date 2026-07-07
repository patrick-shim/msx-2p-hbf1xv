#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/cartridge_cli.h"
#include "machine/hbf1xv_machine.h"

namespace {

// M19-S5 cartridge loading (backlog B7). Shared by BOTH the default/normal
// run path and the existing --parity-trace mode (planner §2.4/§2.7 -- no
// duplicated parser/loader logic). Deliberately STRICTER than the
// RomAssetLoader BIOS/Kanji-font/disk-image policy (which is non-fatal,
// 0xFF-fill + a recorded diagnostic, rom_asset_loader.h:14-22): a
// user-specified --cartN is an explicit, one-off, this-run-only request, so
// an unreadable file OR any non-Ok CartridgeLoadResult prints a specific,
// loud diagnostic to stderr and this function returns a non-zero code --
// NEVER a silent fallback to "no cartridge." This policy lives ONLY here,
// scoped to the new cartridge_cli/load_cartridge call sites; it must never
// leak into RomAssetLoader's existing graceful-degradation call sites
// (Hbf1xvMachine::load_rom_assets).
int load_cartridges_from_args(sony_msx::machine::Hbf1xvMachine& machine, const std::vector<std::string>& args) {
    using sony_msx::devices::cartridge::CartridgeLoadResult;
    using sony_msx::devices::cartridge::to_string;
    using sony_msx::machine::ParsedCartridgeSlotCli;

    const sony_msx::machine::ParsedCartridgeCli parsed = sony_msx::machine::parse_cartridge_cli(args);
    for (const std::string& err : parsed.errors) {
        std::cerr << "cartridge: " << err << "\n";
    }
    if (!parsed.errors.empty()) {
        return 2;
    }

    auto load_one = [&](const int slot_number, const ParsedCartridgeSlotCli& spec) -> int {
        if (!spec.path.has_value()) {
            return 0;
        }
        std::ifstream in(*spec.path, std::ios::binary);
        if (!in) {
            std::cerr << "cartridge: cannot open --cart" << slot_number << " file: " << *spec.path << "\n";
            return 2;
        }
        const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                               std::istreambuf_iterator<char>());
        const CartridgeLoadResult result = machine.load_cartridge(slot_number, spec.type, image);
        if (result != CartridgeLoadResult::Ok) {
            std::cerr << "cartridge: failed to load --cart" << slot_number << " (" << *spec.path << ") as "
                       << to_string(spec.type) << ": ";
            switch (result) {
                case CartridgeLoadResult::ImageSizeInvalidForMapperType:
                    std::cerr << "image size is invalid for this mapper type\n";
                    break;
                case CartridgeLoadResult::InvalidSlotNumber:
                    std::cerr << "invalid slot number\n";
                    break;
                case CartridgeLoadResult::Ok:
                    break;
            }
            return 2;
        }
        std::cerr << "cartridge: --cart" << slot_number << " loaded (" << *spec.path << ", "
                   << to_string(spec.type) << ")\n";
        return 0;
    };

    if (const int rc = load_one(1, parsed.slot1); rc != 0) {
        return rc;
    }
    return load_one(2, parsed.slot2);
}

// M10-S4 parity-trace mode. Loads a flat RAM-only Z80 program at a fixed base,
// forces this emulator's cold_boot reset vector, sets PC to the base, enables
// the deterministic per-instruction trace-export (M10-S1), and single-steps
// until the CPU HALTs (or a bounded step ceiling is hit). The collected trace
// is written to the output path in the exact CpuTraceSink text format so it can
// be diffed line-for-line against the openMSX-side trace produced by
// tools/openmsx-trace-parity.ps1.
//
// `cli_args` is the full argv-derived argument vector (M19-S6): when it
// carries --cart1/--cart1-type (and/or --cart2/--cart2-type), the SAME
// parser/loader used by the default run path (load_cartridges_from_args)
// mounts the requested cartridge(s) right after cold_boot, before the driver
// program runs -- letting a Z80 driver program page a real mapper into a CPU
// page via #A8 and exercise it (planner §2.7). Absent any --cartN flag this
// is a no-op, so every pre-M19 parity-trace invocation is unchanged.
int run_parity_trace(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                     const std::string& out_path, const std::vector<std::string>& cli_args) {
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
    if (const int rc = load_cartridges_from_args(machine, cli_args); rc != 0) {
        return rc;
    }
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

// M21-S7 VDP RENDER parity mode (backlog D1/D5/D6/D7-display-path). Runs a
// flat RAM-only Z80 program (assembled by tools/gen-m21-vdp-render-probe.py)
// that writes VRAM + registers/palette via the SAME #98/#99/#9A ports
// run_vdp_parity uses, then emits: the R#0-R#27 control-register file, the
// 16-entry palette (raw 9-bit GRB, matching openMSX's own "VDP palette"
// SimpleDebuggable byte layout -- verified this cycle via a live WSL probe:
// 2 bytes/entry, value = byte[2i] | (byte[2i+1]<<8)), a physical VRAM block
// (the SAME cross-comparable raw-byte gate run_vdp_parity already uses), and
// the renderer's OWN computed RGB555 pixel values for the first
// <pixel_count> pixels of display line 0 (whatever mode the program left
// active) -- a derived-value reference this project's own engine actually
// produced, not a live cross-engine probe (openMSX exposes no
// computed-pixel-color debuggable; `debug list` was checked this cycle and
// no such debuggable exists -- see docs/m21-parity-trace-diff.md for the
// full, honest disposition).
int run_vdp_render_parity(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                          std::uint32_t vram_bytes, std::uint32_t pixel_count, const std::string& out_path) {
    std::ifstream in(bin_path, std::ios::binary);
    if (!in) {
        std::cerr << "vdp-render-parity: cannot open program: " << bin_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> program((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
    if (program.empty()) {
        std::cerr << "vdp-render-parity: empty program: " << bin_path << "\n";
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
    auto dec2 = [](int v) {
        std::string s;
        s.push_back(static_cast<char>('0' + (v / 10) % 10));
        s.push_back(static_cast<char>('0' + v % 10));
        return s;
    };

    const auto& vdp = machine.vdp();
    std::string out;
    out += "[VDP-RENDER-PARITY]\n";
    for (int r = 0; r <= 27; ++r) {
        out += "REG" + dec2(r) + "=" + hex2(vdp.control_register(r)) + "\n";
    }
    out += "PALETTE\n";
    for (int i = 0; i < 16; ++i) {
        const std::uint16_t entry = vdp.palette_entry(i);
        out += "PAL" + dec2(i) + "=" + hex2(entry & 0xFF) + hex2((entry >> 8) & 0xFF) + "\n";
    }
    out += "VRAM\n";
    for (std::uint32_t off = 0; off < vram_bytes; off += 16) {
        out += hex4(off);
        for (std::uint32_t i = 0; i < 16 && off + i < vram_bytes; ++i) {
            out += " " + hex2(vdp.vram().read(off + i));
        }
        out += "\n";
    }
    // A fixed extra 16-byte window at physical 0x10000 (the G6/G7 planar
    // "bank1" region, A-M21-10) -- always emitted regardless of vram_bytes,
    // so the D7 CPU-port planar-transform probe's odd-logical-address
    // writes (which land here) are visible without requiring an enormous
    // contiguous dump from address 0.
    out += "VRAM_BANK1\n";
    out += "10000";
    for (std::uint32_t i = 0; i < 16; ++i) {
        out += " " + hex2(vdp.vram().read(0x10000 + i));
    }
    out += "\n";

    const auto frame = machine.render_frame();
    out += "RENDER width=" + std::to_string(frame.width) + " height=" + std::to_string(frame.height) + "\n";
    out += "BORDER=" + hex4(frame.border_color) + "\n";
    out += "PIXELS\n";
    for (std::uint32_t i = 0; i < pixel_count && static_cast<int>(i) < frame.width; ++i) {
        out += "PX" + dec2(static_cast<int>(i)) + "=" + hex4(frame.at(static_cast<int>(i), 0)) + "\n";
    }

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        std::cerr << "vdp-render-parity: cannot write output: " << out_path << "\n";
        return 2;
    }
    out_file.write(out.data(), static_cast<std::streamsize>(out.size()));
    std::cerr << "vdp-render-parity: steps=" << steps
              << " halted=" << (machine.cpu().state().halted() ? 1 : 0) << "\n";
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

// M20-S4 Halnote/MSX-JE firmware openMSX A/B parity mode (backlog B4+B6).
// Cold-boots with real ROM assets from <bios_dir> (the real bios/f1xvfirm.rom,
// unmodified -- no synthetic swap needed, planner §2.6/A-M20 grounding: this
// local file's SHA1 was independently confirmed byte-identical to the real,
// installed WSL openMSX system ROM, tools/openmsx-m20-halnote-parity.ps1),
// routes the ENTIRE Halnote 64 KB window into view via the debug-harness
// technique (no CPU driver program needed -- Halnote's mem_read/mem_write are
// pure, combinational functions of the raw 16-bit address, planner §2.5), then
// exercises the identical write/read protocol sequence the openMSX-side Tcl
// script performs, dumping each read-back as a deterministic "LABEL=name
// VALUE=hex" line for cross-emulator diffing.
int run_halnote_parity(const std::string& bios_dir, const std::string& out_path) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    for (const std::string& note : machine.rom_diagnostics()) {
        std::cerr << "halnote-parity: " << note << "\n";
    }

    // Force the authentic reset default (#A8=0, every page primary 0), then
    // route ALL FOUR pages of primary 0 to secondary slot 3 (Halnote) via a
    // single #FFFF write (SlotBus::write_ffff targets
    // sub_slot_register_[primary_for_page(3)] == sub_slot_register_[0];
    // 0xFF = 0b11_11_11_11 -> every 2-bit page field independently decodes to
    // secondary 3).
    machine.debug_io_write(0xA8, 0x00);
    machine.debug_bus_write(0xFFFF, 0xFF);

    auto hex2 = [](std::uint32_t v) {
        static const char* d = "0123456789ABCDEF";
        std::string s;
        s.push_back(d[(v >> 4) & 0xF]);
        s.push_back(d[v & 0xF]);
        return s;
    };

    std::string out;
    out += "[HALNOTE-PARITY]\n";
    auto emit = [&](const char* label, const std::uint16_t address) {
        out += std::string("LABEL=") + label + " VALUE=" + hex2(machine.debug_bus_read(address)) + "\n";
    };
    auto poke = [&](const std::uint16_t address, const std::uint8_t value) {
        machine.debug_bus_write(address, value);
    };

    // Main bank-switch: bank(4) region 0x8000-0x9FFF, trigger 0x8FFF.
    poke(0x8FFF, 0x03);
    emit("BANK4_BASE", 0x8000);
    emit("BANK4_LAST", 0x9FFF);

    // bank(5) region 0xA000-0xBFFF, trigger 0xAFFF.
    poke(0xAFFF, 0x04);
    emit("BANK5_BASE", 0xA000);

    // bank(2) double-duty: bit7 set both enables SRAM AND sets the bank via
    // the mask fallback (0x85 & 0x7F = 5, since 0x85 >= 128 blocks).
    poke(0x4FFF, 0x85);
    emit("BANK2_BASE_DOUBLE_DUTY", 0x4000);

    // SRAM read/write (now enabled), both region boundaries.
    poke(0x0000, 0x5A);
    emit("SRAM_FIRST", 0x0000);
    poke(0x3FFF, 0xA5);
    emit("SRAM_LAST", 0x3FFF);

    // bank(3) double-duty: bit7 set enables the sub-mapper AND sets the bank.
    // 0x6000-0x6FFF is NEVER shadowed (outside the narrower 0x7000-0x7FFF
    // sub-mapper range), so both reads here must reflect the PLAIN window.
    poke(0x6FFF, 0x87);
    emit("BANK3_BASE_DOUBLE_DUTY", 0x6000);
    emit("BANK3_LAST_BEFORE_SHADOW", 0x6FFF);

    // Sub-bank registers: 0x77FF -> sub-bank 0 (shadows 0x7000-0x77FF);
    // 0x7FFF -> sub-bank 1 (shadows 0x7800-0x7FFF).
    poke(0x77FF, 0x05);
    emit("SUBBANK0_SHADOW", 0x7000);
    emit("SUBBANK0_SHADOW_LAST", 0x77FF);

    poke(0x7FFF, 0x0A);
    emit("SUBBANK1_SHADOW", 0x7800);
    emit("SUBBANK1_SHADOW_LAST", 0x7FFF);

    // Window-slots 6/7 (0xC000-0xFFFF) stay permanently 0xFF regardless of
    // any bank-switch/SRAM/sub-bank traffic above, and a write never takes.
    emit("UPPERQUARTER_BEFORE_WRITE", 0xC000);
    poke(0xC000, 0x77);
    emit("UPPERQUARTER_AFTER_WRITE", 0xC000);

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        std::cerr << "halnote-parity: cannot write output: " << out_path << "\n";
        return 2;
    }
    out_file.write(out.data(), static_cast<std::streamsize>(out.size()));
    std::cerr << "halnote-parity: wrote " << out_path << "\n";
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
    // Full argv (minus argv[0]) for the M19 cartridge CLI (--cart1/
    // --cart1-type/--cart2/--cart2-type), parsed order-independently
    // regardless of which mode (default run or --parity-trace) is active.
    const std::vector<std::string> args(argv + 1, argv + argc);

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

    if (argc >= 2 && std::string(argv[1]) == "--vdp-render-parity") {
        if (argc < 8) {
            std::cerr << "usage: " << argv[0]
                      << " --vdp-render-parity <program.bin> <base_hex> <max_steps> <vram_bytes> "
                         "<pixel_count> <out.txt>\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const auto vram_bytes = static_cast<std::uint32_t>(std::strtoul(argv[5], nullptr, 10));
        const auto pixel_count = static_cast<std::uint32_t>(std::strtoul(argv[6], nullptr, 10));
        const std::string out_path = argv[7];
        return run_vdp_render_parity(bin_path, base, max_steps, vram_bytes, pixel_count, out_path);
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

    if (argc >= 2 && std::string(argv[1]) == "--halnote-parity") {
        if (argc < 4) {
            std::cerr << "usage: " << argv[0] << " --halnote-parity <bios_dir> <out.txt>\n";
            return 2;
        }
        return run_halnote_parity(argv[2], argv[3]);
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
        return run_parity_trace(bin_path, base, max_steps, out_path, args);
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    if (const int rc = load_cartridges_from_args(machine, args); rc != 0) {
        return rc;
    }
    machine.run_frame();

    std::cout << "sony-msx-hbf1xv headless scaffold\n";
    std::cout << "elapsed_cycles=" << machine.elapsed_cycles() << "\n";
    std::cout << "frame_count=" << machine.frame_count() << "\n";
    std::cout << "frame_cycles_per_frame=" << machine.frame_cycles_per_frame() << "\n";

    return 0;
}
