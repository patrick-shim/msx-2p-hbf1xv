#!/usr/bin/env python3
"""M24-S3 ZEXALL/ZEXDOC captured-output PASS/FAIL report parser (backlog C3).

Parses a captured-output log as written by `sony_msx_headless --cpm-run`
(the raw bytes the M24 CpmBdosHarness captured from a zexall.com/zexdoc.com
run's C=9/C=2 BDOS calls -- see src/machine/cpm_bdos_harness.h) into a
structured, human-readable per-group PASS/FAIL table.

LICENSE-ISOLATION NOTE (read before touching this file; cites
docs/m24-planner-package.md SS1.5): this parser recognizes only the
program's OWN, OBSERVED, BLACK-BOX RUNTIME OUTPUT -- the literal byte
sequences "  OK" and "  ERROR **** crc expected:" / " found:" that
zexall.com/zexdoc.com print while running, exactly as any CI harness
recognizes a third-party test runner's own "PASS"/"FAIL" or TAP "ok"/"not
ok" convention, or as this project's own established practice of capturing
and parsing raw openMSX Tcl session output (docs/m19-parity-trace-diff.md,
docs/m23-parity-trace-diff.md, etc.). This is fundamentally different from,
and far lower-risk than, transcribing zexall.z80/zexdoc.z80's OWN source
code or data tables (the 256-entry CRC table, the 67 test descriptors' byte
constants, the expected-CRC constants) into this project -- NONE of that is
reproduced here or anywhere under src/. This file contains zero lines
copied from zexall.z80/zexdoc.z80; it is placed in tools/ (Python,
non-shipped) for maximum insulation, per the planner package's explicit
developer handoff.

Parsing strategy (grounded in the exerciser's own OBSERVED output grammar,
independently confirmed by reading zexall.z80 SS183-192/1205-1210 this
cycle -- see the planner package's A-M24-4): every group's own printed name
is a FIXED-WIDTH 30-byte, '.'-padded field (the `tmsg` macro's own layout),
printed immediately before that group's PASS/FAIL marker, with NO gap
between the name field's end and the marker's start. This parser exploits
that fixed structural offset -- after skipping the one-time startup banner,
it reads each group's name as EXACTLY the next 30 bytes, then requires the
marker to start at that EXACT computed position (never searching for the
marker text anywhere else in the buffer). This defeats the R-M24-4 "decoy
substring" edge case (a group name that happens to itself contain "OK" or
"ERROR" somewhere inside it) by construction, since the parser never treats
a marker-like substring found *inside* a name field as a real verdict --
see run_self_check() below for a fixture that exercises exactly this case.
"""
import argparse
import sys

BANNER_TERMINATOR = b"\n\r"
NAME_FIELD_WIDTH = 30

OK_MARKER = b"  OK"
OK_TERMINATOR = b"\n\r"

ERROR_MARKER = b"  ERROR **** crc expected:"
FOUND_MARKER = b" found:"
ERROR_TERMINATOR = b"\n\r"
CRC_HEX_DIGITS = 8

EXPECTED_GROUP_COUNT = 67


class ParseError(Exception):
    pass


