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

#include <cstddef>
#include <string>
#include <vector>

#include "devices/cpu/z80a_trace.h"

namespace sony_msx::machine {

// Machine-layer collector/serializer for CPU per-instruction trace records
// (M10-S1). Implements the CPU-side observer interface, retains records in a
// stable in-memory vector, and produces a deterministic, diffable text form.
//
// This record store and serialization are the reusable substrate for the later
// M10 slices (S3 full-state debug dump, S4 openMSX parity harness). The output
// is ASCII-only, fixed field order, fixed hex width, and '\n'-terminated so it
// is byte-stable across runs and environments.
class CpuTraceSink final : public devices::cpu::Z80aTraceObserver {
public:
    void on_instruction_retired(const devices::cpu::Z80aTraceRecord& record) override;

    void clear();
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const std::vector<devices::cpu::Z80aTraceRecord>& records() const;
    [[nodiscard]] const devices::cpu::Z80aTraceRecord& at(std::size_t index) const;

    // Deterministic single-line serialization of one record.
    [[nodiscard]] static std::string format_record(const devices::cpu::Z80aTraceRecord& record);

    // Deterministic multi-line serialization of all collected records; one
    // line per record, each '\n'-terminated.
    [[nodiscard]] std::string serialize() const;

private:
    std::vector<devices::cpu::Z80aTraceRecord> records_;
};

}  // namespace sony_msx::machine
