#pragma once

#include <optional>
#include <string_view>

namespace sony_msx::devices::cartridge {

// The six MVP external-cartridge mapper types (M19-S1, backlog B7).
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
//
// The deferred remainder of openMSX's ~90 RomType values (KonamiSCC and the
// long vendor-specific tail) is out of scope for M19 (planner §1.2, backlog
// rows G1/G4) -- never copy openMSX's RomType/RomInfo code itself into src/
// (GPL isolation, guardrails); this enum + the name table below are an
// independent, from-scratch re-expression of the same 6 canonical names.
enum class CartridgeMapperType {
    Mirrored,
    Generic8kB,
    Generic16kB,
    Ascii8kB,
    Ascii16kB,
    Konami,
};

// Case-insensitive parse of one of the 6 canonical name strings above. Returns
// std::nullopt for any unrecognized string -- NEVER a silent default (planner
// A-M19-3/A-M19-5: only an OMITTED `--cartN-type` flag defaults to Mirrored;
// an unrecognized VALUE is always an error at the CLI layer, cartridge_cli.h).
[[nodiscard]] std::optional<CartridgeMapperType> parse_cartridge_mapper_type(std::string_view name);

// The canonical (openMSX-matching) name string for `type`.
[[nodiscard]] std::string_view to_string(CartridgeMapperType type);

}  // namespace sony_msx::devices::cartridge
