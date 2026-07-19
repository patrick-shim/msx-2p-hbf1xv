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

#include <filesystem>
#include <iostream>
#include <string>

#include "frontend/config_paths.h"
#include "machine/emulator_config.h"

// Suite: Frontend_ConfigPaths_Unit (DEC-0095-AMENDMENT-D). The PERMANENT oracle
// for with_absolute_asset_paths().
//
// THE DEFECT IT GUARDS (found in a live v1.6.0 session): the emulator resolves
// relative asset paths against the WORKING DIRECTORY, but v1.6.0 writes the
// settings file beside the EXECUTABLE. A persisted `roms/fmpac.rom` therefore
// resolved only while the emulator happened to be launched from the repo root;
// launched from anywhere else it resolved to <cwd>/roms/fmpac.rom, found
// nothing, and FM-PAC auto-load SILENTLY skipped -- slot 2 empty, no error, and
// nothing on screen to explain it. The BIOS dir escaped only because
// Machine > BIOS Folder... happens to store an absolute path.
//
// The base directory is injected, so every case below is hermetic -- no reliance
// on (or mutation of) the process working directory.

namespace {

using sony_msx::frontend::with_absolute_asset_paths;
using sony_msx::machine::EmulatorConfig;

int g_failures = 0;

void expect(const bool ok, const std::string& case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void expect_eq(const std::string& got, const std::string& want, const std::string& case_name) {
    if (got != want) {
        std::cerr << "Case failed: " << case_name << "\n  got : " << got << "\n  want: " << want
                  << "\n";
        ++g_failures;
    }
}

// Build the expected absolute form the same way the implementation does, so the
// oracle is separator-correct on both Windows and POSIX without hard-coding.
std::string expected_abs(const std::filesystem::path& base, const std::string& rel) {
    std::filesystem::path p = (base / rel).lexically_normal();
    p.make_preferred();
    return p.string();
}

}  // namespace

int main() {
    // A genuinely ABSOLUTE base on both platforms. This matters: on Windows a
    // path needs a root-NAME ("C:") to be absolute, so a bare "/msx-root" is
    // still relative there -- and the helper would (correctly) rewrite it.
#ifdef _WIN32
    const std::filesystem::path base = std::filesystem::path("C:/msx-root").make_preferred();
#else
    const std::filesystem::path base = std::filesystem::path("/msx-root");
#endif
    expect(base.is_absolute(), "fixture / base is absolute on this platform");

    // 1. THE REGRESSION CASE: every relative asset path becomes absolute. If any
    //    of these reverts to relative, the live defect returns -- a persisted
    //    config that silently stops resolving when launched from elsewhere.
    {
        EmulatorConfig cfg;  // built-in defaults: all six are RELATIVE
        const EmulatorConfig out = with_absolute_asset_paths(cfg, base);
        expect_eq(out.fmpac_rom, expected_abs(base, "roms/fmpac.rom"), "relative / fmpac_rom");
        expect_eq(out.fmpac_sram, expected_abs(base, "roms/fmpac.rom.sram"),
                  "relative / fmpac_sram");
        expect_eq(out.bios_dir, expected_abs(base, "bios"), "relative / bios_dir");
        expect_eq(out.cartridge_dir, expected_abs(base, "roms"), "relative / cartridge_dir");
        expect_eq(out.disk_dir, expected_abs(base, "disks"), "relative / disk_dir");
        expect(std::filesystem::path(out.softwaredb_path).is_absolute(),
               "relative / softwaredb_path is absolute");
    }

    // 2. Already-absolute paths are left VERBATIM -- a hand-authored absolute
    //    path (or one written by Machine > BIOS Folder...) must never be rewritten.
    {
        EmulatorConfig cfg;
        const std::string abs_bios = expected_abs(base, "elsewhere/bios");
        const std::string abs_rom = expected_abs(base, "elsewhere/fmpac.rom");
        cfg.bios_dir = abs_bios;
        cfg.fmpac_rom = abs_rom;
        const EmulatorConfig out = with_absolute_asset_paths(cfg, base);
        expect_eq(out.bios_dir, abs_bios, "absolute / bios_dir untouched");
        expect_eq(out.fmpac_rom, abs_rom, "absolute / fmpac_rom untouched");
    }

    // 3. Empty means "unset" -- never invent a path for it.
    {
        EmulatorConfig cfg;
        cfg.fmpac_rom.clear();
        cfg.fmpac_sram.clear();
        cfg.softwaredb_path.clear();
        const EmulatorConfig out = with_absolute_asset_paths(cfg, base);
        expect(out.fmpac_rom.empty(), "empty / fmpac_rom stays empty");
        expect(out.fmpac_sram.empty(), "empty / fmpac_sram stays empty");
        expect(out.softwaredb_path.empty(), "empty / softwaredb_path stays empty");
    }

    // 4. BIOS ROM FILENAMES are NOT paths -- they resolve UNDER bios_dir. Rewriting
    //    them would break the BIOS-folder indirection (Machine > BIOS Folder...).
    {
        EmulatorConfig cfg;
        const EmulatorConfig out = with_absolute_asset_paths(cfg, base);
        expect_eq(out.bios_roms.bios, cfg.bios_roms.bios, "bios_roms / bios filename untouched");
        expect_eq(out.bios_roms.firmware, cfg.bios_roms.firmware,
                  "bios_roms / firmware filename untouched");
        expect(out.bios_roms == cfg.bios_roms, "bios_roms / whole set untouched");
    }

    // 5. Non-path knobs are carried through unchanged -- this is a path rewrite,
    //    not a config transform.
    {
        EmulatorConfig cfg;
        cfg.master_volume = 42;
        cfg.ram_kb = 128;
        cfg.video_filter = "nearest";
        cfg.fullscreen = true;
        const EmulatorConfig out = with_absolute_asset_paths(cfg, base);
        expect(out.master_volume == 42 && out.ram_kb == 128 && out.video_filter == "nearest" &&
                   out.fullscreen,
               "passthrough / non-path knobs unchanged");
    }

    // 6. Idempotent: running it twice is a no-op (the second pass sees absolutes).
    {
        EmulatorConfig cfg;
        const EmulatorConfig once = with_absolute_asset_paths(cfg, base);
        const EmulatorConfig twice = with_absolute_asset_paths(once, base);
        expect(once == twice, "idempotent / second pass changes nothing");
    }

    // ---- DEC-0098: a missing asset path must never be honoured silently ------
    {
        using sony_msx::frontend::resolve_existing_asset;
        const std::string good = expected_abs(base, "roms/fmpac.rom");
        const std::string stale = expected_abs(base, "build/roms/fmpac.rom");

        // The path exists -> used as-is, and nothing is said.
        {
            const auto exists = [&good](const std::string& p) { return p == good; };
            const auto [path, msg] =
                resolve_existing_asset("FM-PAC ROM", good, "roms/fmpac.rom", base, exists);
            expect_eq(path, good, "missing-asset / existing path kept");
            expect(msg.empty(), "missing-asset / existing path is silent");
        }

        // THE REGRESSION CASE: a stale absolute path (exactly what v1.6.2 wrote)
        // falls back to the standard project-root location AND announces it.
        {
            const auto exists = [&good](const std::string& p) { return p == good; };
            const auto [path, msg] =
                resolve_existing_asset("FM-PAC ROM", stale, "roms/fmpac.rom", base, exists);
            expect_eq(path, good, "missing-asset / stale path falls back to root default");
            expect(msg.find("not found") != std::string::npos &&
                       msg.find("using") != std::string::npos,
                   "missing-asset / fallback is announced");
        }

        // Neither exists -> keep the configured value but WARN, so an empty
        // slot 2 is never unexplained again.
        {
            const auto exists = [](const std::string&) { return false; };
            const auto [path, msg] =
                resolve_existing_asset("FM-PAC ROM", stale, "roms/fmpac.rom", base, exists);
            expect_eq(path, stale, "missing-asset / unresolvable keeps configured value");
            expect(msg.find("WARNING") != std::string::npos, "missing-asset / unresolvable warns");
        }

        // An empty (unset) value is not an error.
        {
            const auto exists = [](const std::string&) { return false; };
            const auto [path, msg] =
                resolve_existing_asset("FM-PAC ROM", "", "roms/fmpac.rom", base, exists);
            expect(path.empty() && msg.empty(), "missing-asset / empty stays silent");
        }
    }

    // ---- DEC-0097: relativize (the PERSIST side) ----------------------------
    {
        using sony_msx::frontend::with_relative_asset_paths;

        // THE round-trip property: absolutize for runtime, relativize to persist,
        // and you are back exactly where you started. This is what lets the saved
        // file survive the project directory being renamed or moved.
        {
            const EmulatorConfig original;  // defaults: all relative
            const EmulatorConfig runtime = with_absolute_asset_paths(original, base);
            const EmulatorConfig persisted = with_relative_asset_paths(runtime, base);
            expect(persisted == original, "relativize / round-trip returns the original");
        }

        // Emitted with FORWARD slashes so one config reads correctly on Windows,
        // macOS and Linux alike.
        {
            EmulatorConfig cfg;
            cfg.fmpac_rom = expected_abs(base, "roms/fmpac.rom");
            const EmulatorConfig out = with_relative_asset_paths(cfg, base);
            expect_eq(out.fmpac_rom, "roms/fmpac.rom", "relativize / forward slashes");
        }

        // A path OUTSIDE the root stays ABSOLUTE -- a BIOS folder the user picked
        // elsewhere must not become a brittle "../.." chain.
        {
            EmulatorConfig cfg;
#ifdef _WIN32
            const std::string outside = std::filesystem::path("C:/elsewhere/bios").make_preferred().string();
#else
            const std::string outside = "/elsewhere/bios";
#endif
            cfg.bios_dir = outside;
            const EmulatorConfig out = with_relative_asset_paths(cfg, base);
            expect_eq(out.bios_dir, outside, "relativize / outside-root stays absolute");
        }

        // Already-relative and empty values are left alone.
        {
            EmulatorConfig cfg;
            cfg.fmpac_sram.clear();
            const EmulatorConfig out = with_relative_asset_paths(cfg, base);
            expect_eq(out.bios_dir, "bios", "relativize / already-relative untouched");
            expect(out.fmpac_sram.empty(), "relativize / empty stays empty");
        }

        // Idempotent.
        {
            const EmulatorConfig once = with_relative_asset_paths(
                with_absolute_asset_paths(EmulatorConfig{}, base), base);
            expect(once == with_relative_asset_paths(once, base), "relativize / idempotent");
        }
    }

    // ---- DEC-0097: project-root discovery from the executable location -------
    // The predicate is injected, so these run with no real filesystem. The point
    // is that ONE search absorbs all three layouts -- no per-platform branching.
    {
        using sony_msx::frontend::find_project_root;
        const std::filesystem::path root = base;  // pretend this is the project root
        const auto is_root = [&root](const std::filesystem::path& d) { return d == root; };

        // Windows multi-config: <root>/build/Debug -> 2 levels up.
        expect(find_project_root(root / "build" / "Debug", is_root) == root,
               "root / windows build\\Debug (2 levels)");
        // macOS + Linux single-config: <root>/build -> 1 level up.
        expect(find_project_root(root / "build", is_root) == root,
               "root / unix build (1 level)");
        // Flat install: the exe sits AT the root -> level 0, found immediately.
        expect(find_project_root(root, is_root) == root, "root / flat layout (0 levels)");
        // Deeper nesting still resolves within the level budget.
        expect(find_project_root(root / "a" / "b" / "c", is_root) == root,
               "root / 3 levels up");
        // No match anywhere -> nullopt, so the caller keeps its previous behavior
        // rather than inventing a root.
        expect(!find_project_root(root / "build", [](const std::filesystem::path&) { return false; })
                    .has_value(),
               "root / no match yields nullopt");
        // Beyond the level budget -> nullopt (never walks to the filesystem root).
        expect(!find_project_root(root / "a" / "b" / "c" / "d" / "e", is_root, 2).has_value(),
               "root / respects max_levels");
        // An empty start is not a filesystem walk.
        expect(!find_project_root(std::filesystem::path(), is_root).has_value(),
               "root / empty start yields nullopt");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "Frontend_ConfigPaths_Unit: all cases passed\n";
    return 0;
}
