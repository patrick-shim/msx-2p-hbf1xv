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

// Suite: Machine_Hbf1xvM19Cartridge_Integration (M19-S4, backlog B7)
//
// Machine-level wiring of the two external cartridge bays: cartridge_slot1_/
// cartridge_slot2_ are attached at primary slots 1/2, all 4 CPU pages each,
// with NO SlotBus::set_expanded call for either (A-M19-1 -- confirmed via
// direct XML read, Sony_HB-F1XV.xml:119,121: both are bare, unexpanded
// `<primary external="true">` elements).
//
// ALL fixtures are SYNTHETIC, hand-authored marker images (bank N's every
// byte == N) -- never the scroll-shooter smoke ROM (that file is a SEPARATE,
// dedicated mechanical-smoke-only fixture,
// hbf1xv_m19_scroll_shooter_smoke_integration_test.cpp).
//
// Cases:
//   1. A real CPU program running over the full M11 bus (SlotBus/IoBus)
//      issues `OUT (#A8),A` to repoint CPU page 1 (0x4000-0x7FFF) at primary
//      slot 1 (pages 0/2/3 stay at primary slot 3 RAM, holding the running
//      program/stack), then performs each of the 6 MVP mappers' documented
//      bank-switch write sequence and reads back the expected marker bytes --
//      repeated independently for primary slot 2.
//   2. Independence: two DIFFERENT cartridges loaded into slot 1 and slot 2
//      simultaneously read back their OWN distinct content, never
//      cross-contaminated.
//   3. An unloaded/never-loaded slot is byte-for-byte identical to the
//      M13-M18 open-bus default (regression guard) with the M19 code present
//      but unused.
//   4. cold_boot() reinitializes bank state but NEVER unloads a cartridge
//      (A-M19-9).
//   5. machine.slot_expanded(1) == false and slot_expanded(2) == false
//      (R-M19-8 regression guard).
//
// (Zero-regression across the full M1-M18 suite, in particular the M9/M12
// CPU-timing oracles, is confirmed separately by the full `ctest` run
// captured in the M19 implementation report -- no new clock consumer is
// introduced by any cartridge device, planner §2.5.)
//
// NOTE on the #A8 byte values used below (0xF7 for slot 1, 0xFB for slot 2):
// derived directly from this project's OWN established, already-tested
// SlotBus bit layout (src/devices/chipset/slot_bus.cpp:47-49,
// `primary_for_page(page) = (primary_select_ >> (2*page)) & 0x03`, i.e. 2
// bits per page, page N at bits [2N, 2N+1]) -- NOT the task brief's own
// illustrative "#A8 = 0x04" arithmetic example, which would leave pages
// 0/2/3 at primary slot 0 (BIOS ROM) rather than primary slot 3 (RAM), and so
// cannot hold a running CPU program/stack. To route ONLY page 1 to primary
// slot N while pages 0/2/3 stay at primary slot 3 (matching
// map_flat_ram()'s all-slot-3 default), the byte is:
//   page0=3, page1=N, page2=3, page3=3
//   value = page0 | (page1<<2) | (page2<<4) | (page3<<6)
//   slot 1: 3 | (1<<2) | (3<<4) | (3<<6) = 0xF7
//   slot 2: 3 | (2<<2) | (3<<4) | (3<<6) = 0xFB
// (See the M19 implementation report for the full derivation; this does not
// change scope, only corrects an illustrative arithmetic slip against this
// project's own already-grounded SlotBus behaviour.)

#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::cartridge::CartridgeMapperType;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Tiny self-contained Z80 assembler helper (mirrors the M16 FDC integration
// test's own `Prog` helper).
class Prog {
public:
    void emit(std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            bytes_.push_back(b);
        }
    }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

constexpr std::uint16_t kBase = 0xC000;      // page3 RAM (map_flat_ram default)
constexpr std::uint16_t kDestBase = 0xC400;  // landing zone for read-back bytes, well past the program

