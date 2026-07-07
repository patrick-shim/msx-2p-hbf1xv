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
    devices::cartridge::CartridgeMapperType type = devices::cartridge::CartridgeMapperType::Mirrored;
    bool type_was_explicit = false;
};

struct ParsedCartridgeCli {
    ParsedCartridgeSlotCli slot1;
    ParsedCartridgeSlotCli slot2;
    // Non-empty means at least one flag could not be parsed (missing value
    // argument, or an unrecognized --cartN-type name). Never silently
    // swallowed by the caller (planner §2.4).
    std::vector<std::string> errors;
};

// Pure argv parser (A-M19-4): recognizes `--cart1 <path>`, `--cart1-type
// <name>`, `--cart2 <path>`, `--cart2-type <name>` ANYWHERE in `args`
// (order-independent). Does NO file I/O and has NO Hbf1xvMachine dependency
// (mirrors RomAssetLoader's own separation of "parse/spec" from "load",
// rom_asset_loader.h:26), so it is directly unit-testable without argv/
// process plumbing. Absent --cart1/--cart2 entirely yields a `path ==
// std::nullopt` request for that slot (today's status quo, unchanged).
[[nodiscard]] ParsedCartridgeCli parse_cartridge_cli(const std::vector<std::string>& args);

}  // namespace sony_msx::machine
