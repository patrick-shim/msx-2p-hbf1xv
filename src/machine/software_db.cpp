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

namespace sony_msx::machine {

namespace {

// ---------------------------------------------------------------------------
// Tolerant tag tokenizer. Produces open/close/self-close tags and accumulates
// intervening character data (incl. CDATA content); comments, DOCTYPE and
// processing instructions are skipped wholesale. Never throws.
// ---------------------------------------------------------------------------

enum class TokenKind { OpenTag, CloseTag, SelfCloseTag, Eof };

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string name;  // lowercased element name (this schema is lowercase)
};

class TagScanner {
public:
    explicit TagScanner(std::string_view text) : text_(text) {}

    // Advances to the next tag token. Character data encountered on the way
    // (including CDATA payloads) is APPENDED to `text_accum` when non-null.
    Token next(std::string* text_accum) {
        while (pos_ < text_.size()) {
            const std::size_t lt = text_.find('<', pos_);
            if (lt == std::string_view::npos) {
                if (text_accum != nullptr) {
                    text_accum->append(text_, pos_, text_.size() - pos_);
                }
                pos_ = text_.size();
                return {TokenKind::Eof, {}};
            }
            if (text_accum != nullptr && lt > pos_) {
                text_accum->append(text_, pos_, lt - pos_);
            }
            pos_ = lt;

            if (starts_with("<!--")) {
                const std::size_t end = text_.find("-->", pos_ + 4);
                pos_ = (end == std::string_view::npos) ? text_.size() : end + 3;
                continue;
            }
            if (starts_with("<![CDATA[")) {
                const std::size_t end = text_.find("]]>", pos_ + 9);
                const std::size_t content_end = (end == std::string_view::npos) ? text_.size() : end;
                if (text_accum != nullptr) {
                    text_accum->append(text_, pos_ + 9, content_end - (pos_ + 9));
                }
                pos_ = (end == std::string_view::npos) ? text_.size() : end + 3;
                continue;
            }
            if (starts_with("<!") || starts_with("<?")) {
                // DOCTYPE / processing instruction: skip to the closing '>'.
                const std::size_t end = text_.find('>', pos_ + 2);
                pos_ = (end == std::string_view::npos) ? text_.size() : end + 1;
                continue;
            }

            // A real tag. Parse `</name ...>` / `<name ...>` / `<name .../>`,
            // honoring quoted attribute values.
            std::size_t p = pos_ + 1;
            bool closing = false;
            if (p < text_.size() && text_[p] == '/') {
                closing = true;
                ++p;
            }
            std::string name;
            while (p < text_.size() && is_name_char(text_[p])) {
                name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text_[p]))));
                ++p;
            }
            char quote = '\0';
            bool self_close = false;
            while (p < text_.size()) {
                const char ch = text_[p];
                if (quote != '\0') {
                    if (ch == quote) {
                        quote = '\0';
                    }
                } else if (ch == '"' || ch == '\'') {
                    quote = ch;
                } else if (ch == '>') {
                    self_close = (!closing && p > pos_ + 1 && text_[p - 1] == '/');
                    ++p;
                    break;
                }
                ++p;
            }
            pos_ = p;
            if (name.empty()) {
                continue;  // stray '<': tolerated, skipped
            }
            if (closing) {
                return {TokenKind::CloseTag, std::move(name)};
            }
            if (self_close) {
                return {TokenKind::SelfCloseTag, std::move(name)};
            }
            return {TokenKind::OpenTag, std::move(name)};
        }
        return {TokenKind::Eof, {}};
    }

