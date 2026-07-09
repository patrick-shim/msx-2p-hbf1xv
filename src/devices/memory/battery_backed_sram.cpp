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

#include "devices/memory/battery_backed_sram.h"

#include <algorithm>
#include <fstream>
#include <ios>

namespace sony_msx::devices::memory {

BatteryBackedSram::BatteryBackedSram(const std::size_t byte_count) : bytes_(byte_count, 0) {
}

std::size_t BatteryBackedSram::size() const {
    return bytes_.size();
}

std::uint8_t BatteryBackedSram::read(const std::size_t offset) const {
    return bytes_[offset];
}

void BatteryBackedSram::write(const std::size_t offset, const std::uint8_t value) {
    bytes_[offset] = value;
}

void BatteryBackedSram::clear() {
    std::fill(bytes_.begin(), bytes_.end(), static_cast<std::uint8_t>(0));
}

bool BatteryBackedSram::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;  // absent/unreadable -> keep the current (deterministic) state
    }
    std::vector<char> buffer(bytes_.size());
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (file.gcount() != static_cast<std::streamsize>(bytes_.size())) {
        return false;  // short/wrong-size -> untouched (no partial/garbage load)
    }
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        bytes_[i] = static_cast<std::uint8_t>(buffer[i]);
    }
    return true;
}

bool BatteryBackedSram::save(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes_.data()),
               static_cast<std::streamsize>(bytes_.size()));
    return static_cast<bool>(file);
}

}  // namespace sony_msx::devices::memory
