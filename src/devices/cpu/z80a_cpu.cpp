#include "devices/cpu/z80a_cpu.h"

namespace sony_msx::devices::cpu {

namespace {

constexpr std::uint16_t kIm1Vector = 0x0038;  // RST 38h target (fixed IM1 vector).
constexpr std::uint16_t kNmiVector = 0x0066;

constexpr std::uint8_t kS = Z80aState::kFlagSign;
constexpr std::uint8_t kZ = Z80aState::kFlagZero;
constexpr std::uint8_t kY = Z80aState::kFlagY;
constexpr std::uint8_t kH = Z80aState::kFlagHalfCarry;
constexpr std::uint8_t kX = Z80aState::kFlagX;
constexpr std::uint8_t kPV = Z80aState::kFlagParityOverflow;
constexpr std::uint8_t kN = Z80aState::kFlagSubtract;
constexpr std::uint8_t kC = Z80aState::kFlagCarry;
constexpr std::uint8_t kXY = static_cast<std::uint8_t>(kX | kY);

bool compute_even_parity(std::uint8_t value) {
    value ^= static_cast<std::uint8_t>(value >> 4);
    value ^= static_cast<std::uint8_t>(value >> 2);
    value ^= static_cast<std::uint8_t>(value >> 1);
    return (value & 0x01) == 0;
}

}  // namespace

Z80aCpu::Z80aCpu(CpuBusClient& bus) : bus_(bus) {
    reset();
}

void Z80aCpu::reset() {
    state_.reset();
}

std::uint32_t Z80aCpu::step() {
    trace_begin_instruction();

    // Reset the per-step M1 counter; it accrues one tick per opcode-fetch (and
    // the interrupt-ack M1) via increment_refresh_register() (M11-S1).
    m1_cycle_count_ = 0;

    // Q latch (M12-S4): snapshot the Q produced by the previous instruction, then
    // clear the live latch so that only a flag write by THIS instruction re-sets
    // it (fact-sheet §8). A step that never writes F — including interrupt
    // acceptance below — therefore leaves Q = 0 for the next instruction.
    q_prev_ = state_.regs().q;
    state_.regs().q = 0;

    const bool blocked_this_step = state_.ei_delay_active();

    std::uint32_t tstates = 0;
    if (state_.nmi_pending()) {
        tstates = service_nmi();
    } else if (!blocked_this_step && state_.maskable_interrupt_pending() && state_.iff1()) {
        tstates = service_maskable_interrupt();
    } else if (state_.halted()) {
        tstates = 4;
    } else {
        const std::uint8_t opcode = fetch_opcode();
        tstates = execute_unprefixed(opcode);
    }

    if (blocked_this_step) {
        state_.set_ei_delay_active(false);
    }

    state_.add_tstates(tstates);
    trace_end_instruction(tstates);
    return tstates;
}

void Z80aCpu::set_trace_observer(Z80aTraceObserver* const observer) {
    trace_observer_ = observer;
}

Z80aTraceObserver* Z80aCpu::trace_observer() const {
    return trace_observer_;
}

void Z80aCpu::trace_begin_instruction() {
    if (trace_observer_ == nullptr) {
        return;
    }

    const Z80aRegisters& r = state_.regs();
    Z80aTraceRecord& rec = trace_pending_;
    rec = Z80aTraceRecord{};
    rec.sequence = trace_sequence_;
    rec.pc = r.pc;
    rec.a = r.a();
    rec.f = r.f();
    rec.b = r.b();
    rec.c = r.c();
    rec.d = r.d();
    rec.e = r.e();
    rec.h = r.h();
    rec.l = r.l();
    rec.af = r.af;
    rec.bc = r.bc;
    rec.de = r.de;
    rec.hl = r.hl;
    rec.af_shadow = r.af_shadow;
    rec.bc_shadow = r.bc_shadow;
    rec.de_shadow = r.de_shadow;
    rec.hl_shadow = r.hl_shadow;
    rec.ix = r.ix;
    rec.iy = r.iy;
    rec.sp = r.sp;
    rec.i = r.i;
    rec.r = r.r;
    rec.iff1 = state_.iff1();
    rec.iff2 = state_.iff2();
    rec.im = state_.interrupt_mode();
    rec.opcode_length = 0;
}

void Z80aCpu::trace_capture_opcode_byte(const std::uint8_t value) {
    if (trace_observer_ == nullptr) {
        return;
    }
    if (trace_pending_.opcode_length < trace_pending_.opcode_bytes.size()) {
        trace_pending_.opcode_bytes[trace_pending_.opcode_length] = value;
        ++trace_pending_.opcode_length;
    }
}

void Z80aCpu::trace_end_instruction(const std::uint32_t instr_tstates) {
    if (trace_observer_ == nullptr) {
        return;
    }
    trace_pending_.instr_tstates = instr_tstates;
    trace_pending_.cumulative_tstates = state_.total_tstates();
    trace_observer_->on_instruction_retired(trace_pending_);
    ++trace_sequence_;
}

void Z80aCpu::request_maskable_interrupt() {
    state_.request_maskable_interrupt();
}

void Z80aCpu::request_maskable_interrupt(const std::uint8_t bus_vector) {
    state_.request_maskable_interrupt(bus_vector);
}

void Z80aCpu::request_nmi() {
    state_.request_nmi();
}

void Z80aCpu::set_interrupt_mode(const InterruptMode mode) {
    state_.set_interrupt_mode(mode);
}

const Z80aState& Z80aCpu::state() const {
    return state_;
}

Z80aState& Z80aCpu::state() {
    return state_;
}

std::uint8_t Z80aCpu::fetch_opcode() {
    const std::uint16_t address = state_.regs().pc;
    state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc + 1);
    increment_refresh_register();
    const std::uint8_t opcode = bus_.fetch_opcode(address);
    trace_capture_opcode_byte(opcode);
    return opcode;
}

std::uint8_t Z80aCpu::read_imm8() {
    const std::uint16_t address = state_.regs().pc;
    state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc + 1);
    return bus_.read_data(address);
}

