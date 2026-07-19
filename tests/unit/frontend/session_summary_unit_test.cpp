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

#include <iostream>
#include <string>

#include "frontend/session_summary.h"

// Suite: Frontend_SessionSummary_Unit (M46, DEC-0071, AC-14). Pure, SDL-free,
// deterministic ctest seam for the enriched startup banner's label text: the
// RAM stock/modded line, the machine-mode tag, the FM-PAC status line (every
// state), and the SRAM availability line. Non-tautology: each case pins the
// EXACT rendered string, not that the helper echoes its input.

namespace {

using namespace sony_msx::frontend;

int g_failures = 0;

void expect_eq(const std::string& got, const std::string& want, const char* name) {
    if (got != want) {
        std::cerr << "Case failed: " << name << "\n  got : '" << got << "'\n  want: '" << want << "'\n";
        ++g_failures;
    }
}

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

FmPacBannerInfo info(int loaded_slot, FmPacAutoloadOutcome outcome, bool is_stock, bool no_fmpac) {
    FmPacBannerInfo i;
    i.loaded_slot = loaded_slot;
    i.outcome = outcome;
    i.is_stock = is_stock;
    i.no_fmpac = no_fmpac;
    i.autoload_rom_path = "roms/fmpac.rom";
    return i;
}

}  // namespace

int main() {
    // --- RAM line: 64 KB is stock; anything else is the modded label. ---
    expect_eq(format_ram_line(64u * 1024u), "64 KB  (stock)", "Ram_64_Stock");
    expect_eq(format_ram_line(512u * 1024u), "512 KB  (modded; --ram 64 or --stock for stock)",
              "Ram_512_Modded");
    expect_eq(format_ram_line(128u * 1024u), "128 KB  (modded; --ram 64 or --stock for stock)",
              "Ram_128_Modded");

    // --- Mode tag. ---
    expect_eq(format_mode_tag(true), "[--stock]", "ModeTag_Stock");
    expect_eq(format_mode_tag(false), "[convenience defaults]", "ModeTag_Convenience");

    // --- FM-PAC line: loaded slot is authoritative. ---
    expect_eq(format_fmpac_line(info(2, FmPacAutoloadOutcome::LoadedSlot2, false, false)),
              "loaded in slot 2", "Fmpac_LoadedSlot2");
    expect_eq(format_fmpac_line(info(1, FmPacAutoloadOutcome::SkippedAlreadyPresent, false, false)),
              "loaded in slot 1", "Fmpac_LoadedSlot1");
    // Not loaded: skip reasons take precedence over the opt-out labels.
    expect_eq(format_fmpac_line(info(0, FmPacAutoloadOutcome::SkippedAbsent, false, false)),
              "auto-load skipped: roms/fmpac.rom not found", "Fmpac_Absent");
    expect_eq(format_fmpac_line(info(0, FmPacAutoloadOutcome::SkippedInvalid, false, false)),
              "auto-load skipped: roms/fmpac.rom invalid", "Fmpac_Invalid");
    expect_eq(format_fmpac_line(info(0, FmPacAutoloadOutcome::SkippedSlot2InUse, false, false)),
              "not loaded (slot 2 in use)", "Fmpac_Slot2InUse");
    expect_eq(format_fmpac_line(info(0, FmPacAutoloadOutcome::NotAttempted, /*stock=*/true, false)),
              "not loaded (--stock)", "Fmpac_NotLoaded_Stock");
    expect_eq(format_fmpac_line(info(0, FmPacAutoloadOutcome::NotAttempted, false, /*no_fmpac=*/true)),
              "not loaded (--no-fmpac)", "Fmpac_NotLoaded_NoFmpac");

    // --- SRAM line (DEC-0050): available -> path, else the bare-machine note. ---
    expect_eq(format_sram_line(true, "C:/x/roms/fmpac.rom.sram"),
              "available -> C:/x/roms/fmpac.rom.sram", "Sram_Available");
    expect_eq(format_sram_line(false, ""),
              "not available (no FM-PAC; bare machine, DEC-0050)", "Sram_NotAvailable");

    // --- Non-tautology guard: the stock RAM label and the modded label DIFFER,
    // and a bad implementation echoing "512 KB" as stock would fail Ram_512. ---
    expect(format_ram_line(64u * 1024u) != format_ram_line(512u * 1024u),
           "Ram_StockAndModded_Differ");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_SessionSummary_Unit cases passed\n";
    return 0;
}
