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

// M46 (DEC-0071): pure, SDL-free helpers for the enriched startup banner's
// "This session" block. Kept OUT of sdl3_main.cpp so the deterministic label
// text (RAM stock/modded, machine-mode tag, FM-PAC status, SRAM availability)
// has a ctest seam (AC-14) that needs no process capture and no SDL3. The
// FmPacAutoloadOutcome enum also lives here so both sdl3_app.h (which records
// it) and these helpers (which render it) share it without an SDL include.
namespace sony_msx::frontend {

// The outcome of the FM-PAC slot-2 auto-load attempt (planner §2.5). Recorded
// by Sdl3App::load_configured_assets() / the headless --debug-session load path
// and read by the banner.
enum class FmPacAutoloadOutcome {
    NotAttempted,           // fmpac_autoload resolved false (stock config default / --no-fmpac / --stock)
    LoadedSlot2,            // auto-loaded into primary slot 2
    SkippedSlot2InUse,      // an explicit --slot2/--cart2 cart occupies slot 2
    SkippedAlreadyPresent,  // an FM-PAC is already inserted (the human's slot-1 habit) -- no double-load
    SkippedAbsent,          // the auto-load ROM file was not found
    SkippedInvalid,         // the auto-load ROM failed to load (bad size / corrupt)
};

// The banner-relevant FM-PAC facts, resolved from the actual machine state +
// the recorded auto-load outcome + the CLI mode.
struct FmPacBannerInfo {
    // Which bay actually holds an FM-PAC after init (from machine.fmpac(1)/(2)):
    // 1, 2, or 0 for none. Authoritative -- takes precedence over `outcome`.
    int loaded_slot = 0;
    FmPacAutoloadOutcome outcome = FmPacAutoloadOutcome::NotAttempted;
    bool is_stock = false;  // --stock preset (distinguishes the "not loaded" reason)
    bool no_fmpac = false;  // --no-fmpac flag (distinguishes the "not loaded" reason)
    // The auto-load ROM path (e.g. "roms/fmpac.rom"), for the not-found/invalid note.
    std::string autoload_rom_path;
};

// "512 KB  (modded; --ram 64 or --stock for stock)" vs "64 KB  (stock)".
[[nodiscard]] std::string format_ram_line(std::size_t ram_bytes);

// The machine-mode tag: "[--stock]" when stock, else "[convenience defaults]".
[[nodiscard]] std::string format_mode_tag(bool is_stock);

// The FM-PAC status line body (planner §2.6): "loaded in slot 2" /
// "loaded in slot 1" / "not loaded (--stock)" / "not loaded (--no-fmpac)" /
// "auto-load skipped: <path> not found" / "auto-load skipped: <path> invalid".
[[nodiscard]] std::string format_fmpac_line(const FmPacBannerInfo& info);

// The SRAM availability line body (planner §2.6, DEC-0050): when an FM-PAC is
// loaded + persistence on, "available -> <resolved path>"; otherwise
// "not available (no FM-PAC; bare machine, DEC-0050)".
[[nodiscard]] std::string format_sram_line(bool available, const std::string& resolved_path);

}  // namespace sony_msx::frontend