def parse_captured_output(data):
    """Parse a captured BDOS-output byte buffer into a list of per-group dicts.

    Each dict: {"index": int (1-based), "name": str, "verdict": "PASS"|"FAIL",
    "expected_crc": str|None, "found_crc": str|None}.

    Raises ParseError on a structurally malformed buffer (never silently
    returns a partial/guessed result).
    """
    banner_end = data.find(BANNER_TERMINATOR)
    if banner_end == -1:
        raise ParseError("no startup-banner terminator ('\\n\\r') found -- not a "
                          "recognizable zexall/zexdoc captured-output stream")
    pos = banner_end + len(BANNER_TERMINATOR)

    groups = []
    while True:
        name_start = pos
        name_end = name_start + NAME_FIELD_WIDTH
        if name_end > len(data):
            break  # no more complete group-name fields remain (trailing text, e.g. "Tests complete")
        name = data[name_start:name_end].decode("ascii", errors="replace")
        marker_pos = name_end

        if data[marker_pos:marker_pos + len(OK_MARKER)] == OK_MARKER:
            term_pos = marker_pos + len(OK_MARKER)
            if data[term_pos:term_pos + len(OK_TERMINATOR)] != OK_TERMINATOR:
                raise ParseError("malformed OK marker (missing LF,CR terminator) at offset {}"
                                  .format(marker_pos))
            groups.append({
                "index": len(groups) + 1,
                "name": name,
                "verdict": "PASS",
                "expected_crc": None,
                "found_crc": None,
            })
            pos = term_pos + len(OK_TERMINATOR)
            continue

        if data[marker_pos:marker_pos + len(ERROR_MARKER)] == ERROR_MARKER:
            expected_start = marker_pos + len(ERROR_MARKER)
            expected = data[expected_start:expected_start + CRC_HEX_DIGITS].decode("ascii", errors="replace")
            found_marker_pos = expected_start + CRC_HEX_DIGITS
            if data[found_marker_pos:found_marker_pos + len(FOUND_MARKER)] != FOUND_MARKER:
                raise ParseError("malformed ERROR marker (missing ' found:') at offset {}"
                                  .format(marker_pos))
            found_start = found_marker_pos + len(FOUND_MARKER)
            found = data[found_start:found_start + CRC_HEX_DIGITS].decode("ascii", errors="replace")
            term_pos = found_start + CRC_HEX_DIGITS
            if data[term_pos:term_pos + len(ERROR_TERMINATOR)] != ERROR_TERMINATOR:
                raise ParseError("malformed ERROR terminator (missing LF,CR) at offset {}".format(term_pos))
            groups.append({
                "index": len(groups) + 1,
                "name": name,
                "verdict": "FAIL",
                "expected_crc": expected,
                "found_crc": found,
            })
            pos = term_pos + len(ERROR_TERMINATOR)
            continue

        # No recognizable marker at the expected structural position -- stop.
        # This is the honest end-of-groups condition (trailing "Tests
        # complete" banner text), NOT a decoy-substring false match: a
        # substring appearing INSIDE the 30-byte name field itself is never
        # consulted here, only the exact byte range immediately following it.
        break

    return groups


def render_markdown(groups, source_label):
    lines = []
    lines.append("# ZEXALL/ZEXDOC Report: {}".format(source_label))
    lines.append("")
    lines.append("Groups parsed: {} (expected {})".format(len(groups), EXPECTED_GROUP_COUNT))
    fail_count = sum(1 for g in groups if g["verdict"] == "FAIL")
    lines.append("PASS: {}  FAIL: {}".format(len(groups) - fail_count, fail_count))
    lines.append("")
    lines.append("| # | Verdict | Name (observed output) | Expected CRC | Found CRC |")
    lines.append("|---|---------|-------------------------|--------------|-----------|")
    for g in groups:
        expected = g["expected_crc"] or ""
        found = g["found_crc"] or ""
        lines.append("| {} | {} | `{}` | {} | {} |".format(
            g["index"], g["verdict"], g["name"], expected, found))
    lines.append("")
    return "\n".join(lines) + "\n"


def check(name, condition, failures):
    status = "OK" if condition else "FAIL"
    print("  [{}] {}".format(status, name))
    if not condition:
        failures.append(name)


