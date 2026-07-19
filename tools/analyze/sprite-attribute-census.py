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

"""M51 (DEC-0078) SAT census — guard prong 1 of docs/m51-planner-package.md
section 1.6, plus the SAT-anchored oracle box source (T0.2).

Reads a directory of --snapshot outputs (HBF1XV-SNAPSHOT v1:
<root>/snapshot/f<NNNNNN>_c<...>/{vdp.txt,vram.txt}) and, per frame:

  * locates the PLAYER sprite entries in the live mode-2 SAT by their pinned
    pattern-number signature (per-title profile below);
  * reports whether the player is present in the SAT (alive in GAME state),
    its display-line band (via the R#23 inverse: dtop = (y - R23 + 1) & 0xFF),
    and an oracle bounding box (band +/- margin at the player's x);
  * counts sprites-per-display-line over the player band (mode-2 hardware
    draws max 8/line, tier-1 fact-sheet section 6 line 117): max_line_sprites
    <= 8 on an affected line means any wholesale absence is a DEFECT, never
    authentic multiplex dropping.

Output CSV columns:
  frame,in_sat,x,dtop0,dtop1,box_x0,box_y0,box_x1,box_y1,max_line_sprites,r23

The CSV feeds tools/analyze/sprite-oracle.py --boxes-csv so the presence box
FOLLOWS the game's own sprite placement (needed for the sprite-scroll title, whose player
drifts vertically with the scroll while its SAT x stays 120).

PINNED PROFILES (derived from verified snapshots on current main @ 0fd9690):
  scroll-shooter      : player plane = pattern pair {56,60}, SAT x in [110,140],
             one 16x16 pair (band = 16 lines).
  sprite-scroll-title : player = pattern pairs {0,4} + {8,12}, SAT x in [110,130],
             two vertically stacked 16x16 pairs (band = 32 lines).
"""

import argparse
import csv
import glob
import os
import re
import sys

PROFILES = {
    "scroll-shooter": {
        "pats": [{56, 60}],          # one stacked segment: pattern pair
        "x_range": (110, 140),
        "box_margin_x": (10, 13),    # box = [x - mx0, x + 16 + mx1)
        "box_margin_y": (5, 6),
    },
    "sprite-scroll-title": {
        "pats": [{0, 4}, {8, 12}],   # two stacked segments (upper+lower body)
        "x_range": (110, 130),
        "box_margin_x": (2, 4),
        "box_margin_y": (2, 3),
    },
}


def load_vram(path):
    data = bytearray(128 * 1024)
    prev_off = None
    prev_row = None
    star = False
    in_region = False
    for ln in open(path):
        ln = ln.rstrip("\n")
        if ln.startswith("[VRAM"):
            in_region = True
            continue
        if not in_region:
            continue
        if ln.startswith("["):
            break
        s = ln.strip()
        if not s:
            continue
        if s == "*":
            star = True
            continue
        toks = ln.split()
        try:
            off = int(toks[0], 16)
        except ValueError:
            continue
        row = [int(t, 16) for t in toks[1:17]]
        if star and prev_off is not None:
            fill = prev_off + 16
            while fill < off:
                data[fill:fill + 16] = prev_row
                fill += 16
            star = False
        data[off:off + len(row)] = bytes(row)
        prev_off = off
        prev_row = bytes(row)
    return data


def read_regs(path):
    regs = {}
    for ln in open(path):
        if ln.startswith("R") and "=" in ln:
            for tok in ln.split():
                if tok.startswith("R") and "=" in tok:
                    name, val = tok.split("=")
                    try:
                        regs[int(name[1:], 16)] = int(val, 16)
                    except ValueError:
                        pass
    return regs


