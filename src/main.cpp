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

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/fdc/disk_image.h"
#include "frontend/machine_audio_mixer.h"  // M39-A: full-machine audio mix for --audio-dump
#include "frontend/psg_audio_dump.h"
#include "frontend/sdl3_cli.h"  // M37/M46: shared parse_speed_level/parse_ram_kb/resolve_session_defaults
#include "frontend/session_summary.h"  // M46 (DEC-0071): compact headless "This session" banner
#include "machine/cartridge_cli.h"
#include "machine/cartridge_identifier.h"
#include "machine/cpm_bdos_harness.h"
#include "machine/debug_format.h"
#include "machine/emulator_config.h"  // M50-S2 (DEC-0077): --config strict-XML session defaults
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

namespace {

// M19-S5 cartridge loading (backlog B7). Shared by both the default run path
// and --parity-trace (planner §2.4/§2.7) to avoid duplicated parser/loader
// logic. Deliberately STRICTER than RomAssetLoader's BIOS/Kanji-font/disk-
// image policy (non-fatal 0xFF-fill + diagnostic, rom_asset_loader.h:14-22):
// a user-specified --cartN is explicit and one-off, so any failure prints a
// loud diagnostic and returns non-zero -- never a silent fallback. Scoped
// only to this cartridge_cli/load_cartridge path; must not leak into
// RomAssetLoader's graceful-degradation call sites
// (Hbf1xvMachine::load_rom_assets).
int load_cartridges_from_args(sony_msx::machine::Hbf1xvMachine& machine, const std::vector<std::string>& args,
                              std::optional<std::string> softwaredb_override = std::nullopt) {
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

    // M30 (backlog G2): auto-identifies type-less --cartN requests via the
    // shared resolver (cartridge_identifier.h, also used by the SDL3
    // frontend). Explicit --cartN-type specs pass through untouched.
    // Identified-but-unsupported -> loud message + exit 2.
    // M50-S3 (DEC-0077): softwaredb path precedence CLI --softwaredb > XML
    // override > default. softwaredb_override is std::nullopt unless a loaded
    // config set a NON-default path (byte-identical to pre-M50 otherwise).
    const std::optional<std::string> softwaredb_path =
        parsed.softwaredb_path.has_value() ? parsed.softwaredb_path : softwaredb_override;
    sony_msx::machine::CartridgeIdentificationSession ident_session(softwaredb_path);

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
// forces cold_boot's reset vector, sets PC to base, enables the
// per-instruction trace-export (M10-S1), and single-steps until HALT (or a
// step ceiling). The trace is written in CpuTraceSink text format for a
// line-for-line diff against the openMSX-side trace
// (tools/openmsx/trace-parity.ps1).
//
// `cli_args` (M19-S6): when it carries --cart1/--cart1-type (and/or --cart2/
// --cart2-type), load_cartridges_from_args mounts the cartridge(s) right
// after cold_boot, before the driver program runs -- letting the Z80 program
// page a real mapper in via #A8. Absent --cartN this is a no-op (pre-M19
// behavior unchanged).
// `halt_idle_extra_steps` (M23-S3, backlog C2): OPTIONAL, defaults to 0
// (byte-identical pre-M23 behavior -- the loop still stops exactly at halt,
// tests/CLAUDE.md's "stop at halt" convention). When > 0 and the CPU is
// halted after that loop, steps this many MORE times while halted -- the
// only way to observe the M23-S1 HALT-R phantom-M1-refetch behavior (R
// increments once per halted idle step) through the trace export.
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

    // M23-S3 (see halt_idle_extra_steps doc above): only engages when the
    // caller asks for it AND the CPU is halted; otherwise a no-op.
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
// exercising control registers, #98 VRAM auto-increment fill, palette, #9B
// indirect, and the read-ahead path) like parity-trace, then dumps the
// comparable VDP state: physical VRAM, 14-bit VRAM pointer, R#14, and the
// control-register file. The same program runs on openMSX's V9958
// (tools/openmsx/vdp-parity.ps1) for a diff. VRAM is now comparable (it was
// excluded from M13's diff).
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
    // R#0..R#27 (decimal-labeled). Only registers the driver program
    // EXPLICITLY writes are cross-comparable with openMSX (whose BIOS
    // pre-sets the rest); the harness gates on that subset + VRAM.
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
// flat RAM-only Z80 program (tools/gen/vdp-render-probe.py) that writes
// VRAM/registers/palette via the same #98/#99/#9A ports as run_vdp_parity,
// then emits: R#0-R#27, the 16-entry palette (raw 9-bit GRB, matching
// openMSX's "VDP palette" SimpleDebuggable byte layout: 2 bytes/entry,
// value = byte[2i] | (byte[2i+1]<<8)), a physical VRAM block (same gate as
// run_vdp_parity), and the renderer's own computed RGB555 pixel values for
// the first <pixel_count> pixels of display line 0 (whatever mode the
// program left active). The pixel dump is this engine's own derived output,
// not a cross-engine probe -- openMSX exposes no computed-pixel-color
// debuggable (`debug list` confirms none exists; see
// docs/m21-parity-trace-diff.md for the full disposition).
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
    // Fixed extra 16-byte window at physical 0x10000 (the G6/G7 planar
    // "bank1" region, A-M21-10), always emitted regardless of vram_bytes,
    // so the D7 planar-transform probe's odd-logical-address writes are
    // visible without an enormous dump from address 0.
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
// Runs a flat RAM-only Z80 program (tools/gen/sprite-command-probe.py, using
// the same OUT (#98)/(#99)/(#9B) port sequence a real CPU uses for sprites
// and the R#32-46 command engine) like run_vdp_render_parity, then emits:
// the full R#0-R#46 control + command-engine register file (openMSX's "VDP
// regs" SimpleDebuggable is size 64, confirmed via a live WSL `debug size`
// query, so R#32-46 are already in that same range -- no separate probe
// point needed), the S#0-S#9 status registers read NON-destructively (the
// CPU program's own `IN (#99)` already exercised any destructive read side
// effects; this dump captures the settled state afterward -- comparable to
// openMSX's "VDP status regs" SimpleDebuggable, size 16), and a physical
// VRAM window (same vram_bytes + fixed 0x10000 "bank1" precedent as
// run_vdp_render_parity, reusing the same "physical VRAM" SimpleDebuggable).
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

    // Explicitly trigger the sprite-check recompute (S#0/S#3-S#6): this
    // engine's SpriteEngine is driven ONLY by the on_vsync() frame-boundary
    // hook (no CPU-visible port triggers it, deliberately -- no new clock
    // consumer, planner package §1.4 Resolution 1), unlike openMSX's
    // raster-time-driven SpriteChecker::checkUntil(), which advances "for
    // free" as real emulated time elapses. The companion PS1 script
    // correspondingly waits real emulated time (`after time`) after the CPU
    // program's writes for openMSX's own check to complete naturally --
    // this call is this engine's architectural equivalent, not a workaround.
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
// (tools/gen/ym2413-probe.py, using `OUT (#7C),reg ; OUT (#7D),value`)
// like run_parity_trace, then dumps the YM2413 register file (all 64 bytes,
// $00-$3F) via the debug-only `register_value(addr)` accessor (A-M17-6). The
// same program runs on openMSX's YM2413 (tools/openmsx/ym2413-parity.ps1),
// read via its "MSX Music regs" SimpleDebuggable
// (references/openmsx-21.0/src/sound/YM2413.hh:40-44), for a per-address
// diff.
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

// M26-S4 decoded-FrameBuffer-dump evidence generator (backlog C9,
// docs/m26-planner-package.md §2.5 point 3). Drives a colorful, deterministic
// GRAPHIC4 (SCREEN5) test scene through the real #98/#99/#9A VDP port
// protocol via the non-perturbing debug_io_write() seam (M13) -- no CPU
// driver program needed (same port-write sequences as
// tools/gen/vdp-render-probe.py, re-expressed here in C++). Vertical
// color bars spanning all 16 palette entries, with a vivid hand-chosen
// palette -- a recognizable picture, not a blank boot screen.
// machine.write_frame_dump() produces the raw dump; tools/convert/frame-to-png.py
// converts it to the committed PNG.
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
    // M1=M2=0) -> mode base 0x0C, VdpMode::Graphic4 (devices/video/
    // vdp_mode.h base-byte formula).
    set_register(0, 0x06);
    // M34 (DEC-0043 Defect B): R#1 bit6 BL=1 -- the render gate blanks BL=0
    // lines (real hardware behavior), so a demo scene modelling a DISPLAYED
    // screen must enable the display like a real program would. Renders
    // byte-identical to the pre-M34 output (bit6 previously had no effect).
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
    // nibble first -- render_graphic4()'s convention), 16 vertical color
    // bars of 16px each (8 bytes/bar; 8*16=128 matches the row length),
    // identical on every row.
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
// Cold-boots with real ROM assets from <bios_dir> (bios/f1xvfirm.rom,
// unmodified -- this local file's SHA1 was independently confirmed
// byte-identical to the installed WSL openMSX system ROM, planner
// §2.6/A-M20, tools/openmsx/halnote-parity.ps1), routes the entire
// Halnote 64 KB window into view via the debug-harness technique (no CPU
// driver program needed -- Halnote's mem_read/mem_write are pure functions
// of the raw 16-bit address, planner §2.5), then runs the same write/read
// protocol sequence as the openMSX-side Tcl script, dumping each read-back
// as a "LABEL=name VALUE=hex" line for cross-emulator diffing.
int run_halnote_parity(const std::string& bios_dir, const std::string& out_path) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    for (const std::string& note : machine.rom_diagnostics()) {
        std::cerr << "halnote-parity: " << note << "\n";
    }

    // Force the authentic reset default (#A8=0, every page primary 0), then
    // route all four pages of primary 0 to secondary slot 3 (Halnote) via a
    // single #FFFF write (SlotBus::write_ffff targets
    // sub_slot_register_[primary_for_page(3)] == sub_slot_register_[0];
    // 0xFF = 0b11_11_11_11 decodes every 2-bit page field to secondary 3).
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
// items 1/3/4). A wholly additive new mode -- never touches the pre-existing
// default run path below main()'s "headless scaffold" block, unchanged
// byte-for-byte (R-M27-1). Resolves A-M27-1/A-M27-2 (the default path never
// drives the CPU nor loads real BIOS assets): this mode loads real ROM
// assets from <bios_dir> and drives the CPU with a bounded, halt-respecting
// step_cpu_instruction() loop -- the same "stop exactly at halt" shape as
// run_bios_boot_trace (tests/CLAUDE.md's convention), no new CPU-stepping
// semantics invented.
struct DebugSessionOptions {
    // M35-S1: repeatable --disk list (AC-8: same mechanism as SDL3 frontend).
    std::vector<std::string> disk_paths;
    std::optional<std::string> debug_root;
    std::optional<std::string> dump_state_name;
    std::optional<std::string> trace_cpu_name;
    std::optional<std::string> event_log_name;
    std::optional<std::string> input_script_path;
    // M32 closure additions (QA condition, docs/m32-qa-signoff.md: committed
    // evidence needs RECORDED, re-runnable recipes; consumed by
    // tools/capture/cartridge-evidence.ps1). Both additive; absent flags
    // leave every pre-M32 invocation byte-identical.
    //   --frames <N>: drive N frames via the real frame-loop shape
    //     (step_cpu_instruction() to each frame boundary, then
    //     on_vsync_boundary() -- Sdl3App::run_one_frame()'s C5-closure
    //     shape) instead of the step-only loop. <max_steps> is ignored in
    //     this mode (frame count is the budget); the loop deliberately does
    //     NOT stop on HALT -- real titles HALT-wait for the VBlank interrupt
    //     every frame (the DEC-0034 loop-shape finding).
    //   --dump-frame <name>: write_frame_dump(<name>) after the run
    //     (to <debug_root>/frames/<name>).
    std::optional<std::uint32_t> frames;
    std::optional<std::string> dump_frame_name;
    // M36-S-c: opt-in host-file disk-save persistence. Default false =
    // in-memory-only (byte-for-byte the pre-M36 behavior; never clobbers a
    // real .dsk). When set, each mounted disk gets its host path bound and a
    // dirty image is flushed at end-of-run (and before a scripted swap).
    bool disk_writable = false;
    // Fast-disk (FDC turbo) QoL mode. M46 (DEC-0071) makes this TRI-STATE so the
    // DEFAULT lives in resolve_session_defaults() (headless/SDL3 parity), NOT the
    // field: convenience default ON, --no-fast-disk/--stock force OFF, explicit
    // --fast-disk forces ON. Applied once after cold_boot() via set_fast_disk().
    std::optional<bool> fast_disk_opt;
    // M46 (DEC-0071): --ram <64|128|256|512> (tri-state; nullopt = unspecified),
    // --no-fmpac (opt out of the slot-2 FM-PAC auto-load), and --stock (one-shot
    // bare machine: RAM 64 + accurate disk + no FM-PAC). Resolved through the
    // shared resolve_session_defaults() so the headless --debug-session game path
    // gets the same flipped convenience defaults as the SDL3 exe.
    std::optional<int> ram_kb;
    bool no_fmpac = false;
    bool stock = false;
    // M50-S2 (DEC-0077, docs/m50-planner-package.md §4.6): --config <path>
    // FORCE-loads an externalized strict-XML config even headless (the opt-in
    // that overrides the determinism gate). std::nullopt (default, every ctest
    // invocation) = the headless path NEVER auto-loads a config -> the resolved
    // session defaults stay byte-identical to pre-M50. Only the session/machine
    // knobs (RAM / fast-disk / FM-PAC auto-load) apply headless; the presentation
    // knobs are SDL3-only and ignored here.
    std::optional<std::string> config_path;
    // M36 deterministic repro/testing enabler: swap to the next disk in the
    // repeatable --disk list at this frame (frame-loop mode only). Reuses
    // M35's multi-disk cache + swap semantics headlessly so a two-disk game
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
    // for a deterministic mid-run capture (default: once at end-of-run).
    // Additive + default-off: absent flags leave every prior invocation
    // byte-for-byte unchanged (no snapshot dir created, no file written).
    std::optional<std::string> snapshot_dir;
    std::optional<std::uint32_t> snapshot_frame;
    // DEC-0052 stream-light (M36 Bug B long-session upstream hunt):
    // --stream-light arms the machine-level live stream-capture in the
    // LIGHTWEIGHT mode for the whole run (per-frame snapshot bundles
    // suppressed -> coarse anchors + the per-event watchlog
    // stream_<id>_watch.log). Default OFF = no stream capture (byte-for-byte
    // the pre-DEC-0052 behavior). Headless analogue of the SDL3 F10 toggle,
    // for a deterministic, reproducible light capture without a window.
    bool stream_light = false;
    // M39-A Step 1 confirmation diagnostic. --io-observe <out.csv> writes a
    // per-frame CSV of the PPI port-C bit-7 (1-bit-DAC) EDGE rate and the PSG
    // R8/R9/R10 (software-PCM volume) write rate, over the --frames run --
    // distinguishing the 1-bit-DAC voice hypothesis (Fix A: bit-7 edges >> 60/
    // frame) from a PSG software-PCM one (Fix B: R8/R9/R10 hammered). Counting
    // only (O(1) memory/frame), so it does not OOM like --trace-cpu. Enables
    // click-DAC capture for the run.
    std::optional<std::string> io_observe_name;
    // M39-A Step 3 validation. --audio-dump <name> mixes the FULL machine audio
    // (PSG + SCC + built-in FM + FM-PAC + the M39 1-bit-DAC click) every frame
    // via MachineAudioMixer -- the same law the SDL3 frontend uses -- and writes
    // the voice-window PCM to <debug_root>/sounds/<name> ("HBF1XV-AUDIO-DUMP
    // v1", -> WAV via tools/convert/audio-dump-to-wav.py). --frames mode only. Enables
    // click-DAC capture unless --audio-no-click (the BEFORE/no-click A/B leg,
    // click term forced 0). --audio-from/--audio-to bound the appended frame
    // window (all generators still advance every frame to stay in lockstep).
    std::optional<std::string> audio_dump_name;
    bool audio_no_click = false;
    std::optional<std::uint32_t> audio_from;
    std::optional<std::uint32_t> audio_to;
    // M39-A Fix B: --audio-sync switches --audio-dump from the once-per-frame
    // BATCH mix (the pre-fix behavior -- the BEFORE leg, sub-frame PSG volume
    // writes collapse, voice silent) to the INTERLEAVED sync-before-change
    // production (the AFTER leg -- samples produced during CPU stepping over
    // machine-cycle windows, so the software-PCM voice survives). One build, one
    // A/B: BEFORE = --audio-dump, AFTER = --audio-dump --audio-sync.
    bool audio_sync = false;
    // M39-A: isolate the PSG in the --audio-dump (drop SCC/FM/FM-PAC/click), so
    // the BEFORE(batch)/AFTER(sync) A/B differs ONLY in the PSG production
    // method -- cleanly exposing the software-PCM voice without the SCC music
    // (which Aleste 2 plays loudly) masking it.
    bool audio_psg_only = false;
    // DEC-0072 replay-fidelity diagnostic (M47-followup, this run). The default
    // --input-script replay applies key edges AFTER the frame's first instruction
    // (script_player.apply_due() sits inside the CPU step sub-loop). The LIVE SDL3
    // recorder instead applied the very same edges via poll_and_dispatch_events()
    // at FRAME-START, BEFORE the first instruction of the frame (and stamped the
    // frame-start cycle as the event T). --input-frame-start makes the headless
    // replay apply due events at frame-start too, cycle-faithful to how the live
    // run drove them -- so a cycle-identical run reproduces the same state at save
    // time. Opt-in; absent leaves the pre-existing after-instruction replay path
    // byte-for-byte unchanged.
    bool input_frame_start = false;
    // Per-frame CPU-state fingerprint CSV (frame,cycles,pc,sp,af,bc,de,hl,ix,iy,r)
    // sampled at each frame boundary (after on_vsync_boundary()). Used to diff two
    // drivers (headless vs SDL3 hidden-window) and name the first divergence frame.
    std::optional<std::string> fingerprint_name;
    // DEC-0072 watchpoint (Step 2): watch writes to a physical-DRAM window and log
    // any byte whose value != the SRAM-correct reference at the matching in-sector
    // index. --watch-save <out.log> enables; the window/reference are wired in the
    // frames loop. Opt-in, off by default.
    std::optional<std::string> watch_save_name;
    // DEC-0072 physical-DRAM memory-write WATCHPOINT (M47 kill-step). --watch-mem
    // <LO> <HI> <out.csv> traps every CPU write whose folded PHYSICAL DRAM address
    // is in [LO, HI) (both hex, e.g. E000 E200 -- the YS II save-build buffer) and
    // logs the writing instruction's PC + live A/BC/DE/HL/IX/IY/SP + cycle to the
    // CSV. --watch-from/--watch-to bound the logged FRAME window (default: whole
    // run). Opt-in, off by default; byte-identical when absent.
    std::optional<std::string> watch_mem_name;
    std::size_t watch_mem_lo = 0;
    std::size_t watch_mem_hi = 0;
    std::uint32_t watch_mem_from = 0;
    std::uint32_t watch_mem_to = 0xFFFFFFFFu;
    // DEC-0072 causal probe: signed T-state bias added to the VDP command-engine
    // CE busy-window duration (V9958Vdp::set_cmd_busy_bias). Sweeping this tests
    // whether the save-build corruption is gated by the command-engine finish
    // timing (the openMSX calcFinishTime approximation). 0 (default) = unchanged.
    std::int64_t vdp_cmd_bias = 0;
    // DEC-0072 raster-phase sweep: add a constant T-state offset to EVERY input
    // event, delaying the whole (relative-timing-preserved) input timeline so the
    // save-build runs at a shifted raster/VBlank phase. The live corruption is
    // phase-sensitive (a corrupting session saved at a different frame than the
    // clean owner script), so sweeping this probes for the "bad" phase that
    // reproduces the -0x8A save corruption deterministically. 0 = unchanged.
    std::uint64_t input_shift = 0;
};

std::optional<std::string> take_debug_session_value(const std::vector<std::string>& args, const std::size_t i,
                                                     const char* flag_name, std::vector<std::string>& errors) {
    if (i + 1 >= args.size()) {
        errors.push_back(std::string("debug-session: ") + flag_name + " requires a value argument");
        return std::nullopt;
    }
    return args[i + 1];
}

// Pure argv parser for --debug-session's own optional flags (order-
// independent scanning, mirrors sdl3_cli.cpp's parse_sdl3_cli()). Cartridge
// flags (--cart1/--cart1-type/--cart2/--cart2-type) are intentionally NOT
// recognized here -- they fall through untouched, for
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
        } else if (arg == "--fast-disk") {
            opts.fast_disk_opt = true;  // M46: explicit ON (overrides --stock / the default)
        } else if (arg == "--no-fast-disk") {
            opts.fast_disk_opt = false;  // M46: explicit OFF (accurate FDC timing)
        } else if (arg == "--no-fmpac") {
            opts.no_fmpac = true;  // M46: opt out of the FM-PAC slot-2 auto-load
        } else if (arg == "--stock") {
            opts.stock = true;  // M46: one-shot authentic bare machine preset
        } else if (arg == "--config") {
            // M50-S2 (DEC-0077): force-load a strict-XML config even headless.
            if (auto v = take_debug_session_value(args, i, "--config", errors)) {
                opts.config_path = *v;
                ++i;
            }
        } else if (arg == "--ram") {
            // M46 (DEC-0071): shared {64,128,256,512} enum validator (headless/
            // SDL3 parity). A bad/missing value is a loud parse error.
            if (auto v = take_debug_session_value(args, i, "--ram", errors)) {
                ++i;
                int kb = 0;
                if (sony_msx::frontend::parse_ram_kb(*v, kb, errors, "debug-session")) {
                    opts.ram_kb = kb;
                }
            }
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
        } else if (arg == "--stream-light") {
            opts.stream_light = true;  // DEC-0052: arm lightweight stream capture (boolean flag)
        } else if (arg == "--io-observe") {
            if (auto v = take_debug_session_value(args, i, "--io-observe", errors)) {
                opts.io_observe_name = *v;  // M39-A Step 1 diagnostic CSV path
                ++i;
            }
        } else if (arg == "--audio-dump") {
            if (auto v = take_debug_session_value(args, i, "--audio-dump", errors)) {
                opts.audio_dump_name = *v;  // M39-A Step 3 voice-window PCM dump
                ++i;
            }
        } else if (arg == "--audio-no-click") {
            opts.audio_no_click = true;  // M39-A: BEFORE leg -- click term forced 0
        } else if (arg == "--audio-sync") {
            opts.audio_sync = true;  // M39-A Fix B: interleaved sync-before-change (AFTER leg)
        } else if (arg == "--audio-psg-only") {
            opts.audio_psg_only = true;  // M39-A: isolate the PSG voice for a clean A/B
        } else if (arg == "--input-frame-start") {
            opts.input_frame_start = true;  // DEC-0072: apply script edges at frame-start (live-faithful)
        } else if (arg == "--fingerprint") {
            if (auto v = take_debug_session_value(args, i, "--fingerprint", errors)) {
                opts.fingerprint_name = *v;  // per-frame CPU fingerprint CSV
                ++i;
            }
        } else if (arg == "--watch-save") {
            if (auto v = take_debug_session_value(args, i, "--watch-save", errors)) {
                opts.watch_save_name = *v;  // DEC-0072 Step 2 memory-write watchpoint log
                ++i;
            }
        } else if (arg == "--watch-mem") {
            // Three values: LO HI OUT (LO/HI hex physical addresses).
            if (i + 3 >= args.size()) {
                errors.push_back("debug-session: --watch-mem requires <LO_hex> <HI_hex> <out.csv>");
            } else {
                opts.watch_mem_lo = static_cast<std::size_t>(std::strtoull(args[i + 1].c_str(), nullptr, 16));
                opts.watch_mem_hi = static_cast<std::size_t>(std::strtoull(args[i + 2].c_str(), nullptr, 16));
                opts.watch_mem_name = args[i + 3];
                i += 3;
            }
        } else if (arg == "--watch-from") {
            if (auto v = take_debug_session_value(args, i, "--watch-from", errors)) {
                opts.watch_mem_from = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--watch-to") {
            if (auto v = take_debug_session_value(args, i, "--watch-to", errors)) {
                opts.watch_mem_to = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--vdp-cmd-bias") {
            if (auto v = take_debug_session_value(args, i, "--vdp-cmd-bias", errors)) {
                opts.vdp_cmd_bias = static_cast<std::int64_t>(std::strtoll(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--input-shift") {
            if (auto v = take_debug_session_value(args, i, "--input-shift", errors)) {
                opts.input_shift = static_cast<std::uint64_t>(std::strtoull(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--audio-from") {
            if (auto v = take_debug_session_value(args, i, "--audio-from", errors)) {
                opts.audio_from = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        } else if (arg == "--audio-to") {
            if (auto v = take_debug_session_value(args, i, "--audio-to", errors)) {
                opts.audio_to = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 10));
                ++i;
            }
        }
        // Any other argument (--cart1/--cart1-type/--cart2/--cart2-type) is
        // left untouched for load_cartridges_from_args()'s own parser below.
    }
    return opts;
}

// M39-A Step 1: passive PSG register-write counter. Counts writes to the
// channel-volume registers R8/R9/R10 (the software-PCM path a Fix-B voice would
// hammer) and total register writes -- read per-frame by --io-observe to report
// the write RATE. Non-perturbing (inspection only).
struct PsgWriteCounter final : public sony_msx::devices::audio::PsgWriteObserver {
    std::uint64_t vol_writes = 0;    // R8 (0x08) / R9 (0x09) / R10 (0x0A)
    std::uint64_t total_writes = 0;
    void on_register_write(const std::uint8_t reg, std::uint8_t /*value*/) override {
        ++total_writes;
        if (reg == 0x08 || reg == 0x09 || reg == 0x0A) {
            ++vol_writes;
        }
    }
};

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

    // M50-S2 (DEC-0077, §4.6): the headless path NEVER auto-loads a config; only
    // an explicit --config <path> force-loads one (no ctest invocation passes it,
    // so the resolved defaults stay byte-identical). The loaded config supplies
    // the BASE session defaults (RAM / fast-disk / FM-PAC) that resolve_session_
    // defaults() falls back to -- precedence CLI > XML > convenience default.
    sony_msx::machine::EmulatorConfig cfg;  // built-in defaults
    if (opts.config_path.has_value()) {
        std::vector<std::string> config_warnings;
        cfg = sony_msx::machine::EmulatorConfig::load_from_file(*opts.config_path, config_warnings);
        for (const std::string& w : config_warnings) {
            std::cerr << "debug-session: " << w << "\n";
        }
    }

    // M50-S3 (DEC-0077, docs/m50-planner-package.md §6-S3): resolve the machine-
    // sizing/path fields with precedence CLI > XML > built-in default. Each XML
    // value is surfaced ONLY when it differs from the built-in default, so with no
    // --config (every ctest invocation) these stay byte-identical to pre-M50. The
    // headless <bios_dir> is a mandatory positional CLI arg, so it always wins the
    // BIOS-dir precedence (CLI > XML) and no XML bios-dir applies here by design.
    const sony_msx::machine::EmulatorConfig config_defaults;  // for "XML differs from default"
    const std::string fmpac_autoload_rom = cfg.fmpac_rom;     // XML > "roms/fmpac.rom"
    std::optional<std::string> resolved_fmpac_sram = opts.fmpac_sram_path;  // CLI > XML > derive
    if (!resolved_fmpac_sram.has_value() && cfg.fmpac_sram != config_defaults.fmpac_sram) {
        resolved_fmpac_sram = cfg.fmpac_sram;
    }
    std::optional<std::string> resolved_softwaredb;  // XML override (CLI wins inside loader)
    if (cfg.softwaredb_path != config_defaults.softwaredb_path) {
        resolved_softwaredb = cfg.softwaredb_path;
    }

    // M46 (DEC-0071): resolve the flipped convenience-vs-stock defaults in the
    // CLI layer (headless/SDL3 parity, the anti-drift seam of planner §2.7 -- the
    // Hbf1xvMachine ctor default STAYS 64 KB; this is the only place it flips).
    // Empty CLI + no config -> {512 KB RAM, fast-disk ON, FM-PAC slot-2 auto-load
    // ON}; --stock/--ram 64/--no-fast-disk/--no-fmpac peel it back (order-
    // independent); a loaded config replaces the convenience base default.
    sony_msx::frontend::SessionDefaultsRequest session_request;
    session_request.ram_kb = opts.ram_kb;
    session_request.fast_disk_opt = opts.fast_disk_opt;
    session_request.no_fmpac = opts.no_fmpac;
    session_request.stock = opts.stock;
    session_request.base_ram_kb = cfg.ram_kb;
    session_request.base_fast_disk = cfg.fast_disk;
    session_request.base_fmpac_autoload = cfg.fmpac_autoload;
    const sony_msx::frontend::ResolvedSessionDefaults resolved =
        sony_msx::frontend::resolve_session_defaults(session_request);

    sony_msx::machine::Hbf1xvMachine machine(resolved.ram_bytes);
    if (opts.debug_root.has_value()) {
        machine.set_debug_root(*opts.debug_root);
    }
    // M36 Phase 3: --snapshot <dir> sets the snapshot output root; the
    // capture lands under <dir>/snapshot/<id>/. Takes precedence over
    // --debug-root for a dedicated snapshot run.
    if (opts.snapshot_dir.has_value()) {
        machine.set_debug_root(*opts.snapshot_dir);
    }
    // R-M27-2: event logging MUST be enabled BEFORE cold_boot() to capture
    // the Reset event (hbf1xv_machine.h:306-309's documented ordering
    // requirement).
    if (opts.event_log_name.has_value()) {
        machine.set_event_logging_enabled(true);
    }
    // M50-S3 (DEC-0077): apply the config-resolved role-keyed BIOS filenames
    // BEFORE cold_boot (which drives load_rom_assets). A bare/no-config cfg keeps
    // the strict spec filenames, byte-identical to before.
    machine.set_bios_filenames(cfg.bios_roms);
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    for (const std::string& note : machine.rom_diagnostics()) {
        std::cerr << "debug-session: " << note << "\n";
    }

    // Fast-disk (FDC turbo) QoL mode. Applied AFTER cold_boot() (the FDC/drive
    // reset() does not clear the flag, but set it here for robustness against a
    // future re-cold_boot). Default false => byte-identical accurate FDC timing.
    machine.set_fast_disk(resolved.fast_disk);
    if (resolved.fast_disk) {
        std::cerr << "debug-session: fast-disk (FDC turbo) ENABLED -- disk timing collapsed "
                     "(M46 convenience default; --no-fast-disk / --stock for accurate timing)\n";
    }

    // DEC-0072 causal probe: apply the VDP command-engine CE busy-window bias.
    if (opts.vdp_cmd_bias != 0) {
        machine.vdp().set_cmd_busy_bias(opts.vdp_cmd_bias);
        std::cerr << "debug-session: VDP command-engine CE busy-window bias = " << opts.vdp_cmd_bias
                  << " T-states (DEC-0072 diagnostic)\n";
    }

    // M36-S-d: bind the FM-PAC .sram path BEFORE loading cartridges, so a
    // freshly-inserted FM-PAC loads its battery SRAM on insertion. M50-S3: the
    // resolved path honors CLI --fmpac-sram > XML > auto-derive.
    if (resolved_fmpac_sram.has_value()) {
        machine.set_fmpac_sram_path(*resolved_fmpac_sram);
    }

    // M50-S3: pass the XML softwaredb override (CLI --softwaredb still wins inside
    // the loader); std::nullopt when no config set a non-default path.
    if (const int rc = load_cartridges_from_args(machine, cli_args, resolved_softwaredb); rc != 0) {
        return rc;
    }

    // M46 (DEC-0071): FM-PAC slot-2 auto-load (planner §2.5), the headless
    // analogue of Sdl3App::load_configured_assets(). Runs AFTER the explicit
    // --slotN carts so the occupancy / already-present checks see the final
    // state. Gated by resolved.fmpac_autoload. Every skip is a graceful note --
    // NEVER a boot failure; DEC-0050 "NO S-RAM AVAILABLE" stays correct on skip.
    // M50-S3 (DEC-0077): the auto-load ROM path is config-resolved (XML > the
    // built-in "roms/fmpac.rom"); byte-identical to pre-M50 with no config.
    sony_msx::frontend::FmPacAutoloadOutcome fmpac_outcome =
        sony_msx::frontend::FmPacAutoloadOutcome::NotAttempted;
    if (resolved.fmpac_autoload) {
        const bool slot2_in_use = sony_msx::machine::parse_cartridge_cli(cli_args).slot2.path.has_value();
        if (slot2_in_use) {
            fmpac_outcome = sony_msx::frontend::FmPacAutoloadOutcome::SkippedSlot2InUse;
        } else if (machine.fmpac(1) != nullptr || machine.fmpac(2) != nullptr) {
            fmpac_outcome = sony_msx::frontend::FmPacAutoloadOutcome::SkippedAlreadyPresent;
        } else {
            std::ifstream fmpac_in(fmpac_autoload_rom, std::ios::binary);
            if (!fmpac_in) {
                fmpac_outcome = sony_msx::frontend::FmPacAutoloadOutcome::SkippedAbsent;
                std::cerr << "debug-session: FM-PAC auto-load skipped: " << fmpac_autoload_rom
                          << " not found (boot proceeds; \"NO S-RAM AVAILABLE\" -- DEC-0050)\n";
            } else {
                std::vector<std::uint8_t> fmpac_image((std::istreambuf_iterator<char>(fmpac_in)),
                                                       std::istreambuf_iterator<char>());
                // Bind the .sram BEFORE the insert (restores on load). Default:
                // beside the ROM; a resolved --fmpac-sram/XML path (bound above)
                // overrides (M50-S3: resolved_fmpac_sram already incorporates it).
                if (!resolved_fmpac_sram.has_value()) {
                    machine.set_fmpac_sram_path(fmpac_autoload_rom + ".sram");
                }
                const auto result = machine.load_cartridge(
                    2, sony_msx::devices::cartridge::CartridgeMapperType::FmPac, std::move(fmpac_image));
                if (result != sony_msx::devices::cartridge::CartridgeLoadResult::Ok) {
                    fmpac_outcome = sony_msx::frontend::FmPacAutoloadOutcome::SkippedInvalid;
                    std::cerr << "debug-session: FM-PAC auto-load skipped: " << fmpac_autoload_rom
                              << " invalid (not a 1..4 x 16 KB FM-PAC image)\n";
                } else {
                    fmpac_outcome = sony_msx::frontend::FmPacAutoloadOutcome::LoadedSlot2;
                    std::cerr << "debug-session: FM-PAC auto-loaded into slot 2 from "
                              << fmpac_autoload_rom << "\n";
                }
            }
        }
    }
    (void)fmpac_outcome;  // consumed by the compact session banner below

    // A-M27-3 (headless previously had no --disk flag; SDL3 has one, M26).
    // M35-S2: mirrors Sdl3App::load_configured_assets() with a repeatable
    // disk list. M36: caches ALL disks in memory (deterministic, no runtime
    // I/O) so a scripted --swap-disk-frame can rotate them like the SDL3 F11
    // path. Loads the first disk at boot (AC-S2-1); none if the list is
    // empty (AC-S2-3).
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

    // M46 (DEC-0071): compact headless "This session" banner (planner §2.6,
    // secondary) so the playtest agent / human see the RESOLVED config, and so
    // the two exes are consistent. SDL3's banner is the human-facing artifact;
    // this is the same content in one line block, built from the pure
    // frontend/session_summary.h helpers.
    {
        namespace fe = sony_msx::frontend;
        const int fmpac_slot =
            machine.fmpac(1) != nullptr ? 1 : (machine.fmpac(2) != nullptr ? 2 : 0);
        fe::FmPacBannerInfo fmpac_info;
        fmpac_info.loaded_slot = fmpac_slot;
        fmpac_info.outcome = fmpac_outcome;
        fmpac_info.is_stock = resolved.is_stock;
        fmpac_info.no_fmpac = opts.no_fmpac;
        fmpac_info.autoload_rom_path = fmpac_autoload_rom;
        const bool sram_available = fmpac_slot != 0 && !machine.fmpac_sram_path().empty();
        std::cerr << "debug-session: This session " << fe::format_mode_tag(resolved.is_stock) << "\n"
                  << "  Main RAM  : " << fe::format_ram_line(machine.dram_size()) << "\n"
                  << "  VRAM      : 128 KB\n"
                  << "  Slot 1    : "
                  << (machine.cartridge_slot1().loaded()
                          ? std::string(sony_msx::devices::cartridge::to_string(
                                machine.cartridge_slot1().mapper_type()))
                          : "(empty)")
                  << "\n"
                  << "  Slot 2    : "
                  << (machine.cartridge_slot2().loaded()
                          ? std::string(sony_msx::devices::cartridge::to_string(
                                machine.cartridge_slot2().mapper_type()))
                          : "(empty)")
                  << "\n"
                  << "  FM-PAC    : " << fe::format_fmpac_line(fmpac_info) << "\n"
                  << "  SRAM      : "
                  << fe::format_sram_line(sram_available,
                                          machine.fmpac_sram_path().string())
                  << "\n"
                  << "  Fast-disk : " << (resolved.fast_disk ? "ON" : "OFF (accurate)") << "\n"
                  << "  Disk      : " << (disk_cache.empty() ? "(none)" : opts.disk_paths.front())
                  << "\n";
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
            std::vector<sony_msx::machine::InputScriptEvent> ev = sony_msx::machine::parse_input_script(text);
            // DEC-0072 raster-phase sweep: delay the whole timeline by a constant.
            if (opts.input_shift != 0) {
                for (auto& e : ev) {
                    e.at_tstate += opts.input_shift;
                }
                std::cerr << "debug-session: input timeline shifted by " << opts.input_shift
                          << " T-states (DEC-0072 raster-phase sweep)\n";
            }
            script_player = sony_msx::machine::InputScriptPlayer(std::move(ev));
        } catch (const std::exception& e) {
            std::cerr << "debug-session: malformed --input-script: " << e.what() << "\n";
            return 2;
        }
    }
    // M41-S1: give the player the machine's joystick so JOY= events drive
    // STICK/STRIG. Harmless for KEY-only scripts (never touched) -> the KEY
    // path stays byte-for-byte the pre-M41 behavior.
    script_player.attach_joystick(&machine.joystick());

    // M36 Phase 3: deterministic comprehensive debug-snapshot capture.
    // Default OFF -- only fires when --snapshot <dir> is present, so a run
    // without the flag is byte-for-byte identical to before. The manifest
    // carries the multi-disk index (frontend/headless concept, planner A4)
    // as a caller note.
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

    // DEC-0052 stream-light: arm the lightweight live stream-capture for the
    // whole run (headless analogue of the SDL3 F10 toggle). Armed AFTER
    // debug_root/disk/cart setup so the watchlog + coarse anchors land under
    // the configured root. Default OFF -> no arming, byte-for-byte the
    // pre-DEC-0052 behavior. A step-mode run auto-finalizes on HALT/SP-
    // underflow; a frames-mode run is finalized manually at end-of-run below.
    if (opts.stream_light) {
        machine.set_stream_capture_enabled(true, std::string{}, /*light=*/true);
        std::cerr << "debug-session: stream-light armed: stream_" << machine.stream_capture_id() << "\n";
    }

    // --- M39-A digitized-voice diagnostic (Step 1) + validation (Step 3) ---
    // Both need the port-C bit-7 (1-bit-DAC) edge capture ON; the BEFORE leg
    // (--audio-no-click) keeps it OFF so the click term is exactly 0.
    const bool click_capture =
        (opts.io_observe_name.has_value() || opts.audio_dump_name.has_value()) && !opts.audio_no_click;
    if (click_capture) {
        machine.click_dac().set_capture_enabled(true);
    }
    if ((opts.io_observe_name.has_value() || opts.audio_dump_name.has_value()) && !opts.frames.has_value()) {
        std::cerr << "debug-session: --io-observe/--audio-dump require --frames mode (no output)\n";
    }

    PsgWriteCounter psg_write_counter;
    if (opts.io_observe_name.has_value()) {
        machine.psg().attach_write_observer(&psg_write_counter);
    }
    std::string io_observe_csv;
    std::uint64_t prev_click_edges = 0;
    std::uint64_t prev_psg_vol = 0;
    std::uint64_t prev_psg_total = 0;
    if (opts.io_observe_name.has_value()) {
        io_observe_csv = "frame,click_bit7_edges,psg_vol_writes,psg_total_writes\n";
    }
    // DEC-0072 per-frame CPU fingerprint buffer (opt-in --fingerprint).
    std::string fingerprint_csv;
    if (opts.fingerprint_name.has_value()) {
        fingerprint_csv = "frame,cycles,pc,sp,af,bc,de,hl,ix,iy,r\n";
    }
    // DEC-0072 save-frame detector (--watch-save): per-frame diff of the disk
    // save-slot region (LBA 9..16 = offsets 0x1200..0x2200) to find WHICH frame
    // each slot save lands at (and its first 16 bytes), for bounding the trace.
    std::string watch_save_csv;
    std::vector<std::uint8_t> watch_prev_slots;
    if (opts.watch_save_name.has_value()) {
        watch_save_csv = "frame,cycles,slot,lba,first16,ndiff_vs_prev\n";
    }
    // DEC-0072 physical-DRAM watchpoint: arm BEFORE the frame loop so every
    // in-window write is captured. Off unless --watch-mem was supplied.
    if (opts.watch_mem_name.has_value()) {
        machine.set_mem_watch(opts.watch_mem_lo, opts.watch_mem_hi, opts.watch_mem_from,
                              opts.watch_mem_to);
        std::cerr << "debug-session: mem-watch armed phys=[0x"
                  << sony_msx::machine::debug_format::to_hex(
                         static_cast<std::uint64_t>(opts.watch_mem_lo), 5)
                  << ",0x"
                  << sony_msx::machine::debug_format::to_hex(
                         static_cast<std::uint64_t>(opts.watch_mem_hi), 5)
                  << ") frames[" << opts.watch_mem_from << "," << opts.watch_mem_to << "]\n";
    }

    // Step 3: a MachineAudioMixer mixing the FULL machine audio (the same law +
    // 81-cycle step the SDL3 frontend uses). BATCH mode (BEFORE) mixes once per
    // frame with exact-accounting sample count; --audio-sync (AFTER) produces
    // samples INTERLEAVED with CPU stepping over machine-cycle windows so the
    // PSG sync-before-change voice survives.
    constexpr std::uint64_t kAudioRateHz = 44100;
    constexpr std::uint64_t kAudioSysHz = 3579545;
    const bool audio_dump = opts.audio_dump_name.has_value();
    const bool audio_sync_dump = audio_dump && opts.audio_sync;
    sony_msx::frontend::MachineAudioMixer audio_mixer(kAudioSysHz / kAudioRateHz);
    std::vector<std::int16_t> audio_pcm;
    std::uint64_t audio_emitted = 0;             // BATCH: samples emitted so far
    std::uint64_t audio_sample_index = 1;        // SYNC: next sample's index (boundary = idx*sys/rate)
    std::uint64_t audio_prev_boundary = 0;       // SYNC: previous sample boundary cycle
    std::uint64_t audio_next_boundary = kAudioSysHz / kAudioRateHz;  // SYNC: next boundary
    const auto fmpac_opll = [](sony_msx::devices::cartridge::CartridgeFmPacRom* cart) {
        return cart != nullptr ? &cart->opll() : nullptr;
    };
    if (audio_sync_dump) {
        // Enable the PSG sync-before-change seam (the CONFIRMED Fix B). Aligned
        // to the current machine cycle so absolute-cycle sample boundaries and
        // the sync cursor share the same origin.
        machine.psg().set_audio_sync_enabled(true);
        machine.psg().reset_audio_sync(machine.elapsed_cycles());
    }
    // M39-A Fix B interleaved production: emit every audio sample whose absolute
    // machine-cycle boundary has been reached, weaving per-sample takes into the
    // CPU-step loop so the PSG sync-before-change writes land at their true
    // sub-frame position. Called after each instruction.
    const bool psg_only = opts.audio_psg_only;
    const auto emit_synced_audio = [&](const std::uint32_t frame) {
        const std::uint64_t now = machine.elapsed_cycles();
        while (audio_next_boundary <= now) {
            const std::uint64_t window = audio_next_boundary - audio_prev_boundary;
            const std::array<std::int16_t, 2> smp = audio_mixer.produce_synced_sample(
                machine.psg(),
                psg_only ? sony_msx::frontend::MachineAudioMixer::SccSources{nullptr, nullptr}
                         : sony_msx::frontend::MachineAudioMixer::SccSources{machine.scc_chip(1),
                                                                             machine.scc_chip(2)},
                psg_only ? nullptr : &machine.ym2413(),
                psg_only ? sony_msx::frontend::MachineAudioMixer::FmSources{nullptr, nullptr}
                         : sony_msx::frontend::MachineAudioMixer::FmSources{
                               fmpac_opll(machine.fmpac(1)), fmpac_opll(machine.fmpac(2))},
                (opts.audio_no_click || psg_only) ? nullptr : &machine.click_dac(),
                audio_next_boundary, window);
            const std::uint32_t from = opts.audio_from.value_or(0);
            const std::uint32_t to = opts.audio_to.value_or(*opts.frames);
            if (frame >= from && frame <= to) {
                audio_pcm.push_back(smp[0]);
                audio_pcm.push_back(smp[1]);
            }
            audio_prev_boundary = audio_next_boundary;
            ++audio_sample_index;
            audio_next_boundary = audio_sample_index * kAudioSysHz / kAudioRateHz;
        }
    };

    std::uint32_t steps = 0;
    if (opts.frames.has_value()) {
        // M32 frame-loop mode: the real production drive shape
        // (Sdl3App::run_one_frame() / the DEC-0034 C5-closure loop) -- VBlank
        // interrupts delivered at every boundary, no halt-stop (titles
        // HALT-wait for VBlank). Deterministic: a pure function of the frame
        // count and the input script's cycle stamps.
        const std::uint64_t target = machine.frame_cycles_per_frame();
        // DEC-0072 DIAGNOSTIC (env-gated; default off => unchanged): stop AT the
        // terminal crash-halt (HALT with interrupts disabled => can never wake)
        // instead of stepping idle for the remaining frames, so a bounded
        // --trace-cpu ring retains the DERAILMENT rather than the post-halt spin.
        const bool halt_break_env = [] {
            const char* p = std::getenv("SONY_MSX_HALT_BREAK");
            return p != nullptr && *p != '\0';
        }();
        bool terminal_halt = false;
        for (std::uint32_t frame = 0; frame < *opts.frames && !terminal_halt; ++frame) {
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
            // DEC-0072 replay fidelity: in --input-frame-start mode, apply every
            // due key edge at FRAME-START (before the first instruction), exactly
            // as the live SDL3 poll_and_dispatch_events() did (it ran before the
            // frame's CPU loop and stamped this same frame-start cycle as T). The
            // in-loop apply_due below is then SKIPPED so the edge is not re-timed
            // one instruction late -- making a cycle-identical run bit-faithful to
            // the recording. Default (flag off) keeps the pre-existing
            // after-instruction application byte-for-byte.
            if (opts.input_frame_start) {
                script_player.apply_due(start, machine.keyboard());
            }
            while (machine.elapsed_cycles() - start < target) {
                machine.step_cpu_instruction();
                if (!opts.input_frame_start) {
                    script_player.apply_due(machine.elapsed_cycles(), machine.keyboard());
                }
                ++steps;
                // M39-A Fix B: interleaved sub-frame-accurate audio production.
                if (audio_sync_dump) {
                    emit_synced_audio(frame);
                }
                if (halt_break_env && machine.cpu().state().halted() &&
                    !machine.cpu().state().iff1()) {
                    terminal_halt = true;
                    break;
                }
            }
            machine.on_vsync_boundary();
            // DEC-0072 save-frame detector: compare the disk save-slot region to
            // the previous frame; log any slot whose bytes changed this frame.
            if (opts.watch_save_name.has_value()) {
                const std::vector<std::uint8_t>& disk = machine.disk_image().data();
                constexpr std::size_t kBase = 9u * 512u;   // LBA 9
                constexpr std::size_t kSpan = 8u * 512u;   // slots 9..16
                if (disk.size() >= kBase + kSpan) {
                    if (watch_prev_slots.size() == kSpan) {
                        for (int s = 0; s < 8; ++s) {
                            const std::size_t off = static_cast<std::size_t>(s) * 512u;
                            int nd = 0;
                            for (std::size_t k = 0; k < 512u; ++k) {
                                if (disk[kBase + off + k] != watch_prev_slots[off + k]) ++nd;
                            }
                            if (nd != 0) {
                                std::string first16;
                                for (std::size_t k = 0; k < 16u; ++k) {
                                    first16 += sony_msx::machine::debug_format::to_hex(disk[kBase + off + k], 2);
                                }
                                watch_save_csv += std::to_string(frame) + "," +
                                                  std::to_string(machine.elapsed_cycles()) + "," +
                                                  std::to_string(s + 1) + "," + std::to_string(9 + s) + "," +
                                                  first16 + "," + std::to_string(nd) + "\n";
                            }
                        }
                    }
                    watch_prev_slots.assign(disk.begin() + static_cast<std::ptrdiff_t>(kBase),
                                            disk.begin() + static_cast<std::ptrdiff_t>(kBase + kSpan));
                }
            }
            // DEC-0072 per-frame CPU-state fingerprint (post-boundary), for the
            // headless-vs-SDL3 determinism cross-check and first-divergence hunt.
            if (opts.fingerprint_name.has_value()) {
                const auto& r = machine.cpu().state().regs();
                fingerprint_csv += std::to_string(frame) + "," +
                                   std::to_string(machine.elapsed_cycles()) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.pc, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.sp, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.af, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.bc, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.de, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.hl, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.ix, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.iy, 4) + "," +
                                   sony_msx::machine::debug_format::to_hex(r.r, 2) + "\n";
            }
            // M39-A Step 1: per-frame edge/write-rate diagnostic row (deltas).
            if (opts.io_observe_name.has_value()) {
                const std::uint64_t edges = machine.click_dac().edge_count();
                io_observe_csv += std::to_string(frame) + "," +
                                  std::to_string(edges - prev_click_edges) + "," +
                                  std::to_string(psg_write_counter.vol_writes - prev_psg_vol) + "," +
                                  std::to_string(psg_write_counter.total_writes - prev_psg_total) + "\n";
                prev_click_edges = edges;
                prev_psg_vol = psg_write_counter.vol_writes;
                prev_psg_total = psg_write_counter.total_writes;
            }
            // M39-A Step 3 (BATCH / BEFORE leg only -- the interleaved AFTER leg
            // produces samples inside the step loop above): mix this frame's
            // audio (exact-accounting count) to keep every generator -- incl. the
            // 1-bit DAC -- in lockstep; append only within the frame window.
            if (audio_dump && !audio_sync_dump) {
                const std::uint64_t total_target = machine.elapsed_cycles() * kAudioRateHz / kAudioSysHz;
                const std::size_t n =
                    total_target > audio_emitted ? static_cast<std::size_t>(total_target - audio_emitted) : 0;
                audio_emitted += n;
                std::vector<std::int16_t> frame_pcm = audio_mixer.mix_interleaved_stereo(
                    machine.psg(),
                    psg_only ? sony_msx::frontend::MachineAudioMixer::SccSources{nullptr, nullptr}
                             : sony_msx::frontend::MachineAudioMixer::SccSources{machine.scc_chip(1),
                                                                                 machine.scc_chip(2)},
                    psg_only ? nullptr : &machine.ym2413(),
                    psg_only ? sony_msx::frontend::MachineAudioMixer::FmSources{nullptr, nullptr}
                             : sony_msx::frontend::MachineAudioMixer::FmSources{
                                   fmpac_opll(machine.fmpac(1)), fmpac_opll(machine.fmpac(2))},
                    (opts.audio_no_click || psg_only) ? nullptr : &machine.click_dac(), n);
                const std::uint32_t from = opts.audio_from.value_or(0);
                const std::uint32_t to = opts.audio_to.value_or(*opts.frames);
                if (frame >= from && frame <= to) {
                    audio_pcm.insert(audio_pcm.end(), frame_pcm.begin(), frame_pcm.end());
                }
            }
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

    // DEC-0052 stream-light: finalize a still-armed capture (a frames-mode run
    // that never HALTed / underflowed the stack). Dumps the trace ring + a final
    // MANUAL_ snapshot; the watch.log was appended incrementally during the run.
    if (opts.stream_light && machine.stream_capture_active()) {
        machine.set_stream_capture_enabled(false);
        std::cerr << "debug-session: stream-light finalized: stream_" << machine.stream_capture_id()
                  << " (watch.log under " << machine.debug_root().string() << "/traces)\n";
    }

    // M36-S-c: persist any pending writable-disk writes back to the host .dsk
    // (opt-in only; no-op when --disk-writable was absent -> no host path).
    if (opts.disk_writable && !disk_cache.empty()) {
        if (machine.disk_image().flush()) {
            std::cerr << "debug-session: flushed writable disk to \""
                      << opts.disk_paths[current_disk_index] << "\"\n";
        }
    }

    // M36-S-d / M46: persist the FM-PAC battery SRAM back to its .sram file --
    // whether the FM-PAC came from an explicit --slotN cart or the M46 slot-2
    // auto-load. flush_fmpac_sram() is a no-op (returns false) unless an FM-PAC
    // is inserted AND a path was bound, so a bare/--stock/--no-fmpac run never
    // writes anything (DEC-0050).
    if (machine.flush_fmpac_sram()) {
        std::cerr << "debug-session: flushed FM-PAC SRAM to \"" << machine.fmpac_sram_path().string()
                  << "\"\n";
    }

    // M39-A Step 1: write the per-frame edge/write-rate diagnostic CSV.
    if (opts.io_observe_name.has_value()) {
        std::ofstream csv(*opts.io_observe_name, std::ios::binary | std::ios::trunc);
        if (!csv) {
            std::cerr << "debug-session: cannot write --io-observe " << *opts.io_observe_name << "\n";
            return 2;
        }
        csv.write(io_observe_csv.data(), static_cast<std::streamsize>(io_observe_csv.size()));
        std::cerr << "debug-session: wrote io-observe CSV " << *opts.io_observe_name
                  << " total_click_bit7_edges=" << prev_click_edges
                  << " total_psg_vol_writes=" << prev_psg_vol
                  << " total_psg_writes=" << prev_psg_total << "\n";
    }

    // DEC-0072 per-frame CPU fingerprint dump.
    if (opts.fingerprint_name.has_value()) {
        std::ofstream fp(*opts.fingerprint_name, std::ios::binary | std::ios::trunc);
        if (!fp) {
            std::cerr << "debug-session: cannot write --fingerprint " << *opts.fingerprint_name << "\n";
            return 2;
        }
        fp.write(fingerprint_csv.data(), static_cast<std::streamsize>(fingerprint_csv.size()));
        std::cerr << "debug-session: wrote fingerprint CSV " << *opts.fingerprint_name << "\n";
    }

    // DEC-0072 save-frame detector dump.
    if (opts.watch_save_name.has_value()) {
        std::ofstream ws(*opts.watch_save_name, std::ios::binary | std::ios::trunc);
        if (ws) {
            ws.write(watch_save_csv.data(), static_cast<std::streamsize>(watch_save_csv.size()));
            std::cerr << "debug-session: wrote save-frame log " << *opts.watch_save_name << "\n";
        }
    }

    // DEC-0072 physical-DRAM watchpoint dump.
    if (opts.watch_mem_name.has_value()) {
        const std::string& log = machine.mem_watch_log();
        std::ofstream wm(*opts.watch_mem_name, std::ios::binary | std::ios::trunc);
        if (wm) {
            wm.write(log.data(), static_cast<std::streamsize>(log.size()));
            std::cerr << "debug-session: wrote mem-watch CSV " << *opts.watch_mem_name
                      << " (dropped_over_cap=" << machine.mem_watch_dropped() << ")\n";
        }
    }

    // M39-A Step 3: write the voice-window full-machine audio dump.
    if (audio_dump) {
        if (!sony_msx::frontend::write_pcm_audio_dump(machine.debug_root(), *opts.audio_dump_name,
                                                      audio_pcm, kAudioRateHz)) {
            std::cerr << "debug-session: failed to write --audio-dump " << *opts.audio_dump_name << "\n";
            return 2;
        }
        std::cerr << "debug-session: wrote audio dump " << machine.debug_root().string() << "/sounds/"
                  << *opts.audio_dump_name << " samples=" << (audio_pcm.size() / 2)
                  << (opts.audio_no_click ? " (no-click/BEFORE)" : " (click on/AFTER)") << "\n";
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
    // DEC-0072 diagnostic: did the run exercise the VDP command-engine color/LMCM path?
    std::cerr << "debug-session: vdp S#7-color-reads=" << machine.vdp().cmd_engine().color_read_count()
              << " LMCM-issues=" << machine.vdp().cmd_engine().lmcm_start_count() << "\n";
    return 0;
}

// M27-S5 headless PSG audio-dump demo (docs/m27-planner-package.md §2.3
// point 3, mirrors --frame-dump-demo's precedent). Programs a fixed tone on
// PSG channel A via the real public register-write API
// (write_address()/write_data() -- #A0/#A1, the same ports the CPU/IoBus
// itself uses; PsgYm2149::write_register() is a private implementation
// detail, psg_ym2149.h, not directly callable here), then dumps a fixed
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
    // disabled (bits1-2=1), all noise disabled (bits3-5=1). Bits 6/7 (I/O
    // direction) are forced by the device itself regardless of what is
    // written here (PsgYm2149::write_register()'s R_ENABLE handling).
    psg.write_address(7);
    psg.write_data(0x3E);
    // R8: channel A volume -- fixed level 15 (max), resolved_amplitude() ==
    // 2*15+1 == 31 (A-M27-4's documented maximum).
    psg.write_address(8);
    psg.write_data(15);

    constexpr std::uint64_t kSampleRateHz = 44100;  // A-M27-5.
    constexpr std::size_t kSampleCount = 44100;     // 1 second, an audibly non-trivial length.
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
// single-steps up to <max_steps> instructions, exporting the per-instruction
// trace for a diff against openMSX's reset-step trace. No map_flat_ram: this
// is the real BIOS execution from slot 0.
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
// machine, loads it via the generic CpmBdosHarness (src/machine/
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
    // Exit 0 only on a genuine CP/M warm-boot completion, never on budget
    // exhaustion.
    return run_result.finished ? 0 : 3;
}

int main(int argc, char** argv) {
    // Full argv (minus argv[0]) for the M19 cartridge CLI (--cart1/
    // --cart1-type/--cart2/--cart2-type), parsed order-independently in
    // either the default run or --parity-trace mode.
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
                      << " --debug-session <bios_dir> <max_steps> [--disk <path>] [--slot1 <path>]"
                         " [--slot1-type <T>|auto] [--slot2 <path>] [--slot2-type <T>|auto]"
                         " [--softwaredb <path>] [--debug-root <path>]"
                         " [--dump-state <name>] [--trace-cpu <name>] [--event-log <name>]"
                         " [--input-script <path>] [--frames <N>] [--dump-frame <name>]"
                         " [--ram <64|128|256|512>] [--stock] [--fast-disk] [--no-fast-disk]"
                         " [--no-fmpac] [--disk-writable] [--swap-disk-frame <N>] [--fmpac-sram <path>]"
                         " [--snapshot <dir>] [--snapshot-frame <N>]\n"
                         "  (--cart1/--cart2/--cartN-type are accepted as silent aliases for --slotN.)\n"
                         "  M46 (DEC-0071) convenience defaults: RAM 512 KB, fast-disk ON, and an\n"
                         "  FM-PAC auto-loaded into slot 2 from roms/fmpac.rom (SRAM roms/fmpac.rom.sram,\n"
                         "  skipped gracefully if absent). --stock = the authentic bare machine\n"
                         "  (RAM 64 + accurate disk + no FM-PAC); --no-fast-disk / --no-fmpac / --ram\n"
                         "  override individual fields (order-independent). --fast-disk collapses the\n"
                         "  WD2793/floppy timing (near-instant loads) and MAY affect rare\n"
                         "  copy-protected disks; --no-fast-disk restores 100% cycle-accurate timing.\n"
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

    // M42 (DEC-0061): optional --ram <64|128|256|512> main-RAM size (KB) --
    // headless parity with the SDL3 frontend's --ram. Parsed BEFORE the machine
    // is constructed (its DRAM is sized at construction). Absent -> the stock
    // 64 KB spec (kDramBytes), byte-identical to before. Uses the SHARED
    // parse_ram_kb validator (ONE {64,128,256,512} enum policy across both exes);
    // an out-of-range/non-numeric/missing value is a loud parse error + non-zero
    // exit (mirrors the --speed policy below). 128/256/512 are opt-in NON-STOCK
    // "fully-populated S1985" sizes; 512 KB is the internal ceiling.
    std::size_t ram_bytes = sony_msx::machine::Hbf1xvMachine::kDramBytes;
    {
        std::vector<std::string> ram_errors;
        std::optional<int> ram_kb;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i] != "--ram") {
                continue;
            }
            if (i + 1 >= args.size()) {
                ram_errors.push_back("main: --ram requires a value argument");
                break;
            }
            int kb = 0;
            if (sony_msx::frontend::parse_ram_kb(args[i + 1], kb, ram_errors, "main")) {
                ram_kb = kb;
            }
            ++i;
        }
        for (const std::string& err : ram_errors) {
            std::cerr << err << "\n";
        }
        if (!ram_errors.empty()) {
            return 2;
        }
        if (ram_kb.has_value()) {
            ram_bytes = static_cast<std::size_t>(*ram_kb) * 1024u;
        }
    }

    sony_msx::machine::Hbf1xvMachine machine(ram_bytes);
    machine.cold_boot();
    if (const int rc = load_cartridges_from_args(machine, args); rc != 0) {
        return rc;
    }

    // M37 Slice D (DEC-0056): optional --speed <0..7> launch-time initial Sony
    // Speed Controller level -- headless parity with the SDL3 frontend. Uses
    // the same shared validator (parse_speed_level, one range policy) and is
    // applied AFTER cold_boot() (which resets the controller to level 0),
    // before the run loop below. Absent --speed leaves the controller
    // untouched (level 0, full speed), byte-identical to before. An
    // out-of-range/non-numeric/missing value is a loud parse error +
    // non-zero exit (mirrors the --max-frames policy).
    {
        std::vector<std::string> speed_errors;
        std::optional<int> speed_level;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i] != "--speed") {
                continue;
            }
            if (i + 1 >= args.size()) {
                speed_errors.push_back("main: --speed requires a value argument");
                break;
            }
            int level = 0;
            if (sony_msx::frontend::parse_speed_level(args[i + 1], level, speed_errors, "main")) {
                speed_level = level;
            }
            ++i;
        }
        for (const std::string& err : speed_errors) {
            std::cerr << err << "\n";
        }
        if (!speed_errors.empty()) {
            return 2;
        }
        if (speed_level.has_value()) {
            machine.pause_controller().set_speed_level(*speed_level);
        }
    }

    machine.run_frame();

    std::cout << "sony-msx-hbf1xv headless scaffold\n";
    std::cout << "elapsed_cycles=" << machine.elapsed_cycles() << "\n";
    std::cout << "frame_count=" << machine.frame_count() << "\n";
    std::cout << "frame_cycles_per_frame=" << machine.frame_cycles_per_frame() << "\n";

    return 0;
}