std::uint16_t Z80aCpu::read_imm16() {
    const std::uint8_t lo = read_imm8();
    const std::uint8_t hi = read_imm8();
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

void Z80aCpu::push16(const std::uint16_t value) {
    state_.regs().sp = static_cast<std::uint16_t>(state_.regs().sp - 1);
    bus_.write_data(state_.regs().sp, static_cast<std::uint8_t>((value >> 8) & 0xFF));
    state_.regs().sp = static_cast<std::uint16_t>(state_.regs().sp - 1);
    bus_.write_data(state_.regs().sp, static_cast<std::uint8_t>(value & 0xFF));
}

std::uint16_t Z80aCpu::pop16() {
    const std::uint8_t lo = bus_.read_data(state_.regs().sp);
    state_.regs().sp = static_cast<std::uint16_t>(state_.regs().sp + 1);
    const std::uint8_t hi = bus_.read_data(state_.regs().sp);
    state_.regs().sp = static_cast<std::uint16_t>(state_.regs().sp + 1);
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

void Z80aCpu::increment_refresh_register() {
    // Every call marks one M1 cycle: this is invoked from fetch_opcode() for each
    // opcode/prefix byte and once from service_maskable_interrupt() for the
    // acknowledge M1 — exactly the cycles that assert /M1 and take the S1985 wait.
    ++m1_cycle_count_;

    const std::uint8_t preserved_msb = static_cast<std::uint8_t>(state_.regs().r & 0x80);
    const std::uint8_t low7 = static_cast<std::uint8_t>((state_.regs().r + 1) & 0x7F);
    state_.regs().r = static_cast<std::uint8_t>(preserved_msb | low7);
}

std::uint32_t Z80aCpu::m1_cycles_last_step() const {
    return m1_cycle_count_;
}

std::uint8_t Z80aCpu::read_reg_index(const int index, bool& touched_memory) {
    touched_memory = false;
    switch (index) {
    case 0:
        return state_.regs().b();
    case 1:
        return state_.regs().c();
    case 2:
        return state_.regs().d();
    case 3:
        return state_.regs().e();
    case 4:
        return state_.regs().h();
    case 5:
        return state_.regs().l();
    case 6:
        touched_memory = true;
        return bus_.read_data(state_.regs().hl);
    default:
        return state_.regs().a();
    }
}

void Z80aCpu::write_reg_index(const int index, const std::uint8_t value) {
    switch (index) {
    case 0:
        state_.regs().set_b(value);
        break;
    case 1:
        state_.regs().set_c(value);
        break;
    case 2:
        state_.regs().set_d(value);
        break;
    case 3:
        state_.regs().set_e(value);
        break;
    case 4:
        state_.regs().set_h(value);
        break;
    case 5:
        state_.regs().set_l(value);
        break;
    case 6:
        bus_.write_data(state_.regs().hl, value);
        break;
    default:
        state_.regs().set_a(value);
        break;
    }
}

std::uint16_t Z80aCpu::read_rp(const int index) const {
    switch (index) {
    case 0:
        return state_.regs().bc;
    case 1:
        return state_.regs().de;
    case 2:
        return state_.regs().hl;
    default:
        return state_.regs().sp;
    }
}

void Z80aCpu::write_rp(const int index, const std::uint16_t value) {
    switch (index) {
    case 0:
        state_.regs().bc = value;
        break;
    case 1:
        state_.regs().de = value;
        break;
    case 2:
        state_.regs().hl = value;
        break;
    default:
        state_.regs().sp = value;
        break;
    }
}

std::uint16_t Z80aCpu::read_rp2(const int index) const {
    switch (index) {
    case 0:
        return state_.regs().bc;
    case 1:
        return state_.regs().de;
    case 2:
        return state_.regs().hl;
    default:
        return state_.regs().af;
    }
}

void Z80aCpu::write_rp2(const int index, const std::uint16_t value) {
    switch (index) {
    case 0:
        state_.regs().bc = value;
        break;
    case 1:
        state_.regs().de = value;
        break;
    case 2:
        state_.regs().hl = value;
        break;
    default:
        state_.regs().af = value;
        break;
    }
}

bool Z80aCpu::check_condition(const int index) const {
    const std::uint8_t f = state_.regs().f();
    switch (index) {
    case 0:
        return (f & kZ) == 0;  // NZ
    case 1:
        return (f & kZ) != 0;  // Z
    case 2:
        return (f & kC) == 0;  // NC
    case 3:
        return (f & kC) != 0;  // C
    case 4:
        return (f & kPV) == 0;  // PO (parity odd)
    case 5:
        return (f & kPV) != 0;  // PE (parity even)
    case 6:
        return (f & kS) == 0;  // P (sign positive)
    default:
        return (f & kS) != 0;  // M (sign negative)
    }
}

std::uint8_t Z80aCpu::alu_inc8(const std::uint8_t value) {
    const std::uint8_t result = static_cast<std::uint8_t>(value + 1);
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if ((value & 0x0F) == 0x0F) {
        flags |= kH;
    }
    if (value == 0x7F) {
        flags |= kPV;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_f(flags);
    return result;
}

std::uint8_t Z80aCpu::alu_dec8(const std::uint8_t value) {
    const std::uint8_t result = static_cast<std::uint8_t>(value - 1);
    std::uint8_t flags = static_cast<std::uint8_t>((state_.regs().f() & kC) | kN);
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if ((value & 0x0F) == 0x00) {
        flags |= kH;
    }
    if (value == 0x80) {
        flags |= kPV;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_f(flags);
    return result;
}

void Z80aCpu::alu_apply(const int op, const std::uint8_t operand) {
    const std::uint8_t a = state_.regs().a();
    std::uint8_t flags = 0;
    std::uint8_t result = 0;
    bool store = true;

    switch (op) {
    case 0:    // ADD A,operand
    case 1: {  // ADC A,operand
        const std::uint8_t carry_in = (op == 1 && (state_.regs().f() & kC) != 0) ? 1 : 0;
        const std::uint16_t sum = static_cast<std::uint16_t>(a + operand + carry_in);
        result = static_cast<std::uint8_t>(sum);
        if (((a & 0x0F) + (operand & 0x0F) + carry_in) > 0x0F) {
            flags |= kH;
        }
        if (((~(a ^ operand)) & (a ^ result) & 0x80) != 0) {
            flags |= kPV;
        }
        if (sum > 0xFF) {
            flags |= kC;
        }
        break;
    }
    case 2:    // SUB operand
    case 3:    // SBC A,operand
    case 7: {  // CP operand
        const std::uint8_t carry_in = (op == 3 && (state_.regs().f() & kC) != 0) ? 1 : 0;
        const int diff = static_cast<int>(a) - static_cast<int>(operand) - static_cast<int>(carry_in);
        result = static_cast<std::uint8_t>(diff);
        flags |= kN;
        if (((a & 0x0F) - (operand & 0x0F) - carry_in) < 0) {
            flags |= kH;
        }
        if (((a ^ operand) & (a ^ result) & 0x80) != 0) {
            flags |= kPV;
        }
        if (diff < 0) {
            flags |= kC;
        }
        if (op == 7) {
            store = false;  // CP does not write A; X/Y come from the operand.
        }
        break;
    }
    case 4:  // AND operand
        result = static_cast<std::uint8_t>(a & operand);
        flags |= kH;
        if (compute_even_parity(result)) {
            flags |= kPV;
        }
        break;
    case 5:  // XOR operand
        result = static_cast<std::uint8_t>(a ^ operand);
        if (compute_even_parity(result)) {
            flags |= kPV;
        }
        break;
    default:  // 6: OR operand
        result = static_cast<std::uint8_t>(a | operand);
        if (compute_even_parity(result)) {
            flags |= kPV;
        }
        break;
    }

    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    flags |= static_cast<std::uint8_t>((op == 7 ? operand : result) & kXY);

    if (store) {
        state_.regs().set_a(result);
    }
    state_.regs().set_f(flags);
}

std::uint16_t Z80aCpu::alu_add16(const std::uint16_t base, const std::uint16_t addend) {
    const std::uint32_t sum = static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(addend);
    const std::uint16_t result = static_cast<std::uint16_t>(sum);
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & (kS | kZ | kPV));
    if (((base & 0x0FFF) + (addend & 0x0FFF)) > 0x0FFF) {
        flags |= kH;
    }
    if (sum > 0xFFFF) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>((result >> 8) & kXY);
    state_.regs().set_f(flags);
    return result;
}

void Z80aCpu::alu_rotate_accumulator(const int op) {
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t carry_in = (state_.regs().f() & kC) != 0 ? 1 : 0;
    std::uint8_t result = 0;
    std::uint8_t carry_out = 0;

    switch (op) {
    case 0:  // RLCA
        carry_out = static_cast<std::uint8_t>(a >> 7);
        result = static_cast<std::uint8_t>((a << 1) | carry_out);
        break;
    case 1:  // RRCA
        carry_out = static_cast<std::uint8_t>(a & 0x01);
        result = static_cast<std::uint8_t>((a >> 1) | (carry_out << 7));
        break;
    case 2:  // RLA
        carry_out = static_cast<std::uint8_t>(a >> 7);
        result = static_cast<std::uint8_t>((a << 1) | carry_in);
        break;
    default:  // 3: RRA
        carry_out = static_cast<std::uint8_t>(a & 0x01);
        result = static_cast<std::uint8_t>((a >> 1) | (carry_in << 7));
        break;
    }

    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & (kS | kZ | kPV));
    if (carry_out != 0) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_a(result);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_daa() {
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t f = state_.regs().f();
    const bool nf = (f & kN) != 0;
    const bool hf = (f & kH) != 0;
    const bool cf = (f & kC) != 0;

    std::uint8_t correction = 0;
    bool carry_out = false;
    if (hf || (a & 0x0F) > 0x09) {
        correction |= 0x06;
    }
    if (cf || a > 0x99) {
        correction |= 0x60;
        carry_out = true;
    }

    const std::uint8_t result = nf ? static_cast<std::uint8_t>(a - correction)
                                   : static_cast<std::uint8_t>(a + correction);

    std::uint8_t flags = nf ? kN : 0;
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if (((a ^ result) & 0x10) != 0) {
        flags |= kH;
    }
    if (compute_even_parity(result)) {
        flags |= kPV;
    }
    if (carry_out) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_a(result);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_cpl() {
    const std::uint8_t result = static_cast<std::uint8_t>(~state_.regs().a());
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & (kS | kZ | kPV | kC));
    flags |= kH | kN;
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_a(result);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_scf() {
    // Genuine-Zilog NMOS SCF undocumented X/Y (M12-S4, gap #20/#21): X/Y are bits
    // 3/5 of ((Q ^ F) | A), where Q is the flag byte latched by the previous
    // instruction (0 if it did not modify flags). Fact-sheet §8 (Patrik-Rak).
    // NOTE: this deliberately diverges from openMSX's (F | A) OR-form
    // (CPUCore.cc:4268-4269), which omits the Q latch — see planner A-4/R-2.
    const std::uint8_t f = state_.regs().f();
    const std::uint8_t xy_source = static_cast<std::uint8_t>((q_prev_ ^ f) | state_.regs().a());
    std::uint8_t flags = static_cast<std::uint8_t>(f & (kS | kZ | kPV));
    flags |= kC;
    flags |= static_cast<std::uint8_t>(xy_source & kXY);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_ccf() {
    // Genuine-Zilog NMOS CCF: same ((Q ^ F) | A) X/Y rule as SCF; H takes the old
    // carry (fact-sheet §8). Diverges from openMSX (F | A) form (A-4/R-2).
    const std::uint8_t f = state_.regs().f();
    const std::uint8_t old_carry = static_cast<std::uint8_t>(f & kC);
    const std::uint8_t xy_source = static_cast<std::uint8_t>((q_prev_ ^ f) | state_.regs().a());
    std::uint8_t flags = static_cast<std::uint8_t>(f & (kS | kZ | kPV));
    if (old_carry == 0) {
        flags |= kC;
    } else {
        flags |= kH;  // H becomes the previous carry.
    }
    flags |= static_cast<std::uint8_t>(xy_source & kXY);
    state_.regs().set_f(flags);
}

std::uint8_t Z80aCpu::alu_shift_rotate(const int op, const std::uint8_t value) {
    const std::uint8_t carry_in = (state_.regs().f() & kC) != 0 ? 1 : 0;
    std::uint8_t result = 0;
    std::uint8_t carry_out = 0;

    switch (op) {
    case 0:  // RLC
        carry_out = static_cast<std::uint8_t>(value >> 7);
        result = static_cast<std::uint8_t>((value << 1) | carry_out);
        break;
    case 1:  // RRC
        carry_out = static_cast<std::uint8_t>(value & 0x01);
        result = static_cast<std::uint8_t>((value >> 1) | (carry_out << 7));
        break;
    case 2:  // RL
        carry_out = static_cast<std::uint8_t>(value >> 7);
        result = static_cast<std::uint8_t>((value << 1) | carry_in);
        break;
    case 3:  // RR
        carry_out = static_cast<std::uint8_t>(value & 0x01);
        result = static_cast<std::uint8_t>((value >> 1) | (carry_in << 7));
        break;
    case 4:  // SLA
        carry_out = static_cast<std::uint8_t>(value >> 7);
        result = static_cast<std::uint8_t>(value << 1);
        break;
    case 5:  // SRA (arithmetic: preserve bit 7)
        carry_out = static_cast<std::uint8_t>(value & 0x01);
        result = static_cast<std::uint8_t>((value >> 1) | (value & 0x80));
        break;
    case 6:  // SLL (undocumented: shift left, set bit 0)
        carry_out = static_cast<std::uint8_t>(value >> 7);
        result = static_cast<std::uint8_t>((value << 1) | 0x01);
        break;
    default:  // 7: SRL
        carry_out = static_cast<std::uint8_t>(value & 0x01);
        result = static_cast<std::uint8_t>(value >> 1);
        break;
    }

    std::uint8_t flags = 0;
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if (compute_even_parity(result)) {
        flags |= kPV;
    }
    if (carry_out != 0) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_f(flags);
    return result;
}

void Z80aCpu::alu_bit(const int bit, const std::uint8_t value, const std::uint8_t undoc_source) {
    const bool bit_set = (value & (1u << bit)) != 0;
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    flags |= kH;
    if (!bit_set) {
        flags |= kZ | kPV;
    }
    if (bit == 7 && bit_set) {
        flags |= kS;
    }
    flags |= static_cast<std::uint8_t>(undoc_source & kXY);
    state_.regs().set_f(flags);
}

std::uint32_t Z80aCpu::execute_unprefixed(const std::uint8_t opcode) {
    const int x = opcode >> 6;
    const int y = (opcode >> 3) & 0x07;
    const int z = opcode & 0x07;
    const int p = y >> 1;
    const int q = y & 0x01;

    switch (x) {
    case 0:
        switch (z) {
        case 0:
            switch (y) {
            case 0:  // NOP
                return 4;
            case 1: {  // EX AF,AF'
                const std::uint16_t tmp = state_.regs().af;
                state_.regs().af = state_.regs().af_shadow;
                state_.regs().af_shadow = tmp;
                return 4;
            }
            case 2: {  // DJNZ d
                const std::int8_t d = static_cast<std::int8_t>(read_imm8());
                const std::uint8_t b = static_cast<std::uint8_t>(state_.regs().b() - 1);
                state_.regs().set_b(b);
                if (b != 0) {
                    state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc + d);
                    state_.regs().wz = state_.regs().pc;  // WZ=dest when taken (§4)
                    return 13;
                }
                return 8;
            }
            case 3: {  // JR d
                const std::int8_t d = static_cast<std::int8_t>(read_imm8());
                state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc + d);
                state_.regs().wz = state_.regs().pc;  // WZ=dest (§4)
                return 12;
            }
            default: {  // JR cc[y-4], d
                const std::int8_t d = static_cast<std::int8_t>(read_imm8());
                if (check_condition(y - 4)) {
                    state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc + d);
                    state_.regs().wz = state_.regs().pc;  // WZ=dest only when taken (§4)
                    return 12;
                }
                return 7;
            }
            }
        case 1:
            if (q == 0) {  // LD rp[p],nn
                write_rp(p, read_imm16());
                return 10;
            }
            // ADD HL,rp[p]. WZ = HL+1 taken before the add (§4; openMSX
            // CPUCore.cc:3247).
            state_.regs().wz = static_cast<std::uint16_t>(state_.regs().hl + 1);
            state_.regs().hl = alu_add16(state_.regs().hl, read_rp(p));
            return 11;
        case 2:
            if (q == 0) {
                switch (p) {
                case 0:  // LD (BC),A: WZ = (A<<8)|((BC+1)&0xFF)  (§4; openMSX:2711)
                    state_.regs().wz = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(state_.regs().a()) << 8) |
                        ((state_.regs().bc + 1) & 0xFF));
                    bus_.write_data(state_.regs().bc, state_.regs().a());
                    return 7;
                case 1:  // LD (DE),A: WZ = (A<<8)|((DE+1)&0xFF)
                    state_.regs().wz = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(state_.regs().a()) << 8) |
                        ((state_.regs().de + 1) & 0xFF));
                    bus_.write_data(state_.regs().de, state_.regs().a());
                    return 7;
                case 2: {  // LD (nn),HL: WZ = nn+1  (§4; openMSX:2760)
                    const std::uint16_t nn = read_imm16();
                    state_.regs().wz = static_cast<std::uint16_t>(nn + 1);
                    bus_.write_word_le(nn, state_.regs().hl);
                    return 16;
                }
                default: {  // LD (nn),A: WZ = (A<<8)|((nn+1)&0xFF)  (openMSX:2752)
                    const std::uint16_t nn = read_imm16();
                    state_.regs().wz = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(state_.regs().a()) << 8) | ((nn + 1) & 0xFF));
                    bus_.write_data(nn, state_.regs().a());
                    return 13;
                }
                }
            }
            switch (p) {
            case 0:  // LD A,(BC): WZ = BC+1  (§4; openMSX:2773)
                state_.regs().wz = static_cast<std::uint16_t>(state_.regs().bc + 1);
                state_.regs().set_a(bus_.read_data(state_.regs().bc));
                return 7;
            case 1:  // LD A,(DE): WZ = DE+1
                state_.regs().wz = static_cast<std::uint16_t>(state_.regs().de + 1);
                state_.regs().set_a(bus_.read_data(state_.regs().de));
                return 7;
            case 2: {  // LD HL,(nn): WZ = nn+1  (§4; openMSX:2808)
                const std::uint16_t nn = read_imm16();
                state_.regs().wz = static_cast<std::uint16_t>(nn + 1);
                state_.regs().hl = bus_.read_word_le(nn);
                return 16;
            }
            default: {  // LD A,(nn): WZ = nn+1  (§4; openMSX:2781)
                const std::uint16_t nn = read_imm16();
                state_.regs().wz = static_cast<std::uint16_t>(nn + 1);
                state_.regs().set_a(bus_.read_data(nn));
                return 13;
            }
            }
        case 3:
            if (q == 0) {  // INC rp[p]
                write_rp(p, static_cast<std::uint16_t>(read_rp(p) + 1));
            } else {  // DEC rp[p]
                write_rp(p, static_cast<std::uint16_t>(read_rp(p) - 1));
            }
            return 6;
        case 4: {  // INC r[y]
            bool memory = false;
            const std::uint8_t value = read_reg_index(y, memory);
            write_reg_index(y, alu_inc8(value));
            return memory ? 11 : 4;
        }
        case 5: {  // DEC r[y]
            bool memory = false;
            const std::uint8_t value = read_reg_index(y, memory);
            write_reg_index(y, alu_dec8(value));
            return memory ? 11 : 4;
        }
        case 6: {  // LD r[y],n
            const std::uint8_t n = read_imm8();
            write_reg_index(y, n);
            return (y == 6) ? 10 : 7;
        }
        default:  // z == 7: accumulator / flag operations
            switch (y) {
            case 0:
            case 1:
            case 2:
            case 3:
                alu_rotate_accumulator(y);
                return 4;
            case 4:
                alu_daa();
                return 4;
            case 5:
                alu_cpl();
                return 4;
            case 6:
                alu_scf();
                return 4;
            default:
                alu_ccf();
                return 4;
            }
        }
    case 1: {  // LD r[y],r[z] with HALT special case
        if (y == 6 && z == 6) {
            state_.set_halted(true);
            return 4;
        }
        bool read_memory = false;
        const std::uint8_t value = read_reg_index(z, read_memory);
        write_reg_index(y, value);
        return (y == 6 || z == 6) ? 7 : 4;
    }
    case 2: {  // alu[y] A,r[z]
        bool memory = false;
        const std::uint8_t value = read_reg_index(z, memory);
        alu_apply(y, value);
        return memory ? 7 : 4;
    }
    default:  // x == 3
        switch (z) {
        case 0:  // RET cc[y]
            if (check_condition(y)) {
                state_.regs().pc = pop16();
                state_.regs().wz = state_.regs().pc;  // WZ = return address (openMSX:3898)
                return 11;
            }
            return 5;
        case 1:
            if (q == 0) {  // POP rp2[p]
                write_rp2(p, pop16());
                return 10;
            }
            switch (p) {
            case 0:  // RET
                state_.regs().pc = pop16();
                state_.regs().wz = state_.regs().pc;  // WZ = return address (openMSX:3898)
                return 10;
            case 1: {  // EXX
                std::uint16_t tmp = state_.regs().bc;
                state_.regs().bc = state_.regs().bc_shadow;
                state_.regs().bc_shadow = tmp;
                tmp = state_.regs().de;
                state_.regs().de = state_.regs().de_shadow;
                state_.regs().de_shadow = tmp;
                tmp = state_.regs().hl;
                state_.regs().hl = state_.regs().hl_shadow;
                state_.regs().hl_shadow = tmp;
                return 4;
            }
            case 2:  // JP (HL)
                state_.regs().pc = state_.regs().hl;
                return 4;
            default:  // LD SP,HL
                state_.regs().sp = state_.regs().hl;
                return 6;
            }
        case 2: {  // JP cc[y],nn: WZ = nn even when not taken (§4; openMSX:3926)
            const std::uint16_t address = read_imm16();
            state_.regs().wz = address;
            if (check_condition(y)) {
                state_.regs().pc = address;
            }
            return 10;
        }
        case 3:
            switch (y) {
            case 0: {  // JP nn: WZ = nn (§4; openMSX:3926)
                const std::uint16_t address = read_imm16();
                state_.regs().wz = address;
                state_.regs().pc = address;
                return 10;
            }
            case 1:  // CB prefix
                return execute_cb_prefixed();
            case 2: {  // OUT (n),A  -- port = (A<<8)|n, data = A (M11-S1 I/O seam).
                const std::uint8_t n = read_imm8();
                const std::uint16_t port =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(state_.regs().a()) << 8) | n);
                // WZ = (A<<8)|((n+1)&0xFF)  (§4; openMSX:4052).
                state_.regs().wz = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(state_.regs().a()) << 8) | ((n + 1) & 0xFF));
                bus_.io_write(port, state_.regs().a());
                return 11;
            }
            case 3: {  // IN A,(n)   -- port = (A<<8)|n, A <- I/O bus (open-bus 0xFF).
                const std::uint8_t n = read_imm8();
                const std::uint16_t port =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(state_.regs().a()) << 8) | n);
                // WZ = port+1 = ((A<<8)|n)+1  (§4; openMSX:4026).
                state_.regs().wz = static_cast<std::uint16_t>(port + 1);
                state_.regs().set_a(bus_.io_read(port));
                return 11;
            }
            case 4: {  // EX (SP),HL
                const std::uint8_t lo = bus_.read_data(state_.regs().sp);
                const std::uint8_t hi = bus_.read_data(static_cast<std::uint16_t>(state_.regs().sp + 1));
                const std::uint16_t stacked = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
                bus_.write_data(state_.regs().sp, static_cast<std::uint8_t>(state_.regs().hl & 0xFF));
                bus_.write_data(static_cast<std::uint16_t>(state_.regs().sp + 1),
                                static_cast<std::uint8_t>((state_.regs().hl >> 8) & 0xFF));
                state_.regs().hl = stacked;
                state_.regs().wz = stacked;  // WZ = value exchanged from stack (openMSX:3999)
                return 19;
            }
            case 5: {  // EX DE,HL
                const std::uint16_t tmp = state_.regs().de;
                state_.regs().de = state_.regs().hl;
                state_.regs().hl = tmp;
                return 4;
            }
            case 6:  // DI
                state_.set_iff1(false);
                state_.set_iff2(false);
                return 4;
            default:  // EI
                state_.set_iff1(true);
                state_.set_iff2(true);
                state_.set_ei_delay_active(true);
                return 4;
            }
        case 4: {  // CALL cc[y],nn: WZ = nn even when not taken (§4; openMSX:3866)
            const std::uint16_t address = read_imm16();
            state_.regs().wz = address;
            if (check_condition(y)) {
                push16(state_.regs().pc);
                state_.regs().pc = address;
                return 17;
            }
            return 10;
        }
        case 5:
            if (q == 0) {  // PUSH rp2[p]
                push16(read_rp2(p));
                return 11;
            }
            switch (p) {
            case 0: {  // CALL nn: WZ = nn (§4; openMSX:3866)
                const std::uint16_t address = read_imm16();
                state_.regs().wz = address;
                push16(state_.regs().pc);
                state_.regs().pc = address;
                return 17;
            }
            case 2:  // ED prefix
                return execute_ed_prefixed();
            default:  // DD (p==1) -> IX prefix, FD (p==3) -> IY prefix.
                // The 4T here accounts for the prefix M1; execute_indexed adds
                // the cost of the index-modified (or fallen-through) base op.
                return 4 + execute_indexed(p == 3);
            }
        case 6: {  // alu[y] A,n
            const std::uint8_t n = read_imm8();
            alu_apply(y, n);
            return 7;
        }
        default:  // z == 7: RST y*8: WZ = target (openMSX:3884)
            push16(state_.regs().pc);
            state_.regs().pc = static_cast<std::uint16_t>(y * 8);
            state_.regs().wz = state_.regs().pc;
            return 11;
        }
    }
}

