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

#include <iostream>
#include <string>
#include <vector>

#include "machine/xml_tokenizer.h"

// Suite: Machine_XmlTokenizer_Unit (M50-S1, docs/m50-planner-package.md §5).
//
// Two guarantees:
//   (1) REFACTOR EQUIVALENCE -- the field software_db consumes (kind + name +
//       the accumulated character-data text) is IDENTICAL whether or not
//       attribute capture is enabled. software_db constructs the scanner with
//       capture_attributes=false, so this proves the extraction is
//       behavior-preserving at the tokenizer level (the software_db unit test
//       is the higher-level guard; this pins the mechanism).
//   (2) ATTRIBUTE EXTRACTION -- the new opt-in capture parses name="value"
//       pairs (quote styles, entity-decoding, self-close, missing) correctly.

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Drive a scanner to EOF, recording a compact "kind:name" trace and the total
// accumulated text (exactly what the software_db state machine reads).
struct ScanResult {
    std::vector<std::string> trace;
    std::string text;
};

ScanResult scan_all(std::string_view doc, bool capture_attributes) {
    using namespace sony_msx::machine;
    ScanResult result;
    XmlTagScanner scanner(doc, capture_attributes);
    for (;;) {
        const XmlToken tok = scanner.next(&result.text);
        if (tok.kind == XmlTokenKind::Eof) {
            break;
        }
        std::string kind;
        switch (tok.kind) {
            case XmlTokenKind::OpenTag:
                kind = "open";
                break;
            case XmlTokenKind::CloseTag:
                kind = "close";
                break;
            case XmlTokenKind::SelfCloseTag:
                kind = "self";
                break;
            case XmlTokenKind::Eof:
                kind = "eof";
                break;
        }
        result.trace.push_back(kind + ":" + tok.name);
    }
    return result;
}

}  // namespace

int main() {
    using namespace sony_msx::machine;

    // A document exercising open/close/self-close, attributes, comments, CDATA,
    // DOCTYPE, entities, and character data -- the mixed shape software_db sees.
    const std::string doc =
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE root SYSTEM \"x.dtd\">\n"
        "<!-- a <comment> that must not tokenize -->\n"
        "<root a=\"1\" b='two'>\n"
        "  <leaf x=\"9\"/>\n"
        "  <title>Snake &amp; Ladder</title>\n"
        "  <![CDATA[ raw <not-a-tag> text ]]>\n"
        "</root>\n";

    // (1) Equivalence: capture on vs off yields identical kind:name trace + text.
    {
        const ScanResult off = scan_all(doc, /*capture_attributes=*/false);
        const ScanResult on = scan_all(doc, /*capture_attributes=*/true);
        expect(off.trace == on.trace, "CaptureOnOff_SameTokenTrace");
        expect(off.text == on.text, "CaptureOnOff_SameAccumulatedText");

        // Concrete trace oracle (comments/DOCTYPE/PI skipped; CDATA is text).
        const std::vector<std::string> expected = {
            "open:root", "self:leaf", "open:title", "close:title", "close:root"};
        expect(off.trace == expected, "TokenTrace_MatchesExpected");

        // Character DATA is accumulated RAW (entities NOT decoded here -- that
        // is the consumer's job; software_db calls xml_decode_entities on the
        // title itself). The CDATA payload is likewise passed through verbatim.
        expect(off.text.find("Snake &amp; Ladder") != std::string::npos, "Text_CharDataRaw");
        expect(off.text.find("raw <not-a-tag> text") != std::string::npos, "Text_CdataRawPreserved");
    }

    // (2) Attribute extraction (capture on): double- and single-quoted values,
    //     entity decoding, self-close, and a missing attribute -> nullptr.
    {
        XmlTagScanner scanner(doc, /*capture_attributes=*/true);
        // <root a="1" b='two'>
        XmlToken root = scanner.next(nullptr);
        expect(root.kind == XmlTokenKind::OpenTag && root.name == "root", "Attr_RootOpen");
        const std::string* a = root.attribute("a");
        const std::string* b = root.attribute("b");
        expect(a != nullptr && *a == "1", "Attr_DoubleQuoted");
        expect(b != nullptr && *b == "two", "Attr_SingleQuoted");
        expect(root.attribute("missing") == nullptr, "Attr_MissingReturnsNull");

        // <leaf x="9"/>
        XmlToken leaf = scanner.next(nullptr);
        expect(leaf.kind == XmlTokenKind::SelfCloseTag && leaf.name == "leaf", "Attr_LeafSelfClose");
        const std::string* x = leaf.attribute("x");
        expect(x != nullptr && *x == "9", "Attr_SelfCloseValue");
    }

    // (3) A close tag never carries attributes; a value containing an entity is
    //     decoded; a path-like value keeps its slashes/dots.
    {
        XmlTagScanner scanner("<n p=\"a/b.c\" q=\"x&amp;y\"></n>", /*capture_attributes=*/true);
        XmlToken open = scanner.next(nullptr);
        expect(open.attribute("p") != nullptr && *open.attribute("p") == "a/b.c", "Attr_PathValue");
        expect(open.attribute("q") != nullptr && *open.attribute("q") == "x&y", "Attr_ValueEntityDecoded");
        XmlToken close = scanner.next(nullptr);
        expect(close.kind == XmlTokenKind::CloseTag && close.attributes.empty(),
               "Attr_CloseTagHasNoAttributes");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_XmlTokenizer_Unit cases passed\n";
    return 0;
}
