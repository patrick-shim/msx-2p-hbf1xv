#include "devices/chipset/s1985_engine.h"

#include <fstream>
#include <ios>

namespace sony_msx::devices::chipset {

void S1985Engine::reset() {
    // openMSX MSXS1985::reset clears color1/color2/pattern/address (not the SRAM
    // contents, which are battery-backed). M11 backup RAM is volatile (A-5); we
    // additionally zero it here so cold_boot is deterministic.
    address_ = 0;
    pattern_ = 0;
    color1_ = 0;
    color2_ = 0;
    sram_.fill(0);
}

void S1985Engine::configure_mapper(MapperIo& mapper) const {
    mapper.set_readback(MapperIo::kReadBackBase, MapperIo::kReadBackMask);
}

std::uint32_t S1985Engine::m1_wait_tstates(const std::uint32_t m1_cycles) const {
    return m1_cycles * kM1WaitTStates;
}

std::uint8_t S1985Engine::id() const {
    return kId;
}

core::BusData S1985Engine::peek_switched(const core::BusAddress port) const {
    // MSXS1985::peekSwitchedIO — dispatch on port & 0x0F.
    switch (port & 0x0F) {
    case 0:
        return static_cast<core::BusData>(~kId);  // ~0xFE = 0x01
    case 2:
        return sram_[address_];
    case 7:
        return (pattern_ & 0x80) ? color2_ : color1_;
    default:
        return 0xFF;
    }
}

core::BusData S1985Engine::switched_read(const core::BusAddress port) {
    // MSXS1985::readSwitchedIO — peek, then rotate pattern on index 7.
    const core::BusData result = peek_switched(port);
    if ((port & 0x0F) == 7) {
        pattern_ = static_cast<std::uint8_t>((pattern_ << 1) | (pattern_ >> 7));
    }
    return result;
}

void S1985Engine::switched_write(const core::BusAddress port, const core::BusData value) {
    // MSXS1985::writeSwitchedIO — dispatch on port & 0x0F.
    switch (port & 0x0F) {
    case 1:
        address_ = static_cast<std::uint8_t>(value & 0x0F);
        break;
    case 2:
        sram_[address_] = value;
        break;
    case 6:
        color2_ = color1_;
        color1_ = value;
        break;
    case 7:
        pattern_ = value;
        break;
    default:
        break;
    }
}

std::uint8_t S1985Engine::backup_byte(const std::uint8_t index) const {
    return sram_[index & 0x0F];
}

bool S1985Engine::load_backup_ram(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;  // absent/unreadable -> keep the deterministic reset state
    }
    std::array<char, kBackupRamBytes> buffer{};
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (file.gcount() != static_cast<std::streamsize>(kBackupRamBytes)) {
        return false;  // short/wrong-size -> untouched (no partial/garbage load)
    }
    for (std::size_t i = 0; i < kBackupRamBytes; ++i) {
        sram_[i] = static_cast<std::uint8_t>(buffer[i]);
    }
    return true;
}

bool S1985Engine::save_backup_ram(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(sram_.data()),
               static_cast<std::streamsize>(sram_.size()));
    return static_cast<bool>(file);
}

}  // namespace sony_msx::devices::chipset
