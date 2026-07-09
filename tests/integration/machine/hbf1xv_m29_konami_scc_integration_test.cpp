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

// Suite: Machine_Hbf1xvM29KonamiScc_Integration (M29-S4, backlog G1)
//
// Bus-level wiring of the KonamiSCC mapper + SCC wavetable chip: a real
// Hbf1xvMachine with a SYNTHETIC KonamiSCC image loaded via load_cartridge()
// (never roms/* -- A-M29-4: no real SCC-game ROM is known-present), driven by
// REAL Z80 instructions over the M11 bus (the M19 `LD (nn),A` probe shape):
//   1. Bank switch over the bus -> marker readback (incl. the KonamiSCC-
//      specific mirror direction observed at CPU 0x0000/0xC000 regions).
//   2. SCC enable (0x3F at 0x9000) -> waveform/freq/vol/enable writes over
//      the bus -> wave readback over the bus (incl. the 0x9900 mirror,
//      A-M29-1).
//   3. scc_chip() accessor: non-null exactly when a KonamiSCC cart is
//      loaded (both bays); nullptr for empty slots, other mapper types, and
//      invalid slot numbers -- the v1.0.29 regression null (§2.3).
//   4. Slot-2 variant of the CPU-driven probe.
//
// #A8 routing values, derived from this project's own SlotBus bit layout
// (slot_bus.cpp: 2 bits per page, page N at bits [2N, 2N+1]); the SCC probe
// needs pages 1 AND 2 (0x4000-0xBFFF) routed to the cartridge bay while
// pages 0/3 stay at primary slot 3 RAM (program + stack):
//   slot 1: page0=3,page1=1,page2=1,page3=3 -> 3|(1<<2)|(1<<4)|(3<<6) = 0xD7
//   slot 2: page0=3,page1=2,page2=2,page3=3 -> 3|(2<<2)|(2<<4)|(3<<6) = 0xEB

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "devices/cartridge/cartridge_rom_window.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::cartridge::CartridgeLoadResult;
using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::devices::cartridge::CartridgeRomWindow;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

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

constexpr std::uint16_t kBase = 0xC000;      // page 3 RAM (map_flat_ram default)
constexpr std::uint16_t kDestBase = 0xC600;  // landing zone, well past the program
constexpr std::uint8_t kA8Slot1Pages12 = 0xD7;
constexpr std::uint8_t kA8Slot2Pages12 = 0xEB;

// OUT (#A8),a8 ; then (addr,value) writes via LD A/LD (nn),A ; then reads
// via LD A,(nn) / LD (kDestBase+i),A ; HALT.
std::vector<std::uint8_t> build_probe(const std::uint8_t a8_value,
                                       const std::vector<std::pair<std::uint16_t, std::uint8_t>>& writes,
                                       const std::vector<std::uint16_t>& reads) {
    Prog p;
    p.emit({0x3E, a8_value});
    p.emit({0xD3, 0xA8});
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

std::vector<std::uint8_t> make_marker_image(const unsigned num_banks) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_banks) * CartridgeRomWindow::kBankSize);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        const auto marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[static_cast<std::size_t>(bank) * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }
    return image;
}

bool run_to_halt(Hbf1xvMachine& machine, const std::vector<std::uint8_t>& program) {
    machine.load_memory(kBase, program.data(), static_cast<std::uint32_t>(program.size()));
    machine.cpu().state().regs().pc = kBase;
    int guard = 0;
    while (!machine.cpu().state().halted() && guard < 200'000) {
        machine.step_cpu_instruction();
        ++guard;
    }
    return machine.cpu().state().halted();
}

