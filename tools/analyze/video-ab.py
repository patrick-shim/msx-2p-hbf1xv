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

"""M41-S2 geometry-aware VIDEO A/B differ.

Generalizes tools/analyze/scroll-ab-diff.py to the FOUR openMSX raw-screenshot geometry
classes empirically resolved in M41-S2 (closing assumption A-3,
docs/m41-production-qa-plan.md):

  * text240  : Text1 (SCREEN0, 40x24). ours 240x192; openMSX `screenshot -raw`
               = 320x240, active crop (41,24).
  * g256_192 : 256-wide @192 (SCREEN1/2/G3 char / sprite mode-1). ours 256x192;
               openMSX raw 320x240, crop (32,24)  [M38-proven].
  * g256_212 : 256-wide @212 (SCREEN5 G4 / SCREEN8 G7 / YJK / YJK+YAE). ours
               256x212; openMSX raw 320x240, crop (32,14).
  * g512_212 : 512-wide @212 (SCREEN6 G5 / SCREEN7 G6). openMSX `screenshot -raw`
               DOWN-samples 512->320 (measured: ~half the horizontal runs), so a
               512-exact A/B needs `screenshot -raw -doublesize` (640x480). ours
               512x212; crop (64,27) width 512 height 424, then take openMSX row
               2*r+1 for r in [0,212)  (line-doubled -> exact original line).

Metric is identical to m38-ab-diff (mean per-channel distance + pct of pixels
past a per-channel tolerance; tol 40 absorbs the RGB555->888 ~1.6/ch rounding).
Verdict MATCH iff pct_mismatch < 0.5%. A named-known-delta mode (YJK) is still
diffed the same way -- the caller interprets the measured pct against the
KD-YJK band; this tool only measures.

Usage:
  python tools/analyze/video-ab.py --ours ours.png --omx omx_raw.png \
      --geom g256_212 --out comp.png --label m5_g4
  python tools/analyze/video-ab.py --self-check
"""
import argparse
import sys

from PIL import Image, ImageDraw

# geom -> (ox, oy, doublesize, rowoff). doublesize=True means the omx image is
# the 640x480 -doublesize raw and rows are subsampled 2*r+rowoff.
GEOM = {
    "text240":  (41, 24, False, 0),
    "g256_192": (32, 24, False, 0),
    "g256_212": (32, 14, False, 0),
    "g512_212": (64, 27, True, 1),
}


def align_omx(ours, omx, geom):
    """Return an omx crop the same size as `ours`, per the geometry class."""
    w, h = ours.size
    ox, oy, doublesize, rowoff = GEOM[geom]
    if not doublesize:
        return omx.crop((ox, oy, ox + w, oy + h))
    # doublesize: crop 512 x (2*h) then take every-other row.
    src = omx.crop((ox, oy, ox + w, oy + 2 * h))
    out = Image.new("RGB", (w, h))
    sp = src.load()
    op = out.load()
    for r in range(h):
        sy = 2 * r + rowoff
        for x in range(w):
            op[x, r] = sp[x, sy][:3]
    return out


def metrics(ours, omx_crop, tol):
    w, h = ours.size
    ap = ours.load()
    bp = omx_crop.load()
    total = 0
    mismatch = 0
    n = w * h
    for y in range(h):
        for x in range(w):
            r1, g1, b1 = ap[x, y][:3]
            r2, g2, b2 = bp[x, y][:3]
            total += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2)
            if max(abs(r1 - r2), abs(g1 - g2), abs(b1 - b2)) > tol:
                mismatch += 1
    return total / (n * 3.0), 100.0 * mismatch / n


def diff_heat(ours, omx_crop):
    w, h = ours.size
    out = Image.new("RGB", (w, h))
    ap = ours.load()
    bp = omx_crop.load()
    op = out.load()
    for y in range(h):
        for x in range(w):
            r1, g1, b1 = ap[x, y][:3]
            r2, g2, b2 = bp[x, y][:3]
            d = min(255, (abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2)))
            op[x, y] = (d, 0, 0) if d > 0 else (0, 0, 0)
    return out


def upscale(img, factor):
    return img.resize((img.size[0] * factor, img.size[1] * factor), Image.NEAREST)


