#pragma once

#include <optional>
#include <string_view>

namespace sony_msx::devices::cartridge {

// The external-cartridge mapper types: the six MVP types (M19-S1, backlog
// B7) plus KonamiSCC (M29-S1, backlog G1).
//
// Every value + its canonical name string is taken VERBATIM from openMSX's own
// RomType/RomInfo tables (never an invented vocabulary), so a `--cartN-type`
// value on this project's CLI is the exact string an openMSX user/config would
// already recognize (planner A-M19-3):
//   references/openmsx-21.0/src/memory/RomInfo.cc:19-20,23,26-27,92
//     GENERIC_8KB  -> "8kB"
//     GENERIC_16KB -> "16kB"
//     KONAMI       -> "Konami"
//     ASCII8       -> "ASCII8"
//     ASCII16      -> "ASCII16"
//     MIRRORED     -> "Mirrored"
//   references/openmsx-21.0/src/memory/RomInfo.cc:24 (M29, backlog G1)
//     KONAMI_SCC   -> "KonamiSCC"
//
// The deferred remainder of openMSX's ~90 RomType values (the long
// vendor-specific tail, backlog row G4; SCC-I/"SCC+" split out as row G5) is
// still out of scope -- never copy openMSX's RomType/RomInfo code itself into
// src/ (GPL isolation, guardrails); this enum + the name table below are an
// independent, from-scratch re-expression of the same canonical names.
enum class CartridgeMapperType {
    Mirrored,
    Generic8kB,
    Generic16kB,
    Ascii8kB,
    Ascii16kB,
    Konami,
    // M29 (backlog G1): the SCC-bearing Konami MegaROM sibling
    // (cartridge_konami_scc_rom.h). Canonical openMSX display string
    // "KonamiSCC" (RomInfo.cc:24).
    KonamiSCC,
};

// Case-insensitive parse of one of the canonical name strings above. Returns
// std::nullopt for any unrecognized string -- NEVER a silent default (planner
// A-M19-3/A-M19-5: only an OMITTED `--cartN-type` flag defaults to Mirrored;
// an unrecognized VALUE is always an error at the CLI layer, cartridge_cli.h).
[[nodiscard]] std::optional<CartridgeMapperType> parse_cartridge_mapper_type(std::string_view name);

// The canonical (openMSX-matching) name string for `type`.
[[nodiscard]] std::string_view to_string(CartridgeMapperType type);

}  // namespace sony_msx::devices::cartridge
