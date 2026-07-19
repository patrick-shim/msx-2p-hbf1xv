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

#pragma once

#include <cstdint>

namespace sony_msx::devices::cpu {

enum class InterruptMode : std::uint8_t {
    Im0 = 0,
    Im1 = 1,
    Im2 = 2,
};

struct Z80aRegisters {
    std::uint16_t af = 0;
    std::uint16_t bc = 0;
    std::uint16_t de = 0;
    std::uint16_t hl = 0;
    std::uint16_t af_shadow = 0;
    std::uint16_t bc_shadow = 0;
    std::uint16_t de_shadow = 0;
    std::uint16_t hl_shadow = 0;
    std::uint16_t ix = 0;
    std::uint16_t iy = 0;
    std::uint16_t sp = 0xFFFF;
    std::uint16_t pc = 0;
    std::uint8_t i = 0;
    std::uint8_t r = 0;

    // Internal WZ / MEMPTR register. Not directly software-
    // accessible; observable only via the X/Y flags of BIT n,(HL) and
    // BIT n,(IX/IY+d), which source bits 11/13 from WZ (fact-sheet §4). Updated
    // at the enumerated rule sites in the CPU. Deterministically reset to 0.
    std::uint16_t wz = 0;

    // Q latch. Mirrors the value written to F by the most
    // recent flag-modifying operation; it is set by set_f() and cleared to 0 at
    // the start of every instruction (so an instruction that never writes F
    // leaves Q = 0). Feeds the genuine-Zilog SCF/CCF undocumented X/Y rule
    // X/Y = ((Q ^ F) | A) bits 5/3 (fact-sheet §8, Patrik-Rak). Reset to 0.
    std::uint8_t q = 0;

    [[nodiscard]] std::uint8_t a() const;
    [[nodiscard]] std::uint8_t f() const;
    [[nodiscard]] std::uint8_t b() const;
    [[nodiscard]] std::uint8_t c() const;
    [[nodiscard]] std::uint8_t d() const;
    [[nodiscard]] std::uint8_t e() const;
    [[nodiscard]] std::uint8_t h() const;
    [[nodiscard]] std::uint8_t l() const;

    void set_a(std::uint8_t value);
    void set_f(std::uint8_t value);
    void set_b(std::uint8_t value);
    void set_c(std::uint8_t value);
    void set_d(std::uint8_t value);
    void set_e(std::uint8_t value);
    void set_h(std::uint8_t value);
    void set_l(std::uint8_t value);
};

class Z80aState {
public:
    static constexpr std::uint8_t kFlagSign = 0x80;
    static constexpr std::uint8_t kFlagZero = 0x40;
    static constexpr std::uint8_t kFlagY = 0x20;  // Undocumented (bit 5, copy of result bit 5).
    static constexpr std::uint8_t kFlagHalfCarry = 0x10;
    static constexpr std::uint8_t kFlagX = 0x08;  // Undocumented (bit 3, copy of result bit 3).
    static constexpr std::uint8_t kFlagParityOverflow = 0x04;
    static constexpr std::uint8_t kFlagSubtract = 0x02;
    static constexpr std::uint8_t kFlagCarry = 0x01;

    void reset();

    [[nodiscard]] const Z80aRegisters& regs() const;
    Z80aRegisters& regs();

    [[nodiscard]] bool iff1() const;
    [[nodiscard]] bool iff2() const;
    void set_iff1(bool enabled);
    void set_iff2(bool enabled);

    [[nodiscard]] InterruptMode interrupt_mode() const;
    void set_interrupt_mode(InterruptMode mode);

    [[nodiscard]] bool halted() const;
    void set_halted(bool halted);

    [[nodiscard]] bool ei_delay_active() const;
    void set_ei_delay_active(bool enabled);

    [[nodiscard]] bool maskable_interrupt_pending() const;
    [[nodiscard]] bool nmi_pending() const;
    void request_maskable_interrupt();
    // Overload: the acknowledging device places a byte on the data bus. For IM0
    // this is the opcode executed on acknowledge; for IM2 it is the low byte of
    // the vector-table address. Modeled as an injectable, deterministic value.
    void request_maskable_interrupt(std::uint8_t bus_vector);
    void request_nmi();
    void clear_maskable_interrupt();
    void clear_nmi();

    // Data-bus value supplied by the acknowledging device (defaults to the
    // floating-bus 0xFF when no device drives the bus). Meaningful only for
    // IM0/IM2; IM1 ignores it.
    [[nodiscard]] std::uint8_t interrupt_bus_vector() const;
    [[nodiscard]] bool interrupt_vector_supplied() const;

    [[nodiscard]] std::uint64_t total_tstates() const;
    void add_tstates(std::uint64_t delta);

private:
    Z80aRegisters regs_{};
    bool iff1_ = false;
    bool iff2_ = false;
    InterruptMode interrupt_mode_ = InterruptMode::Im1;
    bool halted_ = false;
    bool ei_delay_active_ = false;
    bool maskable_interrupt_pending_ = false;
    bool nmi_pending_ = false;
    std::uint8_t interrupt_bus_vector_ = 0xFF;
    bool interrupt_vector_supplied_ = false;
    std::uint64_t total_tstates_ = 0;
};

}  // namespace sony_msx::devices::cpu