def run_self_check():
    """Hermetic self-check against a SYNTHETIC captured-output fixture (NOT
    real zexall/zexdoc output -- hand-built for this check only). Exercises
    the R-M24-4 decoy-substring edge case: a group name that itself contains
    "OK"/"ERROR" text, proving the parser anchors on structural position
    rather than a naive substring search. Returns 0 on success, 1 on failure.
    """
    failures = []

    banner = b"SYNTHETIC TEST BANNER\n\r"

    # Group 1: a 30-byte name field that itself contains the substring "OK"
    # (a decoy), followed by the REAL "  OK" marker. If the parser naively
    # searched the whole buffer for "OK", it could false-match inside the
    # name field; this fixture proves it does not. Padding is computed (not
    # hand-counted) so the field is exactly NAME_FIELD_WIDTH bytes, mirroring
    # the real tmsg macro's own '.'-pad-to-30 behavior.
    name1_core = b"decoy OK inside name"
    name1 = name1_core + b"." * (NAME_FIELD_WIDTH - len(name1_core))
    assert len(name1) == NAME_FIELD_WIDTH, "fixture bug: name1 must be exactly 30 bytes"
    group1 = name1 + b"  OK\n\r"

    # Group 2: a 30-byte name field that itself contains the substring
    # "ERROR" (a decoy), followed by a REAL error marker with a specific
    # expected/found CRC pair.
    name2_core = b"decoy ERROR inside name"
    name2 = name2_core + b"." * (NAME_FIELD_WIDTH - len(name2_core))
    assert len(name2) == NAME_FIELD_WIDTH, "fixture bug: name2 must be exactly 30 bytes"
    group2 = name2 + b"  ERROR **** crc expected:DEADBEEF found:CAFEBABE\n\r"

    trailer = b"Tests complete"

    data = banner + group1 + group2 + trailer

    print("Self-check: parsing a synthetic fixture with decoy OK/ERROR substrings inside group names")
    groups = parse_captured_output(data)

    check("exactly 2 groups parsed", len(groups) == 2, failures)
    if len(groups) == 2:
        check("group 1 verdict is PASS (not falsely short-circuited by the decoy 'OK')",
              groups[0]["verdict"] == "PASS", failures)
        check("group 1 name captured verbatim (including the decoy substring)",
              groups[0]["name"] == name1.decode("ascii"), failures)
        check("group 2 verdict is FAIL", groups[1]["verdict"] == "FAIL", failures)
        check("group 2 name captured verbatim (including the decoy substring)",
              groups[1]["name"] == name2.decode("ascii"), failures)
        check("group 2 expected CRC parsed correctly", groups[1]["expected_crc"] == "DEADBEEF", failures)
        check("group 2 found CRC parsed correctly", groups[1]["found_crc"] == "CAFEBABE", failures)

    # A buffer with no banner terminator is a parse ERROR, not a silent empty
    # result -- proves the tool never treats an unrecognizable/malformed log
    # as a vacuous pass.
    try:
        parse_captured_output(b"not a zexall-style log at all")
        check("missing-banner buffer raises ParseError", False, failures)
    except ParseError:
        check("missing-banner buffer raises ParseError", True, failures)

    # A group whose OK terminator is corrupted (missing LF,CR) is a parse
    # ERROR, not silently accepted.
    try:
        corrupted = banner + name1 + b"  OKXX"
        parse_captured_output(corrupted)
        check("corrupted OK terminator raises ParseError", False, failures)
    except ParseError:
        check("corrupted OK terminator raises ParseError", True, failures)

    if failures:
        print("SELF-CHECK FAILED: {}".format(", ".join(failures)))
        return 1
    print("SELF-CHECK PASSED")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captured_log", nargs="?", help="path to a --cpm-run captured-output log")
    parser.add_argument("-o", "--out", help="write the Markdown report to this path (default: stdout only)")
    parser.add_argument("--self-check", action="store_true", help="run the hermetic parser self-check")
    args = parser.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()

    if not args.captured_log:
        parser.error("captured_log is required (or use --self-check)")

    with open(args.captured_log, "rb") as handle:
        data = handle.read()

    try:
        groups = parse_captured_output(data)
    except ParseError as exc:
        sys.stderr.write("zexall-report: parse failure: {}\n".format(exc))
        return 2

    report = render_markdown(groups, args.captured_log)
    print(report)
    if args.out:
        with open(args.out, "w", encoding="ascii", newline="\n") as handle:
            handle.write(report)

    if len(groups) != EXPECTED_GROUP_COUNT:
        sys.stderr.write("zexall-report: WARNING: expected {} groups, parsed {}\n"
                          .format(EXPECTED_GROUP_COUNT, len(groups)))
        return 2

    fail_count = sum(1 for g in groups if g["verdict"] == "FAIL")
    return 1 if fail_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
