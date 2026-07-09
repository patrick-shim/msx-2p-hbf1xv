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
    // M30 (backlog G2): the load layer now consults `type_was_explicit`
    // before trusting this default -- an omitted type (or the explicit value
    // `auto`, below) triggers auto-identification one layer up
    // (cartridge_identifier.h). The PARSER default itself is unchanged.
    devices::cartridge::CartridgeMapperType type = devices::cartridge::CartridgeMapperType::Mirrored;
    bool type_was_explicit = false;
};

struct ParsedCartridgeCli {
    ParsedCartridgeSlotCli slot1;
    ParsedCartridgeSlotCli slot2;
    // M30 (backlog G2): additive `--softwaredb <path>` override for the
    // cartridge-identification database. std::nullopt (flag absent) selects
    // the CWD-relative default (cartridge_identifier.h's
    // kDefaultSoftwareDbPath). Recognized here in the SHARED parser so ALL
    // consumers (headless default run, --parity-trace, --debug-session,
    // SDL3) get it with zero per-mode wiring (planner §2.2.3).
    std::optional<std::string> softwaredb_path;
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or an unrecognized --cartN-type name). Never silently
    // swallowed by the caller (planner §2.4).
    std::vector<std::string> errors;
};

// Pure argv parser (A-M19-4): recognizes `--cart1 <path>`, `--cart1-type
// <name>`, `--cart2 <path>`, `--cart2-type <name>`, and (M30) `--softwaredb
// <path>` ANYWHERE in `args` (order-independent). Does NO file I/O and has
// NO Hbf1xvMachine dependency (mirrors RomAssetLoader's own separation of
// "parse/spec" from "load", rom_asset_loader.h:26), so it is directly
// unit-testable without argv/process plumbing. Absent --cart1/--cart2
// entirely yields a `path == std::nullopt` request for that slot (today's
// status quo, unchanged).
//
// M30: `--cartN-type auto` is accepted as an explicit request for
// auto-identification (openMSX's own vocabulary, RomFactory.cc:180): it
// behaves exactly as if the flag were omitted (`type_was_explicit = false`,
// type left at the Mirrored struct default). `auto` is recognized ONLY at
// this CLI layer -- the device-level enum parser
// (parse_cartridge_mapper_type) stays pure, with no Auto value.
[[nodiscard]] ParsedCartridgeCli parse_cartridge_cli(const std::vector<std::string>& args);

}  // namespace sony_msx::machine