// The full CPU-driven KonamiSCC probe against one bay.
void run_scc_probe(const int slot_number, const std::uint8_t a8_value, const char* prefix) {
    // --- Bank switching + KonamiSCC-specific mirrors over the bus. Note:
    //     with pages 0/3 at RAM, the mirror window-slots 0/1 (CPU
    //     0x0000-0x3FFF) and 6/7 (0xC000-0xFFFF) are NOT CPU-visible here
    //     (they hold the program/stack) -- the mirror-direction assertion is
    //     made through the mapper seam after the bus-driven switch. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        const auto load_ok = machine.load_cartridge(slot_number, CartridgeMapperType::KonamiSCC,
                                                     make_marker_image(16));
        expect(load_ok == CartridgeLoadResult::Ok, (std::string(prefix) + "_Load_Ok").c_str());

        const auto program = build_probe(a8_value,
                                          {{0x5000, 4},     // page 2 canonical
                                           {0x7400, 5},     // page 3 mid-window (0x800-wide decode)
                                           {0xB000, 6}},    // page 5 canonical
                                          {0x4000, 0x6000, 0xA000});
        expect(run_to_halt(machine, program), (std::string(prefix) + "_Banks_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 4, (std::string(prefix) + "_Page2_Bank4").c_str());
        expect(machine.read_memory(kDestBase + 1) == 5,
               (std::string(prefix) + "_Page3_MidWindowWrite_Bank5").c_str());
        expect(machine.read_memory(kDestBase + 2) == 6, (std::string(prefix) + "_Page5_Bank6").c_str());

        // KonamiSCC-specific mirror direction (R-M29-1), asserted at the
        // mapper seam after the REAL bus-driven switches above: pages 2/3
        // land in window-slots 6/7, page 5 in window-slot 1.
        auto& slot = slot_number == 1 ? machine.cartridge_slot1() : machine.cartridge_slot2();
        auto* mapper = slot.mapper();
        expect(mapper->mem_read(0xC000) == 4, (std::string(prefix) + "_MirrorSlot6_TracksPage2").c_str());
        expect(mapper->mem_read(0xE000) == 5, (std::string(prefix) + "_MirrorSlot7_TracksPage3").c_str());
        expect(mapper->mem_read(0x2000) == 6, (std::string(prefix) + "_MirrorSlot1_TracksPage5").c_str());
    }

    // --- SCC enable + register traffic over the bus. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_cartridge(slot_number, CartridgeMapperType::KonamiSCC, make_marker_image(16));

        const auto program = build_probe(a8_value,
                                          {{0x9000, 0x3F},   // enable SCC (+ bank switch, both-effects)
                                           {0x9800, 0x20},   // ch1 wave[0] = +32
                                           {0x9920, 0x15},   // ch2 wave[0] via the 0x9900 mirror (A-M29-1)
                                           {0x9880, 0x63},   // ch1 period low = 99
                                           {0x988A, 0x0F},   // ch1 volume = 15
                                           {0x988F, 0x01}},  // enable ch1
                                          {0x9800,           // wave readback (base)
                                           0x9900,           // same register via the mirror
                                           0x98A0});         // dead region 0xA0-0xDF -> 0xFF
        expect(run_to_halt(machine, program), (std::string(prefix) + "_Scc_ReachesHalt").c_str());
        expect(machine.read_memory(kDestBase + 0) == 0x20,
               (std::string(prefix) + "_Scc_WaveReadbackOverBus").c_str());
        expect(machine.read_memory(kDestBase + 1) == 0x20,
               (std::string(prefix) + "_Scc_0x9900MirrorReadsSameRegister").c_str());
        expect(machine.read_memory(kDestBase + 2) == 0xFF,
               (std::string(prefix) + "_Scc_DeadRegionReadsFF").c_str());

        // The accessor exposes the SAME chip the bus traffic just configured.
        auto* scc = machine.scc_chip(slot_number);
        expect(scc != nullptr, (std::string(prefix) + "_SccChip_NonNull").c_str());
        if (scc != nullptr) {
            expect(scc->wave(0, 0) == 0x20, (std::string(prefix) + "_SccChip_SameChipAsBusTraffic").c_str());
            expect(scc->wave(1, 0) == 0x15, (std::string(prefix) + "_SccChip_MirrorWriteLanded").c_str());
            expect(scc->period(0) == 0x63 && scc->volume(0) == 15 && scc->enable_bits() == 0x01,
                   (std::string(prefix) + "_SccChip_FreqVolEnableLanded").c_str());
            // Held output was refreshed by the period write: (32*15)>>4 = 30.
            expect(scc->sample() == 30, (std::string(prefix) + "_SccChip_SampleMatchesHandComputed").c_str());
        }
    }
}

}  // namespace

int main() {
    // --- 1./2. CPU-driven probes, both bays independently. ---
    run_scc_probe(1, kA8Slot1Pages12, "Slot1");
    run_scc_probe(2, kA8Slot2Pages12, "Slot2");

    // --- 3. scc_chip() accessor nullability matrix (§2.3 regression null). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        expect(machine.scc_chip(1) == nullptr, "SccChip_EmptySlot1_Nullptr");
        expect(machine.scc_chip(2) == nullptr, "SccChip_EmptySlot2_Nullptr");
        expect(machine.scc_chip(0) == nullptr, "SccChip_InvalidSlot0_Nullptr");
        expect(machine.scc_chip(3) == nullptr, "SccChip_InvalidSlot3_Nullptr");

        // Another mapper type in the bay -> still nullptr.
        machine.load_cartridge(1, CartridgeMapperType::Konami, make_marker_image(32));
        expect(machine.scc_chip(1) == nullptr, "SccChip_PlainKonamiLoaded_Nullptr");

        // A KonamiSCC cart -> non-null for THAT bay only.
        machine.load_cartridge(2, CartridgeMapperType::KonamiSCC, make_marker_image(16));
        expect(machine.scc_chip(2) != nullptr, "SccChip_KonamiSccInSlot2_NonNull");
        expect(machine.scc_chip(1) == nullptr, "SccChip_Slot1StillPlainKonami_Nullptr");

        // Unload -> back to the regression null.
        machine.unload_cartridge(2);
        expect(machine.scc_chip(2) == nullptr, "SccChip_AfterUnload_Nullptr");
    }

    // --- 4. Empty-machine byte-identity guard: with no cartridge loaded the
    //     M29 code is present but unused -- open-bus behaviour identical to
    //     the M13-M28 default (v1.0.29 regression guard). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0xA8, kA8Slot1Pages12);
        expect(machine.debug_bus_read(0x4000) == 0xFF, "EmptySlot_Page1_OpenBus");
        expect(machine.debug_bus_read(0x9800) == 0xFF, "EmptySlot_SccWindowAddress_OpenBus");
        machine.debug_bus_write(0x9000, 0x3F);  // must be a no-op
        expect(machine.debug_bus_read(0x9800) == 0xFF, "EmptySlot_EnableWriteIsNoOp");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
