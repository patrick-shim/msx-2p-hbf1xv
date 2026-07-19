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

#include "machine/cpu_trace_sink.h"

#include <cstdint>
#include <cstdlib>

#include "machine/debug_format.h"

namespace sony_msx::machine {

namespace {

using debug_format::flag_string;
using debug_format::to_dec;
using debug_format::to_hex;

}  // namespace

void CpuTraceSink::on_instruction_retired(const devices::cpu::Z80aTraceRecord& record) {
    records_.push_back(record);
    // Diagnostic trace-ring cap (env-gated; default 0 => unbounded original
    // behavior). SONY_MSX_TRACE_RING=<N> retains only the last ~N records
    // (drop-oldest in amortized-O(1) chunks) so a very long run's crash TAIL is
    // capturable without an unbounded trace. The global .sequence stays absolute
    // across drops. (DEC-0072)
    static const std::size_t ring = []() -> std::size_t {
        const char* p = std::getenv("SONY_MSX_TRACE_RING");
        return (p != nullptr && *p != '\0') ? std::strtoul(p, nullptr, 10) : 0U;
    }();
    if (ring > 0 && records_.size() >= 2U * ring) {
        records_.erase(records_.begin(),
                       records_.begin() + static_cast<std::ptrdiff_t>(ring));
    }
}

void CpuTraceSink::clear() {
    records_.clear();
}

std::size_t CpuTraceSink::size() const {
    return records_.size();
}

bool CpuTraceSink::empty() const {
    return records_.empty();
}

const std::vector<devices::cpu::Z80aTraceRecord>& CpuTraceSink::records() const {
    return records_;
}

const devices::cpu::Z80aTraceRecord& CpuTraceSink::at(const std::size_t index) const {
    return records_.at(index);
}

std::string CpuTraceSink::format_record(const devices::cpu::Z80aTraceRecord& r) {
    std::string opcode;
    if (r.opcode_length == 0) {
        opcode = "--";
    } else {
        for (std::uint8_t i = 0; i < r.opcode_length; ++i) {
            opcode += to_hex(r.opcode_bytes[i], 2);
        }
    }

    std::string line;
    line += "SEQ=" + to_hex(r.sequence, 4);
    line += " PC=" + to_hex(r.pc, 4);
    line += " OP=" + opcode;
    line += " A=" + to_hex(r.a, 2);
    line += " F=" + to_hex(r.f, 2) + "[" + flag_string(r.f) + "]";
    line += " B=" + to_hex(r.b, 2);
    line += " C=" + to_hex(r.c, 2);
    line += " D=" + to_hex(r.d, 2);
    line += " E=" + to_hex(r.e, 2);
    line += " H=" + to_hex(r.h, 2);
    line += " L=" + to_hex(r.l, 2);
    line += " AF=" + to_hex(r.af, 4);
    line += " BC=" + to_hex(r.bc, 4);
    line += " DE=" + to_hex(r.de, 4);
    line += " HL=" + to_hex(r.hl, 4);
    line += " AF'=" + to_hex(r.af_shadow, 4);
    line += " BC'=" + to_hex(r.bc_shadow, 4);
    line += " DE'=" + to_hex(r.de_shadow, 4);
    line += " HL'=" + to_hex(r.hl_shadow, 4);
    line += " IX=" + to_hex(r.ix, 4);
    line += " IY=" + to_hex(r.iy, 4);
    line += " SP=" + to_hex(r.sp, 4);
    line += " I=" + to_hex(r.i, 2);
    line += " R=" + to_hex(r.r, 2);
    line += " IFF1=" + std::string(r.iff1 ? "1" : "0");
    line += " IFF2=" + std::string(r.iff2 ? "1" : "0");
    line += " IM=" + to_dec(static_cast<std::uint64_t>(static_cast<std::uint8_t>(r.im)));
    line += " T=" + to_dec(r.instr_tstates);
    line += " TC=" + to_dec(r.cumulative_tstates);
    return line;
}

std::string CpuTraceSink::serialize() const {
    std::string out;
    for (const devices::cpu::Z80aTraceRecord& r : records_) {
        out += format_record(r);
        out.push_back('\n');
    }
    return out;
}

}  // namespace sony_msx::machine
