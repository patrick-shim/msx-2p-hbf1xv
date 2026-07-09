#!/usr/bin/env python3
# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================

"""Deterministic per-instruction trace-diff for the M10-S4 openMSX parity harness.

Compares this emulator's CpuTraceSink trace (trace A) against the openMSX-driven
per-instruction trace (trace B, "OMTRACE" key=value lines emitted by
tools/openmsx-trace-parity.ps1).

Comparison contract (matches docs/m10-planner-package.md Section 4):
  - ARCHITECTURAL fields are the R5 pass gate: PC, opcode bytes, and every
    register/flag: A, F (+decomposed bits), B, C, D, E, H, L, AF, BC, DE, HL,
    AF', BC', DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM.
  - T-state / cycle fields (T=, TC=) are NOT part of the architectural gate.
    openMSX inserts MSX M1 wait-states (planner DP-4) so exact cross-emulator
    cycle parity is structurally out of M10 scope; those columns are reported
    separately, never used to fail R5.

Exit codes: 0 = architectural parity (empty diff); 1 = architectural divergence;
2 = harness/parse failure (traces not comparable).
"""
import sys

ARCH_FIELDS = [
    "pc", "op",
    "af", "bc", "de", "hl",
    "afs", "bcs", "des", "hls",
    "ix", "iy", "sp",
    "i", "r",
    "iff1", "iff2", "im",
]


def parse_trace_a(path):
    """Parse the CpuTraceSink text format into a list of per-instruction dicts."""
    records = []
    with open(path, "r", encoding="ascii") as handle:
        for raw in handle:
            line = raw.strip()
            if not line:
                continue
            kv = {}
            for tok in line.split(" "):
                if "=" not in tok:
                    continue
                key, val = tok.split("=", 1)
                kv[key] = val
            f_hex = kv["F"].split("[", 1)[0]
            rec = {
                "seq": int(kv["SEQ"], 16),
                "pc": int(kv["PC"], 16),
                "op": kv["OP"].upper(),
                "af": int(kv["AF"], 16),
                "bc": int(kv["BC"], 16),
                "de": int(kv["DE"], 16),
                "hl": int(kv["HL"], 16),
                "afs": int(kv["AF'"], 16),
                "bcs": int(kv["BC'"], 16),
                "des": int(kv["DE'"], 16),
                "hls": int(kv["HL'"], 16),
                "ix": int(kv["IX"], 16),
                "iy": int(kv["IY"], 16),
                "sp": int(kv["SP"], 16),
                "i": int(kv["I"], 16),
                "r": int(kv["R"], 16),
                "iff1": int(kv["IFF1"]),
                "iff2": int(kv["IFF2"]),
                "im": int(kv["IM"]),
                "_f": int(f_hex, 16),
                "_t": kv.get("T"),
                "_tc": kv.get("TC"),
            }
            records.append(rec)
    return records


def parse_trace_b(path):
    """Parse openMSX OMTRACE key=value lines into per-instruction dicts."""
    records = []
    with open(path, "r", encoding="ascii", errors="replace") as handle:
        for raw in handle:
            line = raw.strip()
            if "OMTRACE" not in line:
                continue
            line = line[line.index("OMTRACE") + len("OMTRACE"):].strip()
            kv = {}
            for tok in line.split(" "):
                if "=" not in tok:
                    continue
                key, val = tok.split("=", 1)
                kv[key] = val
            af = int(kv["af"], 16)
            iff = int(kv["iff"])
            rec = {
                "seq": int(kv["seq"]),
                "pc": int(kv["pc"], 16),
                "op": kv["op"].upper(),
                "af": af,
                "bc": int(kv["bc"], 16),
                "de": int(kv["de"], 16),
                "hl": int(kv["hl"], 16),
                "afs": int(kv["af2"], 16),
                "bcs": int(kv["bc2"], 16),
                "des": int(kv["de2"], 16),
                "hls": int(kv["hl2"], 16),
                "ix": int(kv["ix"], 16),
                "iy": int(kv["iy"], 16),
                "sp": int(kv["sp"], 16),
                "i": int(kv["i"], 16),
                "r": int(kv["r"], 16),
                "iff1": iff & 1,
                "iff2": (iff >> 1) & 1,
                "im": int(kv["im"]),
                "_f": af & 0xFF,
            }
            records.append(rec)
    return records


