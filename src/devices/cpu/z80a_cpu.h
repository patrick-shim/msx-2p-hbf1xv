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

#include <array>

#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_state.h"
#include "devices/cpu/z80a_trace.h"

namespace sony_msx::devices::cpu {

class Z80aCpu {
public:
    explicit Z80aCpu(CpuBusClient& bus);

    void reset();
    std::uint32_t step();

    // Nullable per-instruction trace observer. Off by default: when
    // no observer is attached the step path builds no record and has zero
    // behavioral/state/cycle effect. Passing nullptr detaches.
    void set_trace_observer(Z80aTraceObserver* observer);
    [[nodiscard]] Z80aTraceObserver* trace_observer() const;

    void request_maskable_interrupt();
    // Acknowledge hook: the interrupting device supplies a byte on the data bus.
    // IM0 executes it as an opcode; IM2 uses it as the vector-table low byte.
    void request_maskable_interrupt(std::uint8_t bus_vector);
    // Release a level-held maskable-interrupt request. Thin pass-through
    // for a device that owns the /INT line (e.g. the V9958); no accept logic.
    void clear_maskable_interrupt();
    void request_nmi();

    void set_interrupt_mode(InterruptMode mode);

    [[nodiscard]] const Z80aState& state() const;
    Z80aState& state();

    // M1 opcode-fetch cycle count for the most recently completed step().
    // One M1 cycle per opcode-fetch byte (each ED/CB/DD/FD prefix is
    // its own M1). It EXCLUDES the interrupt/NMI-acknowledge M1: that special M1
    // asserts /M1 + /IORQ (not the opcode-fetch /M1 + /MREQ the S1985 gates its
    // +1 wait on) and already carries the Z80's own 2 automatic ack wait-states,
    // so it ticks R but registers NO S1985 wait cycle (openMSX bills IM1=13 /
    // IM2=19 / NMI=11 bare; live openMSX A/B confirmed a 13T IM1 accept). The
    // count is a pure CPU signal: datasheet T-state returns are UNCHANGED. The
    // S1985 layer maps this to the MSX +1-per-M1 wait, applied by the machine to
    // the scheduler (S1985 fact-sheet §8). Reset at each step() entry.
    [[nodiscard]] std::uint32_t m1_cycles_last_step() const;

private:
    // Trace helpers. Snapshot the pre-execution register file and
    // record M1 opcode-fetch bytes only while an observer is attached.
    void trace_begin_instruction();
    void trace_capture_opcode_byte(std::uint8_t value);
    void trace_end_instruction(std::uint32_t instr_tstates);

    std::uint8_t fetch_opcode();
    std::uint8_t read_imm8();
    std::uint16_t read_imm16();
    void push16(std::uint16_t value);
    std::uint16_t pop16();
    // Tick the 7-bit R refresh register (low 7 bits +1, bit 7 frozen) for one M1
    // machine cycle, WITHOUT counting an S1985 M1-wait cycle. Used for the
    // interrupt/NMI-acknowledge special M1 (/M1 + /IORQ), which the S1985's
    // opcode-fetch-gated (/M1 + /MREQ) wait generator does not stretch.
    void tick_refresh_only();
    // Normal opcode-fetch M1: tick R AND register one S1985 +1-per-M1 wait cycle
    // (== tick_refresh_only() + ++m1_cycle_count_).
    void increment_refresh_register();

    // Indexed register-file access (Z80 r[] table order: B,C,D,E,H,L,(HL),A).
    // Returns whether the access touched memory (used for T-state accounting).
    std::uint8_t read_reg_index(int index, bool& touched_memory);
    void write_reg_index(int index, std::uint8_t value);

    // 16-bit register-pair helpers (rp[] table: BC,DE,HL,SP; rp2[]: BC,DE,HL,AF).
    std::uint16_t read_rp(int index) const;
    void write_rp(int index, std::uint16_t value);
    std::uint16_t read_rp2(int index) const;
    void write_rp2(int index, std::uint16_t value);
    [[nodiscard]] bool check_condition(int index) const;

