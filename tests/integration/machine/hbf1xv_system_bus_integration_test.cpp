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

#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvSystemBus_Integration
//
// M11-S5 (reconciled for M13-S4): drives the S1985 system bus end-to-end through
// the CPU. A single program exercises: primary-slot #A8 write/readback, the
// engine-detection mirror #AC == #A8 (fact-sheet §10), the mapper `100xxxxx`
// readback (§4), the switched-I/O ~ID discriminator on #40 (§6), and the
// expanded slot-3 #FFFF sub-slot readback 0xFF^reg (§3, A-3).
//
// M13-S4 reconciliation (non-weakening): M11 stored each readback to page-0 DRAM
// and read it back via the linear debug accessor. With the M13 REAL 64 KB mapper
// + populated slots, writing a mapper segment or the sub-slot register can remap
// the very page a store would target, so the flat-RAM store/read-back is no
// longer valid. The results are therefore captured in CPU REGISTERS (B/C/D/E/H)
// and asserted at HALT — the same discipline as the M11/M12 parity checkpoints —
// which verifies the identical I/O behaviours robustly. To keep the running code
// stable, the program executes from page 0 (kept in slot 3-0 RAM, sub 0, via
// #A8=0xC7) and reprograms page 1's mapper segment (#FD) rather than its own.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// #A8 = 0xC7: page0 -> slot 3 (bits1-0 = 11) so the running code stays in slot
// 3-0 RAM (sub 0) throughout; other pages vary.
constexpr std::uint8_t kSlotByte = 0xC7;

const std::array<std::uint8_t, 34> kProgram{
    0x3E, 0xC7,        // 0x0000 LD A,0xC7
    0xD3, 0xA8,        // 0x0002 OUT (0xA8),A     ; primary slot select
    0xDB, 0xA8,        // 0x0004 IN A,(0xA8)      ; readback -> 0xC7
    0x47,              // 0x0006 LD B,A
    0xDB, 0xAC,        // 0x0007 IN A,(0xAC)      ; engine mirror -> 0xC7
    0x4F,              // 0x0009 LD C,A
    0x3E, 0x25,        // 0x000A LD A,0x25
    0xD3, 0xFD,        // 0x000C OUT (0xFD),A     ; mapper seg PAGE 1 = 0x25 (not the code page)
    0xDB, 0xFD,        // 0x000E IN A,(0xFD)      ; readback -> 0x80|(0x25&0x1F)=0x85
    0x57,              // 0x0010 LD D,A
    0x3E, 0xFE,        // 0x0011 LD A,0xFE
    0xD3, 0x40,        // 0x0013 OUT (0x40),A     ; select switched device 0xFE
    0xDB, 0x40,        // 0x0015 IN A,(0x40)      ; ~ID -> 0x01
    0x5F,              // 0x0017 LD E,A
    0x3E, 0xC0,        // 0x0018 LD A,0xC0
    0x32, 0xFF, 0xFF,  // 0x001A LD (0xFFFF),A    ; slot-3 sub-slot reg (page0 sub stays 0)
    0x3A, 0xFF, 0xFF,  // 0x001D LD A,(0xFFFF)    ; readback -> 0xFF^0xC0 = 0x3F
    0x67,              // 0x0020 LD H,A
    0x76,              // 0x0021 HALT
};

struct Results {
    std::uint8_t a8_readback;
    std::uint8_t ac_mirror;
    std::uint8_t mapper_readback;
    std::uint8_t switched_id;
    std::uint8_t ffff_readback;
};

std::uint64_t run(Results& results) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    // Authentic reset boots slot-0 BIOS (#A8=0). Page flat 64 KB RAM in so the
    // program (loaded in RAM) executes; it then drives #A8 itself (M13-S4).
    machine.map_flat_ram();
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 64 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
    const auto& r = machine.cpu().state().regs();
    results = {r.b(), r.c(), r.d(), r.e(), r.h()};
    return machine.elapsed_cycles();
}

}  // namespace

int main() {
    static_cast<void>(kSlotByte);

    Results res{};
    const std::uint64_t cycles = run(res);

    if (!expect_true(res.a8_readback == 0xC7, "PrimarySelect_A8_Readback")) {
        return 1;
    }
    if (!expect_true(res.ac_mirror == res.a8_readback, "EngineDetection_AcMirror_EqualsA8")) {
        return 1;
    }
    if (!expect_true(res.mapper_readback == 0x85, "MapperReadback_Fd_Is100xxxxxPattern")) {
        return 1;
    }
    if (!expect_true(res.switched_id == 0x01, "SwitchedIo_Id0xFE_ReturnsComplementOfId")) {
        return 1;
    }
    if (!expect_true(res.ffff_readback == 0x3F, "Ffff_ExpandedSlot3_Readback_IsOnesComplement")) {
        return 1;
    }

    // Determinism oracle: an identical run yields identical results AND identical
    // machine cycle total (datasheet + M1 waits).
    Results res2{};
    const std::uint64_t cycles2 = run(res2);
    const bool same = res.a8_readback == res2.a8_readback && res.ac_mirror == res2.ac_mirror &&
                      res.mapper_readback == res2.mapper_readback &&
                      res.switched_id == res2.switched_id &&
                      res.ffff_readback == res2.ffff_readback && cycles == cycles2;
    if (!expect_true(same, "TwoRuns_SameProgram_IdenticalResultsAndCycles")) {
        return 1;
    }

    return 0;
}
