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

#include "frontend/psg_audio_pump.h"

namespace sony_msx::frontend {

PsgAudioPump::PsgAudioPump(const std::uint64_t cycles_per_sample) : cycles_per_sample_(cycles_per_sample) {}

devices::audio::PsgYm2149::StereoSample PsgAudioPump::pump_one_sample(devices::audio::PsgYm2149& psg) const {
    // M34 (DEC-0043 Defect A, docs/m34-planner-package.md §2.3.6): advance by
    // exactly one sample window, then take the chip's exact box-average over
    // that window -- the integrated-sample production switch. The pump
    // guarantees the take-API's §2.3.7 precondition by construction (it
    // advances exactly cycles_per_sample_ between takes). cycles_per_sample_
    // == 0 (the M26 idle case) advances nothing and takes the guarded {0,0}.
    psg.advance_cycles(cycles_per_sample_);
    return psg.take_integrated_sample(cycles_per_sample_);
}

std::vector<devices::audio::PsgYm2149::StereoSample> PsgAudioPump::pump_samples(
    devices::audio::PsgYm2149& psg, const std::size_t sample_count) const {
    std::vector<devices::audio::PsgYm2149::StereoSample> out;
    out.reserve(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        out.push_back(pump_one_sample(psg));
    }
    return out;
}

}  // namespace sony_msx::frontend
