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

#include "devices/fdc/disk_drive.h"

namespace sony_msx::devices::fdc {

void DiskDrive::reset() {
    physical_track_ = 0;
    side_ = 0;
    available_ = true;
    motor_on_ = false;
    motor_off_pending_ = false;
    motor_off_deadline_ = 0;
    disk_changed_ = false;
}

void DiskDrive::set_motor(const bool on, const std::uint64_t now) {
    if (on) {
        motor_on_ = true;
        motor_off_pending_ = false;
    } else if (motor_on_ && !motor_off_pending_) {
        // Start the ~4 s delayed motor-off timer (RealDrive.cc:263-321; XML
        // motor_off_timeout_ms=4000). The motor stays spinning until the deadline.
        motor_off_pending_ = true;
        motor_off_deadline_ = now + kMotorOffCycles;
    }
}

bool DiskDrive::motor_on(const std::uint64_t now) const {
    if (motor_on_ && motor_off_pending_ && now >= motor_off_deadline_) {
        return false;
    }
    return motor_on_;
}

void DiskDrive::step(const bool inward) {
    if (inward) {
        if (physical_track_ < (DiskImage::kTracks - 1)) {
            ++physical_track_;
        }
    } else {
        if (physical_track_ > 0) {
            --physical_track_;
        }
    }
}

void DiskDrive::restore() {
    physical_track_ = 0;
}

bool DiskDrive::ready() const {
    return available_ && image_ != nullptr && image_->present();
}

bool DiskDrive::is_track00() const {
    return available_ && physical_track_ == 0;
}

bool DiskDrive::write_protected() const {
    return image_ != nullptr && image_->write_protected();
}

bool DiskDrive::index_pulse(const std::uint64_t now) const {
    if (!ready()) {
        return false;
    }
    return (now % kIndexPeriodCycles) < kIndexPulseWidthCycles;
}

std::uint64_t DiskDrive::cycles_until_index_pulse(const std::uint64_t now) const {
    const std::uint64_t phase = now % kIndexPeriodCycles;
    if (phase < kIndexPulseWidthCycles) {
        return 0;  // `now` is already inside a pulse window.
    }
    return kIndexPeriodCycles - phase;
}

std::uint64_t DiskDrive::cycles_until_sector_id(const std::uint32_t sector_index,
                                                const std::uint64_t now) const {
    // Evenly-spaced sequential model (see header): sector k's ID mark sits at
    // angle k/9 of the rotation, computed as k*P/9 (not k*(P/9)) so integer
    // rounding is spread across the 9 slots instead of accumulating. sector_index
    // is 0..8 (SR 1..9 validated by the WD2793 caller), so sector_angle <
    // kIndexPeriodCycles.
    const std::uint64_t sector_angle =
        (static_cast<std::uint64_t>(sector_index) * kIndexPeriodCycles) /
        DiskImage::kSectorsPerTrack;
    const std::uint64_t phase = now % kIndexPeriodCycles;
    // Cycles from the current angle forward to the sector's angle (wrapping once
    // per rotation). Deterministic function of `now` alone.
    return (sector_angle + kIndexPeriodCycles - phase) % kIndexPeriodCycles;
}

bool DiskDrive::read_sector(const std::uint8_t sector, std::uint8_t* out) const {
    if (image_ == nullptr) {
        return false;
    }
    return image_->read_chs(physical_track_, side_, sector, out);
}

bool DiskDrive::write_sector(const std::uint8_t sector, const std::uint8_t* in) {
    if (image_ == nullptr) {
        return false;
    }
    return image_->write_chs(physical_track_, side_, sector, in);
}

}  // namespace sony_msx::devices::fdc
