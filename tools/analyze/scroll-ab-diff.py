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

"""M38 Phase-A A/B image differ.

Takes OUR decoded active-display PNG (256x192 or 512x212, from
tools/convert/frame-to-png.py) and openMSX's RAW screenshot (320x240 for 256-wide
modes), crops the openMSX active area to align pixel-for-pixel with ours
(empirically verified offset (32,24) for 256-wide modes -- the 256x192 active
window centred in openMSX's 320x240 raw buffer), then:

  * builds a labelled side-by-side composite  [ OURS | openMSX | DIFF-heat ]
    (2x nearest-neighbour upscale) for unambiguous visual inspection, and
  * prints a deterministic similarity metric: mean per-channel distance and
    the fraction of pixels differing beyond a tolerance (default 40/channel,
    which absorbs the RGB555->RGB888 rounding difference between the two
    engines -- ~1.6/channel on a matching baseline -- while still flagging a
    real 1-pixel scroll shift, which moves a whole colour band).

The tolerance is NOT a fudge: on the aligned baseline the metric reads ~0%
mismatch; a genuine horizontal-scroll divergence lights up the band edges.

Usage:
  python tools/analyze/scroll-ab-diff.py --ours ours.png --omx omx_raw.png \
      --out composite.png [--ox 32 --oy 24] [--label "s02 coarse"]
"""
import argparse
import sys

from PIL import Image, ImageDraw


def crop_omx_active(omx, ours_size, ox, oy):
    w, h = ours_size
    return omx.crop((ox, oy, ox + w, oy + h))


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
            d = abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2)
            total += d
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


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ours", required=True)
    p.add_argument("--omx", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--ox", type=int, default=32)
    p.add_argument("--oy", type=int, default=24)
    p.add_argument("--tol", type=int, default=40)
    p.add_argument("--scale", type=int, default=2)
    p.add_argument("--label", default="")
    args = p.parse_args(argv[1:])

    ours = Image.open(args.ours).convert("RGB")
    omx = Image.open(args.omx).convert("RGB")
    omx_crop = crop_omx_active(omx, ours.size, args.ox, args.oy)

    mean_dist, pct = metrics(ours, omx_crop, args.tol)
    verdict = "MATCH" if pct < 0.5 else "MISMATCH"

    heat = diff_heat(ours, omx_crop)
    s = args.scale
    panels = [
        labelled(upscale(ours, s), "OURS"),
        labelled(upscale(omx_crop, s), "openMSX"),
        labelled(upscale(heat, s), "DIFF (red=delta)"),
    ]
    gap = 6
    W = sum(pn.size[0] for pn in panels) + gap * (len(panels) - 1)
    Hh = max(pn.size[1] for pn in panels) + 14
    comp = Image.new("RGB", (W, Hh), (48, 48, 48))
    x = 0
    for pn in panels:
        comp.paste(pn, (x, 14))
        x += pn.size[0] + gap
    d = ImageDraw.Draw(comp)
    d.text((2, 2), "%s  mean_dist=%.2f/ch  mismatch=%.2f%%  -> %s"
           % (args.label, mean_dist, pct, verdict), fill=(255, 255, 255))
    comp.save(args.out)

    print("M38DIFF label=%s mean_dist=%.3f pct_mismatch=%.3f verdict=%s out=%s"
          % (args.label, mean_dist, pct, verdict, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
