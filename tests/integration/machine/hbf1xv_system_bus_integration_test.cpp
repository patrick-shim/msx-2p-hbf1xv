#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvSystemBus_Integration
//
// M11-S5: drives the S1985 system bus end-to-end through the CPU. A single
// program exercises: primary-slot #A8 write/readback, the engine-detection
// mirror #AC == #A8 (fact-sheet §10), the mapper `100xxxxx` readback on #FC
// (§4), the switched-I/O ~ID discriminator on #40 (§6), and the expanded slot-3
// #FFFF sub-slot readback 0xFF^reg (§3, A-3). Results are stored to page-0 DRAM
// and asserted via the DRAM-direct debug accessor, plus a determinism oracle.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// #A8 = 0xC7: page0 -> slot 3 (bits1-0 = 11) so the program in page 0 stays in
// DRAM; other pages vary. Chosen so slot changes never unmap the running code.
constexpr std::uint8_t kSlotByte = 0xC7;

const std::array<std::uint8_t, 44> kProgram{
    0x3E, 0xC7,        // 0x0000 LD A,0xC7
    0xD3, 0xA8,        // 0x0002 OUT (0xA8),A     ; primary slot select
    0xDB, 0xA8,        // 0x0004 IN A,(0xA8)      ; readback -> 0xC7
    0x32, 0x00, 0x02,  // 0x0006 LD (0x0200),A
    0xDB, 0xAC,        // 0x0009 IN A,(0xAC)      ; mirror -> 0xC7
    0x32, 0x01, 0x02,  // 0x000B LD (0x0201),A
    0x3E, 0x25,        // 0x000E LD A,0x25
    0xD3, 0xFC,        // 0x0010 OUT (0xFC),A     ; mapper seg page0 = 0x25
    0xDB, 0xFC,        // 0x0012 IN A,(0xFC)      ; readback -> 0x80|(0x25&0x1F)=0x85
    0x32, 0x02, 0x02,  // 0x0014 LD (0x0202),A
    0x3E, 0xFE,        // 0x0017 LD A,0xFE
    0xD3, 0x40,        // 0x0019 OUT (0x40),A     ; select switched device 0xFE
    0xDB, 0x40,        // 0x001B IN A,(0x40)      ; ~ID -> 0x01
    0x32, 0x03, 0x02,  // 0x001D LD (0x0203),A
    0x3E, 0xC0,        // 0x0020 LD A,0xC0
    0x32, 0xFF, 0xFF,  // 0x0022 LD (0xFFFF),A    ; slot-3 sub-slot reg (page0 sub stays 0)
    0x3A, 0xFF, 0xFF,  // 0x0025 LD A,(0xFFFF)    ; readback -> 0xFF^0xC0 = 0x3F
    0x32, 0x04, 0x02,  // 0x0028 LD (0x0204),A
    0x76,              // 0x002B HALT
};

std::uint64_t run(std::array<std::uint8_t, 5>& results) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 64 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
    for (int i = 0; i < 5; ++i) {
        results[static_cast<std::size_t>(i)] =
            machine.read_memory(static_cast<std::uint16_t>(0x0200 + i));
    }
    return machine.elapsed_cycles();
}

}  // namespace

int main() {
    static_cast<void>(kSlotByte);

    std::array<std::uint8_t, 5> res{};
    const std::uint64_t cycles = run(res);

    if (!expect_true(res[0] == 0xC7, "PrimarySelect_A8_Readback")) {
        return 1;
    }
    if (!expect_true(res[1] == res[0], "EngineDetection_AcMirror_EqualsA8")) {
        return 1;
    }
    if (!expect_true(res[2] == 0x85, "MapperReadback_Fc_Is100xxxxxPattern")) {
        return 1;
    }
    if (!expect_true(res[3] == 0x01, "SwitchedIo_Id0xFE_ReturnsComplementOfId")) {
        return 1;
    }
    if (!expect_true(res[4] == 0x3F, "Ffff_ExpandedSlot3_Readback_IsOnesComplement")) {
        return 1;
    }

    // Determinism oracle: an identical run yields identical results AND identical
    // machine cycle total (datasheet + M1 waits).
    std::array<std::uint8_t, 5> res2{};
    const std::uint64_t cycles2 = run(res2);
    if (!expect_true(res == res2 && cycles == cycles2,
                     "TwoRuns_SameProgram_IdenticalResultsAndCycles")) {
        return 1;
    }

    return 0;
}