std::uint32_t Z80aCpu::execute_cb_prefixed() {
    const std::uint8_t opcode = fetch_opcode();
    const int x = opcode >> 6;
    const int y = (opcode >> 3) & 0x07;
    const int z = opcode & 0x07;
    const bool is_memory = (z == 6);

    if (x == 0) {  // Rotate / shift rot[y] on r[z]
        bool memory = false;
        const std::uint8_t value = read_reg_index(z, memory);
        write_reg_index(z, alu_shift_rotate(y, value));
        return is_memory ? 15 : 8;
    }

    if (x == 1) {  // BIT y, r[z]
        bool memory = false;
        const std::uint8_t value = read_reg_index(z, memory);
        // M12-S3 (gap #4): for BIT n,(HL) the undocumented X/Y bits come from the
        // high byte of WZ/MEMPTR (bits 11/13 of WZ), NOT the tested value
        // (fact-sheet §4; openMSX bit_N_xhl(), CPUCore.cc:3420). Register-operand
        // BIT still sources X/Y from the tested register value.
        const std::uint8_t undoc_source =
            is_memory ? static_cast<std::uint8_t>(state_.regs().wz >> 8) : value;
        alu_bit(y, value, undoc_source);
        return is_memory ? 12 : 8;
    }

    if (x == 2) {  // RES y, r[z]
        bool memory = false;
        const std::uint8_t value = read_reg_index(z, memory);
        write_reg_index(z, static_cast<std::uint8_t>(value & ~(1u << y)));
        return is_memory ? 15 : 8;
    }

    // x == 3: SET y, r[z]
    bool memory = false;
    const std::uint8_t value = read_reg_index(z, memory);
    write_reg_index(z, static_cast<std::uint8_t>(value | (1u << y)));
    return is_memory ? 15 : 8;
}