def census_one(snapdir, prof):
    regs = read_regs(os.path.join(snapdir, "vdp.txt"))
    vram = load_vram(os.path.join(snapdir, "vram.txt"))
    sat = ((regs[11] & 0x03) << 15) | ((regs[5] & 0xFC) << 7)
    r23 = regs.get(0x17, 0)
    r1 = regs.get(1, 0)
    mag = (r1 & 0x01) != 0
    size16 = (r1 & 0x02) != 0
    span = ((16 if size16 else 8) * (2 if mag else 1))

    entries = []  # live SAT entries up to the mode-2 sentinel 216
    for i in range(32):
        y, x, p, _ = vram[sat + i * 4:sat + i * 4 + 4]
        if y == 216:
            break
        entries.append((y, x, p))

    # player segments by pinned pattern signature + x-range
    found = []  # (dtop, x) per stacked segment
    for pats in prof["pats"]:
        seg = None
        for (y, x, p) in entries:
            if p in pats and prof["x_range"][0] <= x <= prof["x_range"][1]:
                seg = ((y - r23 + 1) & 0xFF, x)
                break
        found.append(seg)

    in_sat = all(s is not None for s in found)
    if not in_sat:
        return {"in_sat": 0, "r23": r23}

    dtops = [s[0] for s in found]
    x = found[0][1]
    band0 = min(dtops)
    band1 = max(dtops) + span - 1  # inclusive last display line of the band

    # prong-1: sprites per display line over the player band (live SAT,
    # entries before the sentinel; parked entries with off-screen dtop simply
    # never cover a display line in-band).
    max_line = 0
    for line in range(band0, band1 + 1):
        n = 0
        for (y, xx, p) in entries:
            if ((line - 1 + r23 - y) & 0xFF) < span:
                n += 1
        max_line = max(max_line, n)

    mx0, mx1 = prof["box_margin_x"]
    my0, my1 = prof["box_margin_y"]
    return {
        "in_sat": 1,
        "x": x,
        "dtop0": band0,
        "dtop1": max(dtops),
        "box": (x - mx0, max(0, band0 - my0), x + 16 + mx1, min(212, band1 + 1 + my1)),
        "max_line_sprites": max_line,
        "r23": r23,
    }


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--snapshot-root", required=True,
                    help="directory containing snapshot/f*_c*/ subdirs")
    ap.add_argument("--profile", choices=sorted(PROFILES.keys()), required=True)
    ap.add_argument("--csv", required=True, help="output CSV path")
    args = ap.parse_args(argv[1:])

    prof = PROFILES[args.profile]
    dirs = sorted(glob.glob(os.path.join(args.snapshot_root, "snapshot", "f*_c*")))
    if not dirs:
        print("m51-sat-census: no snapshots under %s" % args.snapshot_root, file=sys.stderr)
        return 2

    rows = []
    n_in = 0
    max_line_overall = 0
    for d in dirs:
        m = re.match(r"f(\d+)_c", os.path.basename(d))
        if not m:
            continue
        frame = int(m.group(1))
        r = census_one(d, prof)
        if r["in_sat"]:
            n_in += 1
            b = r["box"]
            max_line_overall = max(max_line_overall, r["max_line_sprites"])
            rows.append([frame, 1, r["x"], r["dtop0"], r["dtop1"],
                         b[0], b[1], b[2], b[3], r["max_line_sprites"], r["r23"]])
        else:
            rows.append([frame, 0, "", "", "", "", "", "", "", "", r["r23"]])

    os.makedirs(os.path.dirname(args.csv) or ".", exist_ok=True)
    with open(args.csv, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["frame", "in_sat", "x", "dtop0", "dtop1",
                    "box_x0", "box_y0", "box_x1", "box_y1",
                    "max_line_sprites", "r23"])
        w.writerows(rows)

    print("M51-SAT-CENSUS profile=%s frames=%d in_sat=%d not_in_sat=%d "
          "max_line_sprites=%d (mode-2 hardware limit 8) csv=%s"
          % (args.profile, len(rows), n_in, len(rows) - n_in,
             max_line_overall, args.csv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
