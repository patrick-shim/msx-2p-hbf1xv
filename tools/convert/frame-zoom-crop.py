#!/usr/bin/env python3
# Local diagnostic helper (NOT part of src/): crop a row band from a
# HBF1XV-FRAME-DUMP v1 file and scale it up (nearest) into a PNG, with an
# optional 1px row-boundary grid so exact scanlines are countable by eye.
# Reuses tools/convert/frame-to-png.py's parser + RGB decode (imported, never copied).
import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location(
    "frame_to_png", os.path.join(os.path.dirname(os.path.abspath(__file__)), "frame-to-png.py"))
f2p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(f2p)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--y0", type=int, default=0)
    ap.add_argument("--y1", type=int, default=20)
    ap.add_argument("--x0", type=int, default=0)
    ap.add_argument("--x1", type=int, default=256)
    ap.add_argument("--zoom", type=int, default=8)
    ap.add_argument("--grid", action="store_true", help="draw a dark row-boundary grid line every source row")
    args = ap.parse_args(argv[1:])

    with open(args.input, "r", encoding="ascii", errors="replace") as h:
        text = h.read()
    w, hgt, border, pix = f2p.parse_frame_dump(text)
    z = args.zoom
    cx0, cx1 = max(0, args.x0), min(w, args.x1)
    cy0, cy1 = max(0, args.y0), min(hgt, args.y1)
    cw, ch = cx1 - cx0, cy1 - cy0
    out_w, out_h = cw * z, ch * z

    raw = bytearray()
    for oy in range(out_h):
        raw.append(0)
        sy = cy0 + oy // z
        row_grid = args.grid and (oy % z == 0)  # top edge of each source row
        for ox in range(out_w):
            sx = cx0 + ox // z
            i = (sy * w + sx) * 2
            pixel = pix[i] | (pix[i + 1] << 8)
            r8, g8, b8 = f2p.rgb555_to_rgb888(pixel)
            if row_grid:
                r8, g8, b8 = 255, 0, 255  # magenta boundary line at each source-row top
            raw.append(r8); raw.append(g8); raw.append(b8)

    ihdr = struct.pack(">IIBBBBB", out_w, out_h, 8, 2, 0, 0, 0)
    png = (f2p.PNG_SIGNATURE + f2p._chunk(b"IHDR", ihdr) +
           f2p._chunk(b"IDAT", f2p._zlib_stored(bytes(raw))) + f2p._chunk(b"IEND", b""))
    with open(args.out, "wb") as h:
        h.write(png)
    sys.stderr.write("zoom-crop: src rows [{},{}) cols [{},{}) zoom {} -> {}x{} {}\n".format(
        cy0, cy1, cx0, cx1, z, out_w, out_h, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