std::uint16_t Z80aCpu::alu_adc16(const std::uint16_t addend) {
    const std::uint16_t hl = state_.regs().hl;
    const std::uint8_t carry_in = (state_.regs().f() & kC) != 0 ? 1 : 0;
    const std::uint32_t sum = static_cast<std::uint32_t>(hl) + addend + carry_in;
    const std::uint16_t result = static_cast<std::uint16_t>(sum);
    std::uint8_t flags = 0;
    if ((result & 0x8000) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if (((hl & 0x0FFF) + (addend & 0x0FFF) + carry_in) > 0x0FFF) {
        flags |= kH;
    }
    if (((~(hl ^ addend)) & (hl ^ result) & 0x8000) != 0) {
        flags |= kPV;
    }
    if (sum > 0xFFFF) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>((result >> 8) & kXY);
    state_.regs().set_f(flags);
    return result;
}

std::uint16_t Z80aCpu::alu_sbc16(const std::uint16_t subtrahend) {
    const std::uint16_t hl = state_.regs().hl;
    const std::uint8_t carry_in = (state_.regs().f() & kC) != 0 ? 1 : 0;
    const std::int32_t diff =
        static_cast<std::int32_t>(hl) - static_cast<std::int32_t>(subtrahend) - static_cast<std::int32_t>(carry_in);
    const std::uint16_t result = static_cast<std::uint16_t>(diff);
    std::uint8_t flags = kN;
    if ((result & 0x8000) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if ((static_cast<std::int32_t>(hl & 0x0FFF) - static_cast<std::int32_t>(subtrahend & 0x0FFF) - carry_in) < 0) {
        flags |= kH;
    }
    if (((hl ^ subtrahend) & (hl ^ result) & 0x8000) != 0) {
        flags |= kPV;
    }
    if (diff < 0) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>((result >> 8) & kXY);
    state_.regs().set_f(flags);
    return result;
}

void Z80aCpu::alu_neg() {
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t result = static_cast<std::uint8_t>(0 - a);
    std::uint8_t flags = kN;
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if ((a & 0x0F) != 0) {
        flags |= kH;
    }
    if (a == 0x80) {
        flags |= kPV;
    }
    if (a != 0) {
        flags |= kC;
    }
    flags |= static_cast<std::uint8_t>(result & kXY);
    state_.regs().set_a(result);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_rrd() {
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t m = bus_.read_data(state_.regs().hl);
    const std::uint8_t new_a = static_cast<std::uint8_t>((a & 0xF0) | (m & 0x0F));
    const std::uint8_t new_m = static_cast<std::uint8_t>(((a & 0x0F) << 4) | (m >> 4));
    bus_.write_data(state_.regs().hl, new_m);
    state_.regs().set_a(new_a);
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    if ((new_a & 0x80) != 0) {
        flags |= kS;
    }
    if (new_a == 0) {
        flags |= kZ;
    }
    if (compute_even_parity(new_a)) {
        flags |= kPV;
    }
    flags |= static_cast<std::uint8_t>(new_a & kXY);
    state_.regs().set_f(flags);
}

void Z80aCpu::alu_rld() {
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t m = bus_.read_data(state_.regs().hl);
    const std::uint8_t new_a = static_cast<std::uint8_t>((a & 0xF0) | (m >> 4));
    const std::uint8_t new_m = static_cast<std::uint8_t>(((m & 0x0F) << 4) | (a & 0x0F));
    bus_.write_data(state_.regs().hl, new_m);
    state_.regs().set_a(new_a);
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    if ((new_a & 0x80) != 0) {
        flags |= kS;
    }
    if (new_a == 0) {
        flags |= kZ;
    }
    if (compute_even_parity(new_a)) {
        flags |= kPV;
    }
    flags |= static_cast<std::uint8_t>(new_a & kXY);
    state_.regs().set_f(flags);
}

void Z80aCpu::ld_a_ir(const std::uint8_t value) {
    state_.regs().set_a(value);
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    if ((value & 0x80) != 0) {
        flags |= kS;
    }
    if (value == 0) {
        flags |= kZ;
    }
    if (state_.iff2()) {
        flags |= kPV;  // P/V reflects IFF2 for LD A,I / LD A,R.
    }
    // NMOS LD A,I / LD A,R interrupt bug (M12-S5, gap #31): on the genuine Zilog
    // NMOS part, if a maskable interrupt is accepted during this instruction, the
    // P/V flag reads 0 even though IFF2 was set (fact-sheet §5; openMSX models it
    // as a fix-up at IRQ accept, CPUCore.cc:2476-2496). In this instruction-atomic
    // core we approximate the silicon race at the boundary: an IRQ is accepted at
    // the step immediately following LD A,I/R iff it is pending and IFF1 is set
    // (LD A,I/R itself never arms an EI-delay, so the following boundary is never
    // EI-blocked; this matches the openMSX note that the quirk is independent of a
    // preceding EI, CPUCore.cc:2490-2493). This is an approximation (planner R-4);
    // ZEXALL does not exercise the IRQ race, so it is unit-proven only.
    if (state_.maskable_interrupt_pending() && state_.iff1()) {
        flags &= static_cast<std::uint8_t>(~kPV);
    }
    flags |= static_cast<std::uint8_t>(value & kXY);
    state_.regs().set_f(flags);
}

void Z80aCpu::io_in_flags(const std::uint8_t data) {
    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    if ((data & 0x80) != 0) {
        flags |= kS;
    }
    if (data == 0) {
        flags |= kZ;
    }
    if (compute_even_parity(data)) {
        flags |= kPV;
    }
    flags |= static_cast<std::uint8_t>(data & kXY);
    state_.regs().set_f(flags);
}

std::uint32_t Z80aCpu::block_transfer(const int delta, const bool repeat) {
    const std::uint8_t value = bus_.read_data(state_.regs().hl);
    bus_.write_data(state_.regs().de, value);
    state_.regs().hl = static_cast<std::uint16_t>(state_.regs().hl + delta);
    state_.regs().de = static_cast<std::uint16_t>(state_.regs().de + delta);
    state_.regs().bc = static_cast<std::uint16_t>(state_.regs().bc - 1);
    const bool bc_nonzero = state_.regs().bc != 0;

    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & (kS | kZ | kC));
    if (bc_nonzero) {
        flags |= kPV;
    }
    const std::uint8_t n = static_cast<std::uint8_t>(value + state_.regs().a());
    if ((n & 0x08) != 0) {
        flags |= kX;  // bit 3
    }
    if ((n & 0x02) != 0) {
        flags |= kY;  // bit 1 feeds F5
    }
    state_.regs().set_f(flags);

    if (repeat && bc_nonzero) {
        state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc - 2);
        // LDIR/LDDR mid-iteration: WZ = PC+1 (§4; openMSX BLOCK_LD:4111).
        state_.regs().wz = static_cast<std::uint16_t>(state_.regs().pc + 1);
        return 21;
    }
    return 16;
}

std::uint32_t Z80aCpu::block_compare(const int delta, const bool repeat) {
    // CPI/CPD: WZ = WZ + delta (§4; openMSX BLOCK_CP:4061).
    state_.regs().wz = static_cast<std::uint16_t>(state_.regs().wz + delta);
    const std::uint8_t a = state_.regs().a();
    const std::uint8_t value = bus_.read_data(state_.regs().hl);
    const std::uint8_t result = static_cast<std::uint8_t>(a - value);
    const bool half = (static_cast<int>(a & 0x0F) - static_cast<int>(value & 0x0F)) < 0;
    state_.regs().hl = static_cast<std::uint16_t>(state_.regs().hl + delta);
    state_.regs().bc = static_cast<std::uint16_t>(state_.regs().bc - 1);
    const bool bc_nonzero = state_.regs().bc != 0;

    std::uint8_t flags = static_cast<std::uint8_t>(state_.regs().f() & kC);
    flags |= kN;
    if ((result & 0x80) != 0) {
        flags |= kS;
    }
    if (result == 0) {
        flags |= kZ;
    }
    if (half) {
        flags |= kH;
    }
    if (bc_nonzero) {
        flags |= kPV;
    }
    const std::uint8_t n = static_cast<std::uint8_t>(result - (half ? 1 : 0));
    if ((n & 0x08) != 0) {
        flags |= kX;  // bit 3
    }
    if ((n & 0x02) != 0) {
        flags |= kY;  // bit 1 feeds F5
    }
    state_.regs().set_f(flags);

    if (repeat && bc_nonzero && result != 0) {
        state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc - 2);
        // CPIR/CPDR mid-iteration: WZ = PC+1 (§4; openMSX BLOCK_CP:4081).
        state_.regs().wz = static_cast<std::uint16_t>(state_.regs().pc + 1);
        return 21;
    }
    return 16;
}

