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

// Suite: Machine_Hbf1xvBootLogo_Integration  (boot-logo fix, targeted defect cycle)
//
// End-to-end regression guard for the MSX2+ animated boot logo: a real-BIOS
// cold boot must (1) present the #F4 reset-status cold flag (0x00), (2)
// enter the SCREEN 6 (GRAPHIC5) logo phase, (3) LEAVE it for the SCREEN 1
// (GRAPHIC1) BASIC screen within the openMSX-matched time window, and (4)
// end with #F4 == 0x80 (the BIOS's own post-logo write). Pre-fix behavior,
// for the record: #F4 read open-bus 0xFF, the BIOS took the warm-restart
// path and the GRAPHIC5 phase never occurred at all; two further defects
// (frame-granular S#0 collision re-latch and the missing pre-armed R#44
// transfer unit) then each froze the logo mid-way once #F4 was fixed --
// this test pins the WHOLE sequence.
//
// Ground truth (openMSX 19.1 Sony_HB-F1XV, natural boot, WSL): logo visible
// t=1.25s..4.0s, BASIC (GRAPHIC1, R#7=0x07) from t=4.5s, #F4 00 -> 0x80.
// This machine, post-fix: GRAPHIC5 entry ~frame 75, exit to GRAPHIC1 ~frame
// 270 (4.5 s) -- asserted below with generous deterministic bounds.
//
// Deterministic oracle: two independent cold-boot runs produce the
// identical (entry, exit, final-#F4) triple and a byte-identical rendered
// frame at the fixed sampling frame.

#include <cstdint>
#include <iostream>
#include <string>

#include "devices/video/vdp_mode.h"
#include "machine/frame_dump.h"
#include "machine/hbf1xv_machine.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

using sony_msx::devices::video::VdpMode;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

struct BootResult {
    std::uint8_t f4_at_cold = 0xEE;
    std::uint8_t f4_at_end = 0xEE;
    int logo_entry_frame = -1;  // first frame in GRAPHIC5
    int logo_exit_frame = -1;   // first frame back in GRAPHIC1 after the logo
    std::string sampled_frame_dump;  // serialized frame at kSampleFrame
};

constexpr int kTotalFrames = 340;
constexpr int kSampleFrame = 300;  // inside the BASIC screen, post-logo

BootResult boot_and_observe() {
    BootResult result;
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    result.f4_at_cold = machine.debug_io_read(0xF4);

    const std::uint64_t target = machine.frame_cycles_per_frame();
    for (int frame = 1; frame <= kTotalFrames; ++frame) {
        const std::uint64_t frame_start_cycle = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - frame_start_cycle < target) {
            machine.step_cpu_instruction();
        }
        machine.on_vsync_boundary();

        const VdpMode mode = machine.vdp().mode().mode;
        if (result.logo_entry_frame < 0 && mode == VdpMode::Graphic5) {
            result.logo_entry_frame = frame;
        }
        if (result.logo_entry_frame >= 0 && result.logo_exit_frame < 0 && mode == VdpMode::Graphic1) {
            result.logo_exit_frame = frame;
        }
        if (frame == kSampleFrame) {
            result.sampled_frame_dump =
                sony_msx::machine::frame_dump::serialize_frame_dump(machine.render_frame());
        }
    }

    result.f4_at_end = machine.debug_io_read(0xF4);
    return result;
}

}  // namespace

int main() {
    // --- #F4 device wiring shape (fast, no BIOS execution needed). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(machine.debug_io_read(0xF4) == 0x00, "F4_ColdBoot_ReadsZero_NotOpenBusFF");
        machine.debug_io_write(0xF4, 0xFF);
        expect(machine.debug_io_read(0xF4) == 0xA0, "F4_WriteMask_OnlyBits7And5");
        machine.cold_boot();
        expect(machine.debug_io_read(0xF4) == 0x00, "F4_PowerCycle_ClearsAgain");
    }

    // --- The full boot-logo sequence against the real BIOS. ---
    const BootResult a = boot_and_observe();

    expect(a.f4_at_cold == 0x00, "Boot_F4ColdFlagPresented");
    expect(a.logo_entry_frame > 0, "Boot_LogoPhaseEntered_Graphic5");
    expect(a.logo_entry_frame > 0 && a.logo_entry_frame <= 150,
           "Boot_LogoEntry_WithinExpectedWindow");
    expect(a.logo_exit_frame > 0, "Boot_LogoPhaseExits_ToGraphic1Basic");
    expect(a.logo_exit_frame > 0 && a.logo_exit_frame >= 200 && a.logo_exit_frame <= 330,
           "Boot_LogoExit_MatchesOpenMsxWindow_About4Point5Seconds");
    expect(a.f4_at_end == 0x80, "Boot_BiosWroteF4Bit7_AfterLogo_MatchesOpenMsx");

    // --- Determinism: a second cold boot reproduces everything byte-for-
    //     byte. ---
    const BootResult b = boot_and_observe();
    expect(a.f4_at_cold == b.f4_at_cold && a.f4_at_end == b.f4_at_end, "Determinism_F4Values");
    expect(a.logo_entry_frame == b.logo_entry_frame && a.logo_exit_frame == b.logo_exit_frame,
           "Determinism_LogoPhaseFrames");
    expect(!a.sampled_frame_dump.empty() && a.sampled_frame_dump == b.sampled_frame_dump,
           "Determinism_SampledFrameByteIdentical");

    std::cerr << "boot-logo checkpoint: entry=" << a.logo_entry_frame << " exit=" << a.logo_exit_frame
              << " f4_end=0x" << std::hex << static_cast<int>(a.f4_at_end) << std::dec << "\n";

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvBootLogo_Integration cases passed\n";
    return 0;
}
