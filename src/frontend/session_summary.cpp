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

#include "frontend/session_summary.h"

namespace sony_msx::frontend {

std::string format_ram_line(const std::size_t ram_bytes) {
    const std::string kb = std::to_string(ram_bytes / 1024u);
    // 64 KB is the strict target-spec stock RAM (DEC-0050/DEC-0061); anything
    // else is an opt-in NON-STOCK "fully-populated S1985" mod. The convenience
    // default is 512 KB, so the modded label points the user back to stock.
    if (ram_bytes == 64u * 1024u) {
        return kb + " KB  (stock)";
    }
    return kb + " KB  (modded; --ram 64 or --stock for stock)";
}

std::string format_mode_tag(const bool is_stock) {
    return is_stock ? "[--stock]" : "[convenience defaults]";
}

std::string format_fmpac_line(const FmPacBannerInfo& info) {
    // The actual machine state is authoritative: if a bay holds an FM-PAC, say
    // so regardless of HOW it got there (auto-load, or an explicit --slotN cart).
    if (info.loaded_slot == 1) {
        return "loaded in slot 1";
    }
    if (info.loaded_slot == 2) {
        return "loaded in slot 2";
    }
    // No FM-PAC anywhere -> explain why. Skip reasons take precedence over the
    // plain opt-out labels so a missing/corrupt asset is always surfaced.
    switch (info.outcome) {
        case FmPacAutoloadOutcome::SkippedAbsent:
            return "auto-load skipped: " + info.autoload_rom_path + " not found";
        case FmPacAutoloadOutcome::SkippedInvalid:
            return "auto-load skipped: " + info.autoload_rom_path + " invalid";
        case FmPacAutoloadOutcome::SkippedSlot2InUse:
            return "not loaded (slot 2 in use)";
        case FmPacAutoloadOutcome::NotAttempted:
        case FmPacAutoloadOutcome::SkippedAlreadyPresent:
        case FmPacAutoloadOutcome::LoadedSlot2:
            break;
    }
    if (info.is_stock) {
        return "not loaded (--stock)";
    }
    if (info.no_fmpac) {
        return "not loaded (--no-fmpac)";
    }
    return "not loaded";
}

std::string format_sram_line(const bool available, const std::string& resolved_path) {
    if (available) {
        return "available -> " + resolved_path;
    }
    // DEC-0050 invariant: a bare HB-F1XV has NO internal SRAM. The auto-load
    // never fabricates any; only an inserted FM-PAC cartridge provides it.
    return "not available (no FM-PAC; bare machine, DEC-0050)";
}

}  // namespace sony_msx::frontend