std::uint32_t Z80aCpu::block_input(const int delta, const bool repeat) {
    // INI/IND: WZ = BC + delta taken BEFORE B is decremented (§4; openMSX:4126).
    state_.regs().wz = static_cast<std::uint16_t>(state_.regs().bc + delta);
    // INI/IND/INIR/INDR: read the input port (C) into (HL). Dispatch keys on
    // port & 0xFF = C, so the pre-decrement B in the high byte is immaterial.
    const std::uint8_t data = bus_.io_read(state_.regs().bc);
    bus_.write_data(state_.regs().hl, data);
    const std::uint8_t b = static_cast<std::uint8_t>(state_.regs().b() - 1);
    state_.regs().set_b(b);
    state_.regs().hl = static_cast<std::uint16_t>(state_.regs().hl + delta);

    std::uint8_t flags = 0;
    if ((b & 0x80) != 0) {
        flags |= kS;
    }
    if (b == 0) {
        flags |= kZ;
    }
    flags |= static_cast<std::uint8_t>(b & kXY);
    if ((data & 0x80) != 0) {
        flags |= kN;
    }
    const std::uint16_t c_adj = static_cast<std::uint16_t>((state_.regs().c() + delta) & 0xFF);
    const std::uint16_t k = static_cast<std::uint16_t>(data + c_adj);
    if (k > 0xFF) {
        flags |= static_cast<std::uint8_t>(kH | kC);
    }
    if (compute_even_parity(static_cast<std::uint8_t>((k & 0x07) ^ b))) {
        flags |= kPV;
    }
    state_.regs().set_f(flags);

    if (repeat && b != 0) {
        state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc - 2);
        return 21;
    }
    return 16;
}

