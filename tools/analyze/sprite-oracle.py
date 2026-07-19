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

"""M51 (DEC-0078) Task 0 sprite-presence/flicker oracle
(docs/m51-planner-package.md §3 T0.2).

Given a sequence of frame PNGs (ours: tools/convert/frame-to-png.py output, 1:1
active-area pixels; openMSX: `screenshot -raw` 320x240 border canvas, mapped
via --ox/--oy), a pinned bounding box and a pinned sprite-colour signature,
emit per-frame presence (count + fraction of box pixels matching the
signature), classify present/absent by a fixed pixel-count threshold, and
report: absent-frame count, longest absent run, present<->absent transition
count. Output: CSV + a machine-parsable verdict line.

The SAME oracle (same box/signature constants, offset-shifted) processes the
openMSX discriminator leg (T0.4) so authentic sprite-multiplex flicker is
measured by the identical instrument (A-M51-2).

PINNED PROFILES (constants derived once from verified present-frames on
current main @ 0fd9690, per §3 T0.2 "recorded as constants in the tool"):

  scroll-shooter      : player plane at spawn, gameplay window f2900+ (recipe
             tools/input-scripts/play-evidence.script). Verified present-frame
             f2900: 93 pure-white fuselage pixels in x[120,133] y[153,168];
             background (forest greens/black) contains ZERO white. Box is
             padded for sprite bob; signature = white (r,g,b >= 200 each);
             present threshold = 20 px.
  sprite-scroll-title : profile constants derived in Task 0 discovery (see
             docs/m51-implementation-report.md).

Verdict rule (fixed): FAIL if absent_frames > --max-absent (default 0).
Transitions are REPORTED for the openMSX-rate comparison (AC-F1) but only
gate when --max-transitions is given.
"""

import argparse
import csv
import glob
import os
import re
import sys

from PIL import Image

PROFILES = {
    # box = (x0, y0, x1, y1) exclusive hi, in OUR active-area coordinates.
    # sig  = ("white",) -> r,g,b >= 200 each
    #        ("rgb", (R,G,B), tol) -> |r-R|<=tol and |g-G|<=tol and |b-B|<=tol
    #        ("rgbset", {(R,G,B), ...}) -> exact 8-bit match against a pinned
    #            set (RGB555 sprite colours bit-replicated 5->8, the exact
    #            tools/convert/frame-to-png.py expansion -- lossless, so exact match holds)
    "scroll-shooter": {
        "box": (110, 145, 146, 190),
        "sig": ("white",),
        "min_pixels": 20,
    },
    # Sprite-scroll title: player character (two stacked 16x16
    # pairs, pats {0,4}+{8,12}, SAT x always 120 in the idle window; drifts
    # vertically with scroll) -- box comes per-frame from --boxes-csv
    # (tools/analyze/sprite-attribute-census.py). Signature pinned from verified present
    # frames f1800/f2000/f2200/f2400 on main @ 0fd9690: body dark-red
    # RGB555 0x5880 -> (181,33,0), bright-red 0x7D24 -> (255,74,33), skin
    # 0x7ED2 -> (255,181,148); counts 84-125 when present.
    "sprite-scroll-title": {
        "box": (110, 40, 144, 160),  # static fallback; --boxes-csv preferred
        "sig": ("rgbset", {(181, 33, 0), (255, 74, 33), (255, 181, 148)}),
        "min_pixels": 20,
    },
    # Sprite-scroll-title WIDE static box (validated on main captures: absent frames
    # score 0 -- no pumpkin/vine contamination inside the centre path column
    # x[110,144) y[30,170) in the f2000-f2039 scene window). Used where the
    # SAT census is unavailable (openMSX leg + bisect legs).
    "sprite-scroll-title-wide": {
        "box": (110, 30, 144, 170),
        "sig": ("rgbset", {(181, 33, 0), (255, 74, 33), (255, 181, 148)}),
        "min_pixels": 20,
    },
    # The SAME wide box against openMSX 19.1 `screenshot -raw` output
    # (--ox 32 --oy 14): openMSX's own 3-bit->8-bit palette expansion of the
    # IDENTICAL game colours, sampled from the openMSX leg's own verified
    # present-frame omx_f02000.png: body (186,39,0), bright (255,82,39),
    # skin (255,186,155).
    "sprite-scroll-title-omx": {
        "box": (110, 30, 144, 170),
        "sig": ("rgbset", {(186, 39, 0), (255, 82, 39), (255, 186, 155)}),
        "min_pixels": 20,
    },
    # Split-screen title (disk): player ship, gameplay window (SHOULD leg).
    "split-screen-title": {
        "box": (96, 80, 192, 176),
        "sig": ("white",),
        "min_pixels": 12,
    },
}


def match_pixel(sig, rgb):
    r, g, b = rgb
    if sig[0] == "white":
        return r >= 200 and g >= 200 and b >= 200
    if sig[0] == "rgb":
        (R, G, B), tol = sig[1], sig[2]
        return abs(r - R) <= tol and abs(g - G) <= tol and abs(b - B) <= tol
    if sig[0] == "rgbset":
        return (r, g, b) in sig[1]
    raise ValueError("unknown signature kind: %r" % (sig[0],))


