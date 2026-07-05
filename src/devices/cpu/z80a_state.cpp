#include "devices/cpu/z80a_state.h"

namespace sony_msx::devices::cpu {

namespace {

constexpr std::uint8_t hi_byte(const std::uint16_t value) {
    return static_cast<std::uint8_t>((value >> 8) & 0x00FF);
}

constexpr std::uint8_t lo_byte(const std::uint16_t value) {
    return static_cast<std::uint8_t>(value & 0x00FF);
}

constexpr std::uint16_t with_hi_byte(const std::uint16_t value, const std::uint8_t byte) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byte) << 8) | (value & 0x00FF));
}

constexpr std::uint16_t with_lo_byte(const std::uint16_t value, const std::uint8_t byte) {
    return static_cast<std::uint16_t>((value & 0xFF00) | byte);
}

}  // namespace

std::uint8_t Z80aRegisters::a() const {
    return hi_byte(af);
}

std::uint8_t Z80aRegisters::f() const {
    return lo_byte(af);
}

std::uint8_t Z80aRegisters::b() const {
    return hi_byte(bc);
}

std::uint8_t Z80aRegisters::c() const {
    return lo_byte(bc);
}

std::uint8_t Z80aRegisters::d() const {
    return hi_byte(de);
}

std::uint8_t Z80aRegisters::e() const {
    return lo_byte(de);
}

std::uint8_t Z80aRegisters::h() const {
    return hi_byte(hl);
}

std::uint8_t Z80aRegisters::l() const {
    return lo_byte(hl);
}

void Z80aRegisters::set_a(const std::uint8_t value) {
    af = with_hi_byte(af, value);
}

void Z80aRegisters::set_f(const std::uint8_t value) {
    af = with_lo_byte(af, value);
}

void Z80aRegisters::set_b(const std::uint8_t value) {
    bc = with_hi_byte(bc, value);
}

void Z80aRegisters::set_c(const std::uint8_t value) {
    bc = with_lo_byte(bc, value);
}

void Z80aRegisters::set_d(const std::uint8_t value) {
    de = with_hi_byte(de, value);
}

void Z80aRegisters::set_e(const std::uint8_t value) {
    de = with_lo_byte(de, value);
}

void Z80aRegisters::set_h(const std::uint8_t value) {
    hl = with_hi_byte(hl, value);
}

void Z80aRegisters::set_l(const std::uint8_t value) {
    hl = with_lo_byte(hl, value);
}

void Z80aState::reset() {
    regs_ = Z80aRegisters{};
    iff1_ = false;
    iff2_ = false;
    interrupt_mode_ = InterruptMode::Im1;
    halted_ = false;
    ei_delay_active_ = false;
    maskable_interrupt_pending_ = false;
    nmi_pending_ = false;
    interrupt_bus_vector_ = 0xFF;
    interrupt_vector_supplied_ = false;
    total_tstates_ = 0;
}

const Z80aRegisters& Z80aState::regs() const {
    return regs_;
}

Z80aRegisters& Z80aState::regs() {
    return regs_;
}

bool Z80aState::iff1() const {
    return iff1_;
}

bool Z80aState::iff2() const {
    return iff2_;
}

void Z80aState::set_iff1(const bool enabled) {
    iff1_ = enabled;
}

void Z80aState::set_iff2(const bool enabled) {
    iff2_ = enabled;
}

InterruptMode Z80aState::interrupt_mode() const {
    return interrupt_mode_;
}

void Z80aState::set_interrupt_mode(const InterruptMode mode) {
    interrupt_mode_ = mode;
}

bool Z80aState::halted() const {
    return halted_;
}

void Z80aState::set_halted(const bool halted) {
    halted_ = halted;
}

bool Z80aState::ei_delay_active() const {
    return ei_delay_active_;
}

void Z80aState::set_ei_delay_active(const bool enabled) {
    ei_delay_active_ = enabled;
}

bool Z80aState::maskable_interrupt_pending() const {
    return maskable_interrupt_pending_;
}

bool Z80aState::nmi_pending() const {
    return nmi_pending_;
}

void Z80aState::request_maskable_interrupt() {
    maskable_interrupt_pending_ = true;
    interrupt_bus_vector_ = 0xFF;  // Floating bus: no device-driven value.
    interrupt_vector_supplied_ = false;
}

void Z80aState::request_maskable_interrupt(const std::uint8_t bus_vector) {
    maskable_interrupt_pending_ = true;
    interrupt_bus_vector_ = bus_vector;
    interrupt_vector_supplied_ = true;
}

void Z80aState::request_nmi() {
    nmi_pending_ = true;
}

void Z80aState::clear_maskable_interrupt() {
    maskable_interrupt_pending_ = false;
    interrupt_bus_vector_ = 0xFF;
    interrupt_vector_supplied_ = false;
}

std::uint8_t Z80aState::interrupt_bus_vector() const {
    return interrupt_bus_vector_;
}

bool Z80aState::interrupt_vector_supplied() const {
    return interrupt_vector_supplied_;
}

void Z80aState::clear_nmi() {
    nmi_pending_ = false;
}

std::uint64_t Z80aState::total_tstates() const {
    return total_tstates_;
}

void Z80aState::add_tstates(const std::uint64_t delta) {
    total_tstates_ += delta;
}

}  // namespace sony_msx::devices::cpu