std::uint32_t Z80aCpu::block_output(const int delta, const bool repeat) {
    const std::uint8_t data = bus_.read_data(state_.regs().hl);
    // OUTI/OUTD/OUTIR/OUTDR: write (HL) to the output port (C). Dispatch keys on
    // port & 0xFF = C.
    bus_.io_write(state_.regs().bc, data);
    const std::uint8_t b = static_cast<std::uint8_t>(state_.regs().b() - 1);
    state_.regs().set_b(b);
    state_.regs().hl = static_cast<std::uint16_t>(state_.regs().hl + delta);
    // OUTI/OUTD: WZ = BC + delta taken AFTER B is decremented (§4; openMSX:4160).
    state_.regs().wz = static_cast<std::uint16_t>(state_.regs().bc + delta);

    std::uint8_t flags = 0;
    if ((b & 0x80) != 0) {
        flags |= kS;
    }
    if (b == 0) {
        flags |= kZ;
    }
    flags |= static_cast<std::uint8_t>(b & kXY);
    if ((data & 0x80) != 0) {
        flags |= kN;
    }
    const std::uint16_t k = static_cast<std::uint16_t>(data + state_.regs().l());
    if (k > 0xFF) {
        flags |= static_cast<std::uint8_t>(kH | kC);
    }
    if (compute_even_parity(static_cast<std::uint8_t>((k & 0x07) ^ b))) {
        flags |= kPV;
    }
    state_.regs().set_f(flags);

    if (repeat && b != 0) {
        state_.regs().pc = static_cast<std::uint16_t>(state_.regs().pc - 2);
        return 21;
    }
    return 16;
}

std::uint32_t Z80aCpu::execute_ed_prefixed() {
    const std::uint8_t opcode = fetch_opcode();
    const int x = opcode >> 6;
    const int y = (opcode >> 3) & 0x07;
    const int z = opcode & 0x07;
    const int p = y >> 1;
    const int q = y & 0x01;

    if (x == 1) {
        switch (z) {
        case 0: {  // IN r[y],(C)  (y == 6 -> IN (C): flags only, no register write)
            state_.regs().wz = static_cast<std::uint16_t>(state_.regs().bc + 1);  // WZ=BC+1 (§4;4008)
            const std::uint8_t data = bus_.io_read(state_.regs().bc);  // port = BC
            io_in_flags(data);
            if (y != 6) {
                write_reg_index(y, data);
            }
            return 12;
        }
        case 1: {  // OUT (C),r[y]  (y == 6 -> OUT (C),0)
            state_.regs().wz = static_cast<std::uint16_t>(state_.regs().bc + 1);  // WZ=BC+1 (§4;4042)
            bool touched_memory = false;
            const std::uint8_t data = (y == 6) ? 0x00 : read_reg_index(y, touched_memory);
            bus_.io_write(state_.regs().bc, data);  // port = BC
            return 12;
        }
        case 2:
            // ADC/SBC HL,rp: WZ = HL+1 taken before the op (§4; openMSX:3799/3817).
            state_.regs().wz = static_cast<std::uint16_t>(state_.regs().hl + 1);
            if (q == 0) {  // SBC HL,rp[p]
                state_.regs().hl = alu_sbc16(read_rp(p));
            } else {  // ADC HL,rp[p]
                state_.regs().hl = alu_adc16(read_rp(p));
            }
            return 15;
        case 3: {
            const std::uint16_t address = read_imm16();
            state_.regs().wz = static_cast<std::uint16_t>(address + 1);  // WZ=nn+1 (§4;2760/2808)
            if (q == 0) {  // LD (nn),rp[p]
                bus_.write_word_le(address, read_rp(p));
            } else {  // LD rp[p],(nn)
                write_rp(p, bus_.read_word_le(address));
            }
            return 20;
        }
        case 4:  // NEG (all y are documented/redundant NEG aliases)
            alu_neg();
            return 8;
        case 5:  // RETN (y!=1) / RETI (y==1): all ED-prefixed RETxx copy IFF2->IFF1
            // M12-S5 (gap #30): RETI is functionally identical to RETN inside the
            // CPU — both restore IFF1 from IFF2 (fact-sheet §5; openMSX retn() is
            // also reti(), CPUCore.cc:3911-3915). WZ = return address.
            state_.regs().pc = pop16();
            state_.regs().wz = state_.regs().pc;
            state_.set_iff1(state_.iff2());
            return 14;
        case 6: {  // IM im[y]
            static constexpr InterruptMode kImTable[8] = {
                InterruptMode::Im0, InterruptMode::Im0, InterruptMode::Im1, InterruptMode::Im2,
                InterruptMode::Im0, InterruptMode::Im0, InterruptMode::Im1, InterruptMode::Im2,
            };
            state_.set_interrupt_mode(kImTable[y]);
            return 8;
        }
        default:  // z == 7: interrupt/refresh registers, RRD/RLD
            switch (y) {
            case 0:  // LD I,A
                state_.regs().i = state_.regs().a();
                return 9;
            case 1:  // LD R,A
                state_.regs().r = state_.regs().a();
                return 9;
            case 2:  // LD A,I
                ld_a_ir(state_.regs().i);
                return 9;
            case 3:  // LD A,R
                ld_a_ir(state_.regs().r);
                return 9;
            case 4:  // RRD: WZ = HL+1 (§4; openMSX:3339)
                state_.regs().wz = static_cast<std::uint16_t>(state_.regs().hl + 1);
                alu_rrd();
                return 18;
            case 5:  // RLD: WZ = HL+1 (§4; openMSX:3366)
                state_.regs().wz = static_cast<std::uint16_t>(state_.regs().hl + 1);
                alu_rld();
                return 18;
            default:  // 6, 7: NOP
                return 8;
            }
        }
    }

    if (x == 2 && z <= 3 && y >= 4) {
        const int delta = (y & 0x01) == 0 ? 1 : -1;  // y=4/6 increment, y=5/7 decrement
        const bool repeat = y >= 6;                  // y=6/7 are the repeating variants
        switch (z) {
        case 0:  // LDI / LDD / LDIR / LDDR
            return block_transfer(delta, repeat);
        case 1:  // CPI / CPD / CPIR / CPDR
            return block_compare(delta, repeat);
        case 2:  // INI / IND / INIR / INDR
            return block_input(delta, repeat);
        default:  // z == 3: OUTI / OUTD / OTIR / OTDR
            return block_output(delta, repeat);
        }
    }

    // x == 0, x == 3, and undefined x == 2 entries: NOP (8T).
    return 8;
}

