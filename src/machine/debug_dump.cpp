#include "machine/debug_dump.h"

#include "machine/debug_format.h"

namespace sony_msx::machine::debug_dump {

namespace {

using debug_format::flag_string;
using debug_format::to_dec;
using debug_format::to_hex;

}  // namespace

std::string serialize_cpu(const devices::cpu::Z80aState& state) {
    const devices::cpu::Z80aRegisters& r = state.regs();

    std::string out;
    out += "[CPU]\n";
    out += "PC=" + to_hex(r.pc, 4) + " SP=" + to_hex(r.sp, 4) + "\n";
    out += "AF=" + to_hex(r.af, 4) + " BC=" + to_hex(r.bc, 4) + " DE=" + to_hex(r.de, 4) +
           " HL=" + to_hex(r.hl, 4) + "\n";
    out += "AF'=" + to_hex(r.af_shadow, 4) + " BC'=" + to_hex(r.bc_shadow, 4) +
           " DE'=" + to_hex(r.de_shadow, 4) + " HL'=" + to_hex(r.hl_shadow, 4) + "\n";
    out += "IX=" + to_hex(r.ix, 4) + " IY=" + to_hex(r.iy, 4) + "\n";
    out += "A=" + to_hex(r.a(), 2) + " F=" + to_hex(r.f(), 2) + "[" + flag_string(r.f()) + "]" +
           " B=" + to_hex(r.b(), 2) + " C=" + to_hex(r.c(), 2) + " D=" + to_hex(r.d(), 2) +
           " E=" + to_hex(r.e(), 2) + " H=" + to_hex(r.h(), 2) + " L=" + to_hex(r.l(), 2) + "\n";
    out += "I=" + to_hex(r.i, 2) + " R=" + to_hex(r.r, 2) +
           " IFF1=" + std::string(state.iff1() ? "1" : "0") +
           " IFF2=" + std::string(state.iff2() ? "1" : "0") +
           " IM=" + to_dec(static_cast<std::uint64_t>(static_cast<std::uint8_t>(state.interrupt_mode()))) +
           " HALT=" + std::string(state.halted() ? "1" : "0") + "\n";
    out += "TSTATES=" + to_dec(state.total_tstates()) + "\n";
    return out;
}

std::string serialize_region(const std::string& name, const std::uint8_t* data, const std::size_t size) {
    std::string out;
    out += "[" + name + "] size=" + to_dec(size) + "\n";

    if (size == 0) {
        return out;
    }

    const std::size_t line_count = (size + 15) / 16;

    auto emit_line = [&out, data, size](const std::size_t line_index) {
        const std::size_t offset = line_index * 16;
        out += to_hex(offset, 8);
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t index = offset + i;
            out.push_back(' ');
            if (index < size) {
                out += to_hex(data[index], 2);
            } else {
                out += "  ";  // Pad past the tail so column alignment is fixed.
            }
        }
        out.push_back('\n');
    };

    auto line_equals_previous = [data](const std::size_t line_index) {
        const std::size_t offset = line_index * 16;
        const std::size_t prev = offset - 16;
        for (std::size_t i = 0; i < 16; ++i) {
            if (data[prev + i] != data[offset + i]) {
                return false;
            }
        }
        return true;
    };

    bool star_active = false;
    for (std::size_t line_index = 0; line_index < line_count; ++line_index) {
        const bool is_first = (line_index == 0);
        const bool is_last = (line_index + 1 == line_count);

        // Interior line identical to its predecessor: fold into a single '*'.
        // Only full 16-byte lines are candidates for folding (the last line may
        // be short); first and last lines are always emitted verbatim.
        if (!is_first && !is_last && line_equals_previous(line_index)) {
            if (!star_active) {
                out += "*\n";
                star_active = true;
            }
            continue;
        }

        emit_line(line_index);
        star_active = false;
    }

    return out;
}

}  // namespace sony_msx::machine::debug_dump
