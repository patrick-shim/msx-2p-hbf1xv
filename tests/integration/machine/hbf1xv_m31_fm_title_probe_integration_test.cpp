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

// Suite: Machine_Hbf1xvM31FmTitleProbe_Integration (M31-S6, RC criterion
// 7(f) probe + A-M31-1/A-M31-2 verifications, docs/m31-planner-package.md
// §1.3/§4)
//
// 1. A-M31-1: bios/f1xvmus.rom (slot 3-3 FM-MUSIC ROM) carries the
//    internal-MSX-MUSIC "APRLOPLL" ID at ROM offset 0x18-0x1F (maps to CPU
//    0x4018, fact-sheet §2) -- asserted directly on the local asset bytes.
// 2. A-M31-2 (the FM-activity probe on roms/scroll-shooter.rom, a
//    scrolling-shooter title): a scripted 900-
//    frame boot/attract run (the M29 bonus-smoke shape: SPACE held at
//    frames 300-314 and 600-614) polling the YM2413 register file at every
//    frame boundary for $10-$38 register-change observations and key-on
//    edges. HONEST PROBE, NON-GATING on the outcome: whether the
//    scroll-shooter title exercises FM is an UNVERIFIED belief about
//    third-party software; the
//    RC criterion 7(f) has an explicit synthetic-demo fallback if no
//    available title exercises FM. The probe result is printed as evidence
//    and recorded in the implementation report either way.
//
// SKIP DISCIPLINE: skips (never fails) without the BIOS/ROM assets or on a
// different scroll-shooter.rom dump (A-M30-1 precedent).
//
// Deterministic oracle: the probe counters of two identical runs agree
// (fixed frame count, fixed input script, no wall clock).

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "machine/hbf1xv_machine.h"
#include "machine/sha1.h"

#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr const char* kScrollShooterSha1 = "e93d0840c59c6eba273df546d22148d486a150a6";

struct ProbeResult {
    long reg_change_observations = 0;  // changed $00-$38 bytes across frame boundaries
    long key_on_edges = 0;
    int frames = 0;
};

ProbeResult run_probe(const std::vector<std::uint8_t>& image) {
    using sony_msx::devices::cartridge::CartridgeMapperType;
    using sony_msx::machine::Hbf1xvMachine;

    ProbeResult result;
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    if (machine.load_cartridge(1, CartridgeMapperType::KonamiSCC, image) !=
        sony_msx::devices::cartridge::CartridgeLoadResult::Ok) {
        return result;
    }

    const std::uint64_t target = machine.frame_cycles_per_frame();
    std::uint8_t prev_regs[0x39] = {};
    bool prev_keys[10] = {};

    for (int frame = 0; frame < 900; ++frame) {
        // The M29 bonus-smoke scripted input: SPACE (row 8, bit 0).
        if ((frame >= 300 && frame < 315) || (frame >= 600 && frame < 615)) {
            machine.keyboard().set_key(8, 0, true);
        } else {
            machine.keyboard().set_key(8, 0, false);
        }
        const std::uint64_t frame_start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - frame_start < target) {
            machine.step_cpu_instruction();
        }
        machine.on_vsync_boundary();
        ++result.frames;

        const auto& fm = machine.ym2413();
        for (int reg = 0; reg <= 0x38; ++reg) {
            const std::uint8_t value = fm.register_value(static_cast<std::uint8_t>(reg));
            if (frame > 0 && value != prev_regs[reg]) {
                ++result.reg_change_observations;
            }
            prev_regs[reg] = value;
        }
        bool keys[10] = {};
        for (int channel = 0; channel < 9; ++channel) {
            keys[channel] = (fm.register_value(static_cast<std::uint8_t>(0x20 + channel)) & 0x10) != 0;
        }
        keys[9] = fm.rhythm_enabled() && (fm.register_value(0x0E) & 0x1F) != 0;
        for (int k = 0; k < 10; ++k) {
            if (keys[k] && !prev_keys[k]) {
                ++result.key_on_edges;
            }
            prev_keys[k] = keys[k];
        }
    }
    return result;
}

}  // namespace

int main() {
    // --- 1. A-M31-1: the internal-MSX-MUSIC ID string in the local
    //     FM-MUSIC ROM asset. ---
    const std::filesystem::path mus_path =
        std::filesystem::path(SONY_MSX_BIOS_DIR) / "f1xvmus.rom";
    {
        std::ifstream mus_in(mus_path, std::ios::binary);
        if (!mus_in) {
            std::cout << "SKIP: " << mus_path << " not present (local dev asset)\n";
            return 0;
        }
        const std::vector<std::uint8_t> mus((std::istreambuf_iterator<char>(mus_in)),
                                            std::istreambuf_iterator<char>());
        const std::string expected = "APRLOPLL";
        bool id_matches = mus.size() >= 0x20;
        for (std::size_t i = 0; id_matches && i < expected.size(); ++i) {
            if (mus[0x18 + i] != static_cast<std::uint8_t>(expected[i])) {
                id_matches = false;
            }
        }
        expect(id_matches, "AM31_1_F1xvmusRom_APRLOPLL_IdStringAt0x18");
    }

    // --- 2. A-M31-2: the FM-activity probe on the scroll-shooter title (skip-gated). ---
    const std::filesystem::path rom_path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "scroll-shooter.rom";
    std::ifstream rom_in(rom_path, std::ios::binary);
    if (!rom_in) {
        std::cout << "SKIP(probe): " << rom_path << " not present -- A-M31-1 result above stands\n";
        return g_failures == 0 ? 0 : 1;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(rom_in)),
                                          std::istreambuf_iterator<char>());
    if (sony_msx::machine::sha1_hex(image) != kScrollShooterSha1) {
        std::cout << "SKIP(probe): scroll-shooter.rom is a different dump -- A-M31-1 result above stands\n";
        return g_failures == 0 ? 0 : 1;
    }

    const ProbeResult first = run_probe(image);
    expect(first.frames == 900, "Probe_Ran900Frames");
    std::cout << "SCROLL-SHOOTER FM probe: reg_change_observations=" << first.reg_change_observations
              << " key_on_edges=" << first.key_on_edges << " frames=" << first.frames << "\n";
    if (first.key_on_edges > 0) {
        std::cout << "PROBE OUTCOME: the scroll-shooter title DID exercise the YM2413 (A-M31-2 confirmed; RC "
                     "criterion 7(f) real-title path available)\n";
    } else {
        std::cout << "PROBE OUTCOME: no YM2413 key-ons observed in this scripted window "
                     "(honest outcome; RC criterion 7(f) synthetic-demo fallback applies)\n";
    }

    // --- Determinism of the probe itself. ---
    const ProbeResult second = run_probe(image);
    expect(second.reg_change_observations == first.reg_change_observations &&
               second.key_on_edges == first.key_on_edges && second.frames == first.frames,
           "Probe_TwoRuns_IdenticalCounters");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM31FmTitleProbe_Integration cases passed\n";
    return 0;
}
