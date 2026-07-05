#include <cstdint>
#include <iostream>

#include "devices/cpu/z80a_state.h"

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    // Suite: Devices_Z80AState_Unit
    sony_msx::devices::cpu::Z80aState state;
    state.reset();

    if (!expect_true(state.interrupt_mode() == sony_msx::devices::cpu::InterruptMode::Im1,
            "Reset_DefaultMode_Im1")) {
        return 1;
    }

    if (!expect_true(!state.iff1() && !state.iff2(), "Reset_InterruptFlipFlops_Disabled")) {
        return 1;
    }

    if (!expect_true(state.regs().pc == 0 && state.regs().sp == 0xFFFF,
            "Reset_ProgramAndStackPointer_KnownDeterministicValues")) {
        return 1;
    }

    state.regs().set_a(0x12);
    state.regs().set_f(0x34);
    state.regs().set_b(0x56);
    state.regs().set_c(0x78);
    if (!expect_true(state.regs().af == 0x1234 && state.regs().bc == 0x5678,
            "RegisterViews_HighLowWrites_ExpectedWordComposition")) {
        return 1;
    }

    state.set_iff1(true);
    state.set_iff2(true);
    state.request_maskable_interrupt();
    state.request_nmi();
    state.add_tstates(17);
    if (!expect_true(state.iff1() && state.iff2() && state.maskable_interrupt_pending() && state.nmi_pending() &&
            state.total_tstates() == 17, "StateMutations_InterruptAndTiming_StateUpdated")) {
        return 1;
    }

    state.reset();
    if (!expect_true(!state.maskable_interrupt_pending() && !state.nmi_pending() && state.total_tstates() == 0,
            "Reset_AfterMutations_StateCleared")) {
        return 1;
    }

    return 0;
}