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

#pragma once

#include <cstdint>

namespace sony_msx::devices::fdc {

// Deterministic emulated-cycle clock source for the FDC (M16-S2). Mirrors the
// M15 rtc::RtcClockSource (src/devices/rtc/rp5c01.h:14-18): ALL FDC timing
// (Busy/step/settle, DRQ cadence, index pulse, ~4 s motor-off) advances READ-
// ONLY off the machine cycle clock (Hbf1xvMachine::elapsed_cycles() == scheduler
// total cycles) and NEVER the host wall clock. CPU T-state accounting is never
// touched, protecting the M9/M12 timing oracles (planner A-M16-2 / R-M16-2).
class FdcClockSource {
public:
    virtual ~FdcClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

}  // namespace sony_msx::devices::fdc
