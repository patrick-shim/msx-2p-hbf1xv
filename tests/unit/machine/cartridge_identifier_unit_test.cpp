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

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/cartridge_identifier.h"
#include "machine/sha1.h"
#include "machine/software_db.h"

// Suite: Machine_CartridgeIdentifier_Unit (M30-S3, backlog G2, docs/m30-
// planner-package.md §2.3/§2.4/§4-S3)
//
// The re-derived guessRomType heuristic (score table, ASCII8 handicap, the
// >= tie-break order KonamiSCC > Konami > ASCII16 > ASCII8, size gates,
// format signatures), the explicit>DB>heuristic precedence chain, the
// verbatim A-E message formats, and two-run determinism -- all on
// CONSTRUCTED images with hand-computed expected winners (each computed in
// comments below) and FULLY SYNTHETIC database fixtures (fictional titles/
// types; hashes computed by our own sha1 -- never real softwaredb content).

namespace {

using sony_msx::devices::cartridge::CartridgeMapperType;
using sony_msx::machine::CartridgeIdentification;
using sony_msx::machine::CartridgeIdentificationMethod;
using sony_msx::machine::CartridgeIdentificationSession;
using sony_msx::machine::ParsedCartridgeSlotCli;
using sony_msx::machine::SoftwareDb;
using sony_msx::machine::format_cartridge_identification_message;
using sony_msx::machine::format_softwaredb_unavailable_message;
using sony_msx::machine::resolve_cartridge_type;
using sony_msx::machine::sha1_hex;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// A zero-filled image (0x00 is never the 0x32 opcode, so ONLY planted
// sequences score); optional MSX "AB" cartridge header.
std::vector<std::uint8_t> make_image(const std::size_t size, const bool ab_header) {
    std::vector<std::uint8_t> image(size, 0x00);
    if (ab_header && size >= 2) {
        image[0] = 'A';
        image[1] = 'B';
    }
    return image;
}

// Plants the Z80 `LD (addr),A` byte sequence 0x32 <lo> <hi> at `offset`.
void plant_write(std::vector<std::uint8_t>& image, const std::size_t offset, const std::uint16_t addr) {
    image[offset] = 0x32;
    image[offset + 1] = static_cast<std::uint8_t>(addr & 0xFF);
    image[offset + 2] = static_cast<std::uint8_t>(addr >> 8);
}

ParsedCartridgeSlotCli auto_spec() {
    ParsedCartridgeSlotCli spec;
    spec.path = "synthetic.rom";
    spec.type = CartridgeMapperType::Mirrored;  // the untouched parser default
    spec.type_was_explicit = false;
    return spec;
}

// The smallest scanning-eligible image: exactly 64 KB WITH the "AB" header
// (planner §2.3 step 4). Writes are planted from offset 16 up.
std::vector<std::uint8_t> scan_image(const std::vector<std::uint16_t>& addrs) {
    std::vector<std::uint8_t> image = make_image(0x10000, true);
    std::size_t offset = 16;
    for (const std::uint16_t addr : addrs) {
        plant_write(image, offset, addr);
        offset += 4;
    }
    return image;
}

CartridgeMapperType guess_of(const std::vector<std::uint16_t>& addrs) {
    const auto image = scan_image(addrs);
    return resolve_cartridge_type(auto_spec(), image, nullptr).type;
}

std::string write_synthetic_db(const std::string& filename, const std::string& body) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sony_msx_m30_identifier_fixtures";
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / filename;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "<softwaredb>\n" << body << "</softwaredb>\n";
    out.close();
    return path.string();
}

}  // namespace

