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
#include <string>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "machine/input_script.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"

// Suite: Machine_InputScriptJoystick_Integration (M41-S1,
// docs/m41-production-qa-plan.md §5.1 acceptance item).
//
// Proves the additive JOY= input-script verb drives the joystick through the
// SAME read path a BASIC STICK()/STRIG() call exercises on real hardware: an
// InputScriptPlayer applies a JOY= event -> JoystickPorts::set_port() -> the
// value is read back through a real PsgYm2149's R15-select + R14-read (the
// documented S1985/PSG wiring, src/peripherals/joystick.h:47-57). This is the
// cross-subsystem proof (input_script + peripherals::JoystickPorts + PSG) that
// the M41 Joystick scenarios (J1-J3) can reach STICK/STRIG without a frontend.
//
// Deterministic oracle: after `T=0 JOY=1 LEFT DOWN` and selecting port 1,
// PSG R14 bit2 (LEFT, active-low) reads 0; releasing it reads 1; trigger A is
// bit4; port 2 is reached via R15 bit6 = 1.

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::machine::InputScriptPlayer;
using sony_msx::machine::parse_input_script;
using sony_msx::peripherals::JoystickPorts;
using sony_msx::peripherals::KeyboardMatrix;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Read PSG R14 for a given selected port (R15 bit6: 0 = port 1, 1 = port 2).
std::uint8_t read_r14(PsgYm2149& psg, const int port_select_bit) {
    psg.write_address(15);
    psg.write_data(static_cast<std::uint8_t>(port_select_bit ? 0x40 : 0x00));
    psg.write_address(14);
    return psg.read_data();
}

}  // namespace

int main() {
    const std::string script_text =
        "HBF1XV-INPUT-SCRIPT v1\n"
        "T=0 JOY=1 LEFT DOWN\n"
        "T=10 JOY=1 A DOWN\n"
        "T=20 JOY=1 LEFT UP\n"
        "T=30 JOY=2 RIGHT DOWN\n"
        "[END]\n";

    JoystickPorts joy;
    joy.reset();
    PsgYm2149 psg;
    psg.reset();
    psg.attach_port_source(&joy);

    InputScriptPlayer player(parse_input_script(script_text));
    player.attach_joystick(&joy);
    KeyboardMatrix keyboard;
    keyboard.reset();

    constexpr std::uint8_t kLeftBit = 0x04;   // R14 bit2
    constexpr std::uint8_t kTrigABit = 0x10;  // R14 bit4
    constexpr std::uint8_t kRightBit = 0x08;  // R14 bit3

    // Before any event: idle -> all direction/trigger bits high on port 1.
    expect((read_r14(psg, 0) & kLeftBit) != 0, "Idle_Port1_LeftBitHigh");

    // T=0: JOY=1 LEFT DOWN -> R14 bit2 reads 0 (active-low pressed) via the
    // real PSG select+read path -- the headline acceptance oracle.
    player.apply_due(0, keyboard);
    expect((read_r14(psg, 0) & kLeftBit) == 0, "AtT0_Port1_LeftDown_R14Bit2Clear");
    expect((read_r14(psg, 0) & kTrigABit) != 0, "AtT0_Port1_TriggerAStillHigh");

    // T=10: trigger A also held; LEFT still held (both accumulate).
    player.apply_due(10, keyboard);
    expect((read_r14(psg, 0) & kLeftBit) == 0, "AtT10_Port1_LeftStillClear");
    expect((read_r14(psg, 0) & kTrigABit) == 0, "AtT10_Port1_TriggerABitClear");

    // T=20: LEFT released -> bit2 back to 1; trigger A survives.
    player.apply_due(20, keyboard);
    expect((read_r14(psg, 0) & kLeftBit) != 0, "AtT20_Port1_LeftReleased_R14Bit2Set");
    expect((read_r14(psg, 0) & kTrigABit) == 0, "AtT20_Port1_TriggerAStillHeld");

    // T=30: port 2 RIGHT pressed; reachable only via R15 bit6 = 1 (port select).
    player.apply_due(30, keyboard);
    expect((read_r14(psg, 1) & kRightBit) == 0, "AtT30_Port2Select_RightBitClear");
    // Port 1 (bit6 = 0) is independent: its RIGHT bit is untouched (still high).
    expect((read_r14(psg, 0) & kRightBit) != 0, "AtT30_Port1_RightBit_Independent_High");

    expect(player.cursor() == player.event_count(), "Player_AllJoyEventsApplied");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_InputScriptJoystick_Integration cases passed\n";
    return 0;
}
