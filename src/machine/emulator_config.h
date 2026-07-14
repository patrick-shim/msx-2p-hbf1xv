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

#include <string>
#include <string_view>
#include <vector>

namespace sony_msx::machine {

// ---------------------------------------------------------------------------
// Externalized emulator configuration model (M50-S1, docs/m50-planner-
// package.md §1.2/§4.3). Holds the DEFAULTS + KNOBS that M50 moves out of
// hardcoded C++ into a strict XML file, so the machine is configurable without
// recompiling.
//
// SCOPE (M50-S1): this is the PURE model + parser only. It is NOT wired into
// any run path yet -- the CLI-default resolution (precedence CLI > XML >
// default), the determinism/auto-load rule (§4.6), and the machine-sizing
// plumbing are S2/S3. Nothing here reaches a silicon-timing constant (§3
// HARD-EXCLUDE); VRAM is validated-to-128 and slots are out of S1 entirely.
//
// Every field is initialized to its BUILT-IN DEFAULT. `parse()` overwrites a
// field ONLY when the XML supplies a valid value for it; an omitted knob keeps
// its default, and a PRESENT-but-invalid knob keeps its default AND emits a
// per-key WARNING (graceful, never throws -- §4.2). The convenience defaults
// (RAM 512 / fast-disk ON / FM-PAC auto-load ON, per M46/DEC-0071) are the
// base defaults the S2 resolver will fall back to, so they live here.
// ---------------------------------------------------------------------------

struct EmulatorConfig {
    // The 7 BIOS ROM filenames (per role). Defaults are the HB-F1XV spec set
    // (docs/m50-planner-package.md §1.2 table / hbf1xv_machine.cpp load_rom_assets).
    // Only the FILENAME/dir externalizes; the expected size per role stays
    // code-owned (§6-S3) -- it is a spec fact, not a user knob.
    struct BiosRoms {
        std::string bios = "f1xvbios.rom";          // 32 KB BIOS+BASIC
        std::string sub = "f1xvext.rom";            // 16 KB SUB (MSX-BASIC V3.0)
        std::string kanji_driver = "f1xvkdr.rom";   // 32 KB Kanji driver
        std::string disk = "f1xvdisk.rom";          // 16 KB DISK ROM
        std::string fm_music = "f1xvmus.rom";       // 16 KB MSX-MUSIC BIOS (APRLOPLL)
        std::string kanji_font = "f1xvkfn.rom";     // 256 KB Kanji font (#D8-DB)
        std::string firmware = "f1xvfirm.rom";      // 1 MB Halnote/MSX-JE

        bool operator==(const BiosRoms&) const = default;
    };

    // ---- (A) Session / CLI-option defaults ----------------------------------
    bool fast_disk = true;                 // <defaults><fast-disk enabled>  (convenience ON)
    bool fmpac_autoload = true;            // <defaults><fmpac autoload>     (convenience ON)
    int fmpac_slot = 2;                    // <defaults><fmpac slot>         1|2
    bool border_enabled = false;          // <defaults><border enabled>
    bool disk_writable = false;           // <defaults><disk-writable enabled>
    int speed_level = 0;                  // <defaults><speed level>        0..7
    int video_scale = 3;                  // <defaults><video scale>        1..8 (960x720)
    std::string video_filter = "linear";  // <defaults><video filter>       nearest|linear
    int persistence_percent = 0;          // <defaults><persistence percent> 0..100
    std::string persistence_mode = "avg"; // <defaults><persistence mode>   avg|peak
    bool fullscreen = false;              // <defaults><fullscreen enabled>
    bool capture_enabled = false;         // <defaults><capture enabled>    (F10 gate)

    // ---- (B) Machine sizing + asset paths -----------------------------------
    int ram_kb = 512;   // <machine><ram kb>   64|128|256|512  (convenience 512)
    int vram_kb = 128;  // <machine><vram kb>  strict spec: 128 ONLY (validated-to-128)
    std::string bios_dir = "bios";                                                  // <machine><bios dir>
    BiosRoms bios_roms{};                                                           // <machine><bios><rom>*
    std::string fmpac_rom = "roms/fmpac.rom";                                       // <machine><fmpac rom>
    std::string fmpac_sram = "roms/fmpac.rom.sram";                                 // <machine><fmpac sram>
    std::string softwaredb_path = "references/openmsx-21.0/share/softwaredb.xml";   // <machine><softwaredb path>

    bool operator==(const EmulatorConfig&) const = default;

    // Parse a strict `<hbf1xv-config>` XML document. Returns a config with every
    // valid key applied over the built-in defaults; `warnings` collects one
    // human-readable WARNING per unknown / bad-type / out-of-range key (naming
    // the offending key) plus, on a structurally-unrecognizable document, a
    // single whole-file WARNING with all built-in defaults returned. NEVER
    // throws (§4.2).
    [[nodiscard]] static EmulatorConfig parse(std::string_view xml_text,
                                              std::vector<std::string>& warnings);

    // Convenience: read `path` and parse it. An unreadable file yields one
    // WARNING + built-in defaults (never throws). NOTE (M50-S1): this is not
    // wired into any run path; the determinism/auto-load search order (§4.6) is
    // S2's concern.
    [[nodiscard]] static EmulatorConfig load_from_file(const std::string& path,
                                                       std::vector<std::string>& warnings);
};

}  // namespace sony_msx::machine
