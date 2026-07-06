#include "devices/memory/rom_device.h"

namespace sony_msx::devices::memory {

RomDevice::RomDevice(const std::uint16_t base, const std::uint32_t size)
    : base_(base), size_(size), image_(size, kOpenBus) {
}

RomDevice::RomDevice(const std::uint16_t base, const std::uint32_t size, std::vector<std::uint8_t> image)
    : base_(base), size_(size), image_(std::move(image)) {
    normalize_image();
}

void RomDevice::set_image(std::vector<std::uint8_t> image) {
    image_ = std::move(image);
    normalize_image();
}

void RomDevice::normalize_image() {
    // A ROM device is always exactly `size_` bytes so reads are deterministic
    // across the whole window. A short image is 0xFF-padded (open-bus-like); an
    // over-long image is truncated.
    image_.resize(size_, kOpenBus);
}

bool RomDevice::in_window(const core::BusAddress address) const {
    const std::uint32_t addr = address;
    return addr >= base_ && addr < (static_cast<std::uint32_t>(base_) + size_);
}

core::BusData RomDevice::mem_read(const core::BusAddress address) {
    if (!in_window(address)) {
        return kOpenBus;
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(address) - base_;
    return image_[offset];
}

void RomDevice::mem_write(const core::BusAddress /*address*/, const core::BusData /*value*/) {
    // Read-only: mask-ROM writes are ignored on real hardware. Any bank/mapper
    // behaviour (Halnote, FM-PAC) is a separate device and out of M13 scope.
}

}  // namespace sony_msx::devices::memory
