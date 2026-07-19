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

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_command_engine.h"
#include "devices/video/vdp_vram.h"
#include "machine/cpm_bdos_harness.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvM24CpmRun_Integration (M24-S2, backlog C3 -- ZEXDOC/
// ZEXALL full parity sweep).
//
// Drives the SAME synthetic fixture as tests/unit/machine/
// cpm_bdos_harness_unit_test.cpp, but through the FULL Hbf1xvMachine +
// CpmBdosHarness API (not bypassing the machine), proving the machine wiring
// end-to-end. Additionally asserts the device-isolation invariant (planner
// package SS1.3 A-M24-8 / Acceptance Criterion 6): this CPU-only exerciser
// never issues IN/OUT instructions and never calls run_frame(), so PSG/VDP/
// PPI/RTC/FDC device state taken immediately after cold_boot()+map_flat_ram()
// must be byte-for-byte identical to a snapshot taken again after the harness
// run completes.

namespace {

using sony_msx::machine::CpmBdosHarness;
using sony_msx::machine::Hbf1xvMachine;

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Identical to multi_bdos_fixture() in cpm_bdos_harness_unit_test.cpp -- see
// that file for the full byte-by-byte layout comment. Duplicated here (not
// shared via a header) so this integration test stays self-contained and
// mirrors this project's existing per-file fixture convention.
std::vector<std::uint8_t> multi_bdos_fixture() {
    return {
        0x11, 0x21, 0x01,  // LD DE,0x0121
        0x0E, 0x09,        // LD C,9
        0xCD, 0x05, 0x00,  // CALL 5           (1st: C=9 "HI")
        0x1E, 0x21,        // LD E,'!'
        0x0E, 0x02,        // LD C,2
        0xCD, 0x05, 0x00,  // CALL 5           (2nd: C=2 '!')
        0x11, 0x24, 0x01,  // LD DE,0x0124
        0x0E, 0x09,        // LD C,9
        0xCD, 0x05, 0x00,  // CALL 5           (3rd: C=9 "BYE")
        0x1E, 0x3F,        // LD E,'?'
        0x0E, 0x02,        // LD C,2
        0xCD, 0x05, 0x00,  // CALL 5           (4th: C=2 '?')
        0xC3, 0x00, 0x00,  // JP 0             (warm boot)
        'H', 'I', 0x24,           // "HI$"  at 0x0121
        'B', 'Y', 'E', 0x24,      // "BYE$" at 0x0124
    };
}

// Non-perturbing snapshot of every PSG/VDP/PPI/RTC/FDC introspection point
// this project's machine exposes (M24-S2 device-isolation invariant). Every
// accessor consulted here is documented as a debug/test, non-side-effecting
// "peek" -- none of them are the CPU-visible port paths (#A0-A2, #98-#9B,
// #A8-#AB, #B4/#B5, the Sony FDC window) an actual IN/OUT instruction would
// use, so calling this function itself never perturbs the very state it is
// observing.
std::vector<std::uint8_t> capture_device_snapshot(const Hbf1xvMachine& machine) {
    std::vector<std::uint8_t> snapshot;

    const auto& psg = machine.psg();
    for (int reg = 0; reg < 16; ++reg) {
        snapshot.push_back(psg.register_value(static_cast<std::uint8_t>(reg)));
    }

    const auto& vdp = machine.vdp();
    for (int r = 0; r < sony_msx::devices::video::V9958Vdp::kNumControlRegs; ++r) {
        snapshot.push_back(vdp.control_register(r));
    }
    for (int r = 0; r < 15; ++r) {  // R#32-46, the VdpCommandEngine's own file
        snapshot.push_back(vdp.cmd_engine().read_register(r));
    }
    for (int s = 0; s < sony_msx::devices::video::V9958Vdp::kNumStatusRegs; ++s) {
        std::uint8_t value = vdp.peek_status_register(s);
        if (s == 2) {
            // Bug fix (post-M28): S#2 bits 6/5 (VR/HR) are now LIVE,
            // genuinely elapsed-cycles-since-vsync-derived (previously
            // hardcoded 0 -- the fix for a real BIOS boot hang). This test
            // runs 10,000 real CPU instructions between the two snapshots
            // via a flat-RAM driver program that never calls
            // on_vsync_boundary(), so elapsed time genuinely advances and
            // VR/HR MAY legitimately differ between "before" and "after" --
            // that is correct, intended behavior, not a device-isolation
            // violation (the CP/M harness never issues a single VDP I/O
            // instruction, still verified by every OTHER bit/register/VRAM
            // byte in this snapshot). Masked out here so this invariant
            // continues to test what it actually means to test: no
            // CROSS-DEVICE STATE LEAKAGE from the CP/M harness itself.
            value = static_cast<std::uint8_t>(value & ~0x60);
        }
        snapshot.push_back(value);
    }
    for (int i = 0; i < sony_msx::devices::video::V9958Vdp::kNumPaletteEntries; ++i) {
        const std::uint16_t entry = vdp.palette_entry(i);
        snapshot.push_back(static_cast<std::uint8_t>(entry & 0xFF));
        snapshot.push_back(static_cast<std::uint8_t>((entry >> 8) & 0xFF));
    }
    {
        const std::uint16_t ptr = vdp.vram_pointer();
        snapshot.push_back(static_cast<std::uint8_t>(ptr & 0xFF));
        snapshot.push_back(static_cast<std::uint8_t>((ptr >> 8) & 0xFF));
    }
    // Full 128 KB VRAM content: the ultimate "nothing strayed into VDP state"
    // check, not merely the register file.
    for (std::size_t off = 0; off < sony_msx::devices::video::VdpVram::kVramBytes; ++off) {
        snapshot.push_back(vdp.vram().read(off));
    }

    const auto& ppi = machine.ppi();
    snapshot.push_back(ppi.control());
    snapshot.push_back(ppi.port_c_latch());
    snapshot.push_back(static_cast<std::uint8_t>(ppi.selected_row()));
    snapshot.push_back(ppi.cassette_motor_off() ? 1 : 0);
    snapshot.push_back(ppi.caps_led_off() ? 1 : 0);
    snapshot.push_back(ppi.key_click() ? 1 : 0);

    const auto& rtc = machine.rtc();
    for (std::uint8_t block = 0; block < sony_msx::devices::rtc::Rp5c01::kBlocks; ++block) {
        for (std::uint8_t reg = 0; reg < sony_msx::devices::rtc::Rp5c01::kRegsPerBlock; ++reg) {
            snapshot.push_back(rtc.peek_register(block, reg));
        }
    }
    snapshot.push_back(rtc.mode_register());
    snapshot.push_back(rtc.address_latch());

    const auto& fdc = machine.fdc();
    snapshot.push_back(fdc.peek_status());
    snapshot.push_back(fdc.track_register());
    snapshot.push_back(fdc.sector_register());
    snapshot.push_back(fdc.command_register());

    return snapshot;
}

}  // namespace

