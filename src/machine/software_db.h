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
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sony_msx::machine {

// One softwaredb dump entry, keyed by its 40-char lowercase SHA1 hex string.
// `type_name` is the RAW database type string (e.g. "KonamiSCC", "ASCII8",
// the <rom>-without-<type> default "Mirrored", a <start>-composed subtype
// like "Mirrored0000", or "" for a <megarom> with no <type>) -- NEVER
// pre-mapped to this project's mapper enum: the identifier layer
// (cartridge_identifier.h) decides supported-vs-unsupported, so composed/
// unknown names route to the loud "identified but unsupported" outcome
// instead of being silently truncated.
struct SoftwareDbEntry {
    std::string title;
    std::string type_name;
};

// Minimal, tolerant, runtime parser for the SUBSET of the openMSX
// `softwaredb.xml` schema this project needs:
// <softwaredb>/<software>/<title>/<dump>/<rom>|<megarom>/<type>/
// <hash>/<start>. This is NOT a general XML parser -- it is a tolerant
// scanner for exactly this schema.
//
// Semantics grounded in the file's own producer/consumer (verified against
// the openMSX sources, never copied -- the DB file itself is never shipped
// with this project; this class only PARSES a locally-present copy at
// runtime, the same posture as reading the user's own bios/ ROMs; nothing
// from the file ships in src/ or tests/):
//   - <rom> WITHOUT <type> defaults to "Mirrored"
//     (openMSX 21.0: src/memory/RomDatabase.cc:201-208);
//   - <megarom> has NO default -- a missing <type> stays "" and later routes
//     to the loud unsupported outcome (RomDatabase.cc:193-199);
//   - a <dump> with no <hash> is dropped (RomDatabase.cc:490-494);
//   - a <start> value like "0x4000" composes "Mirrored4000"/"Normal4000"
//     (only for those two base names), recorded RAW (RomDatabase.cc:500-522);
//   - duplicate hashes: FIRST occurrence wins (RomDatabase.cc:437-449's
//     keep-the-old rule, deterministic);
//   - XML comments, CDATA, DOCTYPE, processing instructions, attributes and
//     unknown elements are tolerated/skipped; a malformed region skips to
//     the next <software> with a collected diagnostic -- never a crash.
class SoftwareDb {
public:
    // std::nullopt when the file is absent/unreadable (graceful degradation
    // -- the emulator never DEPENDS on the DB file to
    // function). `diagnostics` collects per-entry skip notes (never fatal).
    static std::optional<SoftwareDb> load_from_file(const std::string& path,
                                                    std::vector<std::string>& diagnostics);

    // `sha1_hex_lowercase`: 40-char lowercase hex. Returns nullptr on miss.
    [[nodiscard]] const SoftwareDbEntry* lookup(std::string_view sha1_hex_lowercase) const;

    [[nodiscard]] std::size_t entry_count() const { return entries_.size(); }

private:
    SoftwareDb() = default;

    std::unordered_map<std::string, SoftwareDbEntry> entries_;
};

}  // namespace sony_msx::machine
