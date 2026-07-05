#pragma once

#include <array>
#include <cstdint>

#include "devices/cpu/z80a_state.h"

namespace sony_msx::devices::cpu {

// Deterministic per-instruction CPU trace record (M10-S1).
//
// Capture conventions (fixed and documented for byte-stable, diffable output):
//  - Register/flag fields hold the PRE-EXECUTION snapshot taken at the
//    instruction boundary, before any opcode/prefix fetch. `pc` is therefore
//    the address of the instruction and `r` is its value before the M1
//    refresh tick of this instruction.
//  - `opcode_bytes` / `opcode_length` hold the M1 opcode-fetch bytes consumed
//    by this instruction (unprefixed opcode plus any CB/ED/DD/FD prefix bytes
//    fetched as opcode). Operand/immediate/displacement bytes are NOT opcode
//    bytes and are excluded. For non-fetching steps (HALT idle, interrupt
//    acknowledge) `opcode_length` is 0.
//  - `instr_tstates` is the T-state cost of THIS instruction.
//  - `cumulative_tstates` is the running total AFTER this instruction retires.
//
// The observer is nullable and off by default in the CPU; when unattached
// there is zero behavioral, state, or cycle effect (no record is built).
struct Z80aTraceRecord {
    std::uint64_t sequence = 0;  // 0-based index since the observer attached.

    std::uint16_t pc = 0;  // Pre-execution program counter (instruction address).

    std::array<std::uint8_t, 4> opcode_bytes{};
    std::uint8_t opcode_length = 0;

    // Primary 8-bit register file (pre-execution).
    std::uint8_t a = 0;
    std::uint8_t f = 0;
    std::uint8_t b = 0;
    std::uint8_t c = 0;
    std::uint8_t d = 0;
    std::uint8_t e = 0;
    std::uint8_t h = 0;
    std::uint8_t l = 0;

    // 16-bit register file (pre-execution).
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
    std::uint16_t sp = 0;

    std::uint8_t i = 0;
    std::uint8_t r = 0;

    bool iff1 = false;
    bool iff2 = false;
    InterruptMode im = InterruptMode::Im0;

    std::uint32_t instr_tstates = 0;
    std::uint64_t cumulative_tstates = 0;

    // Individual flag bits, derived from F (S,Z,Y,H,X,P/V,N,C).
    [[nodiscard]] bool flag_s() const { return (f & Z80aState::kFlagSign) != 0; }
    [[nodiscard]] bool flag_z() const { return (f & Z80aState::kFlagZero) != 0; }
    [[nodiscard]] bool flag_y() const { return (f & Z80aState::kFlagY) != 0; }
    [[nodiscard]] bool flag_h() const { return (f & Z80aState::kFlagHalfCarry) != 0; }
    [[nodiscard]] bool flag_x() const { return (f & Z80aState::kFlagX) != 0; }
    [[nodiscard]] bool flag_pv() const { return (f & Z80aState::kFlagParityOverflow) != 0; }
    [[nodiscard]] bool flag_n() const { return (f & Z80aState::kFlagSubtract) != 0; }
    [[nodiscard]] bool flag_c() const { return (f & Z80aState::kFlagCarry) != 0; }
};

// Per-instruction observer hook. Implementations live in the machine layer
// (see src/machine/cpu_trace_sink.h). The CPU invokes on_instruction_retired
// exactly once per completed step() when an observer is attached.
class Z80aTraceObserver {
public:
    virtual ~Z80aTraceObserver() = default;
    virtual void on_instruction_retired(const Z80aTraceRecord& record) = 0;
};

}  // namespace sony_msx::devices::cpu
