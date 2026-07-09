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

#include "machine/cartridge_identifier.h"

#include <cstring>
#include <utility>

#include "machine/sha1.h"

namespace sony_msx::machine {

namespace {

using devices::cartridge::CartridgeMapperType;
using devices::cartridge::parse_cartridge_mapper_type;
using devices::cartridge::to_string;

// §2.3 step 1 -- third-party ROM-format magic values at offset 16 (facts of
// those file formats, not license-sensitive data; RomFactory.cc:90-95 checks
// the same three strings). All three name G4-tail mapper types this project
// does not implement, so a match is a loud identified-but-unsupported
// outcome -- misbooting them as Generic8kB is exactly the garbage-screen
// class M30 exists to kill.
constexpr std::size_t kSignatureOffset = 16;
constexpr std::size_t kSignatureSize = 8;
constexpr const char* kFormatSignatures[] = {"ASCII16X", "ROM_NEO8", "ROM_NE16"};

std::string unsigned_to_string(const unsigned value) {
    return std::to_string(value);
}

// The §2.3 step-4 bank-select-write scan, re-derived from
// RomFactory.cc:117-167 (behavior only, never code).
CartridgeIdentification run_bank_write_scan(std::span<const std::uint8_t> image) {
    unsigned konami_scc = 0;
    unsigned konami = 0;
    unsigned ascii8 = 0;
    unsigned ascii16 = 0;

    // For every i in [0, size-3): a 0x32 byte (the Z80 `LD (nn),A` opcode)
    // followed by a little-endian 16-bit address scores per the table.
    // (Only ever called with size >= 0x10000, so `size - 3` cannot wrap.)
    for (std::size_t i = 0; i < image.size() - 3; ++i) {
        if (image[i] != 0x32) {
            continue;
        }
        const std::uint16_t addr =
            static_cast<std::uint16_t>(image[i + 1] | (static_cast<std::uint16_t>(image[i + 2]) << 8));
        switch (addr) {
            case 0x5000:
            case 0x9000:
            case 0xB000:
                ++konami_scc;
                break;
            case 0x4000:
            case 0x8000:
            case 0xA000:
                ++konami;
                break;
            case 0x6800:
            case 0x7800:
                ++ascii8;
                break;
            case 0x6000:
                ++konami;
                ++ascii8;
                ++ascii16;
                break;
            case 0x7000:
                ++konami_scc;
                ++ascii8;
                ++ascii16;
                break;
            case 0x77FF:
                ++ascii16;
                break;
            default:
                break;
        }
    }

    // The deliberate ASCII8 handicap (RomFactory.cc:160), guarded exactly as
    // the reference guards its unsigned underflow.
    //
    // DEC-0030 disagreement record #2 (baseline biasing): openMSX
    // zero-initializes all counters and applies only this conditional ASCII8
    // decrement; fMSX initializes ALL counters to 1, then GEN8 +1 and
    // ASCII8 -1 (MSX.c:2840-2844) -- e.g. a single 0x77FF hit yields ASCII16
    // under openMSX but GEN8 under fMSX. Software policy, no hardware truth;
    // adopted: openMSX (planner §2.3).
    if (ascii8 > 0) {
        --ascii8;
    }

    // Winner selection: start from Generic8kB at score 0 and evaluate the
    // candidates in the FIXED order ASCII8, ASCII16, Konami, KonamiSCC,
    // replacing the current winner when a candidate's NONZERO score is >=
    // the winner's. This documented order reproduces openMSX's
    // enum-declaration-order iteration + `tg && (tg >= ...)` guard
    // (RomFactory.cc:161-166; RomTypes.hh:8-38 declares ASCII8 < ASCII16 <
    // GENERIC_8KB < KONAMI < KONAMI_SCC, and GENERIC_8KB's zero score can
    // never re-win) -- ties therefore resolve KonamiSCC > Konami > ASCII16 >
    // ASCII8.
    //
    // DEC-0030 disagreement record #3 (tie-break direction): fMSX uses a
    // strict `>` first-index-wins scan instead (MSX.c:2872-2874). Software
    // policy; adopted: openMSX (planner §2.3).
    CartridgeMapperType winner = CartridgeMapperType::Generic8kB;
    unsigned winner_score = 0;
    const struct {
        CartridgeMapperType type;
        unsigned score;
    } candidates[] = {
        {CartridgeMapperType::Ascii8kB, ascii8},
        {CartridgeMapperType::Ascii16kB, ascii16},
        {CartridgeMapperType::Konami, konami},
        {CartridgeMapperType::KonamiSCC, konami_scc},
    };
    for (const auto& candidate : candidates) {
        if (candidate.score != 0 && candidate.score >= winner_score) {
            winner = candidate.type;
            winner_score = candidate.score;
        }
    }

    CartridgeIdentification ident;
    ident.type = winner;
    ident.method = CartridgeIdentificationMethod::HeuristicBankScan;
    // Post-handicap values -- the numbers the decision actually used.
    ident.detail = "KonamiSCC=" + unsigned_to_string(konami_scc) + " Konami=" +
                   unsigned_to_string(konami) + " ASCII8=" + unsigned_to_string(ascii8) +
                   " ASCII16=" + unsigned_to_string(ascii16);
    return ident;
}

}  // namespace

CartridgeIdentification resolve_cartridge_type(const ParsedCartridgeSlotCli& spec,
                                               std::span<const std::uint8_t> image,
                                               const SoftwareDb* db) {
    // Precedence step 1 (planner §2.4.1): an explicit --cartN-type VALUE.
    // No DB read, no heuristic, no SHA1 -- byte-for-byte today's behavior.
    if (spec.type_was_explicit) {
        CartridgeIdentification ident;
        ident.type = spec.type;
        ident.method = CartridgeIdentificationMethod::ExplicitFlag;
        return ident;
    }

    CartridgeIdentification ident;
    ident.sha1_hex = sha1_hex(image);

    // Precedence step 2: softwaredb SHA1 lookup.
    if (db != nullptr) {
        if (const SoftwareDbEntry* entry = db->lookup(ident.sha1_hex)) {
            ident.method = CartridgeIdentificationMethod::SoftwareDbSha1;
            ident.title = entry->title;
            ident.db_type_name = entry->type_name;
            if (const auto type = parse_cartridge_mapper_type(entry->type_name)) {
                ident.type = *type;
            } else {
                // Outside our implemented seven (incl. <start>-composed
                // subtypes and <megarom>-without-<type> ""): loud
                // identified-but-unsupported (planner §2.4.3).
                ident.unsupported = true;
            }
            return ident;
        }
    }

    // Precedence step 3: the heuristic (planner §2.3, re-derived from
    // RomFactory.cc:82-169).

    // §2.3 step 1 -- format signatures (checked BEFORE the size gates, as in
    // RomFactory.cc:90-95).
    if (image.size() >= kSignatureOffset + kSignatureSize) {
        for (const char* signature : kFormatSignatures) {
            if (std::memcmp(image.data() + kSignatureOffset, signature, kSignatureSize) == 0) {
                ident.method = CartridgeIdentificationMethod::HeuristicBankScan;
                ident.title = signature;  // keeps message B readable; no DB title exists
                ident.db_type_name = signature;
                ident.unsupported = true;
                return ident;
            }
        }
    }

    // §2.3 step 2 -- size < 0x10000: plain-ROM rule -> Mirrored. openMSX's
    // PAGE2 refinement for <= 16 KB "AB" images (RomFactory.cc:97-108) is a
    // DISCLOSED simplification here (planner §1.3): our Mirrored device
    // mirrors the image across the whole 64 KB window
    // (cartridge_mirrored_rom.h, ratified A-M19-8), so a PAGE2-class ROM's
    // content is present at 0x8000 anyway and the BIOS calls the header's
    // own INIT address. No PageNN enum value is added.
    //
    // DEC-0030 disagreement record #1 (eligibility threshold): openMSX scans
    // only at >= 64 KB (RomFactory.cc:96-117); fMSX scans anything > 32 KB
    // (MSX.c:3246). A 48 KB image: openMSX -> Mirrored, fMSX -> scan.
    // Software policy; adopted: openMSX (planner §2.3).
    if (image.size() < 0x10000) {
        ident.type = CartridgeMapperType::Mirrored;
        ident.method = CartridgeIdentificationMethod::SmallImagePlainRule;
        ident.detail = "image < 64 KB";
        return ident;
    }

    // §2.3 step 3 -- exactly 64 KB without an "AB" header: plain -> Mirrored
    // (RomFactory.cc:112-116).
    if (image.size() == 0x10000 && !(image[0] == 'A' && image[1] == 'B')) {
        ident.type = CartridgeMapperType::Mirrored;
        ident.method = CartridgeIdentificationMethod::SmallImagePlainRule;
        ident.detail = "image is 64 KB without an \"AB\" header";
        return ident;
    }

    // §2.3 step 4 -- the bank-select-write scan (64 KB with "AB", or bigger).
    //
    // DEC-0030 disagreement record #4 (an AGREEMENT, recorded as
    // corroboration): both references run their database lookup BEFORE this
    // heuristic (RomFactory.cc:180-201; MSX.c:2790-2837 -- CRC, then SHA,
    // then scan), independently corroborating this file's precedence design;
    // their DB FORMATS differ (CARTS.SHA/CARTS.CRC evaluated and skipped,
    // planner §2.2.5).
    CartridgeIdentification scan = run_bank_write_scan(image);
    scan.sha1_hex = std::move(ident.sha1_hex);
    return scan;
}

std::string format_cartridge_identification_message(const int slot_number,
                                                    const CartridgeIdentification& ident,
                                                    const std::string& db_path) {
    const std::string slot_flag = "--cart" + std::to_string(slot_number);

    switch (ident.method) {
        case CartridgeIdentificationMethod::ExplicitFlag:
            // The explicit path produces NO new output (planner §2.4.1).
            return "";

        case CartridgeIdentificationMethod::SoftwareDbSha1:
        case CartridgeIdentificationMethod::HeuristicBankScan:
            if (ident.unsupported) {
                // Message B (planner §2.4.4).
                const std::string source = (ident.method == CartridgeIdentificationMethod::SoftwareDbSha1)
                                               ? "softwaredb SHA1 match"
                                               : "ROM format signature";
                return "cartridge: " + slot_flag + ": identified as \"" + ident.title + "\" (" +
                       ident.db_type_name + ") via " + source + ", but mapper type \"" +
                       ident.db_type_name +
                       "\" is not implemented (implemented: Mirrored, 8kB, 16kB, ASCII8, ASCII16, "
                       "Konami, KonamiSCC); pass " +
                       slot_flag + "-type to force one. Aborting.";
            }
            if (ident.method == CartridgeIdentificationMethod::SoftwareDbSha1) {
                // Message A (planner §2.4.4).
                return "cartridge: " + slot_flag + ": identified as \"" + ident.title + "\" (" +
                       ident.db_type_name + ") via softwaredb SHA1 match [sha1=" + ident.sha1_hex +
                       ", db=" + db_path + "]";
            }
            // Message C (planner §2.4.4).
            return "cartridge: " + slot_flag + ": no softwaredb match [sha1=" + ident.sha1_hex +
                   "]; guessed " + std::string(to_string(ident.type)) +
                   " via bank-write heuristic (scores: " + ident.detail + "); pass " + slot_flag +
                   "-type to override";

        case CartridgeIdentificationMethod::SmallImagePlainRule:
            // Message D (planner §2.4.4).
            return "cartridge: " + slot_flag + ": no softwaredb match [sha1=" + ident.sha1_hex +
                   "]; " + ident.detail + " -> Mirrored (plain-ROM rule); pass " + slot_flag +
                   "-type to override";
    }
    return "";
}

std::string format_softwaredb_unavailable_message(const std::string& db_path, const bool was_explicit) {
    if (was_explicit) {
        return "cartridge: WARNING: --softwaredb \"" + db_path +
               "\" could not be read -- SHA1 identification disabled (heuristic only)";
    }
    return "cartridge: softwaredb not found at \"" + db_path +
           "\" -- SHA1 identification disabled (heuristic only); pass --softwaredb <path> to enable";
}

CartridgeIdentificationSession::CartridgeIdentificationSession(std::optional<std::string> softwaredb_path)
    : db_path_(softwaredb_path.has_value() ? *softwaredb_path : std::string(kDefaultSoftwareDbPath)),
      db_path_was_explicit_(softwaredb_path.has_value()) {}

CartridgeIdentificationSession::Resolution CartridgeIdentificationSession::resolve(
    const int slot_number, const ParsedCartridgeSlotCli& spec, std::span<const std::uint8_t> image) {
    Resolution resolution;

    // Explicit path: byte-for-byte today's behavior (no messages, no DB).
    if (spec.type_was_explicit) {
        resolution.type = spec.type;
        resolution.identification = resolve_cartridge_type(spec, image, nullptr);
        return resolution;
    }

    // Empty image: identification skipped -- the existing loud load-failure
    // path owns this case (planner §2.3 preamble). Pass the parser default
    // through unchanged.
    if (image.empty()) {
        resolution.type = spec.type;
        resolution.identification.type = spec.type;
        resolution.identification.method = CartridgeIdentificationMethod::ExplicitFlag;
        return resolution;
    }

    // Lazy, at-most-once DB load (planner §2.2.1).
    if (!db_load_attempted_) {
        db_load_attempted_ = true;
        std::vector<std::string> diagnostics;
        db_ = SoftwareDb::load_from_file(db_path_, diagnostics);
    }
    if (!db_.has_value() && !db_unavailable_message_emitted_) {
        db_unavailable_message_emitted_ = true;
        resolution.messages.push_back(
            format_softwaredb_unavailable_message(db_path_, db_path_was_explicit_));
    }

    resolution.identification =
        resolve_cartridge_type(spec, image, db_.has_value() ? &*db_ : nullptr);
    resolution.type = resolution.identification.type;
    resolution.ok = !resolution.identification.unsupported;

    const std::string report =
        format_cartridge_identification_message(slot_number, resolution.identification, db_path_);
    if (!report.empty()) {
        resolution.messages.push_back(report);
    }
    return resolution;
}

}  // namespace sony_msx::machine