// #A8 values routing ONLY page1 to primary slot N (1 or 2), pages 0/2/3
// staying at primary slot 3 RAM (see file header derivation).
constexpr std::uint8_t kA8Slot1Page1 = 0xF7;
constexpr std::uint8_t kA8Slot2Page1 = 0xFB;

// Builds: OUT (#A8),a8_value ; for each (addr,value) in writes: LD A,value /
// LD (addr),A ; for each addr in reads: LD A,(addr) / LD (kDestBase+i),A ; HALT.
std::vector<std::uint8_t> build_probe(const std::uint8_t a8_value,
                                       const std::vector<std::pair<std::uint16_t, std::uint8_t>>& writes,
                                       const std::vector<std::uint16_t>& reads) {
    Prog p;
    p.emit({0x3E, a8_value});  // LD A, a8_value
    p.emit({0xD3, 0xA8});      // OUT (#A8), A
    for (const auto& w : writes) {
        p.emit({0x3E, w.second});
        p.emit({0x32, static_cast<std::uint8_t>(w.first & 0xFF), static_cast<std::uint8_t>(w.first >> 8)});
    }
    for (std::size_t i = 0; i < reads.size(); ++i) {
        p.emit({0x3A, static_cast<std::uint8_t>(reads[i] & 0xFF), static_cast<std::uint8_t>(reads[i] >> 8)});
        const auto dest = static_cast<std::uint16_t>(kDestBase + i);
        p.emit({0x32, static_cast<std::uint8_t>(dest & 0xFF), static_cast<std::uint8_t>(dest >> 8)});
    }
    p.emit({0x76});  // HALT
    return p.bytes();
}

std::vector<std::uint8_t> make_marker_image(const unsigned num_banks, const std::uint32_t bank_size) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_banks) * bank_size);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        const auto marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < bank_size; ++i) {
            image[static_cast<std::size_t>(bank) * bank_size + i] = marker;
        }
    }
    return image;
}

// Runs `program` from kBase to HALT (bounded), returning the halted flag.
bool run_to_halt(sony_msx::machine::Hbf1xvMachine& machine, const std::vector<std::uint8_t>& program) {
    machine.load_memory(kBase, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = kBase;
    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 200'000) {
        machine.step_cpu_instruction();
        ++guard;
    }
    return machine.cpu().state().halted();
}

