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
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvDebugSession_Integration (M27-S1, "Production
// Hardening + Debug/Test Tooling" item 1, docs/m27-planner-package.md §2.2).
//
// Proves the DEVICE-LEVEL loop the new headless `--debug-session` mode
// (src/main.cpp) and the SDL3-side wiring both drive is genuinely correct:
// a real-BIOS-asset-loaded, bounded, halt-respecting step_cpu_instruction()
// loop is deterministic (A-M27-1/A-M27-2's own confirmed finding that the
// pre-existing default headless run path drives neither), and the new
// --disk flag's underlying mechanism (A-M27-3, mirrors A-M26-6) mounts a
// real disk image correctly. main.cpp's own run_debug_session()/
// run_parity_trace() functions are internal-linkage (anonymous namespace)
// and not link-testable directly -- this test instead exercises the SAME
// public Hbf1xvMachine seams those functions call, mirroring
// hbf1xv_bios_boot_integration_test.cpp's (M13) own established pattern of
// testing the underlying mechanism rather than main()'s argv parsing.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_DISKS_DIR
#error "SONY_MSX_DISKS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

constexpr std::uint32_t kBoundedSteps = 300;

struct RunResult {
    std::uint16_t final_pc = 0;
    std::uint64_t elapsed_cycles = 0;
    bool halted = false;
};

// Mirrors the new --debug-session mode's own loop shape EXACTLY (§2.2): real
// BIOS asset loading via set_asset_root()+cold_boot(), then a bounded,
// halt-respecting step_cpu_instruction() loop -- the SAME "stop stepping
// exactly at the halt boundary" convention every other call site in this
// project uses.
RunResult run_bounded_debug_session(const std::uint32_t max_steps) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        ++steps;
    }

    RunResult result;
    result.final_pc = machine.cpu().state().regs().pc;
    result.elapsed_cycles = machine.elapsed_cycles();
    result.halted = machine.cpu().state().halted();
    return result;
}

}  // namespace

int main() {
    // --- Case 1: A-M27-1/A-M27-2 resolved -- a bounded, real-BIOS-loaded
    // CPU-driven run genuinely advances the machine (unlike the pre-existing
    // default headless run path, which never calls step_cpu_instruction()
    // nor set_asset_root() at all). Two independent runs are byte-for-byte
    // deterministic (elapsed_cycles + final PC), mirroring the M13 boot-
    // checkpoint precedent's own "two-run identical" discipline. ---
    {
        const RunResult run_a = run_bounded_debug_session(kBoundedSteps);
        const RunResult run_b = run_bounded_debug_session(kBoundedSteps);

        expect(run_a.final_pc != 0x0000, "BoundedSession_CpuGenuinelyAdvanced_PcLeftResetVector");
        expect(run_a.elapsed_cycles > 0, "BoundedSession_ElapsedCyclesGenuinelyAdvanced");
        expect(!run_a.halted, "BoundedSession_DidNotHaltWithinBudget");
        expect(run_a.final_pc == run_b.final_pc, "BoundedSession_TwoRuns_IdenticalFinalPc");
        expect(run_a.elapsed_cycles == run_b.elapsed_cycles, "BoundedSession_TwoRuns_IdenticalElapsedCycles");
    }

    // --- Case 2: A-M27-3 (headless --disk flag mechanism, mirrors A-M26-6
    // verbatim) -- a real committed disk image's bytes are byte-readable
    // through the mounted DiskImage/DiskDrive after the SAME
    // assign+attach_image() sequence run_debug_session() uses. ---
    // Post-reorg asset layout: disks/ standardized on msxdos23.dsk (msxdos22.dsk
    // removed). Any real MSX-DOS system disk exercises this mechanism identically
    // (the case checks the mounted image against the file's OWN bytes, no
    // disk-specific size/content constant). disks/ content is untracked, so this
    // case SKIPS (never FAILS) when the asset is absent.
    {
        const std::string disk_path = std::string(SONY_MSX_DISKS_DIR) + "/msxdos23.dsk";
        std::ifstream in(disk_path, std::ios::binary);
        if (!in) {
            std::cout << "SKIP (Case 2, --disk mechanism): " << disk_path
                      << " not present -- skip-when-absent asset guard\n";
        } else {
            const std::vector<std::uint8_t> real_bytes((std::istreambuf_iterator<char>(in)),
                                                        std::istreambuf_iterator<char>());
            expect(!real_bytes.empty(), "DiskFlag_RealAssetFileNonEmpty");

            sony_msx::machine::Hbf1xvMachine machine;
            machine.set_asset_root(SONY_MSX_BIOS_DIR);
            machine.cold_boot();

            machine.disk_image() = sony_msx::devices::fdc::DiskImage(real_bytes);
            machine.disk_drive().attach_image(&machine.disk_image());

            expect(machine.disk_drive().image() == &machine.disk_image(),
                   "DiskFlag_DriveAttachedToNewlyAssignedImage");
            expect(machine.disk_image().size() == real_bytes.size(), "DiskFlag_ImageSize_MatchesRealFile");

            bool bytes_match = true;
            for (std::size_t i = 0; i < real_bytes.size(); ++i) {
                if (machine.disk_image().byte_at(static_cast<std::uint32_t>(i)) != real_bytes[i]) {
                    bytes_match = false;
                    break;
                }
            }
            expect(bytes_match, "DiskFlag_ImageContent_ByteIdenticalToRealFile");

            // Byte-readable through the FDC's own sector-access API too (not
            // merely the raw DiskImage byte_at() accessor) -- LBA0 = track0/
            // side0/sector1, the FAT12 boot sector.
            std::array<std::uint8_t, sony_msx::devices::fdc::DiskImage::kSectorSize> sector{};
            const bool read_ok = machine.disk_drive().read_sector(1, sector.data());
            expect(read_ok, "DiskFlag_DiskDrive_ReadSector1Succeeds");
            bool sector_matches = read_ok;
            for (std::size_t i = 0; sector_matches && i < sector.size(); ++i) {
                if (sector[i] != real_bytes[i]) {
                    sector_matches = false;
                }
            }
            expect(sector_matches, "DiskFlag_ReadSector1_ByteIdenticalToRealFileHead");
        }
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvDebugSession_Integration cases passed\n";
    return 0;
}
