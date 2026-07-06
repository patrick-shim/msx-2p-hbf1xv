#include "devices/video/vdp_vram.h"

#include <algorithm>

namespace sony_msx::devices::video {

VdpVram::VdpVram() : bytes_(kVramBytes, 0) {
}

std::size_t VdpVram::size() const {
    return bytes_.size();
}

void VdpVram::clear() {
    std::fill(bytes_.begin(), bytes_.end(), static_cast<std::uint8_t>(0));
}

std::uint8_t VdpVram::read(const std::size_t address) const {
    if (address >= bytes_.size()) {
        return 0;
    }
    return bytes_[address];
}

void VdpVram::write(const std::size_t address, const std::uint8_t value) {
    if (address >= bytes_.size()) {
        return;
    }
    bytes_[address] = value;
}

void VdpVram::load(const std::size_t offset, const std::uint8_t* bytes, const std::size_t count) {
    if (bytes == nullptr || count == 0 || offset >= bytes_.size()) {
        return;
    }

    const std::size_t available = bytes_.size() - offset;
    const std::size_t writable = std::min(count, available);
    for (std::size_t index = 0; index < writable; ++index) {
        bytes_[offset + index] = bytes[index];
    }
}

std::vector<std::uint8_t> VdpVram::dump() const {
    return bytes_;
}

const std::uint8_t* VdpVram::data() const {
    return bytes_.data();
}

}  // namespace sony_msx::devices::video
