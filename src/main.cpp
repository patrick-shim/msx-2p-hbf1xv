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
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "frontend/psg_audio_dump.h"
#include "machine/cartridge_cli.h"
#include "machine/cartridge_identifier.h"
#include "machine/cpm_bdos_harness.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

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

    // M30 (backlog G2): auto-identification for type-less --cartN requests
    // via the ONE shared resolver (cartridge_identifier.h, also consumed by
    // the SDL3 frontend). Explicit --cartN-type specs pass through the
    // session untouched -- zero new output, byte-for-byte the pre-M30
    // behavior. Identified-but-unsupported -> loud message + exit 2 (the
    // existing error-path convention).
    sony_msx::machine::CartridgeIdentificationSession ident_session(parsed.softwaredb_path);

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
        const auto resolution = ident_session.resolve(slot_number, spec, image);
        for (const std::string& message : resolution.messages) {
            std::cerr << message << "\n";
        }
        if (!resolution.ok) {
            return 2;
        }
        const auto type = resolution.type;
        const CartridgeLoadResult result = machine.load_cartridge(slot_number, type, image);
        if (result != CartridgeLoadResult::Ok) {
            std::cerr << "cartridge: failed to load --cart" << slot_number << " (" << *spec.path << ") as "
                       << to_string(type) << ": ";
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
                   << to_string(type) << ")\n";
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
// `halt_idle_extra_steps` (M23-S3, backlog C2 A/B evidence; mirrors the M19
// --cart1/--cart2 precedent of extending THIS SAME mode additively for a new
// milestone's specific A/B need, planner §2.4/§2.7): OPTIONAL, defaults to 0,
// which preserves the exact pre-M23 behavior byte-for-byte (the main loop
// below is completely unchanged -- it still stops stepping exactly at the
// halt boundary, per the established, safe "stop at halt" convention every
// other call site in this project uses, tests/CLAUDE.md). When > 0 AND the
// CPU is halted after that unchanged loop, this steps `halt_idle_extra_steps`
// MORE times while already halted -- the ONLY way to observe the M23-S1
// HALT-R phantom-M1-refetch behavior (R incrementing once per halted idle
// step) through the existing trace-export mechanism, which records every
// step() call unconditionally regardless of halted state.
int run_parity_trace(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                     const std::string& out_path, const std::vector<std::string>& cli_args,
                     std::uint32_t halt_idle_extra_steps = 0) {
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

    // M23-S3 addition (see the halt_idle_extra_steps doc comment above): only
    // engages when the caller explicitly asks for it AND the CPU is actually
    // halted; otherwise this is a complete no-op (byte-identical pre-M23
    // behavior for every existing invocation).
    if (halt_idle_extra_steps > 0 && machine.cpu().state().halted()) {
        for (std::uint32_t i = 0; i < halt_idle_extra_steps; ++i) {
            machine.step_cpu_instruction();
            ++steps;
        }
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

// M22-S8 sprite/command-engine A/B parity mode (backlog D2/D3, closes D7).
// Runs a flat RAM-only Z80 program (the SAME OUT (#98)/(#99)/(#9B) port
// sequence a real CPU would use to set up sprites and/or drive the R#32-46
// command engine, assembled by tools/gen-m22-sprite-cmd-probe.py) exactly as
// run_vdp_render_parity does, then emits: the FULL R#0-R#46 control +
// command-engine register file (openMSX's own "VDP regs" SimpleDebuggable is
// size 64, confirmed this cycle via a live WSL `debug size` query, so R#32-46
// are already part of the SAME debuggable range -- no separate probe point
// needed), the S#0-S#9 status-register file read NON-destructively (the
// CPU program's OWN `IN (#99)` instructions already exercised any real
// destructive read side effects; this dump captures the settled state
// afterward -- confirmed comparable via openMSX's "VDP status regs"
// SimpleDebuggable, size 16, also confirmed this cycle), and a physical VRAM
// window (matching run_vdp_render_parity's own vram_bytes + fixed 0x10000
// "bank1" window precedent, since the SAME "physical VRAM" SimpleDebuggable
// is reused unchanged).
int run_sprite_cmd_parity(const std::string& bin_path, std::uint16_t base, std::uint32_t max_steps,
                          std::uint32_t vram_bytes, const std::string& out_path) {
    std::ifstream in(bin_path, std::ios::binary);
    if (!in) {
        std::cerr << "sprite-cmd-parity: cannot open program: " << bin_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> program((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());
    if (program.empty()) {
        std::cerr << "sprite-cmd-parity: empty program: " << bin_path << "\n";
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

    // Trigger the sprite-check recompute (S#0/S#3-S#6) explicitly: this
    // project's SpriteEngine is driven ONLY by the on_vsync() frame-boundary
    // hook (no CPU-visible port triggers it, deliberately -- no new clock
    // consumer, planner package §1.4 Resolution 1), unlike openMSX's own
    // raster-time-driven SpriteChecker::checkUntil(), which advances "for
    // free" as real emulated time elapses. The companion PS1 script
    // correspondingly waits enough real emulated time (`after time`) after
    // the CPU program's writes for openMSX's OWN check to complete
    // naturally -- this call is this engine's architectural equivalent, not
    // a workaround.
    machine.vdp().on_vsync();

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
    out += "[SPRITE-CMD-PARITY]\n";
    for (int r = 0; r <= 27; ++r) {
        out += "REG" + dec2(r) + "=" + hex2(vdp.control_register(r)) + "\n";
    }
    for (int r = 0; r < 15; ++r) {
        out += "REG" + dec2(32 + r) + "=" + hex2(vdp.cmd_engine().read_register(r)) + "\n";
    }
    out += "STATUS\n";
    for (int s = 0; s <= 9; ++s) {
        out += "S" + dec2(s) + "=" + hex2(vdp.peek_status_register(s)) + "\n";
    }
    out += "VRAM\n";
    for (std::uint32_t off = 0; off < vram_bytes; off += 16) {
        out += hex4(off);
        for (std::uint32_t i = 0; i < 16 && off + i < vram_bytes; ++i) {
            out += " " + hex2(vdp.vram().read(off + i));
        }
        out += "\n";
    }
    // Fixed extra window at physical 0x10000 (the G6/G7 planar "bank1"
    // region), matching run_vdp_render_parity's own precedent.
    out += "VRAM_BANK1\n";
    out += "10000";
    for (std::uint32_t i = 0; i < 16; ++i) {
        out += " " + hex2(vdp.vram().read(0x10000 + i));
    }
    out += "\n";

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        std::cerr << "sprite-cmd-parity: cannot write output: " << out_path << "\n";
        return 2;
    }
    out_file.write(out.data(), static_cast<std::streamsize>(out.size()));
    std::cerr << "sprite-cmd-parity: steps=" << steps
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

// M26-S4 decoded-FrameBuffer-dump evidence generator (backlog C9, the ONE new
// debug/testing capability this milestone authorizes -- docs/m26-planner-
// package.md §2.5 point 3). Drives a colorful, deterministic GRAPHIC4
// (SCREEN5) test scene directly through the real #98/#99/#9A VDP port
// protocol via the existing, non-perturbing debug_io_write() seam (M13) --
// NO CPU driver program needed (mirrors tools/gen-m21-vdp-render-probe.py's
// own port-write sequences, independently re-expressed here in C++ rather
// than assembled Z80). Vertical color bars spanning all 16 palette entries,
// with a vivid, hand-chosen 16-entry palette -- a real, recognizable,
// deterministic picture, not a blank/near-blank boot screen. Calls
// machine.write_frame_dump() to produce the committed evidence pipeline's
// raw dump; tools/frame-to-png.py converts it to the final committed PNG.
int run_frame_dump_demo(const std::string& out_path) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();

    auto set_register = [&](const int reg, const std::uint8_t value) {
        machine.debug_io_write(0x99, value);
        machine.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
    };
    auto set_write_address = [&](const std::uint16_t addr) {
        machine.debug_io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
        machine.debug_io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
    };
    auto vram_write = [&](const std::uint8_t value) { machine.debug_io_write(0x98, value); };
    auto palette_write = [&](const int r3, const int g3, const int b3) {
        machine.debug_io_write(0x9A, static_cast<std::uint8_t>(((r3 & 7) << 4) | (b3 & 7)));
        machine.debug_io_write(0x9A, static_cast<std::uint8_t>(g3 & 7));
    };

    // GRAPHIC4 mode bits (M3=1,M4=1; R#1 carries no mode bits here, so
    // M1=M2=0 -> mode base 0x0C, VdpMode::Graphic4 -- devices/video/
    // vdp_mode.h's independently re-derived base-byte formula).
    set_register(0, 0x06);
    // M34 (DEC-0043 Defect B): R#1 bit6 BL=1 -- the render gate blanks BL=0
    // lines (as real hardware does), so a demo scene modelling a DISPLAYED
    // screen must enable the display like every real program does. With
    // BL=1 the scene renders byte-identically to the pre-M34 demo output
    // (bit6 previously had no background-render effect).
    set_register(1, 0x40);

    // A vivid, hand-chosen 16-entry palette (3-bit R/G/B each), entry 0 kept
    // black (the conventional MSX2 background index).
    const int palette_rgb[16][3] = {
        {0, 0, 0}, {7, 0, 0}, {0, 7, 0}, {0, 0, 7}, {7, 7, 0}, {7, 0, 7}, {0, 7, 7}, {7, 7, 7},
        {7, 3, 0}, {3, 7, 0}, {0, 7, 3}, {0, 3, 7}, {3, 0, 7}, {7, 0, 3}, {4, 4, 4}, {2, 2, 2},
    };
    set_register(16, 0x00);  // R#16 palette pointer -> entry 0
    for (const auto& c : palette_rgb) {
        palette_write(c[0], c[1], c[2]);
    }

    // 256x212 GRAPHIC4 canvas: 128 bytes/row (4bpp packed, 2px/byte, high
    // nibble first -- render_graphic4()'s own documented convention),
    // 16 vertical color bars of 16px each (8 bytes/bar; 8*16=128 matches the
    // row length exactly), identical on every row.
    set_write_address(0x0000);
    for (int row = 0; row < 212; ++row) {
        for (int byte_index = 0; byte_index < 128; ++byte_index) {
            const std::uint8_t bar = static_cast<std::uint8_t>((byte_index / 8) & 0x0F);
            vram_write(static_cast<std::uint8_t>((bar << 4) | bar));
        }
    }

    const bool ok = machine.write_frame_dump(out_path);
    if (!ok) {
        std::cerr << "frame-dump-demo: failed to write " << out_path << "\n";
        return 2;
    }
    std::cerr << "frame-dump-demo: wrote " << machine.debug_root().string() << "/frames/" << out_path << "\n";
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

// M27-S1..S3/S7 "--debug-session" mode (docs/m27-planner-package.md §2.2,
// items 1/3/4). A wholly additive new mode -- never touching the
// pre-existing default run path below main()'s own "sony-msx-hbf1xv headless
// scaffold" block, unchanged byte-for-byte (R-M27-1). Resolves A-M27-1/
// A-M27-2 (the default path never drives the CPU nor loads real BIOS
// assets): this mode loads real ROM assets from <bios_dir> AND drives the
// CPU for a bounded, real, halt-respecting step_cpu_instruction() loop --
// mirroring run_bios_boot_trace's own "stop stepping exactly at the halt
// boundary" loop shape exactly (tests/CLAUDE.md's established convention),
// zero new CPU-stepping semantics invented.
struct DebugSessionOptions {
    // M35-S1: repeatable --disk list (AC-8: headless supports the same
    // repeatable list mechanism as the SDL3 frontend).
    std::vector<std::string> disk_paths;
    std::optional<std::string> debug_root;
    std::optional<std::string> dump_state_name;
    std::optional<std::string> trace_cpu_name;
    std::optional<std::string> event_log_name;
    std::optional<std::string> input_script_path;
    // M32 closure additions (QA condition, docs/m32-qa-signoff.md: committed
    // evidence must carry RECORDED, re-runnable recipes; consumed by
    // tools/capture-metalgear-evidence.ps1). Both additive; absent flags
    // leave every pre-M32 invocation byte-identical.
    //   --frames <N>: drive N frames via the REAL frame-loop shape
    //     (step_cpu_instruction() to each frame boundary, then
    //     on_vsync_boundary() -- the Sdl3App::run_one_frame()/C5-closure
    //     shape) INSTEAD of the step-only loop. The positional <max_steps>
    //     argument is ignored in this mode (the frame count is the budget),
    //     and the loop deliberately does NOT stop on HALT -- real titles
    //     HALT-wait for the VBlank interrupt every frame (the DEC-0034
    //     loop-shape finding).
    //   --dump-frame <name>: write_frame_dump(<name>) after the run
    //     (to <debug_root>/frames/<name>).
    std::optional<std::uint32_t> frames;
    std::optional<std::string> dump_frame_name;
    // M36-S-c: opt-in host-file disk-save persistence. Default false =
    // in-memory-only (byte-for-byte the pre-M36 behavior; never clobbers a
    // real .dsk). When set, each mounted disk gets its host path bound and a
    // dirty image is flushed at end-of-run (and before a scripted swap).
    bool disk_writable = false;
    // M36 deterministic repro/testing enabler: swap to the NEXT disk in the
    // repeatable --disk list at this frame (frame-loop mode only). Reuses the
    // M35 multi-disk cache + swap semantics headlessly so a two-disk game
    // (e.g. YS II's "INSERT DATADISK" prompt) can be driven deterministically
    // without the SDL3 window / F11.
    std::optional<std::uint32_t> swap_disk_frame;
    // M36-S-d: FM-PAC peripheral cartridge battery-SRAM .sram persistence.
    // When set, the machine loads this .sram on FM-PAC insertion (absent file
    // -> deterministic zero state) and flushes it back at end-of-run.
    std::optional<std::string> fmpac_sram_path;
    // M36 Phase 3 (DEC-0051): comprehensive debug snapshot. --snapshot <dir>
    // enables the capture and sets the output root (<dir>/snapshot/<id>/);
    // --snapshot-frame <N> (frame-loop mode) captures at that specific frame
    // for a deterministic mid-run capture (default: capture once at end-of-run).
    // Additive + default-off: absent flags leave every prior invocation
    // byte-for-byte unchanged (no snapshot dir created, no file written).
    std::optional<std::string> snapshot_dir;
    std::optional<std::uint32_t> snapshot_frame;
};

std::optional<std::string> take_debug_session_value(const std::vector<std::string>& args, const std::size_t i,
                                                     const char* flag_name, std::vector<std::string>& errors) {
    if (i + 1 >= args.size()) {
        errors.push_back(std::string("debug-session: ") + flag_name + " requires a value argument");
        return std::nullopt;
    }
    return args[i + 1];
}

// Pure argv parser for --debug-session's OWN optional flags (order-
// independent scanning, mirrors sdl3_cli.cpp's parse_sdl3_cli() precedent
// exactly). Cartridge flags (--cart1/--cart1-type/--cart2/--cart2-type) are
// intentionally NOT recognized here -- they fall through untouched, for
// load_cartridges_from_args()'s own delegated parser (never reimplemented).
DebugSessionOptions parse_debug_session_options(const std::vector<std::string>& args,
                                                std::vector<std::string>& errors) {
    DebugSessionOptions opts;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--disk") {
            if (auto v = take_debug_session_value(args, i, "--disk", errors)) {
                opts.disk_paths.push_back(*v);  // M35-S1: accumulate, not overwrite
                ++i;
            }
        } else if (arg == "--debug-root") {
            if (auto v = take_debug_session_value(args, i, "--debug-root", errors)) {
                opts.debug_root = *v;
                ++i;
            }
        } else if (arg == "--dump-state") {
            if (auto v = take_debug_session_value(args, i, "--dump-state", errors)) {
                opts.dump_state_name = *v;
                ++i;
            }
        } else if (arg == "--trace-cpu") {
            if (auto v = take_debug_session_value(args, i, "--trace-cpu", errors)) {
                opts.trace_cpu_name = *v;
                ++i;
            }
        } else if (arg == "--event-log") {
            if (auto v = take_debug_session_value(args, i, "--event-log", errors)) {
                opts.event_log_name = *v;
                ++i;
            }
        } else if (arg == "--input-script") {
            if (auto v = take_debug_session_value(args, i, "--input-script", errors)) {
                opts.input_script_path = *v;
                ++i;
            }
        } else if (arg == "--frames") {
            if (auto v = take_debug_session_value(args, i, "--frames", errors)) {
                opts.frames = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--dump-frame") {
            if (auto v = take_debug_session_value(args, i, "--dump-frame", errors)) {
                opts.dump_frame_name = *v;
                ++i;
            }
        } else if (arg == "--disk-writable") {
            opts.disk_writable = true;  // boolean flag, no value
        } else if (arg == "--swap-disk-frame") {
            if (auto v = take_debug_session_value(args, i, "--swap-disk-frame", errors)) {
                opts.swap_disk_frame = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--fmpac-sram") {
            if (auto v = take_debug_session_value(args, i, "--fmpac-sram", errors)) {
                opts.fmpac_sram_path = *v;
                ++i;
            }
        } else if (arg == "--snapshot") {
            if (auto v = take_debug_session_value(args, i, "--snapshot", errors)) {
                opts.snapshot_dir = *v;
                ++i;
            }
        } else if (arg == "--snapshot-frame") {
            if (auto v = take_debug_session_value(args, i, "--snapshot-frame", errors)) {
                opts.snapshot_frame = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        }
        // Any other argument (--cart1/--cart1-type/--cart2/--cart2-type) is
        // left untouched for load_cartridges_from_args()'s own delegated
        // parser below.
    }
    return opts;
}

int run_debug_session(const std::string& bios_dir, const std::uint32_t max_steps,
                      const std::vector<std::string>& cli_args) {
    std::vector<std::string> errors;
    const DebugSessionOptions opts = parse_debug_session_options(cli_args, errors);
    for (const std::string& err : errors) {
        std::cerr << "debug-session: " << err << "\n";
    }
    if (!errors.empty()) {
        return 2;
    }

    sony_msx::machine::Hbf1xvMachine machine;
    if (opts.debug_root.has_value()) {
        machine.set_debug_root(*opts.debug_root);
    }
    // M36 Phase 3: --snapshot <dir> sets the snapshot output root; the capture
    // lands under <dir>/snapshot/<id>/. Takes precedence over --debug-root for a
    // dedicated snapshot run.
    if (opts.snapshot_dir.has_value()) {
        machine.set_debug_root(*opts.snapshot_dir);
    }
    // R-M27-2 (a real, easy-to-get-wrong sequencing constraint): event
    // logging MUST be enabled BEFORE cold_boot() to capture the Reset event
    // (hbf1xv_machine.h:306-309's own documented ordering requirement).
    if (opts.event_log_name.has_value()) {
        machine.set_event_logging_enabled(true);
    }
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    for (const std::string& note : machine.rom_diagnostics()) {
        std::cerr << "debug-session: " << note << "\n";
    }

    // M36-S-d: bind the FM-PAC .sram path BEFORE loading cartridges, so a
    // freshly-inserted FM-PAC loads its battery SRAM on insertion.
    if (opts.fmpac_sram_path.has_value()) {
        machine.set_fmpac_sram_path(*opts.fmpac_sram_path);
    }

    if (const int rc = load_cartridges_from_args(machine, cli_args); rc != 0) {
        return rc;
    }

    // A-M27-3 (headless previously had no --disk flag; SDL3 has one, M26):
    // M35-S2: mirrors Sdl3App::load_configured_assets() with repeatable
    // disk list. M36: cache ALL disks in memory (deterministic, no runtime
    // I/O) so a scripted --swap-disk-frame can rotate them exactly like the
    // SDL3 F11 path. Load the first disk at boot (AC-S2-1); no disk if empty
    // (AC-S2-3).
    std::vector<std::vector<std::uint8_t>> disk_cache;
    std::size_t current_disk_index = 0;
    for (const std::string& disk_path : opts.disk_paths) {
        std::ifstream in(disk_path, std::ios::binary);
        if (!in) {
            std::cerr << "debug-session: cannot open --disk file: " << disk_path << "\n";
            return 2;
        }
        disk_cache.emplace_back((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    if (!disk_cache.empty()) {
        machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk_cache[0]);
        // M36-S-c: opt-in host-file write-back (default false -> no host path,
        // flush() a no-op, byte-for-byte pre-M36 behavior).
        if (opts.disk_writable) {
            machine.disk_image().set_host_path(opts.disk_paths[0]);
        }
        machine.disk_drive().attach_image(&machine.disk_image());
    } else {
        // M35-S2: explicitly detach when no disk in list (safety/clarity)
        machine.disk_drive().attach_image(nullptr);
    }

    if (opts.trace_cpu_name.has_value()) {
        machine.set_cpu_trace_enabled(true);
    }

    sony_msx::machine::InputScriptPlayer script_player;
    if (opts.input_script_path.has_value()) {
        std::ifstream in(*opts.input_script_path, std::ios::binary);
        if (!in) {
            std::cerr << "debug-session: cannot open --input-script file: " << *opts.input_script_path << "\n";
            return 2;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        try {
            script_player = sony_msx::machine::InputScriptPlayer(sony_msx::machine::parse_input_script(text));
        } catch (const std::exception& e) {
            std::cerr << "debug-session: malformed --input-script: " << e.what() << "\n";
            return 2;
        }
    }

    // M36 Phase 3: deterministic comprehensive debug-snapshot capture. Default
    // OFF -- only fires when --snapshot <dir> is present, so a run without the
    // flag is byte-for-byte identical to before (no dir created, no file
    // written). The manifest carries the multi-disk index (frontend/headless
    // concept, planner A4) as a caller note.
    bool snapshot_taken = false;
    auto capture_snapshot = [&]() {
        std::vector<std::string> notes = {
            "disk_index=" + std::to_string(current_disk_index),
            "disk_count=" + std::to_string(disk_cache.size()),
        };
        const std::string sid = machine.snapshot_id();
        if (machine.write_snapshot(sid, notes)) {
            std::cerr << "debug-session: wrote snapshot " << machine.debug_root().string()
                      << "/snapshot/" << sid << "\n";
        } else {
            std::cerr << "debug-session: failed to write snapshot " << sid << "\n";
        }
        snapshot_taken = true;
    };

    std::uint32_t steps = 0;
    if (opts.frames.has_value()) {
        // M32 frame-loop mode: the real production drive shape
        // (Sdl3App::run_one_frame() / the DEC-0034 C5-closure loop) --
        // VBlank interrupts delivered at every boundary, no halt-stop
        // (titles HALT-wait for VBlank). Deterministic: a pure function of
        // the frame count and the input script's cycle stamps.
        const std::uint64_t target = machine.frame_cycles_per_frame();
        for (std::uint32_t frame = 0; frame < *opts.frames; ++frame) {
            // M36: deterministic scripted disk swap at a frame boundary (the
            // headless analogue of the SDL3 F11 swap), for driving a two-disk
            // game past its "insert data disk" prompt reproducibly.
            if (opts.swap_disk_frame.has_value() && frame == *opts.swap_disk_frame &&
                disk_cache.size() > 1) {
                if (opts.disk_writable) {
                    machine.disk_image().flush();
                    disk_cache[current_disk_index] = machine.disk_image().data();
                }
                current_disk_index = (current_disk_index + 1) % disk_cache.size();
                machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk_cache[current_disk_index]);
                if (opts.disk_writable) {
                    machine.disk_image().set_host_path(opts.disk_paths[current_disk_index]);
                }
                machine.disk_drive().attach_image(&machine.disk_image());
                machine.disk_drive().set_disk_changed(true);
                std::cerr << "debug-session: swapped to disk " << current_disk_index << " (\""
                          << opts.disk_paths[current_disk_index] << "\") at frame " << frame << "\n";
            }
            const std::uint64_t start = machine.elapsed_cycles();
            while (machine.elapsed_cycles() - start < target) {
                machine.step_cpu_instruction();
                script_player.apply_due(machine.elapsed_cycles(), machine.keyboard());
                ++steps;
            }
            machine.on_vsync_boundary();
            // M36 Phase 3: deterministic mid-run capture at the requested frame
            // boundary (the same post-on_vsync_boundary() point --dump-frame /
            // --swap-disk-frame already use).
            if (opts.snapshot_dir.has_value() && opts.snapshot_frame.has_value() &&
                frame == *opts.snapshot_frame) {
                capture_snapshot();
            }
        }
    } else {
        while (steps < max_steps && !machine.cpu().state().halted()) {
            machine.step_cpu_instruction();
            script_player.apply_due(machine.elapsed_cycles(), machine.keyboard());
            ++steps;
        }
    }

    // M36-S-c: persist any pending writable-disk writes back to the host .dsk
    // (opt-in only; no-op when --disk-writable was absent -> no host path).
    if (opts.disk_writable && !disk_cache.empty()) {
        if (machine.disk_image().flush()) {
            std::cerr << "debug-session: flushed writable disk to \""
                      << opts.disk_paths[current_disk_index] << "\"\n";
        }
    }

    // M36-S-d: persist the FM-PAC battery SRAM back to its .sram file.
    if (opts.fmpac_sram_path.has_value() && machine.flush_fmpac_sram()) {
        std::cerr << "debug-session: flushed FM-PAC SRAM to \"" << *opts.fmpac_sram_path << "\"\n";
    }

    // M36 Phase 3: end-of-run comprehensive snapshot (unless a mid-run
    // --snapshot-frame capture already fired).
    if (opts.snapshot_dir.has_value() && !snapshot_taken) {
        capture_snapshot();
    }

    if (opts.dump_frame_name.has_value() && !machine.write_frame_dump(*opts.dump_frame_name)) {
        std::cerr << "debug-session: failed to write --dump-frame " << *opts.dump_frame_name << "\n";
        return 2;
    }
    if (opts.dump_state_name.has_value() && !machine.write_state_dump(*opts.dump_state_name)) {
        std::cerr << "debug-session: failed to write --dump-state " << *opts.dump_state_name << "\n";
        return 2;
    }
    if (opts.trace_cpu_name.has_value() && !machine.write_cpu_trace(*opts.trace_cpu_name)) {
        std::cerr << "debug-session: failed to write --trace-cpu " << *opts.trace_cpu_name << "\n";
        return 2;
    }
    if (opts.event_log_name.has_value() && !machine.write_event_log(*opts.event_log_name)) {
        std::cerr << "debug-session: failed to write --event-log " << *opts.event_log_name << "\n";
        return 2;
    }

    std::cerr << "debug-session: steps=" << steps << " halted=" << (machine.cpu().state().halted() ? 1 : 0)
              << " final_pc=" << std::hex << machine.cpu().state().regs().pc << std::dec
              << " elapsed_cycles=" << machine.elapsed_cycles() << "\n";
    return 0;
}

// M27-S5 headless PSG audio-dump demo (docs/m27-planner-package.md §2.3
// point 3, mirrors --frame-dump-demo's exact precedent). Programs a known,
// fixed tone on PSG channel A via the REAL public register-write API
// (write_address()/write_data() -- #A0/#A1, the exact ports the CPU/IoBus
// itself uses; PsgYm2149::write_register() is a PRIVATE implementation
// detail, psg_ym2149.h, NOT directly callable here), then dumps a fixed
// sample count via write_psg_audio_dump() (genuine PsgAudioPump/PsgYm2149
// reuse, §2.3).
int run_audio_dump_demo(const std::string& out_path) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();

    sony_msx::devices::audio::PsgYm2149& psg = machine.psg();
    // R0/R1: channel A tone period = 16 (fine=16, coarse=0).
    psg.write_address(0);
    psg.write_data(16);
    psg.write_address(1);
    psg.write_data(0);
    // R7: mixer -- channel A tone enabled (bit0=0), channels B/C tone
    // disabled (bits1-2=1), ALL noise disabled (bits3-5=1). Bits 6/7 (I/O
    // direction) are forced by the device itself regardless of what is
    // written here (PsgYm2149::write_register()'s own R_ENABLE handling).
    psg.write_address(7);
    psg.write_data(0x3E);
    // R8: channel A volume -- fixed level 15 (max), resolved_amplitude() ==
    // 2*15+1 == 31 (A-M27-4's documented maximum).
    psg.write_address(8);
    psg.write_data(15);

    constexpr std::uint64_t kSampleRateHz = 44100;  // A-M27-5.
    constexpr std::size_t kSampleCount = 44100;     // 1 second, a real, audibly-non-trivial length.
    const bool ok = sony_msx::frontend::write_psg_audio_dump(machine.debug_root(), out_path, psg, kSampleRateHz,
                                                             kSampleCount);
    if (!ok) {
        std::cerr << "audio-dump-demo: failed to write " << out_path << "\n";
        return 2;
    }
    std::cerr << "audio-dump-demo: wrote " << machine.debug_root().string() << "/sounds/" << out_path << "\n";
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

// M24-S2 generic CP/M ".com" runner (backlog C3, ZEXDOC/ZEXALL full parity
// sweep). Reads a flat CP/M-style ".com" file from disk, cold-boots a fresh
// machine, loads it via the GENERIC CpmBdosHarness (src/machine/
// cpm_bdos_harness.h -- zero zexall/zexdoc-specific knowledge), runs it to
// either the CP/M warm-boot trap or <max_instructions> (whichever comes
// first), writes the captured BDOS output bytes verbatim to <out_log_path>,
// and prints a one-line diagnostic summary to stderr mirroring the existing
// parity-trace/bios-boot-trace style. Exits 0 only on a genuine finished
// completion; a distinct non-zero code on budget exhaustion or a load
// failure -- never silently reports an incomplete run as success.
int run_cpm(const std::string& com_path, std::uint64_t max_instructions, const std::string& out_path) {
    std::ifstream in(com_path, std::ios::binary);
    if (!in) {
        std::cerr << "cpm-run: cannot open program: " << com_path << "\n";
        return 2;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    if (image.empty()) {
        std::cerr << "cpm-run: empty program: " << com_path << "\n";
        return 2;
    }

    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    const auto load_result = sony_msx::machine::CpmBdosHarness::load_com(machine, image);
    if (load_result != sony_msx::machine::CpmBdosHarness::LoadResult::Ok) {
        std::cerr << "cpm-run: program too large to load (top-of-memory collision): " << com_path << "\n";
        return 2;
    }

    const auto run_result = sony_msx::machine::CpmBdosHarness::run(machine, max_instructions);

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "cpm-run: cannot write output: " << out_path << "\n";
        return 2;
    }
    out.write(reinterpret_cast<const char*>(run_result.captured_output.data()),
              static_cast<std::streamsize>(run_result.captured_output.size()));
    if (!out) {
        std::cerr << "cpm-run: write failed: " << out_path << "\n";
        return 2;
    }

    std::cerr << "cpm-run: instructions_executed=" << run_result.instructions_executed
              << " finished=" << (run_result.finished ? 1 : 0)
              << " unexpected_bdos_calls=" << run_result.unexpected_bdos_calls.size()
              << " captured_bytes=" << run_result.captured_output.size() << "\n";
    // An honest, non-fabricated "ran out of budget" signal: exit 0 ONLY on a
    // genuine CP/M warm-boot completion, never on budget exhaustion.
    return run_result.finished ? 0 : 3;
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

    if (argc >= 2 && std::string(argv[1]) == "--sprite-cmd-parity") {
        if (argc < 7) {
            std::cerr << "usage: " << argv[0]
                      << " --sprite-cmd-parity <program.bin> <base_hex> <max_steps> <vram_bytes> <out.txt>\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const auto vram_bytes = static_cast<std::uint32_t>(std::strtoul(argv[5], nullptr, 10));
        const std::string out_path = argv[6];
        return run_sprite_cmd_parity(bin_path, base, max_steps, vram_bytes, out_path);
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

    if (argc >= 2 && std::string(argv[1]) == "--frame-dump-demo") {
        if (argc < 3) {
            std::cerr << "usage: " << argv[0] << " --frame-dump-demo <out_filename_under_debug/frames/>\n";
            return 2;
        }
        return run_frame_dump_demo(argv[2]);
    }

    if (argc >= 2 && std::string(argv[1]) == "--halnote-parity") {
        if (argc < 4) {
            std::cerr << "usage: " << argv[0] << " --halnote-parity <bios_dir> <out.txt>\n";
            return 2;
        }
        return run_halnote_parity(argv[2], argv[3]);
    }

    if (argc >= 2 && std::string(argv[1]) == "--debug-session") {
        if (argc < 4) {
            std::cerr << "usage: " << argv[0]
                      << " --debug-session <bios_dir> <max_steps> [--disk <path>] [--cart1 <path>]"
                         " [--cart1-type <T>|auto] [--cart2 <path>] [--cart2-type <T>|auto]"
                         " [--softwaredb <path>] [--debug-root <path>]"
                         " [--dump-state <name>] [--trace-cpu <name>] [--event-log <name>]"
                         " [--input-script <path>] [--frames <N>] [--dump-frame <name>]"
                         " [--disk-writable] [--swap-disk-frame <N>] [--fmpac-sram <path>]"
                         " [--snapshot <dir>] [--snapshot-frame <N>]\n"
                         "  --snapshot <dir> writes a comprehensive per-component debug snapshot\n"
                         "  to <dir>/snapshot/<id>/ (default: once at end-of-run); --snapshot-frame\n"
                         "  <N> captures at that frame boundary in --frames mode instead.\n"
                         "  --disk-writable persists disk writes back to the host .dsk at exit\n"
                         "  (opt-in; default in-memory-only). --swap-disk-frame <N> hot-swaps to\n"
                         "  the next --disk at frame N (repeatable --disk list). --fmpac-sram\n"
                         "  <path> loads/saves an inserted FM-PAC cartridge's 8 KB battery SRAM.\n"
                         "  --frames drives N frames via the real frame loop (VBlank delivered;\n"
                         "  <max_steps> ignored, HALT does not stop the run); --dump-frame writes\n"
                         "  the decoded frame to <debug_root>/frames/<name> after the run (M32).\n"
                         "  --cartN-type is optional: when omitted (or 'auto') the mapper type is\n"
                         "  auto-identified via softwaredb SHA1 match, then a bank-write heuristic\n"
                         "  (M30, --softwaredb overrides the default references/openmsx-21.0/share/"
                         "softwaredb.xml).\n";
            return 2;
        }
        const std::string bios_dir = argv[2];
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10));
        // `args` (built above as argv[1..]) sliced to skip "--debug-session",
        // <bios_dir>, <max_steps> -- the remaining entries are this mode's
        // own optional flags plus the reused cartridge-cli flags.
        const std::vector<std::string> mode_args(args.begin() + 3, args.end());
        return run_debug_session(bios_dir, max_steps, mode_args);
    }

    if (argc >= 2 && std::string(argv[1]) == "--audio-dump-demo") {
        if (argc < 3) {
            std::cerr << "usage: " << argv[0] << " --audio-dump-demo <out_filename_under_debug/sounds/>\n";
            return 2;
        }
        return run_audio_dump_demo(argv[2]);
    }

    if (argc >= 2 && std::string(argv[1]) == "--cpm-run") {
        if (argc < 5) {
            std::cerr << "usage: " << argv[0]
                      << " --cpm-run <program.com> <max_instructions> <out_log_path>\n";
            return 2;
        }
        const std::string com_path = argv[2];
        const auto max_instructions = static_cast<std::uint64_t>(std::strtoull(argv[3], nullptr, 10));
        const std::string out_path = argv[4];
        return run_cpm(com_path, max_instructions, out_path);
    }

    if (argc >= 2 && std::string(argv[1]) == "--parity-trace") {
        if (argc < 6) {
            std::cerr << "usage: " << argv[0]
                      << " --parity-trace <program.bin> <base_hex> <max_steps> <out.txt>"
                         " [halt_idle_extra_steps]\n";
            return 2;
        }
        const std::string bin_path = argv[2];
        const auto base = static_cast<std::uint16_t>(std::strtoul(argv[3], nullptr, 16));
        const auto max_steps = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
        const std::string out_path = argv[5];
        // Optional 6th positional arg (M23-S3, backlog C2 A/B evidence); absent
        // -> 0, the exact pre-M23 behavior (see run_parity_trace's doc comment).
        const auto halt_idle_extra_steps =
            (argc >= 7) ? static_cast<std::uint32_t>(std::strtoul(argv[6], nullptr, 10)) : 0U;
        return run_parity_trace(bin_path, base, max_steps, out_path, args, halt_idle_extra_steps);
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
