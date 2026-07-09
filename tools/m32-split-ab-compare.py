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

"""M32 split-screen A/B region-structural comparator (package section 2.7).

Compares OUR rendered split frame (HBF1XV-FRAME-DUMP v1) against an openMSX
`screenshot -raw` PNG of the SAME synthetic split program, WITHOUT any
byte-level color comparison (different palette/scaler pipelines make that
meaningless). Both sides are analyzed independently through two
palette-independent structural properties of the probe's signature VRAM
(row r byte c = (r+c) & 0xFF, GRAPHIC4):

  P1 (shift chain): within one scroll region, display row y+1 is display
     row y shifted LEFT by exactly 2 pixels (source rows are +1-byte
     shifts of each other). The number and position of CHAIN BREAKS
     therefore recovers the region structure with zero color knowledge.
     A split frame has EXACTLY ONE break, at the split boundary; a
     no-split frame has ZERO breaks.

  P2 (offset-128 wrap): display row 128+k equals display row k pixel-for-
     pixel for k in [0, 64) IFF the bottom region runs at vertical scroll
     128 (source rows (128+k+128) mod 256 = k = the top region's own
     sources). Without the split this equality fails for every k.

Gate: BOTH sides must show the same structure -- exactly one P1 break,
located within +/-2 lines of each other (and inside [S, S+5) of the armed
R#19 = 80), and P2 holding on >= 60 of 64 rows (a small tolerance for
side-B sprite-less noise; in practice 64/64).

Adversarial self-check (M28/M29 precedent): --self-check corrupts one
region of a copy of side A and asserts the comparator FLAGS it.

PNG decoding is done with a minimal from-scratch reader (zlib + Paeth
et al. filters) -- no third-party imaging dependency.
"""

import argparse
import re
import struct
import sys
import zlib

SPLIT_LINE = 80


# --- HBF1XV-FRAME-DUMP v1 reader (side A) ---------------------------------