std::uint16_t Z80aCpu::get_index(const bool use_iy) const {
    return use_iy ? state_.regs().iy : state_.regs().ix;
}

void Z80aCpu::set_index(const bool use_iy, const std::uint16_t value) {
    if (use_iy) {
        state_.regs().iy = value;
    } else {
        state_.regs().ix = value;
    }
}

std::uint8_t Z80aCpu::index_high(const bool use_iy) const {
    return static_cast<std::uint8_t>(get_index(use_iy) >> 8);
}

std::uint8_t Z80aCpu::index_low(const bool use_iy) const {
    return static_cast<std::uint8_t>(get_index(use_iy) & 0xFF);
}

void Z80aCpu::set_index_high(const bool use_iy, const std::uint8_t value) {
    set_index(use_iy, static_cast<std::uint16_t>((get_index(use_iy) & 0x00FF) |
                                                 (static_cast<std::uint16_t>(value) << 8)));
}

void Z80aCpu::set_index_low(const bool use_iy, const std::uint8_t value) {
    set_index(use_iy, static_cast<std::uint16_t>((get_index(use_iy) & 0xFF00) | value));
}

std::uint16_t Z80aCpu::index_displaced_address(const bool use_iy, const std::int8_t d) const {
    return static_cast<std::uint16_t>(get_index(use_iy) + d);
}

std::uint8_t Z80aCpu::read_reg_indexed_half(const int index, const bool use_iy) {
    if (index == 4) {
        return index_high(use_iy);
    }
    if (index == 5) {
        return index_low(use_iy);
    }
    bool memory = false;
    return read_reg_index(index, memory);  // B,C,D,E,A (never (HL) here)
}

void Z80aCpu::write_reg_indexed_half(const int index, const bool use_iy, const std::uint8_t value) {
    if (index == 4) {
        set_index_high(use_iy, value);
        return;
    }
    if (index == 5) {
        set_index_low(use_iy, value);
        return;
    }
    write_reg_index(index, value);
}

std::uint16_t Z80aCpu::read_rp_indexed(const int p, const bool use_iy) const {
    if (p == 2) {
        return get_index(use_iy);  // rp[2] becomes IX/IY under the prefix
    }
    return read_rp(p);  // BC, DE, SP
}

std::uint32_t Z80aCpu::indexed_inc_dec(const int y, const bool use_iy, const bool is_dec) {
    if (y == 6) {  // INC/DEC (IX+d)
        const std::int8_t d = static_cast<std::int8_t>(read_imm8());
        const std::uint16_t address = index_displaced_address(use_iy, d);
        state_.regs().wz = address;  // WZ = index+d (§4; openMSX:2800)
        const std::uint8_t value = bus_.read_data(address);
        const std::uint8_t result = is_dec ? alu_dec8(value) : alu_inc8(value);
        bus_.write_data(address, result);
        return 19;
    }
    // y == 4 -> IXH, y == 5 -> IXL
    const bool high = (y == 4);
    const std::uint8_t value = high ? index_high(use_iy) : index_low(use_iy);
    const std::uint8_t result = is_dec ? alu_dec8(value) : alu_inc8(value);
    if (high) {
        set_index_high(use_iy, result);
    } else {
        set_index_low(use_iy, result);
    }
    return 4;
}

std::uint32_t Z80aCpu::execute_indexed(const bool use_iy) {
    const std::uint8_t opcode = fetch_opcode();

    // Prefix chaining / fallthrough:
    //  - DD/FD after a DD/FD: the earlier prefix is discarded (4T, one R tick)
    //    and the newest DD/FD re-latches the effective index register.
    //  - ED after DD/FD: the index prefix is ignored; the ED instruction runs
    //    unmodified (its returned cost already includes ED's own M1).
    if (opcode == 0xDD) {
        return 4 + execute_indexed(false);
    }
    if (opcode == 0xFD) {
        return 4 + execute_indexed(true);
    }
    if (opcode == 0xED) {
        return execute_ed_prefixed();
    }
    return execute_indexed_opcode(opcode, use_iy);
}

