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

#include <string>
#include <string_view>
#include <vector>

namespace sony_msx::machine {

// ---------------------------------------------------------------------------
// Shared tolerant XML tag tokenizer. This is the exact scanner that lived
// file-local in software_db.cpp (the openMSX softwaredb subset reader); it is
// hoisted here VERBATIM so BOTH software_db.cpp and emulator_config.cpp share
// one implementation with no third-party XML dependency.
//
// It produces open/close/self-close tags and accumulates intervening character
// data (incl. CDATA content); comments, DOCTYPE and processing instructions are
// skipped wholesale. Never throws. Element names are lowercased.
//
// Attribute extraction is OPT-IN (`capture_attributes`). The
// softwaredb schema is element-TEXT based and never read attributes, so it
// constructs the scanner with capture_attributes=false -> the pos_/name/kind
// tokenization is byte-for-byte identical to the pre-extraction code (the
// behavior-preserving guarantee, guarded by the software_db unit tests). The
// config schema is ATTRIBUTE based, so emulator_config.cpp opts in with
// capture_attributes=true; the extra work only runs on that path.
// ---------------------------------------------------------------------------

enum class XmlTokenKind { OpenTag, CloseTag, SelfCloseTag, Eof };

// One name="value" attribute. `name` is lowercased (matching the element-name
// convention); `value` is entity-decoded, case PRESERVED (paths/enums are
// case-sensitive). Populated only when the scanner is capture_attributes=true.
struct XmlAttribute {
    std::string name;
    std::string value;
};

struct XmlToken {
    XmlTokenKind kind = XmlTokenKind::Eof;
    std::string name;  // lowercased element name (both schemas are lowercase)
    std::vector<XmlAttribute> attributes;

    // Returns the value of the named attribute, or nullptr when absent.
    [[nodiscard]] const std::string* attribute(std::string_view attr_name) const {
        for (const XmlAttribute& a : attributes) {
            if (a.name == attr_name) {
                return &a.value;
            }
        }
        return nullptr;
    }
};

class XmlTagScanner {
public:
    explicit XmlTagScanner(std::string_view text, bool capture_attributes = false)
        : text_(text), capture_attributes_(capture_attributes) {}

    // Advances to the next tag token. Character data encountered on the way
    // (including CDATA payloads) is APPENDED to `text_accum` when non-null.
    XmlToken next(std::string* text_accum);

private:
    [[nodiscard]] bool starts_with(std::string_view prefix) const {
        return text_.compare(pos_, prefix.size(), prefix) == 0;
    }

    static bool is_name_char(char c);

    // Parses `name="value"` / `name='value'` (and bare-value) pairs out of the
    // tag's attribute region [begin, end) into `out`. Quote-aware; tolerant of
    // stray whitespace and the self-close '/'.
    void parse_attributes(std::size_t begin, std::size_t end, std::vector<XmlAttribute>& out) const;

    std::string_view text_;
    bool capture_attributes_ = false;
    std::size_t pos_ = 0;
};

// The two shared text helpers, hoisted from software_db.cpp verbatim.
std::string xml_trim(const std::string& s);

// The five predefined XML entities (&amp; &lt; &gt; &quot; &apos;); anything
// unrecognized is left verbatim.
std::string xml_decode_entities(const std::string& s);

}  // namespace sony_msx::machine
