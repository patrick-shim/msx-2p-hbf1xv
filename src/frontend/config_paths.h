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
#include <optional>
#include <string>
#include <utility>

#include "machine/emulator_config.h"

namespace sony_msx::frontend {

// ---------------------------------------------------------------------------
// Make PERSISTED asset paths working-directory-proof (DEC-0095-AMENDMENT-D).
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
// ---------------------------------------------------------------------------
// Find the PROJECT ROOT from the executable's own location (DEC-0097).
//
// The asset directories (bios/, roms/, disks/, games/) and the config file all
// live at the project root, but the executable does NOT: it sits at
// <root>/build/Debug/ on Windows (multi-config) and <root>/build/ on macOS and
// Linux (single-config). Hard-coding "../.." would be wrong on one of them, and
// resolving against the WORKING DIRECTORY makes everything depend on where the
// user happened to launch from -- the defect that made FM-PAC silently fail.
//
// So: walk UP from the start directory and take the first level that looks like
// the project root. `is_root` is INJECTED so the search is unit-testable without
// a real filesystem; discover_project_root() below supplies the real predicate
// (a directory containing `bios/`).
//
// Level 0 is the start directory itself, so a FLAT install (exe sitting beside
// bios/) is found too -- the same code covers all three layouts with no
// per-platform branching.
// ---------------------------------------------------------------------------
template <typename IsRootPredicate>
[[nodiscard]] std::optional<std::filesystem::path> find_project_root(
    const std::filesystem::path& start, IsRootPredicate is_root, const int max_levels = 4) {
    if (start.empty()) {
        return std::nullopt;
    }
    std::filesystem::path dir = start.lexically_normal();
    for (int level = 0; level <= max_levels; ++level) {
        if (is_root(dir)) {
            return dir;
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent.empty() || parent == dir) {
            break;  // hit the filesystem root -- stop rather than spin
        }
        dir = parent;
    }
    return std::nullopt;
}

// The real-filesystem wrapper: the project root is the nearest ancestor of the
// executable that contains a `bios` directory. Returns nullopt when nothing
// matches (a detached/installed binary), leaving the caller on its previous
// working-directory behavior rather than inventing a root.
[[nodiscard]] inline std::optional<std::filesystem::path> discover_project_root(
    const std::filesystem::path& exe_dir) {
    return find_project_root(exe_dir, [](const std::filesystem::path& dir) {
        std::error_code ec;
        return std::filesystem::is_directory(dir / "bios", ec);
    });
}

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

// ---------------------------------------------------------------------------
// Never honour a configured asset path that does not exist, SILENTLY (DEC-0098).
//
// THE FAILURE THIS ENDS: a config written by v1.6.2 could carry an absolute
// path that was correct only for the directory it was launched from once
// (e.g. <root>/build/roms/fmpac.rom). Later versions treated any absolute path
// as the user's deliberate choice and passed it through untouched -- so FM-PAC
// auto-load looked up a file that was not there, skipped, and left cartridge
// slot 2 empty with NOTHING on screen or on stderr to explain it. Three separate
// debugging sessions across three machines traced back to that silence.
//
// Policy: if the configured path is missing but the STANDARD project-root
// location exists, use that and SAY SO. If neither exists, keep the configured
// value (so the existing loader reports it in its own diagnostics) and WARN.
// `exists` is injected, so the decision logic is unit-testable with no
// filesystem.
//
// Returns {effective_path, message}; message empty means nothing notable.
// ---------------------------------------------------------------------------
template <typename ExistsPredicate>
[[nodiscard]] std::pair<std::string, std::string> resolve_existing_asset(
    const std::string& label, const std::string& configured,
    const std::string& default_relative, const std::filesystem::path& root,
    ExistsPredicate exists) {
    if (configured.empty() || exists(configured)) {
        return {configured, std::string()};
    }
    std::filesystem::path fallback = (root / default_relative).lexically_normal();
    fallback.make_preferred();
    const std::string fallback_str = fallback.string();
    if (fallback_str != configured && exists(fallback_str)) {
        return {fallback_str, "config: NOTE " + label + " \"" + configured +
                                  "\" not found; using \"" + fallback_str + "\" instead"};
    }
    return {configured, "config: WARNING " + label + " \"" + configured +
                            "\" not found -- this asset will not load"};
}

// ---------------------------------------------------------------------------
// The INVERSE, applied when PERSISTING (DEC-0097). Runtime wants absolute paths
// (resolved against the project root at startup); the saved FILE wants them
// relative, for two concrete reasons:
//   * renaming or moving the project directory would otherwise break every
//     baked-in absolute path -- resurfacing as "my ROMs stopped loading", the
//     exact failure class this whole area keeps producing;
//   * a relative config is shareable between machines (Windows/macOS/Pi), which
//     an absolute one never is.
//
// A path UNDER `root` becomes root-relative and is emitted with FORWARD SLASHES
// (generic_string) so the same file reads correctly on every platform. A path
// OUTSIDE the root -- a BIOS folder the user deliberately picked elsewhere, or
// one on another drive -- stays ABSOLUTE, because making it relative would be
// both ugly ("../../elsewhere") and fragile.
// ---------------------------------------------------------------------------
[[nodiscard]] inline machine::EmulatorConfig with_relative_asset_paths(
    const machine::EmulatorConfig& cfg, const std::filesystem::path& root) {
    machine::EmulatorConfig out = cfg;

    const auto relativize = [&root](std::string& value) {
        if (value.empty()) {
            return;
        }
        const std::filesystem::path p(value);
        if (p.is_relative()) {
            return;  // already relative -- nothing to do
        }
        const std::filesystem::path rel = p.lexically_relative(root);
        if (rel.empty()) {
            return;  // no relation (e.g. a different Windows drive) -> keep absolute
        }
        if (rel.begin() != rel.end() && *rel.begin() == "..") {
            return;  // outside the project root -> keep absolute (user's own location)
        }
        value = rel.generic_string();  // '/' separators: portable across platforms
    };

    relativize(out.bios_dir);
    relativize(out.cartridge_dir);
    relativize(out.disk_dir);
    relativize(out.fmpac_rom);
    relativize(out.fmpac_sram);
    relativize(out.softwaredb_path);
    return out;
}

}  // namespace sony_msx::frontend
