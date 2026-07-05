#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvSlotMap_Unit
//
// M11-S5: the cold_boot bring-up slot default (A-2 / risk R-1) makes all four
// CPU pages resolve to slot 3-0 DRAM, keeping the machine bootable without BIOS
// (which arrives in M12). Also pins the reset slot registers (#A8 = 0xFF, slot-3
// sub-slot = 0) and confirms the DRAM-direct debug accessors.
//
// R-1 NOTE: this default is an explicit M11 deviation from real hardware (where
// #A8 resets to 0 -> slot-0 BIOS). M12 MUST flip it to #A8 = 0 once ROMs
// populate slot 0. See docs/m11-planner-package.md §1.3 A-2 / §9 R-1.

namespace {

bool expect_true(const bool ok, const char* case_name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    Hbf1xvMachine machine;
    machine.cold_boot();

    // Sentinels in each 16 KB page (DRAM-direct via load_memory).
    const std::uint8_t p0[] = {0x11};
    const std::uint8_t p1[] = {0x22};
    const std::uint8_t p2[] = {0x33};
    const std::uint8_t p3[] = {0x44};
    machine.load_memory(0x0100, p0, 1);
    machine.load_memory(0x4100, p1, 1);
    machine.load_memory(0x8100, p2, 1);
    machine.load_memory(0xC100, p3, 1);

    // read_memory is a DRAM-direct debug alias.
    if (!expect_true(machine.read_memory(0x8100) == 0x33, "ReadMemory_IsDramDirect")) {
        return 1;
    }

    // Program: read each page's sentinel through the CPU/SystemBus into B,C,D,E,
    // then read #A8 (IN) and #FFFF (LD) to pin the reset slot registers, HALT.
    const std::array<std::uint8_t, 22> program{
        0x21, 0x00, 0x01,  // LD HL,0x0100  (page 0)
        0x46,              // LD B,(HL)
        0x21, 0x00, 0x41,  // LD HL,0x4100  (page 1)
        0x4E,              // LD C,(HL)
        0x21, 0x00, 0x81,  // LD HL,0x8100  (page 2)
        0x56,              // LD D,(HL)
        0x21, 0x00, 0xC1,  // LD HL,0xC100  (page 3)
        0x5E,              // LD E,(HL)
        0x3A, 0xFF, 0xFF,  // LD A,(0xFFFF) (expanded slot-3 sub-slot readback)
        0x76,              // HALT
    };
    machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));

    for (int i = 0; i < 16 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }

    if (!expect_true(machine.cpu().state().halted(), "SlotMapProgram_ReachesHalt")) {
        return 1;
    }

    // All four pages routed CPU reads to slot 3-0 DRAM.
    const auto& r = machine.cpu().state().regs();
    if (!expect_true(r.b() == 0x11 && r.c() == 0x22 && r.d() == 0x33 && r.e() == 0x44,
                     "AllFourPages_ResolveToSlot30Dram")) {
        return 1;
    }

    // #FFFF read in the expanded slot 3 returns 0xFF ^ subSlotReg = 0xFF ^ 0 = 0xFF
    // (NOT DRAM content), confirming secondary-slot decode is active on page 3.
    if (!expect_true(r.a() == 0xFF, "Ffff_ExpandedSlot3_ReadbackIsOnesComplementOfZeroReg")) {
        return 1;
    }

    // Pin the reset primary-select register via IN A,(#A8).
    {
        Hbf1xvMachine m2;
        m2.cold_boot();
        const std::array<std::uint8_t, 2> read_a8{0xDB, 0xA8};  // IN A,(0xA8)
        m2.load_memory(0x0000, read_a8.data(), static_cast<std::uint32_t>(read_a8.size()));
        m2.step_cpu_instruction();
        if (!expect_true(m2.cpu().state().regs().a() == 0xFF,
                         "ResetPrimarySelect_A8_IsFF_M11BringUpDefault")) {
            return 1;
        }
    }

    // A CPU write routes to DRAM too: LD A,0x9A ; LD (0x8200),A ; HALT.
    {
        Hbf1xvMachine m3;
        m3.cold_boot();
        const std::array<std::uint8_t, 7> wprog{0x3E, 0x9A, 0x32, 0x00, 0x82, 0x76, 0x00};
        m3.load_memory(0x0000, wprog.data(), static_cast<std::uint32_t>(wprog.size()));
        for (int i = 0; i < 8 && !m3.cpu().state().halted(); ++i) {
            m3.step_cpu_instruction();
        }
        if (!expect_true(m3.read_memory(0x8200) == 0x9A, "CpuWrite_RoutesToSlot30Dram")) {
            return 1;
        }
    }

    return 0;
}