int main() {
    // =====================================================================
    // Heuristic score table -- every row, hand-computed winners.
    // =====================================================================

    // KonamiSCC rows: one hit each of 0x5000/0x9000/0xB000 -> KonamiSCC=1.
    // Winner walk: ASCII8=0 skip, ASCII16=0 skip, Konami=0 skip,
    // KonamiSCC=1 >= 0 -> KonamiSCC.
    expect(guess_of({0x5000}) == CartridgeMapperType::KonamiSCC, "Scan_5000_KonamiSCC");
    expect(guess_of({0x9000}) == CartridgeMapperType::KonamiSCC, "Scan_9000_KonamiSCC");
    expect(guess_of({0xB000}) == CartridgeMapperType::KonamiSCC, "Scan_B000_KonamiSCC");

    // Konami rows: 0x4000/0x8000/0xA000 -> Konami=1 -> Konami.
    expect(guess_of({0x4000}) == CartridgeMapperType::Konami, "Scan_4000_Konami");
    expect(guess_of({0x8000}) == CartridgeMapperType::Konami, "Scan_8000_Konami");
    expect(guess_of({0xA000}) == CartridgeMapperType::Konami, "Scan_A000_Konami");

    // ASCII8 handicap (planner §2.3): a SINGLE 0x6800 hit -> ASCII8=1, then
    // the deliberate decrement -> 0 -> all scores zero -> Generic8kB.
    expect(guess_of({0x6800}) == CartridgeMapperType::Generic8kB,
           "Scan_Single6800_Ascii8Handicap_Generic8kB");
    expect(guess_of({0x7800}) == CartridgeMapperType::Generic8kB,
           "Scan_Single7800_Ascii8Handicap_Generic8kB");

    // TWO ASCII8 hits: ASCII8=2 -> handicap -> 1 -> only nonzero -> ASCII8
    // CAN still win.
    expect(guess_of({0x6800, 0x7800}) == CartridgeMapperType::Ascii8kB,
           "Scan_TwoAscii8Hits_PostHandicap1_Ascii8Wins");

    // 0x6000 row: Konami+1, ASCII8+1, ASCII16+1. Post-handicap ASCII8=0.
    // Winner walk: ASCII16=1 -> winner; Konami=1 >= 1 -> Konami. (Tie:
    // Konami beats ASCII16.)
    expect(guess_of({0x6000}) == CartridgeMapperType::Konami, "Scan_6000_TieKonamiBeatsAscii16");

    // 0x7000 row: KonamiSCC+1, ASCII8+1, ASCII16+1. Post-handicap ASCII8=0.
    // Winner walk: ASCII16=1 -> winner; Konami=0 skip; KonamiSCC=1 >= 1 ->
    // KonamiSCC. (Tie: KonamiSCC beats ASCII16.)
    expect(guess_of({0x7000}) == CartridgeMapperType::KonamiSCC, "Scan_7000_TieKonamiSccBeatsAscii16");

    // 0x77FF row: ASCII16=1 -> ASCII16.
    // DEC-0030 disagreement record #2 lives exactly here: fMSX's all-ones
    // baseline (+GEN8 bias) would yield GEN8 for this same image; adopted
    // openMSX -> ASCII16 (planner §2.3).
    expect(guess_of({0x77FF}) == CartridgeMapperType::Ascii16kB, "Scan_77FF_Ascii16_OpenMsxArbitration");

    // Tie KonamiSCC vs Konami (1-1): winner walk reaches Konami=1 first,
    // then KonamiSCC=1 >= 1 replaces -> KonamiSCC.
    expect(guess_of({0x4000, 0x5000}) == CartridgeMapperType::KonamiSCC,
           "Scan_TieKonamiSccBeatsKonami");

    // Tie ASCII16 vs ASCII8 (post-handicap 1-1): ASCII8 first (winner at 1),
    // ASCII16=1 >= 1 replaces -> ASCII16.
    expect(guess_of({0x6800, 0x7800, 0x77FF}) == CartridgeMapperType::Ascii16kB,
           "Scan_TieAscii16BeatsAscii8");

    // Strictly higher score wins regardless of order: Konami=2 vs
    // KonamiSCC=1 -> Konami (KonamiSCC's 1 < 2 cannot replace).
    expect(guess_of({0x4000, 0x8000, 0x5000}) == CartridgeMapperType::Konami,
           "Scan_HigherScoreBeatsLaterCandidate");

    // No planted writes at all -> every score 0 -> Generic8kB.
    expect(guess_of({}) == CartridgeMapperType::Generic8kB, "Scan_AllZero_Generic8kB");

    // Score detail string (post-handicap values, message-C order).
    {
        const auto image = scan_image({0x5000, 0x9000, 0x4000, 0x77FF, 0x6800, 0x6800});
        // KonamiSCC=2, Konami=1, ASCII8=2-1=1, ASCII16=1. Winner walk:
        // ASCII8=1 -> winner; ASCII16=1 >= 1 -> ASCII16; Konami=1 >= 1 ->
        // Konami; KonamiSCC=2 >= 1 -> KonamiSCC.
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.type == CartridgeMapperType::KonamiSCC, "Scan_MixedScores_Winner");
        expect(ident.detail == "KonamiSCC=2 Konami=1 ASCII8=1 ASCII16=1",
               "Scan_DetailString_PostHandicapScores");
        expect(ident.method == CartridgeIdentificationMethod::HeuristicBankScan,
               "Scan_Method_HeuristicBankScan");
    }

    // =====================================================================
    // Size gates (planner §2.3 steps 2-4).
    // =====================================================================
    {
        // < 64 KB -> Mirrored via the small-image plain-ROM rule, even with
        // planted bank writes (the scan is NOT eligible below 64 KB --
        // DEC-0030 disagreement record #1: fMSX would scan a 48 KB image;
        // adopted openMSX).
        auto image = make_image(0xC000, true);
        plant_write(image, 16, 0x9000);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.type == CartridgeMapperType::Mirrored &&
                   ident.method == CartridgeIdentificationMethod::SmallImagePlainRule,
               "SizeGate_Under64K_SmallImagePlainRule_Mirrored");
    }
    {
        // == 64 KB WITHOUT "AB" -> Mirrored (plain), never scanned.
        auto image = make_image(0x10000, false);
        plant_write(image, 16, 0x9000);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.type == CartridgeMapperType::Mirrored &&
                   ident.method == CartridgeIdentificationMethod::SmallImagePlainRule,
               "SizeGate_64KNoAbHeader_Mirrored");
    }
    {
        // == 64 KB WITH "AB" -> scanned (here: one 0x9000 hit -> KonamiSCC).
        auto image = make_image(0x10000, true);
        plant_write(image, 16, 0x9000);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.type == CartridgeMapperType::KonamiSCC &&
                   ident.method == CartridgeIdentificationMethod::HeuristicBankScan,
               "SizeGate_64KWithAbHeader_Scanned");
    }
    {
        // > 64 KB -> scanned regardless of header.
        auto image = make_image(0x20000, false);
        plant_write(image, 16, 0x8000);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.type == CartridgeMapperType::Konami &&
                   ident.method == CartridgeIdentificationMethod::HeuristicBankScan,
               "SizeGate_Over64K_Scanned");
    }

    // =====================================================================
    // Format signatures (planner §2.3 step 1) -> identified-but-unsupported.
    // =====================================================================
    for (const char* signature : {"ASCII16X", "ROM_NEO8", "ROM_NE16"}) {
        auto image = make_image(0x8000, true);
        std::memcpy(image.data() + 16, signature, 8);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(ident.unsupported && ident.db_type_name == signature,
               "Signature_Unsupported_LoudOutcome");
    }

    // =====================================================================
    // Precedence (planner §2.4.1).
    // =====================================================================

    // Build a scan image whose heuristic answer would be Konami, and a
    // POISONED synthetic DB entry for its exact sha1 claiming ASCII16.
    const auto precedence_image = scan_image({0x8000, 0x8000});
    const std::string precedence_sha1 = sha1_hex(precedence_image);
    const std::string db_path = write_synthetic_db(
        "precedence_db.xml",
        "<software><title>Quantum Badger</title>"
        "<dump><megarom><type>ASCII16</type><hash>" + precedence_sha1 + "</hash></megarom></dump>"
        "</software>\n");
    std::vector<std::string> db_diagnostics;
    const auto db = SoftwareDb::load_from_file(db_path, db_diagnostics);
    expect(db.has_value() && db->entry_count() == 1, "PrecedenceFixtureDb_Loads");

    {
        // Explicit flag bypasses BOTH the (poisoned) DB and the heuristic:
        // no SHA1 is even computed.
        ParsedCartridgeSlotCli spec = auto_spec();
        spec.type = CartridgeMapperType::Konami;
        spec.type_was_explicit = true;
        const auto ident = resolve_cartridge_type(spec, precedence_image, db.has_value() ? &*db : nullptr);
        expect(ident.type == CartridgeMapperType::Konami &&
                   ident.method == CartridgeIdentificationMethod::ExplicitFlag &&
                   ident.sha1_hex.empty() && !ident.unsupported,
               "Precedence_ExplicitFlag_BypassesDbAndHeuristic");
    }
    {
        // DB beats heuristic: the scan says Konami, the DB says ASCII16 ->
        // ASCII16 via SoftwareDbSha1.
        const auto ident =
            resolve_cartridge_type(auto_spec(), precedence_image, db.has_value() ? &*db : nullptr);
        expect(ident.type == CartridgeMapperType::Ascii16kB &&
                   ident.method == CartridgeIdentificationMethod::SoftwareDbSha1 &&
                   ident.title == "Quantum Badger" && ident.db_type_name == "ASCII16",
               "Precedence_DbBeatsHeuristic");
        // Message A verbatim.
        expect(format_cartridge_identification_message(1, ident, "synthetic/db.xml") ==
                   "cartridge: --cart1: identified as \"Quantum Badger\" (ASCII16) via softwaredb "
                   "SHA1 match [sha1=" + precedence_sha1 + ", db=synthetic/db.xml]",
               "MessageA_Verbatim");
    }
    {
        // No DB -> heuristic wins (Konami), message C verbatim.
        const auto ident = resolve_cartridge_type(auto_spec(), precedence_image, nullptr);
        expect(ident.type == CartridgeMapperType::Konami &&
                   ident.method == CartridgeIdentificationMethod::HeuristicBankScan,
               "Precedence_NoDb_HeuristicUsed");
        expect(format_cartridge_identification_message(1, ident, "synthetic/db.xml") ==
                   "cartridge: --cart1: no softwaredb match [sha1=" + precedence_sha1 +
                   "]; guessed Konami via bank-write heuristic (scores: KonamiSCC=0 Konami=2 "
                   "ASCII8=0 ASCII16=0); pass --cart1-type to override",
               "MessageC_Verbatim");
    }

    // =====================================================================
    // Identified-but-unsupported DB type -> message B verbatim.
    // =====================================================================
    {
        const auto small_image = make_image(0x4000, true);
        const std::string small_sha1 = sha1_hex(small_image);
        const std::string vapor_db_path = write_synthetic_db(
            "unsupported_db.xml",
            "<software><title>Vapor Title</title>"
            "<dump><megarom><type>MegaVaporMapper</type><hash>" + small_sha1 +
            "</hash></megarom></dump></software>\n");
        std::vector<std::string> diags;
        const auto vapor_db = SoftwareDb::load_from_file(vapor_db_path, diags);
        expect(vapor_db.has_value(), "UnsupportedFixtureDb_Loads");
        const auto ident = resolve_cartridge_type(auto_spec(), small_image,
                                                  vapor_db.has_value() ? &*vapor_db : nullptr);
        expect(ident.unsupported && ident.db_type_name == "MegaVaporMapper" &&
                   ident.method == CartridgeIdentificationMethod::SoftwareDbSha1,
               "UnsupportedDbType_Flagged");
        expect(format_cartridge_identification_message(1, ident, vapor_db_path) ==
                   "cartridge: --cart1: identified as \"Vapor Title\" (MegaVaporMapper) via "
                   "softwaredb SHA1 match, but mapper type \"MegaVaporMapper\" is not implemented "
                   "(implemented: Mirrored, 8kB, 16kB, ASCII8, ASCII16, Konami, KonamiSCC); pass "
                   "--cart1-type to force one. Aborting.",
               "MessageB_Verbatim");
    }
    {
        // Message B for a format-signature outcome (slot 2 wording).
        auto image = make_image(0x8000, true);
        std::memcpy(image.data() + 16, "ASCII16X", 8);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(format_cartridge_identification_message(2, ident, "synthetic/db.xml") ==
                   "cartridge: --cart2: identified as \"ASCII16X\" (ASCII16X) via ROM format "
                   "signature, but mapper type \"ASCII16X\" is not implemented (implemented: "
                   "Mirrored, 8kB, 16kB, ASCII8, ASCII16, Konami, KonamiSCC); pass --cart2-type "
                   "to force one. Aborting.",
               "MessageB_Signature_Verbatim");
    }

    // =====================================================================
    // Message D (small-image rule) verbatim, both variants.
    // =====================================================================
    {
        const auto image = make_image(0x4000, true);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(format_cartridge_identification_message(1, ident, "synthetic/db.xml") ==
                   "cartridge: --cart1: no softwaredb match [sha1=" + sha1_hex(image) +
                   "]; image < 64 KB -> Mirrored (plain-ROM rule); pass --cart1-type to override",
               "MessageD_Under64K_Verbatim");
    }
    {
        const auto image = make_image(0x10000, false);
        const auto ident = resolve_cartridge_type(auto_spec(), image, nullptr);
        expect(format_cartridge_identification_message(1, ident, "synthetic/db.xml") ==
                   "cartridge: --cart1: no softwaredb match [sha1=" + sha1_hex(image) +
                   "]; image is 64 KB without an \"AB\" header -> Mirrored (plain-ROM rule); "
                   "pass --cart1-type to override",
               "MessageD_64KNoAb_Verbatim");
    }

    // =====================================================================
    // Message E verbatim (both wordings).
    // =====================================================================
    expect(format_softwaredb_unavailable_message("references/openmsx-21.0/share/softwaredb.xml",
                                                 false) ==
               "cartridge: softwaredb not found at \"references/openmsx-21.0/share/softwaredb.xml\""
               " -- SHA1 identification disabled (heuristic only); pass --softwaredb <path> to "
               "enable",
           "MessageE_DefaultPath_Verbatim");
    expect(format_softwaredb_unavailable_message("my/own.xml", true) ==
               "cartridge: WARNING: --softwaredb \"my/own.xml\" could not be read -- SHA1 "
               "identification disabled (heuristic only)",
           "MessageE_ExplicitPath_WarningVariant_Verbatim");

    // =====================================================================
    // Session behavior: lazy DB, message E once per process, explicit
    // pass-through, unsupported -> ok=false.
    // =====================================================================
    {
        CartridgeIdentificationSession session(std::string("this/db/does/not/exist.xml"));
        const auto image = scan_image({0x5000});

        // Explicit spec: zero messages, type passed through.
        ParsedCartridgeSlotCli explicit_spec = auto_spec();
        explicit_spec.type = CartridgeMapperType::Konami;
        explicit_spec.type_was_explicit = true;
        const auto explicit_res = session.resolve(1, explicit_spec, image);
        expect(explicit_res.ok && explicit_res.type == CartridgeMapperType::Konami &&
                   explicit_res.messages.empty(),
               "Session_ExplicitSpec_NoMessages_TypePassedThrough");

        // First auto resolve: message E (WARNING variant -- the path was
        // explicit) then message C.
        const auto first = session.resolve(1, auto_spec(), image);
        expect(first.ok && first.type == CartridgeMapperType::KonamiSCC, "Session_AutoResolve_Heuristic");
        expect(first.messages.size() == 2 &&
                   first.messages[0] == format_softwaredb_unavailable_message(
                                            "this/db/does/not/exist.xml", true),
               "Session_DbUnavailable_MessageE_EmittedFirst");

        // Second auto resolve: message E NOT repeated.
        const auto second = session.resolve(2, auto_spec(), image);
        expect(second.messages.size() == 1, "Session_MessageE_OncePerSession");
    }
    {
        // Session default path constant (planner §2.2.3).
        CartridgeIdentificationSession session(std::nullopt);
        expect(session.db_path() == std::string(sony_msx::machine::kDefaultSoftwareDbPath),
               "Session_DefaultDbPath_Constant");
    }
    {
        // Session unsupported outcome: ok=false, last message is B.
        const auto small_image = make_image(0x4000, true);
        const std::string small_sha1 = sha1_hex(small_image);
        const std::string vapor_db_path = write_synthetic_db(
            "session_unsupported_db.xml",
            "<software><title>Vapor Title</title>"
            "<dump><megarom><type>MegaVaporMapper</type><hash>" + small_sha1 +
            "</hash></megarom></dump></software>\n");
        CartridgeIdentificationSession session(vapor_db_path);
        const auto res = session.resolve(1, auto_spec(), small_image);
        expect(!res.ok && !res.messages.empty() &&
                   res.messages.back().find("Aborting.") != std::string::npos,
               "Session_UnsupportedDbType_NotOk_MessageBLast");
    }
    {
        // Empty image: identification skipped (the loud load-failure path
        // owns it); parser default passes through, zero messages.
        CartridgeIdentificationSession session(std::nullopt);
        const std::vector<std::uint8_t> empty;
        const auto res = session.resolve(1, auto_spec(), empty);
        expect(res.ok && res.type == CartridgeMapperType::Mirrored && res.messages.empty(),
               "Session_EmptyImage_IdentificationSkipped");
    }

    // =====================================================================
    // Two-run determinism (planner §2.4.2): identical inputs -> identical
    // CartridgeIdentification, every field.
    // =====================================================================
    {
        const auto image = scan_image({0x5000, 0x6000, 0x77FF});
        const auto first = resolve_cartridge_type(auto_spec(), image, db.has_value() ? &*db : nullptr);
        const auto second = resolve_cartridge_type(auto_spec(), image, db.has_value() ? &*db : nullptr);
        expect(first.type == second.type && first.method == second.method &&
                   first.sha1_hex == second.sha1_hex && first.title == second.title &&
                   first.db_type_name == second.db_type_name &&
                   first.unsupported == second.unsupported && first.detail == second.detail,
               "Determinism_TwoRuns_IdenticalIdentification_EveryField");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_CartridgeIdentifier_Unit cases passed\n";
    return 0;
}
