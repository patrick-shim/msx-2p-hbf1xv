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

#include "machine/xml_tokenizer.h"

#include <cctype>

namespace sony_msx::machine {

bool XmlTagScanner::is_name_char(const char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_' || c == '-' || c == ':' ||
           c == '.';
}

XmlToken XmlTagScanner::next(std::string* text_accum) {
    while (pos_ < text_.size()) {
        const std::size_t lt = text_.find('<', pos_);
        if (lt == std::string_view::npos) {
            if (text_accum != nullptr) {
                text_accum->append(text_, pos_, text_.size() - pos_);
            }
            pos_ = text_.size();
            return {XmlTokenKind::Eof, {}, {}};
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
        const std::size_t attr_begin = p;  // start of the attribute region
        char quote = '\0';
        bool self_close = false;
        std::size_t attr_end = text_.size();  // end of the attribute region (the '>')
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
                attr_end = p;
                ++p;
                break;
            }
            ++p;
        }
        pos_ = p;
        if (name.empty()) {
            continue;  // stray '<': tolerated, skipped
        }

        std::vector<XmlAttribute> attributes;
        if (capture_attributes_ && !closing && attr_end > attr_begin) {
            parse_attributes(attr_begin, attr_end, attributes);
        }

        if (closing) {
            return {XmlTokenKind::CloseTag, std::move(name), {}};
        }
        if (self_close) {
            return {XmlTokenKind::SelfCloseTag, std::move(name), std::move(attributes)};
        }
        return {XmlTokenKind::OpenTag, std::move(name), std::move(attributes)};
    }
    return {XmlTokenKind::Eof, {}, {}};
}

void XmlTagScanner::parse_attributes(std::size_t i, const std::size_t end,
                                     std::vector<XmlAttribute>& out) const {
    while (i < end) {
        // Skip anything that cannot begin an attribute name (whitespace, the
        // self-close '/', stray '='), so `<a  x="1" />` and `<a x='1'/>` parse.
        while (i < end && !is_name_char(text_[i])) {
            ++i;
        }
        if (i >= end) {
            break;
        }
        std::string attr_name;
        while (i < end && is_name_char(text_[i])) {
            attr_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text_[i]))));
            ++i;
        }
        while (i < end && std::isspace(static_cast<unsigned char>(text_[i])) != 0) {
            ++i;
        }
        std::string attr_value;
        if (i < end && text_[i] == '=') {
            ++i;
            while (i < end && std::isspace(static_cast<unsigned char>(text_[i])) != 0) {
                ++i;
            }
            if (i < end && (text_[i] == '"' || text_[i] == '\'')) {
                const char q = text_[i];
                ++i;
                const std::size_t value_start = i;
                while (i < end && text_[i] != q) {
                    ++i;
                }
                attr_value.assign(text_.substr(value_start, i - value_start));
                if (i < end) {
                    ++i;  // consume the closing quote
                }
            } else {
                // Unquoted value: up to the next whitespace or the self-close '/'.
                const std::size_t value_start = i;
                while (i < end && std::isspace(static_cast<unsigned char>(text_[i])) == 0 &&
                       text_[i] != '/') {
                    ++i;
                }
                attr_value.assign(text_.substr(value_start, i - value_start));
            }
        }
        out.push_back({std::move(attr_name), xml_decode_entities(attr_value)});
    }
}

std::string xml_trim(const std::string& s) {
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

std::string xml_decode_entities(const std::string& s) {
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

}  // namespace sony_msx::machine
