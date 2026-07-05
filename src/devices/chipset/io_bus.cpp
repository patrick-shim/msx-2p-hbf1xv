#include "devices/chipset/io_bus.h"

namespace sony_msx::devices::chipset {

namespace {
constexpr core::BusData kOpenBus = 0xFF;
}  // namespace

void IoBus::ensure_mirror_init() {
    if (mirror_initialized_) {
        return;
    }
    for (int p = 0; p < 256; ++p) {
        mirror_target_[static_cast<std::size_t>(p)] = static_cast<std::uint8_t>(p);
    }
    mirror_initialized_ = true;
}

void IoBus::attach(const std::uint8_t port, core::IoDevice* device) {
    devices_[port] = device;
}

void IoBus::register_mirror(const std::uint8_t base, const std::uint8_t mirror) {
    ensure_mirror_init();
    mirror_target_[mirror] = base;
}

core::IoDevice* IoBus::resolve(const std::uint8_t port) const {
    // Follow the mirror (one hop; mirrors always target a base, never a mirror).
    const std::uint8_t target = mirror_initialized_ ? mirror_target_[port] : port;
    return devices_[target];
}

core::BusData IoBus::io_read(const core::BusAddress port) {
    core::IoDevice* const device = resolve(static_cast<std::uint8_t>(port & 0xFF));
    if (device == nullptr) {
        return kOpenBus;
    }
    return device->io_read(port);
}

void IoBus::io_write(const core::BusAddress port, const core::BusData value) {
    core::IoDevice* const device = resolve(static_cast<std::uint8_t>(port & 0xFF));
    if (device == nullptr) {
        return;
    }
    device->io_write(port, value);
}

core::BusData IoBus::read(core::BusAddress /*address*/) {
    return kOpenBus;
}

void IoBus::write(core::BusAddress /*address*/, core::BusData /*value*/) {
}

}  // namespace sony_msx::devices::chipset