int main() {
    int failures = 0;

    Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();

    const std::vector<std::uint8_t> snapshot_before = capture_device_snapshot(machine);

    const auto program = multi_bdos_fixture();
    const auto load_result = CpmBdosHarness::load_com(machine, program);
    failures += expect_true(load_result == CpmBdosHarness::LoadResult::Ok,
                             "Setup_LoadCom_Ok")
                    ? 0
                    : 1;

    const auto run_result = CpmBdosHarness::run(machine, 10'000);
    const std::vector<std::uint8_t> expected_output = {'H', 'I', '!', 'B', 'Y', 'E', '?'};
    failures += expect_true(run_result.finished,
                             "MachineDriven_MultiBdosFixture_WarmBootReached_FinishedTrue")
                    ? 0
                    : 1;
    failures += expect_true(run_result.captured_output == expected_output,
                             "MachineDriven_MultiBdosFixture_CapturedOutput_Correct")
                    ? 0
                    : 1;

    const std::vector<std::uint8_t> snapshot_after = capture_device_snapshot(machine);
    failures += expect_true(snapshot_before == snapshot_after,
                             "DeviceIsolationInvariant_PsgVdpPpiRtcFdc_ByteForByteUnchanged")
                    ? 0
                    : 1;

    return failures == 0 ? 0 : 1;
}
