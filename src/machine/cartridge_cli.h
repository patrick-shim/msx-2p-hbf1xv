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

#include <optional>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"

namespace sony_msx::machine {

// Per-slot cartridge CLI request (M19-S5, backlog B7).
struct ParsedCartridgeSlotCli {
    std::optional<std::string> path;
    // Defaults to Mirrored when --cartN-type is OMITTED (A-M19-5, matching
    // openMSX's own default, RomFactory.cc:178-179). An unrecognized VALUE is
    // always a parse error (see `errors` below), never silently defaulted.
    //
    // M30 (backlog G2): the load layer consults `type_was_explicit` before
    // trusting this default -- an omitted type (or the explicit value
    // `auto`, below) triggers auto-identification one layer up
    // (cartridge_identifier.h). The parser's own default is unchanged.
    devices::cartridge::CartridgeMapperType type = devices::cartridge::CartridgeMapperType::Mirrored;
    bool type_was_explicit = false;
};

struct ParsedCartridgeCli {
    ParsedCartridgeSlotCli slot1;
    ParsedCartridgeSlotCli slot2;
    // M30 (backlog G2): `--softwaredb <path>` override for the cartridge-
    // identification database. std::nullopt (flag absent) selects the
    // CWD-relative default (cartridge_identifier.h's kDefaultSoftwareDbPath).
    // Recognized in the SHARED parser so all consumers (headless default
    // run, --parity-trace, --debug-session, SDL3) get it for free (planner
    // §2.2.3).
    std::optional<std::string> softwaredb_path;
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or an unrecognized --cartN-type name). Never silently
    // swallowed by the caller (planner §2.4).
    std::vector<std::string> errors;
};

// Pure argv parser (A-M19-4): recognizes `--cart1 <path>`, `--cart1-type
// <name>`, `--cart2 <path>`, `--cart2-type <name>`, and (M30) `--softwaredb
// <path>` anywhere in `args` (order-independent).
//
// M46 (DEC-0071): `--slot1`/`--slot2`/`--slot1-type`/`--slot2-type` are the
// official-MSX-term RENAMES and parse to the byte-identical slot fields;
// `--cartN`/`--cartN-type` are KEPT as accepted silent backward-compat aliases
// (existing scripts unbroken). Because both spellings write the same field on
// the linear scan, a `--slotN`/`--cartN` collision for one slot resolves to the
// LAST occurrence (the parser's standing repeated-flag rule).
//
// Does no file I/O and has
// no Hbf1xvMachine dependency (mirrors RomAssetLoader's separation of
// "parse/spec" from "load", rom_asset_loader.h:26), so it is directly
// unit-testable without argv/process plumbing. Omitting --cart1/--cart2
// yields a `path == std::nullopt` request for that slot.
//
// M30: `--cartN-type auto` is an explicit request for auto-identification
// (openMSX's own vocabulary, RomFactory.cc:180): it behaves exactly as if
// the flag were omitted (`type_was_explicit = false`, type left at the
// Mirrored struct default). `auto` is recognized only at this CLI layer --
// the device-level enum parser (parse_cartridge_mapper_type) stays pure,
// with no Auto value.
[[nodiscard]] ParsedCartridgeCli parse_cartridge_cli(const std::vector<std::string>& args);

}  // namespace sony_msx::machine
