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

namespace sony_msx::core {

class Scheduler {
public:
    void reset();
    void tick(std::uint64_t cycles);
    void tick_many(std::uint32_t steps, std::uint64_t cycles_per_step);
    bool advance_to(std::uint64_t target_cycles);
    [[nodiscard]] std::uint64_t total_cycles() const;

private:
    std::uint64_t total_cycles_ = 0;
};

}  // namespace sony_msx::core
