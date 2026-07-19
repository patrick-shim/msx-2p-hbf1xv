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

#include "peripherals/cassette_interface.h"

#include <utility>

namespace sony_msx::peripherals {

CassetteInterface::CassetteInterface(const devices::chipset::Ppi8255& ppi) : ppi_(ppi) {}

void CassetteInterface::reset() {
    tape_loaded_ = false;
    tape_bits_.clear();
    cycles_per_bit_ = 1;
    start_cycle_ = 0;
    samples_.clear();
}

void CassetteInterface::attach_clock_source(CassetteClockSource* source) {
    clock_source_ = source;
}

bool CassetteInterface::motor_on() const {
    // Port C bit4: "1 = off" (Ppi8255::cassette_motor_off).
    return !ppi_.cassette_motor_off();
}

bool CassetteInterface::output_level() const {
    // Port C bit5: CMO / cassette-write-data (S1985 fact-sheet §3).
    return (ppi_.port_c_latch() & 0x20) != 0;
}

void CassetteInterface::load_synthetic_tape(std::vector<bool> bits, const std::uint64_t cycles_per_bit) {
    tape_bits_ = std::move(bits);
    cycles_per_bit_ = (cycles_per_bit == 0) ? 1 : cycles_per_bit;
    start_cycle_ = (clock_source_ != nullptr) ? clock_source_->cpu_cycles() : 0;
    tape_loaded_ = true;
}

void CassetteInterface::eject_tape() {
    tape_loaded_ = false;
    tape_bits_.clear();
}

bool CassetteInterface::tape_loaded() const {
    return tape_loaded_;
}

bool CassetteInterface::cassette_input_high() const {
    if (!tape_loaded_ || clock_source_ == nullptr) {
        return true;  // idle-high default with no tape/clock attached
    }
    const std::uint64_t now = clock_source_->cpu_cycles();
    const std::uint64_t elapsed = (now >= start_cycle_) ? (now - start_cycle_) : 0;
    const std::uint64_t index = elapsed / cycles_per_bit_;
    if (index >= tape_bits_.size()) {
        return true;  // past-end reverts to idle-high, deterministically
    }
    return tape_bits_[static_cast<std::size_t>(index)];
}

void CassetteInterface::sample_output(const std::uint64_t at_cycle) {
    samples_.push_back(Sample{at_cycle, motor_on(), output_level()});
}

void CassetteInterface::clear_samples() {
    samples_.clear();
}

}  // namespace sony_msx::peripherals