def labelled(img, text):
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, img.size[0] - 1, 11), fill=(0, 0, 0))
    d.text((2, 1), text, fill=(255, 255, 0))
    return img


def build(ours, omx_crop, mean_dist, pct, verdict, label, scale):
    heat = diff_heat(ours, omx_crop)
    panels = [
        labelled(upscale(ours, scale), "OURS"),
        labelled(upscale(omx_crop, scale), "openMSX"),
        labelled(upscale(heat, scale), "DIFF (red=delta)"),
    ]
    gap = 6
    W = sum(p.size[0] for p in panels) + gap * (len(panels) - 1)
    Hh = max(p.size[1] for p in panels) + 14
    comp = Image.new("RGB", (W, Hh), (48, 48, 48))
    x = 0
    for p in panels:
        comp.paste(p, (x, 14))
        x += p.size[0] + gap
    d = ImageDraw.Draw(comp)
    d.text((2, 2), "%s  mean_dist=%.2f/ch  mismatch=%.2f%%  -> %s"
           % (label, mean_dist, pct, verdict), fill=(255, 255, 255))
    return comp


def run_self_check():
    ok = True
    # synthetic 256x192 "ours" and a padded 320x240 "omx" where the active area
    # at (32,24) equals ours -> perfect MATCH; a 1px-shifted crop -> MISMATCH.
    w, h = 64, 48
    ours = Image.new("RGB", (w, h))
    for y in range(h):
        for x in range(w):
            ours.putpixel((x, y), ((x * 4) & 255, (y * 5) & 255, ((x + y) * 3) & 255))
    omx = Image.new("RGB", (320, 240), (0, 0, 0))
    omx.paste(ours, (32, 24))
    GEOM["_selftest"] = (32, 24, False, 0)
    aligned = align_omx(ours, omx, "_selftest")
    md, pct = metrics(ours, aligned, 40)
    print("  self MATCH: mean=%.3f pct=%.3f" % (md, pct))
    ok = ok and pct < 0.5
    GEOM["_selftest_shift"] = (30, 24, False, 0)
    aligned2 = align_omx(ours, omx, "_selftest_shift")
    md2, pct2 = metrics(ours, aligned2, 40)
    print("  shifted:    mean=%.3f pct=%.3f" % (md2, pct2))
    ok = ok and pct2 >= 0.5
    # doublesize round-trip: build a 640x480 line-doubled image from ours.
    ds = Image.new("RGB", (640, 480), (0, 0, 0))
    for r in range(h):
        for x in range(w):
            c = ours.getpixel((x, r))
            ds.putpixel((64 + x, 27 + 2 * r), c)      # even row
            ds.putpixel((64 + x, 27 + 2 * r + 1), c)  # odd row (rowoff=1 target)
    aligned3 = align_omx(ours, ds, "g512_212")
    md3, pct3 = metrics(ours, aligned3, 40)
    print("  ds-subsamp: mean=%.3f pct=%.3f" % (md3, pct3))
    ok = ok and pct3 < 0.5
    print("SELF-CHECK:", "OK" if ok else "FAILED")
    return 0 if ok else 1


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ours")
    p.add_argument("--omx")
    p.add_argument("--geom", choices=list(GEOM.keys()))
    p.add_argument("--out")
    p.add_argument("--tol", type=int, default=40)
    p.add_argument("--scale", type=int, default=2)
    p.add_argument("--label", default="")
    p.add_argument("--self-check", action="store_true")
    args = p.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()
    if not (args.ours and args.omx and args.geom and args.out):
        p.error("--ours --omx --geom --out required (or --self-check)")

    ours = Image.open(args.ours).convert("RGB")
    omx = Image.open(args.omx).convert("RGB")
    omx_crop = align_omx(ours, omx, args.geom)
    mean_dist, pct = metrics(ours, omx_crop, args.tol)
    verdict = "MATCH" if pct < 0.5 else "MISMATCH"
    comp = build(ours, omx_crop, mean_dist, pct, verdict, args.label, args.scale)
    comp.save(args.out)
    print("M41VDIFF label=%s geom=%s mean_dist=%.3f pct_mismatch=%.3f verdict=%s out=%s"
          % (args.label, args.geom, mean_dist, pct, verdict, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