private:
    [[nodiscard]] bool starts_with(std::string_view prefix) const {
        return text_.compare(pos_, prefix.size(), prefix) == 0;
    }

    static bool is_name_char(const char c) {
        return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_' || c == '-' || c == ':' ||
               c == '.';
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// The five predefined XML entities (display fidelity for titles like
// "Snake &amp; Ladder"); anything unrecognized is left verbatim.
std::string decode_entities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) {
                out.push_back('&');
                i += 5;
                continue;
            }
            if (s.compare(i, 4, "&lt;") == 0) {
                out.push_back('<');
                i += 4;
                continue;
            }
            if (s.compare(i, 4, "&gt;") == 0) {
                out.push_back('>');
                i += 4;
                continue;
            }
            if (s.compare(i, 6, "&quot;") == 0) {
                out.push_back('"');
                i += 6;
                continue;
            }
            if (s.compare(i, 6, "&apos;") == 0) {
                out.push_back('\'');
                i += 6;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

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
    TagScanner scanner(content);

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
            const Token t = scanner.next(nullptr);
            if (t.kind == TokenKind::Eof) {
                return false;
            }
            if (t.kind == TokenKind::CloseTag && t.name == "software") {
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
        const Token token = scanner.next(capture_text ? &text_accum : nullptr);

        if (token.kind == TokenKind::Eof) {
            break;
        }
        if (unknown_level > 0) {
            if (token.kind == TokenKind::OpenTag) {
                ++unknown_level;
            } else if (token.kind == TokenKind::CloseTag) {
                --unknown_level;
            }
            continue;
        }
        if (token.kind == TokenKind::SelfCloseTag) {
            continue;  // no state change; tolerated everywhere
        }

        switch (state) {
            case State::Top:
                if (token.kind == TokenKind::OpenTag && token.name == "softwaredb") {
                    state = State::SoftwareDb_;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::SoftwareDb_:
                if (token.kind == TokenKind::OpenTag && token.name == "software") {
                    title.clear();
                    dumps.clear();
                    state = State::Software;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;
                } else if (token.kind == TokenKind::CloseTag && token.name == "softwaredb") {
                    state = State::Top;
                }
                break;

            case State::Software:
                if (token.kind == TokenKind::OpenTag && token.name == "title") {
                    text_accum.clear();
                    state = State::Title;
                } else if (token.kind == TokenKind::OpenTag && token.name == "dump") {
                    dumps.emplace_back();
                    state = State::Dump;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;  // <genmsxid>/<company>/<year>/<country>/<system>/...
                } else if (token.kind == TokenKind::CloseTag && token.name == "software") {
                    commit_software();
                    state = State::SoftwareDb_;
                } else if (token.kind == TokenKind::CloseTag) {
                    if (!resync_after_malformed("unexpected </" + token.name + "> inside <software>")) {
                        done = true;
                    }
                    title.clear();
                    dumps.clear();
                    state = State::SoftwareDb_;
                }
                break;

            case State::Title:
                if (token.kind == TokenKind::CloseTag && token.name == "title") {
                    title = decode_entities(trim(text_accum));
                    text_accum.clear();
                    state = State::Software;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::Dump:
                if (token.kind == TokenKind::OpenTag && (token.name == "rom" || token.name == "megarom")) {
                    PendingDump& dump = dumps.back();
                    dump.element_name = token.name;
                    dump.has_payload = true;
                    // <rom> defaults to "Mirrored"; <megarom> has NO default.
                    dump.type = (token.name == "rom") ? "Mirrored" : "";
                    dump.start.clear();
                    state = State::RomElem;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;  // <original>...
                } else if (token.kind == TokenKind::CloseTag && token.name == "dump") {
                    state = State::Software;
                } else if (token.kind == TokenKind::CloseTag) {
                    if (!resync_after_malformed("unexpected </" + token.name + "> inside <dump>")) {
                        done = true;
                    }
                    title.clear();
                    dumps.clear();
                    state = State::SoftwareDb_;
                }
                break;

            case State::RomElem:
                if (token.kind == TokenKind::OpenTag && token.name == "type") {
                    text_accum.clear();
                    state = State::TypeElem;
                } else if (token.kind == TokenKind::OpenTag && token.name == "hash") {
                    text_accum.clear();
                    state = State::HashElem;
                } else if (token.kind == TokenKind::OpenTag && token.name == "start") {
                    text_accum.clear();
                    state = State::StartElem;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;  // <remark>...
                } else if (token.kind == TokenKind::CloseTag && token.name == dumps.back().element_name) {
                    state = State::Dump;
                } else if (token.kind == TokenKind::CloseTag) {
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
                if (token.kind == TokenKind::CloseTag && token.name == "type") {
                    dumps.back().type = trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::HashElem:
                if (token.kind == TokenKind::CloseTag && token.name == "hash") {
                    dumps.back().hash = trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == TokenKind::OpenTag) {
                    ++unknown_level;
                }
                break;

            case State::StartElem:
                if (token.kind == TokenKind::CloseTag && token.name == "start") {
                    dumps.back().start = trim(text_accum);
                    text_accum.clear();
                    state = State::RomElem;
                } else if (token.kind == TokenKind::OpenTag) {
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
