#!/usr/bin/env python3
# Local diagnostic (NOT src/): render display rows of a HBF1XV-FRAME-DUMP as
# ASCII, marking pixels that differ from the border/backdrop color, so the
# exact scanline where glyphs/content start & stop is countable.
import os, sys, importlib.util
spec = importlib.util.spec_from_file_location(
    "frame_to_png", os.path.join(os.path.dirname(os.path.abspath(__file__)), "frame-to-png.py"))
f2p = importlib.util.module_from_spec(spec); spec.loader.exec_module(f2p)

def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--y0", type=int, default=0)
    ap.add_argument("--y1", type=int, default=20)
    ap.add_argument("--x0", type=int, default=0)
    ap.add_argument("--x1", type=int, default=128)
    args = ap.parse_args(argv[1:])
    with open(args.input, "r", encoding="ascii", errors="replace") as h:
        text = h.read()
    w, hgt, border, pix = f2p.parse_frame_dump(text)
    def px(x, y):
        i = (y * w + x) * 2
        return pix[i] | (pix[i+1] << 8)
    print("frame {}x{} border={:04X}".format(w, hgt, border))
    for y in range(max(0, args.y0), min(hgt, args.y1)):
        row = []
        ncontent = 0
        for x in range(max(0, args.x0), min(w, args.x1)):
            v = px(x, y)
            if v == border:
                row.append(".")
            else:
                row.append("#")
                ncontent += 1
        # also count content across the FULL row width
        full = sum(1 for x in range(w) if px(x, y) != border)
        print("row {:3d} content(full)={:3d}  [{}..{}]: {}".format(y, full, args.x0, args.x1, "".join(row)))

if __name__ == "__main__":
    sys.exit(main(sys.argv))
