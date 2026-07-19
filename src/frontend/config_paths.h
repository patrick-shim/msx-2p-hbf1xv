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

#include <filesystem>
#include <string>

#include "machine/emulator_config.h"

namespace sony_msx::frontend {

// ---------------------------------------------------------------------------
// DEC-0095-AMENDMENT-D: make PERSISTED asset paths working-directory-proof.
//
// THE BUG THIS FIXES: the emulator resolves relative asset paths against the
// WORKING DIRECTORY, but as of v1.6.0 the settings file is written beside the
// EXECUTABLE -- a different directory. So a config carrying `roms/fmpac.rom`
// resolved fine while it lived at the repo root, and silently stopped resolving
// once it moved next to the exe and the emulator was launched from anywhere
// else: FM-PAC auto-load found nothing, skipped, and slot 2 came up empty with
// no error. (The BIOS dir escaped this only by accident -- Machine > BIOS
// Folder... happens to store an ABSOLUTE path.)
//
// THE FIX: at SAVE time, rewrite every relative asset path to the absolute path
// the RUNNING session actually resolved it to. That preserves this session's
// exact meaning rather than re-interpreting it against some future working
// directory. Already-absolute paths and empty values are untouched, so a
// hand-authored absolute path survives verbatim.
//
// NOT absolutized: EmulatorConfig::bios_roms -- those are FILENAMES resolved
// UNDER bios_dir, not paths in their own right. Absolutizing them would break
// the BIOS-folder indirection that makes Machine > BIOS Folder... work.
//
// TRADE-OFF (deliberate): an absolutized config is machine-local -- copying it
// to another machine will not resolve. That is inherent to the persist-beside-
// exe model (the file records one machine's live state), and it is strictly
// better than the alternative, which is paths that break on the SAME machine
// depending on where you launched from.
//
// Pure: the base directory is INJECTED rather than read from the process, so
// the behavior is unit-testable without touching the real working directory.
// ---------------------------------------------------------------------------
[[nodiscard]] inline machine::EmulatorConfig with_absolute_asset_paths(
    const machine::EmulatorConfig& cfg, const std::filesystem::path& base) {
    machine::EmulatorConfig out = cfg;

    const auto absolutize = [&base](std::string& value) {
        if (value.empty()) {
            return;  // "unset" stays unset -- never invent a path
        }
        const std::filesystem::path p(value);
        if (!p.is_relative()) {
            return;  // already absolute: the user's explicit choice, kept verbatim
        }
        std::filesystem::path abs = (base / p).lexically_normal();
        abs.make_preferred();  // native separators (no mixed \ and / on Windows)
        value = abs.string();
    };

    absolutize(out.bios_dir);
    absolutize(out.cartridge_dir);
    absolutize(out.disk_dir);
    absolutize(out.fmpac_rom);
    absolutize(out.fmpac_sram);
    absolutize(out.softwaredb_path);
    return out;
}

}  // namespace sony_msx::frontend