std::uint32_t Z80aCpu::execute_indexed_opcode(const std::uint8_t opcode, const bool use_iy) {
    const int x = opcode >> 6;
    const int y = (opcode >> 3) & 0x07;
    const int z = opcode & 0x07;
    const int p = y >> 1;
    const int q = y & 0x01;

    switch (x) {
    case 0:
        switch (z) {
        case 1:
            if (q == 0) {
                if (p == 2) {  // LD IX/IY,nn
                    set_index(use_iy, read_imm16());
                    return 10;
                }
                return execute_unprefixed(opcode);  // LD BC/DE/SP,nn (prefix ignored)
            }
            // ADD IX/IY,rp[p] (rp[2] is the index register itself). WZ = index+1
            // taken before the add (§4; openMSX:3247 add HL analog for IX).
            state_.regs().wz = static_cast<std::uint16_t>(get_index(use_iy) + 1);
            set_index(use_iy, alu_add16(get_index(use_iy), read_rp_indexed(p, use_iy)));
            return 11;
        case 2:
            if (p == 2) {
                if (q == 0) {  // LD (nn),IX/IY
                    bus_.write_word_le(read_imm16(), get_index(use_iy));
                    return 16;
                }
                // LD IX/IY,(nn)
                set_index(use_iy, bus_.read_word_le(read_imm16()));
                return 16;
            }
            return execute_unprefixed(opcode);  // LD (BC/DE/nn),A etc (prefix ignored)
        case 3:
            if (p == 2) {  // INC/DEC IX/IY
                if (q == 0) {
                    set_index(use_iy, static_cast<std::uint16_t>(get_index(use_iy) + 1));
                } else {
                    set_index(use_iy, static_cast<std::uint16_t>(get_index(use_iy) - 1));
                }
                return 6;
            }
            return execute_unprefixed(opcode);  // INC/DEC BC/DE/SP (prefix ignored)
        case 4:  // INC r[y]
            if (y == 6 || y == 4 || y == 5) {
                return indexed_inc_dec(y, use_iy, /*is_dec=*/false);
            }
            return execute_unprefixed(opcode);
        case 5:  // DEC r[y]
            if (y == 6 || y == 4 || y == 5) {
                return indexed_inc_dec(y, use_iy, /*is_dec=*/true);
            }
            return execute_unprefixed(opcode);
        case 6:  // LD r[y],n
            if (y == 6) {  // LD (IX+d),n  -- displacement precedes the immediate.
                const std::int8_t d = static_cast<std::int8_t>(read_imm8());
                const std::uint8_t n = read_imm8();
                const std::uint16_t address = index_displaced_address(use_iy, d);
                state_.regs().wz = address;  // WZ = index+d (§4; openMSX:2744)
                bus_.write_data(address, n);
                return 15;
            }
            if (y == 4 || y == 5) {  // LD IXH/IXL,n
                const std::uint8_t n = read_imm8();
                write_reg_indexed_half(y, use_iy, n);
                return 7;
            }
            return execute_unprefixed(opcode);
        default:  // z == 0 (NOP/EX AF/DJNZ/JR) and z == 7 (rotates/DAA/...) unaffected
            return execute_unprefixed(opcode);
        }
    case 1: {  // LD r[y],r[z]
        if (y == 6 && z == 6) {  // DD 76 executes as HALT (both operands memory).
            state_.set_halted(true);
            return 4;
        }
        if (y == 6) {  // LD (IX+d),r[z] -- z uses REAL registers (H/L not IXH/IXL).
            const std::int8_t d = static_cast<std::int8_t>(read_imm8());
            const std::uint16_t address = index_displaced_address(use_iy, d);
            state_.regs().wz = address;  // WZ = index+d (§4; openMSX:2726)
            bool memory = false;
            const std::uint8_t value = read_reg_index(z, memory);
            bus_.write_data(address, value);
            return 15;
        }
        if (z == 6) {  // LD r[y],(IX+d) -- y uses REAL registers.
            const std::int8_t d = static_cast<std::int8_t>(read_imm8());
            const std::uint16_t address = index_displaced_address(use_iy, d);
            state_.regs().wz = address;  // WZ = index+d (§4; openMSX:2800)
            const std::uint8_t value = bus_.read_data(address);
            write_reg_index(y, value);
            return 15;
        }
        // Register-to-register: H/L translate to IXH/IXL for both operands.
        write_reg_indexed_half(y, use_iy, read_reg_indexed_half(z, use_iy));
        return 4;
    }
    case 2: {  // alu[y] A,r[z]
        if (z == 6) {  // alu A,(IX+d)
            const std::int8_t d = static_cast<std::int8_t>(read_imm8());
            const std::uint16_t address = index_displaced_address(use_iy, d);
            state_.regs().wz = address;  // WZ = index+d (§4; openMSX:2800)
            const std::uint8_t value = bus_.read_data(address);
            alu_apply(y, value);
            return 15;
        }
        alu_apply(y, read_reg_indexed_half(z, use_iy));
        return 4;
    }
    default:  // x == 3
        switch (z) {
        case 1:
            if (q == 0 && p == 2) {  // POP IX/IY
                set_index(use_iy, pop16());
                return 10;
            }
            if (q == 1 && p == 2) {  // JP (IX/IY)
                state_.regs().pc = get_index(use_iy);
                return 4;
            }
            if (q == 1 && p == 3) {  // LD SP,IX/IY
                state_.regs().sp = get_index(use_iy);
                return 6;
            }
            return execute_unprefixed(opcode);  // RET / EXX (prefix ignored)
        case 3:
            if (y == 4) {  // EX (SP),IX/IY
                const std::uint8_t lo = bus_.read_data(state_.regs().sp);
                const std::uint8_t hi = bus_.read_data(static_cast<std::uint16_t>(state_.regs().sp + 1));
                const std::uint16_t stacked =
                    static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
                const std::uint16_t index = get_index(use_iy);
                bus_.write_data(state_.regs().sp, static_cast<std::uint8_t>(index & 0xFF));
                bus_.write_data(static_cast<std::uint16_t>(state_.regs().sp + 1),
                                static_cast<std::uint8_t>((index >> 8) & 0xFF));
                set_index(use_iy, stacked);
                state_.regs().wz = stacked;  // WZ = value exchanged from stack (openMSX:3999)
                return 19;
            }
            if (y == 1) {  // DDCB / FDCB prefix
                return execute_indexed_cb(use_iy);
            }
            return execute_unprefixed(opcode);  // JP nn / IN / OUT / EX DE,HL / DI / EI
        case 5:
            if (q == 0 && p == 2) {  // PUSH IX/IY
                push16(get_index(use_iy));
                return 11;
            }
            return execute_unprefixed(opcode);  // PUSH BC/DE/AF, CALL nn (prefix ignored)
        default:  // z == 0/2/4/6/7: RET cc, JP cc, CALL cc, alu n, RST -- unaffected
            return execute_unprefixed(opcode);
        }
    }
}

std::uint32_t Z80aCpu::execute_indexed_cb(const bool use_iy) {
    // Byte order: DD/FD CB <displacement> <sub-opcode>. Neither the displacement
    // nor the sub-opcode is an M1 fetch, so R is not incremented for them.
    const std::int8_t d = static_cast<std::int8_t>(read_imm8());
    const std::uint16_t address = index_displaced_address(use_iy, d);
    // M12-S3 (gap #5/#35): WZ = index+d for every DDCB/FDCB access (§4;
    // openMSX bit_N_xix() setMemPtr, CPUCore.cc:3426). BIT n,(IX+d) then sources
    // X/Y from WZ hi == address hi, which this keeps consistent.
    state_.regs().wz = address;
    const std::uint8_t op = read_imm8();
    const int x = op >> 6;
    const int y = (op >> 3) & 0x07;
    const int z = op & 0x07;

    if (x == 1) {  // BIT y,(IX+d): X/Y undocumented flags come from WZ hi (= address hi).
        const std::uint8_t value = bus_.read_data(address);
        alu_bit(y, value, static_cast<std::uint8_t>(state_.regs().wz >> 8));
        return 16;  // 20T total including the 4T prefix.
    }

    const std::uint8_t value = bus_.read_data(address);
    std::uint8_t result = 0;
    if (x == 0) {  // rotate/shift rot[y]
        result = alu_shift_rotate(y, value);
    } else if (x == 2) {  // RES y
        result = static_cast<std::uint8_t>(value & ~(1u << y));
    } else {  // x == 3: SET y
        result = static_cast<std::uint8_t>(value | (1u << y));
    }
    bus_.write_data(address, result);
    if (z != 6) {
        // Undocumented: the result is also copied into the real register r[z]
        // (z==4/5 are the REAL H/L, not IXH/IXL).
        write_reg_index(z, result);
    }
    return 19;  // 23T total including the 4T prefix.
}

std::uint32_t Z80aCpu::service_nmi() {
    state_.clear_nmi();
    state_.set_halted(false);
    push16(state_.regs().pc);
    state_.set_iff1(false);
    state_.regs().pc = kNmiVector;
    return 11;
}

std::uint32_t Z80aCpu::service_maskable_interrupt() {
    // Snapshot the device-supplied acknowledge byte before clearing the request.
    const bool vector_supplied = state_.interrupt_vector_supplied();
    const std::uint8_t bus_vector = state_.interrupt_bus_vector();  // 0xFF when floating.
    state_.clear_maskable_interrupt();
    state_.set_halted(false);
    state_.set_iff1(false);
    state_.set_iff2(false);
    // The interrupt-acknowledge cycle is an M1 that ticks the refresh register.
    increment_refresh_register();

    switch (state_.interrupt_mode()) {
    case InterruptMode::Im0: {
        // IM0: the acknowledging device drives an instruction opcode onto the
        // data bus and the CPU executes it in place (PC is NOT advanced for the
        // fetch, since the byte comes from the device, not memory). With no
        // device vector, the bus floats to 0xFF, which decodes as RST 38h.
        // Two wait states are inserted into the acknowledge M1 (so a bus-supplied
        // RST costs 11 + 2 = 13T, matching the classic IM0-RST figure).
        const std::uint8_t opcode = vector_supplied ? bus_vector : 0xFF;
        return 2 + execute_unprefixed(opcode);
    }
    case InterruptMode::Im2: {
        // IM2: vector-table address = (I << 8) | bus_vector. Read the 16-bit ISR
        // address little-endian and jump there. When no device vector is driven,
        // the low byte floats to 0xFF.
        const std::uint16_t table_address =
            static_cast<std::uint16_t>((static_cast<std::uint16_t>(state_.regs().i) << 8) | bus_vector);
        const std::uint16_t vector = bus_.read_word_le(table_address);
        push16(state_.regs().pc);
        state_.regs().pc = vector;
        state_.regs().wz = vector;  // WZ = handler address (§4; openMSX irq2:816)
        return 19;
    }
    default:  // InterruptMode::Im1: fixed restart to 0x0038.
        push16(state_.regs().pc);
        state_.regs().pc = kIm1Vector;
        state_.regs().wz = kIm1Vector;  // WZ = handler address (§4; openMSX irq1:802)
        return 13;
    }
}

}  // namespace sony_msx::devices::cpu
