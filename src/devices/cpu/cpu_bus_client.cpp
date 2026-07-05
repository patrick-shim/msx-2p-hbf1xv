#include "devices/cpu/cpu_bus_client.h"

namespace sony_msx::devices::cpu {

CpuBusClient::CpuBusClient(core::Bus& bus) : bus_(bus) {
}

core::BusData CpuBusClient::fetch_opcode(const std::uint32_t address) {
    return bus_.read(core::normalize_bus_address(address));
}

core::BusData CpuBusClient::read_data(const std::uint32_t address) {
    return bus_.read(core::normalize_bus_address(address));
}

void CpuBusClient::write_data(const std::uint32_t address, const core::BusData value) {
    bus_.write(core::normalize_bus_address(address), value);
}

core::BusData CpuBusClient::io_read(const std::uint32_t port) {
    return bus_.io_read(core::normalize_bus_address(port));
}

void CpuBusClient::io_write(const std::uint32_t port, const core::BusData value) {
    bus_.io_write(core::normalize_bus_address(port), value);
}

std::uint16_t CpuBusClient::read_word_le(const std::uint32_t address) {
    const std::uint8_t lo = read_data(address);
    const std::uint8_t hi = read_data(address + 1);
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

void CpuBusClient::write_word_le(const std::uint32_t address, const std::uint16_t value) {
    write_data(address, static_cast<std::uint8_t>(value & 0x00FF));
    write_data(address + 1, static_cast<std::uint8_t>((value >> 8) & 0x00FF));
}

std::uint16_t CpuBusClient::read_word_be(const std::uint32_t address) {
    const std::uint8_t hi = read_data(address);
    const std::uint8_t lo = read_data(address + 1);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}

void CpuBusClient::write_word_be(const std::uint32_t address, const std::uint16_t value) {
    write_data(address, static_cast<std::uint8_t>((value >> 8) & 0x00FF));
    write_data(address + 1, static_cast<std::uint8_t>(value & 0x00FF));
}

}  // namespace sony_msx::devices::cpu