def read_frame_dump(path):
    with open(path, "r", encoding="ascii") as f:
        lines = f.read().splitlines()
    assert lines[0] == "HBF1XV-FRAME-DUMP v1"
    m = re.match(r"\[FRAME\] width=(\d+) height=(\d+) border=([0-9A-Fa-f]+)", lines[1])
    width, height = int(m.group(1)), int(m.group(2))
    m = re.match(r"\[PIXELS\] size=(\d+)", lines[2])
    size = int(m.group(1))
    data = bytearray()
    prev_row = None
    prev_offset = None
    for line in lines[3:]:
        if line == "[END]" or line == "*":
            continue
        mm = re.match(r"([0-9A-Fa-f]{8}) ((?:[0-9A-Fa-f]{2} ?)+)", line)
        if not mm:
            continue
        offset = int(mm.group(1), 16)
        row = bytes(int(b, 16) for b in mm.group(2).split())
        if prev_row is not None and offset > prev_offset + len(prev_row):
            gap = offset - (prev_offset + len(prev_row))
            data.extend(prev_row * (gap // len(prev_row)))
        data.extend(row)
        prev_row, prev_offset = row, offset
    if len(data) < size and prev_row:
        data.extend(prev_row * ((size - len(data)) // len(prev_row)))
    assert len(data) == size
    rows = []
    stride = width * 2
    for y in range(height):
        raw = data[y * stride:(y + 1) * stride]
        rows.append(tuple(struct.unpack("<%dH" % width, raw)))
    return rows


# --- Minimal PNG reader (side B: openMSX `screenshot -raw`) ---------------

def read_png(path):
    with open(path, "rb") as f:
        blob = f.read()
    assert blob[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    width = height = bit_depth = color_type = None
    idat = b""
    while pos < len(blob):
        (length,) = struct.unpack(">I", blob[pos:pos + 4])
        ctype = blob[pos + 4:pos + 8]
        payload = blob[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])
            assert bit_depth == 8, f"unsupported bit depth {bit_depth}"
            assert color_type in (2, 6), f"unsupported color type {color_type}"
        elif ctype == b"IDAT":
            idat += payload
        elif ctype == b"IEND":
            break
        pos += 12 + length
    raw = zlib.decompress(idat)
    bpp = 3 if color_type == 2 else 4
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    off = 0
    for _y in range(height):
        filt = raw[off]
        line = bytearray(raw[off + 1:off + 1 + stride])
        off += 1 + stride
        if filt == 1:      # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:    # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:    # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif filt == 4:    # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        prev = line
        rows.append(tuple(
            (line[x * bpp] << 16) | (line[x * bpp + 1] << 8) | line[x * bpp + 2]
            for x in range(width)))
    return rows


# --- Active-area location (side B) -----------------------------------------

def locate_active(rows):
    """Finds the (x0, y0, w, h) of the non-uniform (content) area."""
    height = len(rows)
    width = len(rows[0])
    content_rows = [y for y in range(height) if len(set(rows[y])) > 2]
    assert content_rows, "no content rows found"
    y0, y1 = content_rows[0], content_rows[-1] + 1
    x0, x1 = width, 0
    for y in content_rows:
        row = rows[y]
        # Non-border pixels: differ from the row's edge (border) color.
        border = row[0]
        xs = [x for x in range(width) if row[x] != border]
        if xs:
            x0 = min(x0, xs[0])
            x1 = max(x1, xs[-1] + 1)
    return x0, y0, x1 - x0, y1 - y0


# --- Structural analysis ----------------------------------------------------

def analyze(rows, label, width_hint=256):
    """P1 shift-chain breaks + P2 offset-128 wrap on a list of pixel rows."""
    breaks = []
    for y in range(len(rows) - 1):
        # row y+1 == row y shifted left by 2 pixels (compare the overlap).
        if rows[y + 1][0:width_hint - 2] != rows[y][2:width_hint]:
            breaks.append(y + 1)
    p2_hits = 0
    p2_total = 0
    if len(rows) >= 192:
        for k in range(64):
            p2_total += 1
            if rows[128 + k] == rows[k]:
                p2_hits += 1
    print(f"  [{label}] rows={len(rows)} shift_chain_breaks={breaks[:8]}"
          f"{'...' if len(breaks) > 8 else ''} (n={len(breaks)})"
          f" offset128_wrap={p2_hits}/{p2_total}")
    return breaks, p2_hits, p2_total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ours", help="side A .frame dump")
    parser.add_argument("--openmsx", help="side B raw screenshot PNG")
    parser.add_argument("--expect-no-split", action="store_true")
    parser.add_argument("--self-check", action="store_true",
                        help="corrupt one region of side A and require a FLAG")
    args = parser.parse_args()

    if args.ours is None:
        # Side-B-only mode (the ps1 harness's IE1-off arm).
        assert args.openmsx, "--openmsx required without --ours"
        shot = read_png(args.openmsx)
        x0, y0, w, h = locate_active(shot)
        print(f"side B: {args.openmsx} active area x0={x0} y0={y0} w={w} h={h}")
        active = [row[x0:x0 + 256] for row in shot[y0:y0 + 192]]
        b_breaks, b_p2, b_p2n = analyze(active, "openmsx")
        if args.expect_no_split:
            ok = not b_breaks and b_p2 < 60
            print("side B no-split structure:", "OK" if ok else "VIOLATED")
        else:
            ok = (len(b_breaks) == 1 and SPLIT_LINE <= b_breaks[0] < SPLIT_LINE + 5
                  and b_p2 >= 60)
            print("side B split structure:", "OK" if ok else "VIOLATED")
        print("RESULT:", "PARITY" if ok else "DIVERGENCE")
        sys.exit(0 if ok else 1)

    ours = read_frame_dump(args.ours)
    print(f"side A: {args.ours} ({len(ours)} rows x {len(ours[0])} px)")
    a_breaks, a_p2, a_p2n = analyze(ours, "ours")

    if args.self_check:
        corrupted = [list(r) for r in ours]
        for y in range(100, 140):       # corrupt part of the bottom region
            for x in range(0, 64):
                corrupted[y][x] ^= 0x1F
        corrupted = [tuple(r) for r in corrupted]
        c_breaks, c_p2, c_p2n = analyze(corrupted, "self-check-corrupted")
        flagged = (len(c_breaks) != len(a_breaks)) or (c_p2 < a_p2)
        print(f"self-check: comparator {'FLAGS' if flagged else 'MISSES'} the corruption")
        sys.exit(0 if flagged else 1)

    ok = True
    if args.expect_no_split:
        if a_breaks or a_p2 == a_p2n:
            ok = False
        print(f"side A no-split structure: {'OK' if ok else 'VIOLATED'}")
    else:
        if len(a_breaks) != 1 or not (SPLIT_LINE <= a_breaks[0] < SPLIT_LINE + 5):
            ok = False
        if a_p2 < 60:
            ok = False
        print(f"side A split structure: one break in [{SPLIT_LINE},{SPLIT_LINE + 5}) "
              f"+ wrap: {'OK' if ok else 'VIOLATED'}")

    if args.openmsx:
        shot = read_png(args.openmsx)
        x0, y0, w, h = locate_active(shot)
        print(f"side B: {args.openmsx} active area x0={x0} y0={y0} w={w} h={h}")
        active = [row[x0:x0 + 256] for row in shot[y0:y0 + 192]]
        b_breaks, b_p2, b_p2n = analyze(active, "openmsx")
        if args.expect_no_split:
            b_ok = not b_breaks and b_p2 < b_p2n
            print(f"side B no-split structure: {'OK' if b_ok else 'VIOLATED'}")
            ok = ok and b_ok
        else:
            b_ok = (len(b_breaks) == 1 and SPLIT_LINE <= b_breaks[0] < SPLIT_LINE + 5
                    and b_p2 >= 60)
            print(f"side B split structure: {'OK' if b_ok else 'VIOLATED'}")
            ok = ok and b_ok
            if b_ok and len(a_breaks) == 1:
                delta = abs(a_breaks[0] - b_breaks[0])
                print(f"split boundary: ours={a_breaks[0]} openmsx={b_breaks[0]} "
                      f"delta={delta} (gate: <=2)")
                ok = ok and delta <= 2

    print("RESULT:", "PARITY" if ok else "DIVERGENCE")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
