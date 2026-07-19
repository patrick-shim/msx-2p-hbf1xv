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

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "machine/sha1.h"
#include "machine/software_db.h"

// Suite: Machine_SoftwareDb_Unit (M30-S2, backlog G2, docs/m30-planner-
// package.md §2.2/§4-S2)
//
// Tolerant subset-parser semantics against FULLY SYNTHETIC fixtures written
// to a temp dir at test runtime. Every title/type below is FICTIONAL and
// every hash is computed by OUR OWN sha1 (machine/sha1.h) over tiny
// synthetic images -- co-validating S1+S2 -- or is an obviously synthetic
// constant. NO content from the real softwaredb.xml appears anywhere in
// this file (planner §2.2.4 license posture; QA greps for this).
//
// Deterministic oracle: identical fixture bytes -> identical entry map,
// every run (pure parse, no clock/host state).

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::string write_fixture(const std::string& filename, const std::string& content) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sony_msx_softwaredb_fixtures";
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / filename;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path.string();
}

std::string sha1_of_byte_fill(const std::uint8_t fill, const std::size_t size) {
    const std::vector<std::uint8_t> image(size, fill);
    return sony_msx::machine::sha1_hex(image);
}

}  // namespace

int main() {
    using sony_msx::machine::SoftwareDb;

    // Synthetic hashes: OUR sha1 over tiny synthetic images (S1+S2
    // co-validation -- these are hashes of byte-fill patterns, not of any
    // real software).
    const std::string hash_a = sha1_of_byte_fill(0xA5, 16);      // fictional "Zorblax"
    const std::string hash_b = sha1_of_byte_fill(0x5A, 32);      // fictional "Quantum Badger"
    const std::string hash_c = sha1_of_byte_fill(0x3C, 64);      // fictional "Moon Ferret 2"
    const std::string hash_d = sha1_of_byte_fill(0xC3, 128);     // fictional start-composed dump
    const std::string hash_e = sha1_of_byte_fill(0x0F, 256);     // fictional megarom-without-type
    const std::string hash_dup = sha1_of_byte_fill(0xF0, 512);   // duplicate-hash case

    // --- The main synthetic fixture: exercises defaults, megarom types,
    //     hashless drop, start composition, CDATA/comments/DOCTYPE/attrs/
    //     unknown tags, and duplicate-hash first-wins. ---
    const std::string fixture =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE softwaredb SYSTEM \"softwaredb1.dtd\">\n"
        "<softwaredb timestamp=\"1234567890\">\n"
        "<!-- a comment mentioning <software> tags that must NOT confuse the scanner -->\n"
        "<![CDATA[\n"
        "Synthetic credits block. Not real database content.\n"
        "]]>\n"
        "<software>\n"
        "  <title>Zorblax &amp; the Space Weasels</title>\n"
        "  <madeupid>9999</madeupid>\n"
        "  <system>MSX2</system>\n"
        "  <company>Fictional Softworks</company>\n"
        "  <year>1999</year>\n"
        "  <country>ZZ</country>\n"
        // <rom> WITHOUT <type> -> defaults to Mirrored.
        "  <dump><rom><hash>" + hash_a + "</hash></rom></dump>\n"
        // <megarom> WITH <type> -> that type, raw.
        "  <dump><original value=\"true\">Author</original><megarom><type>KonamiSCC</type><hash>" +
        hash_b + "</hash><remark>synthetic</remark></megarom></dump>\n"
        // hashless dump -> dropped.
        "  <dump><rom><type>Mirrored</type></rom></dump>\n"
        "</software>\n"
        "<software>\n"
        "  <title>Moon Ferret 2</title>\n"
        "  <dump><megarom><type>ASCII16</type><hash>" + hash_c + "</hash></megarom></dump>\n"
        // <start> composes the RAW subtype name Mirrored4000 (never truncated
        // back to plain Mirrored).
        "  <dump><rom><start>0x4000</start><hash>" + hash_d + "</hash></rom></dump>\n"
        // <megarom> WITHOUT <type>: NO default -- raw type stays "".
        "  <dump><megarom><hash>" + hash_e + "</hash></megarom></dump>\n"
        "</software>\n"
        "<software>\n"
        "  <title>First Claimant</title>\n"
        "  <dump><rom><type>Mirrored</type><hash>" + hash_dup + "</hash></rom></dump>\n"
        "</software>\n"
        "<software>\n"
        "  <title>Second Claimant</title>\n"
        // duplicate hash: FIRST occurrence wins.
        "  <dump><megarom><type>ASCII8</type><hash>" + hash_dup + "</hash></megarom></dump>\n"
        "</software>\n"
        "</softwaredb>\n";

    {
        const std::string path = write_fixture("main_fixture.xml", fixture);
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(path, diagnostics);
        expect(db.has_value(), "MainFixture_Loads");
        if (db.has_value()) {
            // 7 hashed dumps, one duplicate collapsed, one hashless dropped
            // -> 6 entries.
            expect(db->entry_count() == 6, "MainFixture_EntryCount_HashlessDroppedDuplicateCollapsed");

            const auto* a = db->lookup(hash_a);
            expect(a != nullptr && a->type_name == "Mirrored",
                   "RomWithoutType_DefaultsToMirrored");
            expect(a != nullptr && a->title == "Zorblax & the Space Weasels",
                   "Title_EntityDecoded_Amp");

            const auto* b = db->lookup(hash_b);
            expect(b != nullptr && b->type_name == "KonamiSCC" &&
                       b->title == "Zorblax & the Space Weasels",
                   "MegaromWithType_RawTypeRecorded");

            const auto* c = db->lookup(hash_c);
            expect(c != nullptr && c->type_name == "ASCII16" && c->title == "Moon Ferret 2",
                   "SecondSoftware_MegaromType_Recorded");

            const auto* d = db->lookup(hash_d);
            expect(d != nullptr && d->type_name == "Mirrored4000",
                   "StartValue_ComposesRawSubtypeName_NeverTruncated");

            const auto* e = db->lookup(hash_e);
            expect(e != nullptr && e->type_name.empty(),
                   "MegaromWithoutType_NoDefault_RawEmptyTypePreserved");

            const auto* dup = db->lookup(hash_dup);
            expect(dup != nullptr && dup->title == "First Claimant" && dup->type_name == "Mirrored",
                   "DuplicateHash_FirstOccurrenceWins");

            // The hashless dump produced a (non-fatal) diagnostic.
            bool saw_hashless_note = false;
            for (const std::string& diag : diagnostics) {
                if (diag.find("without <hash>") != std::string::npos) {
                    saw_hashless_note = true;
                }
            }
            expect(saw_hashless_note, "HashlessDump_DroppedWithDiagnostic");
        }
    }

    // --- Malformed entry: skipped with a diagnostic; later entries still
    //     parsed (skip-to-next-<software> recovery, planner §2.2.1). ---
    {
        const std::string malformed =
            "<softwaredb>\n"
            "<software>\n"
            "  <title>Broken Entry</title>\n"
            "  <dump><rom><type>Mirrored</type></unexpected></rom></dump>\n"
            "</software>\n"
            "<software>\n"
            "  <title>Survivor</title>\n"
            "  <dump><rom><hash>" + hash_a + "</hash></rom></dump>\n"
            "</software>\n"
            "</softwaredb>\n";
        const std::string path = write_fixture("malformed_fixture.xml", malformed);
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(path, diagnostics);
        expect(db.has_value(), "MalformedFixture_StillLoads_NeverFatal");
        if (db.has_value()) {
            expect(db->entry_count() == 1, "MalformedEntry_Skipped_SurvivorParsed_Count");
            const auto* survivor = db->lookup(hash_a);
            expect(survivor != nullptr && survivor->title == "Survivor",
                   "MalformedEntry_Skipped_SurvivorParsed_Lookup");
            expect(!diagnostics.empty(), "MalformedEntry_DiagnosticCollected");
        }
    }

    // --- Uppercase hash in the file: normalized to the lowercase key. ---
    {
        std::string upper = hash_b;
        for (char& ch : upper) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        const std::string mixed_case =
            "<softwaredb>\n"
            "<software><title>Case Test</title>\n"
            "  <dump><rom><hash>" + upper + "</hash></rom></dump>\n"
            "</software>\n"
            "</softwaredb>\n";
        const std::string path = write_fixture("case_fixture.xml", mixed_case);
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(path, diagnostics);
        expect(db.has_value() && db->lookup(hash_b) != nullptr,
               "UppercaseHashInFile_LookupByLowercaseKey_Hits");
    }

    // --- Invalid (non-40-hex) hash: dropped with a diagnostic. ---
    {
        const std::string bad_hash =
            "<softwaredb>\n"
            "<software><title>Bad Hash</title>\n"
            "  <dump><rom><hash>not-a-sha1</hash></rom></dump>\n"
            "</software>\n"
            "</softwaredb>\n";
        const std::string path = write_fixture("bad_hash_fixture.xml", bad_hash);
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(path, diagnostics);
        expect(db.has_value() && db->entry_count() == 0, "InvalidHash_Dropped");
        expect(!diagnostics.empty(), "InvalidHash_DiagnosticCollected");
    }

    // --- Absent file -> std::nullopt (graceful degradation). ---
    {
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(
            "this/path/genuinely/does/not/exist/softwaredb.xml", diagnostics);
        expect(!db.has_value(), "AbsentFile_ReturnsNullopt");
    }

    // --- Lookup miss returns nullptr. ---
    {
        const std::string path = write_fixture("miss_fixture.xml",
                                               "<softwaredb><software><title>Only One</title>"
                                               "<dump><rom><hash>" + hash_a + "</hash></rom></dump>"
                                               "</software></softwaredb>");
        std::vector<std::string> diagnostics;
        const auto db = SoftwareDb::load_from_file(path, diagnostics);
        expect(db.has_value() && db->lookup(hash_b) == nullptr, "LookupMiss_ReturnsNullptr");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_SoftwareDb_Unit cases passed\n";
    return 0;
}
