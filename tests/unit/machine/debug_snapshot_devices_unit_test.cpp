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

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "machine/debug_format.h"
#include "machine/debug_snapshot.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_DebugSnapshotDevices_Unit (M36 Phase 3 slice S4). FDC regs +
// drive/disk, RTC, S1985, cartridge bank state (via the additive rom_window()
// seam), pause/speed, Ren-Sha -- typed exactness + section completeness (AC-1).

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
const std::string* find_file(const sony_msx::machine::debug_snapshot::Snapshot& snap,
                             const std::string& name) {
    for (const auto& f : snap.files) {
        if (f.name == name) {
            return &f.content;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::debug_format::to_dec;
    namespace ds = sony_msx::machine::debug_snapshot;

    Hbf1xvMachine machine;
    machine.cold_boot();

    // FDC register latches + drive head position (pure, deterministic).
    machine.fdc().write_track(0x0A);
    machine.fdc().write_sector(0x05);
    machine.disk_drive().set_physical_track(0x14);
    machine.disk_drive().set_side(1);

    // Pause / speed / Ren-Sha.
    machine.pause_controller().press_pause_button();
    machine.pause_controller().set_speed_level(3);
    machine.rensha_turbo().set_speed(50);

    // Load a Konami MegaROM (128 KB) so the generic bank-dump seam is exercised.
    {
        std::vector<std::uint8_t> image(0x20000, 0x00);
        const auto result =
            machine.load_cartridge(1, sony_msx::devices::cartridge::CartridgeMapperType::Konami,
                                   std::move(image));
        expect(result == sony_msx::devices::cartridge::CartridgeLoadResult::Ok, "Konami_Loads");
    }
    // Switch page-3's bank to 5 (mem_write to 0x6000 region -> bank_switch(3,5)).
    machine.cartridge_slot1().mem_write(0x6000, 5);

    const ds::Snapshot snap = machine.serialize_snapshot("devices");
    const std::string* fdc = find_file(snap, "fdc.txt");
    const std::string* chip = find_file(snap, "chipset.txt");
    const std::string* rtc = find_file(snap, "rtc.txt");
    const std::string* cart = find_file(snap, "cartridges.txt");
    expect(fdc != nullptr && chip != nullptr && rtc != nullptr && cart != nullptr, "AllFiles_Present");
    if (fdc == nullptr || chip == nullptr || rtc == nullptr || cart == nullptr) {
        return 1;
    }

    // --- FDC / drive / disk. ---
    for (const char* sec : {"[FDC.REGS]", "[FDC.DIAG]", "[FDC.FSM]", "[DRIVE]", "[DISK]"}) {
        expect(contains(*fdc, sec), (std::string("FdcSection_") + sec).c_str());
    }
    expect(contains(*fdc, "TRACK=0A SECTOR=05 "), "Fdc_TrackSector_Exact");
    expect(contains(*fdc, "PHYSICAL_TRACK=20 SIDE=1 "), "Drive_TrackSide_Exact");
    expect(contains(*fdc, "SIZE=737280 "), "Disk_Size_Exact");
    expect(contains(*fdc, "PHASE=Idle(0)"), "Fdc_Phase_Idle");

    // --- RTC: both sections present, decoded fields match live getters. ---
    expect(contains(*rtc, "[RTC.REGS]") && contains(*rtc, "[RTC.TIME]"), "Rtc_Sections");
    expect(contains(*rtc, "SECONDS=" + to_dec(machine.rtc().seconds())), "Rtc_Seconds_Exact");
    expect(contains(*rtc, "LAST_TICKS=" + to_dec(machine.rtc().last_rtc_ticks())), "Rtc_LastTicks_Exact");

    // --- S1985 + slots + pause + Ren-Sha (chipset.txt). ---
    expect(contains(*chip, "[S1985.REGS]") && contains(*chip, "[S1985.SRAM] size=16\n"),
           "S1985_Sections");
    expect(contains(*chip, "BUTTON_ENGAGED=1 SPEED_LEVEL=3 "), "Pause_Exact");
    expect(contains(*chip, "[RENSHA]\nSPEED=50 "), "Rensha_Exact");

    // --- Cartridge generic bank dump via rom_window() (Konami mapper). ---
    expect(contains(*cart, "[CART1]\nLOADED=1 MAPPER_TYPE=Konami"), "Cart1_Loaded_Konami");
    expect(contains(*cart, "[CART1.BANKS]"), "Cart1_BanksSection");
    // After reset: slots 2..5 = banks 0,1,2,3; then page-3 switched to bank 5.
    expect(contains(*cart, "SLOT2=0 SLOT3=5"), "Cart1_Bank3_SwitchedTo5");
    expect(contains(*cart, "SLOT4=2 SLOT5=3"), "Cart1_Banks45_Default");
    // CART2 is empty -> LOADED=0, no BANKS section.
    expect(contains(*cart, "[CART2]\nLOADED=0"), "Cart2_Empty");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_DebugSnapshotDevices_Unit cases passed\n";
    return 0;
}
