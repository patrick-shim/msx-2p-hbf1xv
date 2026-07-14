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

#include "machine/software_db.h"

#include <cctype>
#include <fstream>
#include <iterator>

#include "machine/xml_tokenizer.h"

namespace sony_msx::machine {

namespace {

// The tolerant XML tokenizer (TagScanner) + xml_trim()/xml_decode_entities() that used
// to live here file-local were extracted to machine/xml_tokenizer.{h,cpp} in
// M50-S1 (docs/m50-planner-package.md §5) so software_db and the new
// emulator_config parser share ONE reader. This file now consumes the shared
// XmlTagScanner / xml_trim() / xml_decode_entities() with capture_attributes
// left OFF (the default) -- the softwaredb schema is element-TEXT based, so the
// tokenization is byte-for-byte identical to the pre-extraction code (the
// software_db unit tests are the behavior-preserving guard, AC-S1-4).

bool is_valid_sha1_hex(const std::string& s) {
    if (s.size() != 40) {
        return false;
    }
    for (const char c : s) {
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// One <rom>/<megarom> payload under a <dump>.
struct PendingDump {
    std::string element_name;  // "rom" or "megarom" (close-tag matching)
    std::string type;          // raw; "<rom>" pre-defaults to "Mirrored"
    std::string hash;
    std::string start;
    bool has_payload = false;  // a <rom>/<megarom> element was actually seen
};

// RomDatabase.cc:460-464 semantics: <start> composes ONLY for a 6-char
// "0x...."-shaped value, and RomDatabase.cc:500-515 composes it ONLY onto
// the base names "Mirrored" and "Normal" -- everything else ignores <start>.
std::string compose_type_name(const std::string& type, const std::string& start) {
    if (start.size() == 6 && start.compare(0, 2, "0x") == 0 && (type == "Mirrored" || type == "Normal")) {
        return type + start.substr(2);
    }
    return type;
}

}  // namespace

std::optional<SoftwareDb> SoftwareDb::load_from_file(const std::string& path,
                                                     std::vector<std::string>& diagnostics) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    SoftwareDb db;
    XmlTagScanner scanner(content);

    // Flat state machine over the schema subset (mirrors the SHAPE of the
    // reference's DBParser states -- re-derived, never copied).
    enum class State { Top, SoftwareDb_, Software, Title, Dump, RomElem, TypeElem, HashElem, StartElem };
    State state = State::Top;
    int unknown_level = 0;

    std::string title;
    std::vector<PendingDump> dumps;
    std::string text_accum;

    // On a malformed region: record one diagnostic, drop the current
    // <software>, and resync at the next </software> (planner §2.2.1's
    // skip-to-next-<software> recovery rule).
    auto resync_after_malformed = [&](const std::string& why) {
        diagnostics.push_back("software_db: malformed entry skipped (" + why + ")");
        for (;;) {
            const XmlToken t = scanner.next(nullptr);
            if (t.kind == XmlTokenKind::Eof) {
                return false;
            }
            if (t.kind == XmlTokenKind::CloseTag && t.name == "software") {
                return true;
            }
        }
    };

    auto commit_software = [&]() {
        for (const PendingDump& dump : dumps) {
            if (dump.hash.empty()) {
                diagnostics.push_back("software_db: dump without <hash> dropped (title: " + title + ")");
                continue;
            }
            const std::string key = to_lower(dump.hash);
            if (!is_valid_sha1_hex(key)) {
                diagnostics.push_back("software_db: dump with non-SHA1 <hash> '" + dump.hash +
                                      "' dropped (title: " + title + ")");
                continue;
            }
            // First occurrence wins (duplicate silently ignored -- the
            // deterministic keep-the-old rule).
            db.entries_.emplace(key, SoftwareDbEntry{title, compose_type_name(dump.type, dump.start)});
        }
        title.clear();
        dumps.clear();
    };

    bool done = false;
    while (!done) {
        const bool capture_text =
            (state == State::Title || state == State::TypeElem || state == State::HashElem ||
             state == State::StartElem) &&
            unknown_level == 0;
        const XmlToken token = scanner.next(capture_text ? &text_accum : nullptr);

        if (token.kind == XmlTokenKind::Eof) {
            break;
        }
        if (unknown_level > 0) {
            if (token.kind == XmlTokenKind::OpenTag) {
                ++unknown_level;
            } else if (token.kind == XmlTokenKind::CloseTag) {
                --unknown_level;
            }
            continue;
        }
        if (token.kind == XmlTokenKind::SelfCloseTag) {
            continue;  // no state change; tolerated everywhere
        }

        switch (state) {
            case State::Top:
                if (token.kind == XmlTokenKind::OpenTag && token.name == "softwaredb") {
                    state = State::SoftwareDb_;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::SoftwareDb_:
                if (token.kind == XmlTokenKind::OpenTag && token.name == "software") {
                    title.clear();
                    dumps.clear();
                    state = State::Software;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                } else if (token.kind == XmlTokenKind::CloseTag && token.name == "softwaredb") {
                    state = State::Top;
                }
                break;

            case State::Software:
                if (token.kind == XmlTokenKind::OpenTag && token.name == "title") {
                    text_accum.clear();
                    state = State::Title;
                } else if (token.kind == XmlTokenKind::OpenTag && token.name == "dump") {
                    dumps.emplace_back();
                    state = State::Dump;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;  // <genmsxid>/<company>/<year>/<country>/<system>/...
                } else if (token.kind == XmlTokenKind::CloseTag && token.name == "software") {
                    commit_software();
                    state = State::SoftwareDb_;
                } else if (token.kind == XmlTokenKind::CloseTag) {
                    if (!resync_after_malformed("unexpected </" + token.name + "> inside <software>")) {
                        done = true;
                    }
                    title.clear();
                    dumps.clear();
                    state = State::SoftwareDb_;
                }
                break;

            case State::Title:
                if (token.kind == XmlTokenKind::CloseTag && token.name == "title") {
                    title = xml_decode_entities(xml_trim(text_accum));
                    text_accum.clear();
                    state = State::Software;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::Dump:
                if (token.kind == XmlTokenKind::OpenTag && (token.name == "rom" || token.name == "megarom")) {
                    PendingDump& dump = dumps.back();
                    dump.element_name = token.name;
                    dump.has_payload = true;
                    // <rom> defaults to "Mirrored"; <megarom> has NO default.
                    dump.type = (token.name == "rom") ? "Mirrored" : "";
                    dump.start.clear();
                    state = State::RomElem;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;  // <original>...
                } else if (token.kind == XmlTokenKind::CloseTag && token.name == "dump") {
                    state = State::Software;
                } else if (token.kind == XmlTokenKind::CloseTag) {
                    if (!resync_after_malformed("unexpected </" + token.name + "> inside <dump>")) {
                        done = true;
                    }
                    title.clear();
                    dumps.clear();
                    state = State::SoftwareDb_;
                }
                break;

            case State::RomElem:
                if (token.kind == XmlTokenKind::OpenTag && token.name == "type") {
                    text_accum.clear();
                    state = State::TypeElem;
                } else if (token.kind == XmlTokenKind::OpenTag && token.name == "hash") {
                    text_accum.clear();
                    state = State::HashElem;
                } else if (token.kind == XmlTokenKind::OpenTag && token.name == "start") {
                    text_accum.clear();
                    state = State::StartElem;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;  // <remark>...
                } else if (token.kind == XmlTokenKind::CloseTag && token.name == dumps.back().element_name) {
                    state = State::Dump;
                } else if (token.kind == XmlTokenKind::CloseTag) {
                    if (!resync_after_malformed("unexpected </" + token.name + "> inside <" +
                                                dumps.back().element_name + ">")) {
                        done = true;
                    }
                    title.clear();
                    dumps.clear();
                    state = State::SoftwareDb_;
                }
                break;

            case State::TypeElem:
                if (token.kind == XmlTokenKind::CloseTag && token.name == "type") {
                    dumps.back().type = xml_trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::HashElem:
                if (token.kind == XmlTokenKind::CloseTag && token.name == "hash") {
                    dumps.back().hash = xml_trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::StartElem:
                if (token.kind == XmlTokenKind::CloseTag && token.name == "start") {
                    dumps.back().start = xml_trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == XmlTokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;
        }
    }

    return db;
}

const SoftwareDbEntry* SoftwareDb::lookup(std::string_view sha1_hex_lowercase) const {
    const auto it = entries_.find(std::string(sha1_hex_lowercase));
    return (it == entries_.end()) ? nullptr : &it->second;
}

}  // namespace sony_msx::machine
