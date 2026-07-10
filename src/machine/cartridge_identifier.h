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

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "machine/cartridge_cli.h"
#include "machine/software_db.h"

namespace sony_msx::machine {

// M30 (backlog G2): universal cartridge mapper auto-identification --
// explicit flag > softwaredb SHA1 match > bank-write heuristic (with the
// small-image plain-ROM rule inside the heuristic). This is machine/CLI-layer
// POLICY layered ABOVE the cartridge devices: it produces the `type` argument
// for the existing, unchanged Hbf1xvMachine::load_cartridge(slot, type,
// image) API and never touches anything under src/devices/
// (docs/m30-planner-package.md §2.3/§2.4).

// Default softwaredb location when --softwaredb is absent: CWD-relative,
// the same working-directory convention as the default `bios/` asset root
// (planner §2.2.3; degradation is loud -- message E -- and non-fatal).
inline constexpr const char* kDefaultSoftwareDbPath = "references/openmsx-21.0/share/softwaredb.xml";

enum class CartridgeIdentificationMethod {
    ExplicitFlag,         // --cartN-type VALUE given: today's behavior, untouched
    SoftwareDbSha1,       // exact SHA1 match in the softwaredb
    HeuristicBankScan,    // bank-select-write scan (>= 64 KB images)
    SmallImagePlainRule,  // < 64 KB, or == 64 KB without an "AB" header
    SignatureFmPac        // M36: FM-PAC BIOS signature ("PAC2OPLL"@0x18) match
};

struct CartridgeIdentification {
    // Valid unless `unsupported` is set.
    devices::cartridge::CartridgeMapperType type = devices::cartridge::CartridgeMapperType::Mirrored;
    CartridgeIdentificationMethod method = CartridgeIdentificationMethod::ExplicitFlag;
    std::string sha1_hex;      // always computed on the auto path; empty on ExplicitFlag
    std::string title;         // DB title when matched, else empty (signature outcomes
                               // reuse the signature string so message B stays readable)
    std::string db_type_name;  // raw DB/signature type string (also for the unsupported case)
    bool unsupported = false;  // DB/signature named a type outside our implemented seven
    std::string detail;        // heuristic score breakdown (post-handicap), e.g.
                               // "KonamiSCC=12 Konami=2 ASCII8=0 ASCII16=1"; or the
                               // small-image rule's reason text (message D)
};

// THE one shared resolution function (planner §2.4.1), consumed -- via
// CartridgeIdentificationSession below -- by BOTH src/main.cpp
// (load_cartridges_from_args) and src/frontend/sdl3_app.cpp
// (load_configured_assets). Pure function of (spec, image bytes, db):
// no wall clock, no host state, no randomness (planner §2.4.2).
//
// Heuristic semantics: an independent RE-DERIVATION of the BEHAVIOR of
// openMSX's RomFactory::guessRomType
// (references/openmsx-21.0/src/memory/RomFactory.cc:82-169), cross-checked
// against fMSX's GuessROM (references/fmsx-60/source/fMSX/MSX.c:2784-2878).
// The four openMSX-vs-fMSX disagreements are recorded (per DEC-0030) in
// cartridge_identifier.cpp next to the code they affect; all four are
// software-policy divergences with no real-hardware truth, arbitrated to the
// primary reference (openMSX) per the planner's §2.3 ruling.
[[nodiscard]] CartridgeIdentification resolve_cartridge_type(const ParsedCartridgeSlotCli& spec,
                                                             std::span<const std::uint8_t> image,
                                                             const SoftwareDb* db);

// The shared stderr report formatter (messages A-D, planner §2.4.4; tests
// assert these verbatim). Returns "" for ExplicitFlag (the explicit path
// produces NO new output -- byte-for-byte today's behavior). `db_path` is
// the softwaredb path actually consulted (embedded in message A).
// The returned line has no trailing newline.
[[nodiscard]] std::string format_cartridge_identification_message(int slot_number,
                                                                  const CartridgeIdentification& ident,
                                                                  const std::string& db_path);

// Message E (planner §2.4.4): the once-per-process DB-unavailable line.
// `was_explicit` selects the louder WARNING wording used when an explicit
// --softwaredb path could not be read.
[[nodiscard]] std::string format_softwaredb_unavailable_message(const std::string& db_path,
                                                                bool was_explicit);

// Lazy per-run identification session shared by BOTH executables: loads the
// softwaredb at most ONCE per process, and only when at least one slot
// actually needs auto-identification (planner §2.2.1); emits message E at
// most once. The executables keep only I/O + printing glue.
class CartridgeIdentificationSession {
public:
    // `softwaredb_path`: the parsed --softwaredb value; std::nullopt selects
    // kDefaultSoftwareDbPath.
    explicit CartridgeIdentificationSession(std::optional<std::string> softwaredb_path);

    struct Resolution {
        devices::cartridge::CartridgeMapperType type =
            devices::cartridge::CartridgeMapperType::Mirrored;
        // false => identified-but-unsupported: the caller must print
        // `messages`, NOT load the cartridge, and fail loudly (non-zero
        // exit / startup abort -- planner §2.4.3). When false, the LAST
        // entry of `messages` is the message-B line.
        bool ok = true;
        // stderr lines in emit order (message E first when applicable, then
        // A/B/C/D). Empty on the explicit path and for an empty image.
        std::vector<std::string> messages;
        CartridgeIdentification identification;
    };

    // Explicit specs (`spec.type_was_explicit == true`) pass through with
    // zero messages and no DB access. An EMPTY image also passes through
    // unchanged (identification skipped): the existing loud load-failure
    // path owns that case (planner §2.3 preamble).
    [[nodiscard]] Resolution resolve(int slot_number, const ParsedCartridgeSlotCli& spec,
                                     std::span<const std::uint8_t> image);

    [[nodiscard]] const std::string& db_path() const { return db_path_; }

private:
    std::string db_path_;
    bool db_path_was_explicit_ = false;
    bool db_load_attempted_ = false;
    bool db_unavailable_message_emitted_ = false;
    std::optional<SoftwareDb> db_;
};

}  // namespace sony_msx::machine