def load_boxes_csv(path):
    """tools/analyze/sprite-attribute-census.py output -> {frame: box or None}.

    in_sat=0 rows map to None: the game itself does not place the player
    sprite that frame (e.g. a genuine in-game death), so the frame carries NO
    render expectation and is EXCLUDED from absent/transition statistics --
    the guard that kept the f3380-f3419 scroll-shooter death window from being
    misread as the defect (package section 1.6 prong 1)."""
    import csv as _csv
    boxes = {}
    with open(path, newline="") as fh:
        for row in _csv.DictReader(fh):
            frame = int(row["frame"])
            if row["in_sat"] == "1":
                boxes[frame] = (int(row["box_x0"]), int(row["box_y0"]),
                                int(row["box_x1"]), int(row["box_y1"]))
            else:
                boxes[frame] = None
    return boxes


def analyze(path, box, sig, ox, oy):
    img = Image.open(path).convert("RGB")
    x0, y0, x1, y1 = box
    x0 += ox
    x1 += ox
    y0 += oy
    y1 += oy
    w, h = img.size
    x1 = min(x1, w)
    y1 = min(y1, h)
    count = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            if match_pixel(sig, img.getpixel((x, y))):
                count += 1
    area = max(1, (x1 - x0) * (y1 - y0))
    return count, count / area


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", help="directory of frame PNGs (sorted by name)")
    ap.add_argument("--files", nargs="*", help="explicit PNG list (sorted order preserved)")
    ap.add_argument("--profile", choices=sorted(PROFILES.keys()), required=True)
    ap.add_argument("--box", nargs=4, type=int, metavar=("X0", "Y0", "X1", "Y1"),
                    help="override the profile box (our active-area coords)")
    ap.add_argument("--min-pixels", type=int, help="override the profile presence threshold")
    ap.add_argument("--ox", type=int, default=0,
                    help="x offset of the active area inside the image (openMSX raw: 32)")
    ap.add_argument("--oy", type=int, default=0,
                    help="y offset of the active area inside the image (openMSX raw: 14)")
    ap.add_argument("--boxes-csv",
                    help="per-frame SAT-anchored boxes from tools/analyze/sprite-attribute-census.py"
                         " (frames with in_sat=0 are excluded from the"
                         " absent/transition statistics)")
    ap.add_argument("--csv", help="write per-frame CSV here")
    ap.add_argument("--max-absent", type=int, default=0,
                    help="verdict FAILS if absent frames exceed this (default 0)")
    ap.add_argument("--max-transitions", type=int, default=None,
                    help="optionally also gate on transition count")
    ap.add_argument("--label", default="m51", help="verdict label")
    args = ap.parse_args(argv[1:])

    prof = PROFILES[args.profile]
    box = tuple(args.box) if args.box else prof["box"]
    sig = prof["sig"]
    min_pixels = args.min_pixels if args.min_pixels is not None else prof["min_pixels"]

    if args.dir:
        files = sorted(glob.glob(os.path.join(args.dir, "*.png")))
    elif args.files:
        files = list(args.files)
    else:
        ap.error("--dir or --files required")
    if not files:
        print("m51-oracle: no input PNGs found", file=sys.stderr)
        return 2

    boxes = load_boxes_csv(args.boxes_csv) if args.boxes_csv else None

    rows = []
    prev_present = None
    absent = 0
    transitions = 0
    longest_absent_run = 0
    cur_absent_run = 0
    no_sat = 0
    for f in files:
        fbox = box
        if boxes is not None:
            m = re.search(r"f(\d+)", os.path.basename(f))
            frame = int(m.group(1)) if m else None
            if frame not in boxes:
                continue  # frame outside the census window
            if boxes[frame] is None:
                no_sat += 1
                rows.append((os.path.basename(f), "", "", "no-sat"))
                continue  # no render expectation (player absent in GAME state)
            fbox = boxes[frame]
        count, frac = analyze(f, fbox, sig, args.ox, args.oy)
        present = 1 if count >= min_pixels else 0
        if present == 0:
            absent += 1
            cur_absent_run += 1
            longest_absent_run = max(longest_absent_run, cur_absent_run)
        else:
            cur_absent_run = 0
        if prev_present is not None and present != prev_present:
            transitions += 1
        prev_present = present
        rows.append((os.path.basename(f), count, "%.4f" % frac, present))

    if args.csv:
        os.makedirs(os.path.dirname(args.csv) or ".", exist_ok=True)
        with open(args.csv, "w", newline="") as fh:
            wcsv = csv.writer(fh)
            wcsv.writerow(["frame", "match_pixels", "fraction", "present"])
            wcsv.writerows(rows)

    for r in rows:
        print("  %s match=%s frac=%s present=%s" % r)

    n = len(rows)
    scored = n - no_sat
    fail = absent > args.max_absent
    if args.max_transitions is not None and transitions > args.max_transitions:
        fail = True
    verdict = "FAIL" if fail else "PASS"
    print("M51-ORACLE label=%s profile=%s frames=%d scored=%d no_sat=%d "
          "present=%d absent=%d longest_absent_run=%d transitions=%d "
          "min_pixels=%d box=%s verdict=%s"
          % (args.label, args.profile, n, scored, no_sat, scored - absent,
             absent, longest_absent_run, transitions, min_pixels,
             "sat-census" if boxes is not None else list(box), verdict))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