    // Flag-producing ALU primitives.
    std::uint8_t alu_inc8(std::uint8_t value);
    std::uint8_t alu_dec8(std::uint8_t value);
    void alu_apply(int op, std::uint8_t operand);
    std::uint16_t alu_add16(std::uint16_t base, std::uint16_t addend);
    void alu_rotate_accumulator(int op);
    void alu_daa();
    void alu_cpl();
    void alu_scf();
    void alu_ccf();

    // CB-prefixed helpers.
    std::uint8_t alu_shift_rotate(int op, std::uint8_t value);
    void alu_bit(int bit, std::uint8_t value, std::uint8_t undoc_source);

    // ED-prefixed helpers.
    std::uint16_t alu_adc16(std::uint16_t addend);
    std::uint16_t alu_sbc16(std::uint16_t subtrahend);
    void alu_neg();
    void alu_rrd();
    void alu_rld();
    void ld_a_ir(std::uint8_t value);
    void io_in_flags(std::uint8_t data);
    std::uint32_t block_transfer(int delta, bool repeat);
    std::uint32_t block_compare(int delta, bool repeat);
    std::uint32_t block_input(int delta, bool repeat);
    std::uint32_t block_output(int delta, bool repeat);

    std::uint32_t execute_unprefixed(std::uint8_t opcode);
    std::uint32_t execute_cb_prefixed();
    std::uint32_t execute_ed_prefixed();

    // DD/FD indexed prefixes and DDCB/FDCB indexed-bit family.
    // execute_indexed handles prefix chaining/fallthrough; the returned cost
    // EXCLUDES the 4T of the prefix that led into it (the caller adds that).
    std::uint32_t execute_indexed(bool use_iy);
    std::uint32_t execute_indexed_opcode(std::uint8_t opcode, bool use_iy);
    std::uint32_t execute_indexed_cb(bool use_iy);
    std::uint32_t indexed_inc_dec(int y, bool use_iy, bool is_dec);

    // Index-register accessors (IX when use_iy==false, IY when true).
    [[nodiscard]] std::uint16_t get_index(bool use_iy) const;
    void set_index(bool use_iy, std::uint16_t value);
    [[nodiscard]] std::uint8_t index_high(bool use_iy) const;
    [[nodiscard]] std::uint8_t index_low(bool use_iy) const;
    void set_index_high(bool use_iy, std::uint8_t value);
    void set_index_low(bool use_iy, std::uint8_t value);
    [[nodiscard]] std::uint16_t index_displaced_address(bool use_iy, std::int8_t d) const;

    // 8-bit operand access with H/L->IXH/IXL translation (index 6 never passed).
    std::uint8_t read_reg_indexed_half(int index, bool use_iy);
    void write_reg_indexed_half(int index, bool use_iy, std::uint8_t value);
    // 16-bit pair access where p==2 selects the index register instead of HL.
    std::uint16_t read_rp_indexed(int p, bool use_iy) const;

    std::uint32_t service_nmi();
    std::uint32_t service_maskable_interrupt();

    CpuBusClient& bus_;
    Z80aState state_{};

    // Trace-export state. Non-owning observer pointer; the pending
    // record accumulates the pre-execution snapshot and opcode bytes for the
    // in-flight instruction and is emitted when the step completes.
    Z80aTraceObserver* trace_observer_ = nullptr;
    Z80aTraceRecord trace_pending_{};
    std::uint64_t trace_sequence_ = 0;

    // M1 opcode-fetch cycle counter. Reset at step() entry and
    // incremented once per opcode-fetch M1 inside increment_refresh_register()
    // (fetch_opcode() per opcode/prefix byte, plus the halted-idle phantom-M1
    // refetch). The interrupt/NMI-acknowledge M1 ticks R via tick_refresh_only()
    // but deliberately does NOT increment this counter (see m1_cycles_last_step()).
    std::uint32_t m1_cycle_count_ = 0;

    // Q latch snapshot. Captured at the top of each step()
    // as the Q value produced by the PREVIOUS instruction, then regs().q is
    // cleared so this instruction re-latches it via set_f(). alu_scf()/alu_ccf()
    // read q_prev_ to form the genuine-Zilog X/Y = ((Q ^ F) | A) result. After a
    // step that never writes F (incl. interrupt acceptance, EX AF,AF', POP AF),
    // q_prev_ for the next step is 0.
    std::uint8_t q_prev_ = 0;
};

}  // namespace sony_msx::devices::cpu