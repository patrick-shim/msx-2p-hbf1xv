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