// Runs the 6-mapper-type CPU-driven bank-switch matrix against `slot_number`
// (1 or 2), using `a8_value` to route page1 there. Every case is entirely
// reachable within page1 (CPU 0x4000-0x7FFF == window-slots 2/3) since ONLY
// page1 is repointed away from RAM.
void run_all_mapper_types_probe(const int slot_number, const std::uint8_t a8_value,
                                 const char* case_prefix) {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Mirrored: read-only, full mirror, no write. nrBlocks=2 -> slot2
    //     (0x4000) reads bank0, slot3 (0x6000) reads bank1. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const auto load_ok = machine.load_cartridge(slot_number, CartridgeMapperType::Mirrored,
                                                     make_marker_image(2, 0x2000));
        expect(load_ok == sony_msx::devices::cartridge::CartridgeLoadResult::Ok,
               (std::string(case_prefix) + "_Mirrored_LoadOk").c_str());
        const auto program = build_probe(a8_value, {}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Mirrored_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 0, (std::string(case_prefix) + "_Mirrored_Slot2Bank0").c_str());
        expect(machine.read_memory(kDestBase + 1) == 1, (std::string(case_prefix) + "_Mirrored_Slot3Bank1").c_str());
    }

    // --- Generic8kB: reset default slot2=bank0/slot3=bank1; write bank 3 at
    //     0x4000 (slot=addr>>13=2) changes JUST slot2. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::Generic8kB, make_marker_image(4, 0x2000));
        const auto program = build_probe(a8_value, {{0x4000, 3}}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Generic8kB_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 3,
               (std::string(case_prefix) + "_Generic8kB_Slot2_BankSwitchedTo3").c_str());
        expect(machine.read_memory(kDestBase + 1) == 1,
               (std::string(case_prefix) + "_Generic8kB_Slot3_UnaffectedStillBank1").c_str());
    }

    // --- Ascii8kB: reset default slot2=slot3=bank0; write value=3 at 0x6000
    //     lands region=((0x6000>>11)&3)+2=2 -> changes window-slot2 (CPU
    //     0x4000), NOT slot3 -- a genuine cross-address quirk. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::Ascii8kB, make_marker_image(4, 0x2000));
        const auto program = build_probe(a8_value, {{0x6000, 3}}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Ascii8kB_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 3,
               (std::string(case_prefix) + "_Ascii8kB_WriteAt0x6000_ChangesSlot2At0x4000").c_str());
        expect(machine.read_memory(kDestBase + 1) == 0,
               (std::string(case_prefix) + "_Ascii8kB_Slot3_UnaffectedStillBank0").c_str());
    }

    // --- Ascii16kB: reset default slot2=bank0/slot3=bank1 (logical bank1);
    //     write value=2 at 0x6000 (region1) sets logical bank1 -> image
    //     banks 4,5. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::Ascii16kB, make_marker_image(8, 0x2000));
        const auto program = build_probe(a8_value, {{0x6000, 2}}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Ascii16kB_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 4,
               (std::string(case_prefix) + "_Ascii16kB_Slot2_LowHalfImageBank4").c_str());
        expect(machine.read_memory(kDestBase + 1) == 5,
               (std::string(case_prefix) + "_Ascii16kB_Slot3_HighHalfImageBank5").c_str());
    }

    // --- Generic16kB: reset default slot2=bank0/slot3=bank1 (logical bank1);
    //     write value=2 at 0x4000 (bank=addr>>14=1) sets logical bank1 ->
    //     image banks 4,5. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::Generic16kB, make_marker_image(8, 0x2000));
        const auto program = build_probe(a8_value, {{0x4000, 2}}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Generic16kB_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 4,
               (std::string(case_prefix) + "_Generic16kB_Slot2_LowHalfImageBank4").c_str());
        expect(machine.read_memory(kDestBase + 1) == 5,
               (std::string(case_prefix) + "_Generic16kB_Slot3_HighHalfImageBank5").c_str());
    }

    // --- Konami: reset default slot2=bank0 (PERMANENTLY FIXED)/slot3=bank1;
    //     write value=9 at 0x6000 (bank_switch(3,9)) changes slot3 only --
    //     slot2 never moves (R-M19-3, corrected finding per the unit test). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::Konami, make_marker_image(32, 0x2000));
        const auto program = build_probe(a8_value, {{0x6000, 9}}, {0x4000, 0x6000});
        expect(run_to_halt(machine, program), (std::string(case_prefix) + "_Konami_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 0,
               (std::string(case_prefix) + "_Konami_Slot2_PermanentlyFixedAtBank0").c_str());
        expect(machine.read_memory(kDestBase + 1) == 9,
               (std::string(case_prefix) + "_Konami_Slot3_BankSwitchedTo9").c_str());
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- 1. Real CPU-driven #A8 + bank-switch matrix, all 6 MVP types,
    //        primary slot 1. ---
    run_all_mapper_types_probe(1, kA8Slot1Page1, "Slot1");

    // --- Repeated independently for primary slot 2 (proving the two slots
    //     are independently addressable, planner §3 S4). ---
    run_all_mapper_types_probe(2, kA8Slot2Page1, "Slot2");

    // --- 2. Independence: two DIFFERENT cartridges loaded simultaneously
    //        read back their OWN distinct content, never cross-contaminated. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        // Slot 1: Generic8kB with markers 0..3; Slot 2: Generic8kB with a
        // DISTINCT, offset marker sequence (100..103) so cross-contamination
        // would be immediately visible.
        std::vector<std::uint8_t> image1 = make_marker_image(4, 0x2000);
        std::vector<std::uint8_t> image2(4 * 0x2000);
        for (unsigned bank = 0; bank < 4; ++bank) {
            const auto marker = static_cast<std::uint8_t>(100 + bank);
            for (std::uint32_t i = 0; i < 0x2000; ++i) {
                image2[static_cast<std::size_t>(bank) * 0x2000 + i] = marker;
            }
        }
        machine.load_cartridge(1, CartridgeMapperType::Generic8kB, image1);
        machine.load_cartridge(2, CartridgeMapperType::Generic8kB, image2);

        machine.debug_io_write(0xA8, kA8Slot1Page1);
        expect(machine.debug_bus_read(0x4000) == 0, "Independence_Slot1_Page1_ReadsOwnImage_Bank0");
        machine.debug_io_write(0xA8, kA8Slot2Page1);
        expect(machine.debug_bus_read(0x4000) == 100, "Independence_Slot2_Page1_ReadsOwnImage_Bank100");
        // Re-select slot 1 -- still its own content, unaffected by slot 2 access.
        machine.debug_io_write(0xA8, kA8Slot1Page1);
        expect(machine.debug_bus_read(0x4000) == 0, "Independence_Slot1_StillOwnContent_AfterSlot2Access");
    }

    // --- 3. Unloaded/never-loaded slot: byte-for-byte M13-M18 open-bus
    //        default, with the M19 code present but unused. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(!machine.cartridge_slot1().loaded(), "NeverLoaded_Slot1_NotLoaded");
        expect(!machine.cartridge_slot2().loaded(), "NeverLoaded_Slot2_NotLoaded");

        machine.debug_io_write(0xA8, kA8Slot1Page1);
        expect(machine.debug_bus_read(0x4000) == 0xFF, "NeverLoaded_Slot1_Page1_OpenBus");
        expect(machine.debug_bus_read(0x7FFF) == 0xFF, "NeverLoaded_Slot1_Page1TopAddress_OpenBus");
        machine.debug_bus_write(0x6000, 0x99);  // must be a no-op
        expect(machine.debug_bus_read(0x6000) == 0xFF, "NeverLoaded_Slot1_WriteIsNoOp_StillOpenBus");

        machine.debug_io_write(0xA8, kA8Slot2Page1);
        expect(machine.debug_bus_read(0x4000) == 0xFF, "NeverLoaded_Slot2_Page1_OpenBus");
    }

    // --- 4. cold_boot() reinitializes bank state but NEVER unloads (A-M19-9). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.load_cartridge(1, CartridgeMapperType::Generic8kB, make_marker_image(4, 0x2000));

        machine.debug_io_write(0xA8, kA8Slot1Page1);
        // Bank-switch away from the reset default (bank0 at slot2/0x4000).
        // Value 2 is directly in-range for this 4-bank image (no mask
        // fallback involved), so it reads back as marker byte 2 exactly.
        machine.debug_bus_write(0x4000, 2);
        expect(machine.debug_bus_read(0x4000) == 2, "ColdBootPreserves_Setup_BankSwitchedAway");

        machine.cold_boot();  // re-cold-boot; must NOT eject the cartridge
        expect(machine.cartridge_slot1().loaded(), "ColdBootPreserves_StillLoaded_NeverEjected");
        expect(machine.cartridge_slot1().mapper_type() == CartridgeMapperType::Generic8kB,
               "ColdBootPreserves_MapperTypeUnchanged");

        machine.debug_io_write(0xA8, kA8Slot1Page1);  // #A8 itself resets to 0 at cold_boot; re-route
        expect(machine.debug_bus_read(0x4000) == 0,
               "ColdBootPreserves_BankStateRevertsToResetDefault");
    }

    // --- 5. R-M19-8 regression guard: neither external primary slot is
    //        ever marked `expanded`. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(!machine.slot_expanded(1), "SlotExpanded_Primary1_IsFalse");
        expect(!machine.slot_expanded(2), "SlotExpanded_Primary2_IsFalse");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