def fmt(field, value):
    widths = {"pc": 4, "af": 4, "bc": 4, "de": 4, "hl": 4, "afs": 4, "bcs": 4,
              "des": 4, "hls": 4, "ix": 4, "iy": 4, "sp": 4, "i": 2, "r": 2}
    if field == "op":
        return value
    if field in ("iff1", "iff2", "im"):
        return str(value)
    return format(value, "0{}X".format(widths.get(field, 2)))


def main(argv):
    if len(argv) < 4:
        sys.stderr.write("usage: trace-diff.py <trace_a> <trace_b> <out.md>\n")
        return 2
    path_a, path_b, out_path = argv[1], argv[2], argv[3]

    try:
        a = parse_trace_a(path_a)
        b = parse_trace_b(path_b)
    except Exception as exc:  # noqa: BLE001 - report parse failure honestly
        sys.stderr.write("trace-diff: parse failure: {}\n".format(exc))
        return 2

    lines = []
    lines.append("# M10-S4 openMSX Per-Instruction Parity Trace Diff")
    lines.append("")
    lines.append("- Subject A (emulator): `{}`".format(path_a))
    lines.append("- Reference B (openMSX 19.1 / WSL): `{}`".format(path_b))
    lines.append("- Emulator instructions (A): {}".format(len(a)))
    lines.append("- openMSX instructions (B): {}".format(len(b)))
    lines.append("")

    if not a or not b:
        lines.append("## Result: BLOCKED -- a trace side is empty (not comparable)")
        lines.append("")
        lines.append("Trace A rows: {}, Trace B rows: {}. No architectural parity "
                     "asserted.".format(len(a), len(b)))
        _write(out_path, lines)
        return 2

    n = min(len(a), len(b))
    mismatches = []
    for i in range(n):
        ra, rb = a[i], b[i]
        for field in ARCH_FIELDS:
            if ra[field] != rb[field]:
                mismatches.append((i, field, fmt(field, ra[field]), fmt(field, rb[field])))
        if ra["_f"] != rb["_f"]:
            mismatches.append((i, "F", format(ra["_f"], "02X"), format(rb["_f"], "02X")))

    length_mismatch = len(a) != len(b)

    if not mismatches and not length_mismatch:
        lines.append("## Result: ARCHITECTURAL PARITY -- EMPTY DIFF")
        lines.append("")
        lines.append("All {} instructions match on every architectural field "
                     "(PC, opcode, A, F + all flag bits, B, C, D, E, H, L, AF, BC, "
                     "DE, HL, AF', BC', DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM).".format(n))
    else:
        lines.append("## Result: ARCHITECTURAL DIVERGENCE -- {} field mismatch(es)"
                     .format(len(mismatches)))
        lines.append("")
        if length_mismatch:
            lines.append("- Trace length mismatch: A={} B={}".format(len(a), len(b)))
            lines.append("")
        lines.append("| seq | A pc | field | A value | B value |")
        lines.append("|-----|------|-------|---------|---------|")
        for (i, field, va, vb) in mismatches[:200]:
            pc = format(a[i]["pc"], "04X")
            lines.append("| {} | {} | {} | {} | {} |".format(i, pc, field, va, vb))

    lines.append("")
    lines.append("## Cycle / T-state field (reported separately -- NOT part of R5 gate)")
    lines.append("")
    lines.append("Per planner Section 4 / DP-4: openMSX inserts MSX M1 wait-states while "
                 "this emulator's M9 core uses canonical Z80 T-states, so exact "
                 "cross-emulator cycle parity is structurally out of M10 scope. This "
                 "harness does not read openMSX's per-instruction T-state counter; the "
                 "emulator-side canonical Z80 T-states are recorded in trace A (T=/TC= "
                 "columns). Final emulator cumulative T-states: {}."
                 .format(a[-1].get("_tc")))
    lines.append("")

    _write(out_path, lines)

    if length_mismatch or mismatches:
        return 1
    return 0


def _write(path, lines):
    with open(path, "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
