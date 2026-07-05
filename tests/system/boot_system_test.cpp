#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

int main() {
    // Suite: System_Boot_System
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();

    if (machine.elapsed_cycles() != 0) {
        std::cerr << "Case failed: ColdBoot_InitialState_ZeroCycles\n";
        return 1;
    }

    if (machine.frame_count() != 0) {
        std::cerr << "Case failed: ColdBoot_InitialState_ZeroFrames\n";
        return 1;
    }

    machine.run_frame();
    if (machine.elapsed_cycles() == 0) {
        std::cerr << "Case failed: FrameAdvance_AfterBoot_NonZeroCycles\n";
        return 1;
    }

    if (machine.frame_count() != 1) {
        std::cerr << "Case failed: FrameAdvance_AfterBoot_FrameCounterIncrements\n";
        return 1;
    }

    machine.cold_boot();
    if (machine.elapsed_cycles() != 0 || machine.frame_count() != 0) {
        std::cerr << "Case failed: ColdBoot_AfterProgress_ResetsState\n";
        return 1;
    }

    return 0;
}
