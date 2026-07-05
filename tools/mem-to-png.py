#!/usr/bin/env python3
"""Deterministic memory-buffer -> PNG visualizer (M10-S5).

Turns a dumped MSX memory buffer (VRAM or DRAM/SRAM) into an INERT grayscale PNG
so a region can be eyeballed offline. This is a raw byte visualization only: one
source byte becomes one 8-bit grayscale pixel (pixel value == byte value). It does
NOT perform any real V9958 rendering, tile/sprite decoding, or palette lookup
(that is a separate VDP device milestone, planner DP-2). The image is a faithful,
reversible picture of the raw bytes and nothing more.

Input contract (one of):
  * Raw binary buffer  : the default. A file of raw bytes (e.g. a region extracted
                         from a dump, or any committed memory image such as
                         tests/parity/z80_parity_checkpoint.bin).
  * M10-S3 region dump : pass --region NAME to parse the full-state dump text
                         emitted by src/machine/debug_dump.cpp
                         (`write_state_dump`, format tag "HBF1XV-STATE-DUMP v1")
                         and extract the named region ([DRAM]/[SRAM]/[VRAM]). The
                         folded canonical hex ('*' repeat marker, 16 bytes/line,
                         8-hex-digit offset) is expanded losslessly back to bytes.

Output: an 8-bit grayscale PNG (color type 0). `width` pixels per row (default
256); height = ceil(len/width); the final row is padded with `--pad` (default 0x00)
so the last scanline is full width.

Determinism (required, guaranteed by construction):
  * No timestamps, no tEXt/tIME/pHYs/gAMA or other metadata chunks are written.
  * The IDAT zlib stream is emitted as DEFLATE *stored* (uncompressed) blocks, hand
    assembled here rather than via zlib.compress(). Stored blocks are byte-identical
    on every platform and every zlib build, so identical input -> byte-identical PNG
    everywhere (not merely "within one machine"). zlib is used ONLY for the CRC-32
    (chunk checksums) and Adler-32 (zlib trailer), which are fixed algorithms.
  * ASCII-only, fixed field order, big-endian per the PNG spec.

Usage:
  python tools/mem-to-png.py <input> -o <out.png> [--width W] [--pad HEX]
  python tools/mem-to-png.py <dump.txt> --region VRAM -o vram.png
  python tools/mem-to-png.py --self-check
"""
import argparse
import hashlib
import os
import struct
import sys
import tempfile
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

# Golden SHA-256 of the --self-check reference PNG (fixed 256-byte ramp, width 16).
# Stable across platforms because the encoder emits DEFLATE stored blocks only.
SELF_CHECK_SHA256 = "0b51d6574f1b17aefbdba223f42505fa865f6fbfd29e1c639974616f393e0968"


def _chunk(tag, data):
    """Assemble one PNG chunk: length | tag | data | CRC-32(tag+data)."""
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def _zlib_stored(data):
    """Wrap `data` in a zlib stream using DEFLATE stored (uncompressed) blocks.

    Fully deterministic and independent of any zlib compression-level/version:
    only the (fixed-algorithm) Adler-32 trailer comes from zlib.
    """
    out = bytearray([0x78, 0x01])  # zlib header: 32K window, FLEVEL=fastest, no dict.
    n = len(data)
    if n == 0:
        # Single final empty stored block.
        out += bytes([0x01, 0x00, 0x00, 0xFF, 0xFF])
    else:
        i = 0
        while i < n:
            block = data[i:i + 65535]
            i += len(block)
            bfinal = 1 if i >= n else 0
            out.append(bfinal)  # BTYPE=00 (stored) => byte value == BFINAL bit.
            length = len(block)
            out += struct.pack("<H", length)
            out += struct.pack("<H", length ^ 0xFFFF)
            out += block
    out += struct.pack(">I", zlib.adler32(data) & 0xFFFFFFFF)
    return bytes(out)


def encode_png_gray8(pixels, width, pad=0):
    """Encode a flat grayscale byte buffer into a deterministic 8-bit PNG.

    Returns the full PNG byte string. `pixels` is padded to a whole number of
    `width`-wide rows with `pad`.
    """
    if width <= 0:
        raise ValueError("width must be positive")
    data = bytearray(pixels)
    if not data:
        data.append(pad)  # A 0-byte buffer still yields a 1x1 image.
    height = (len(data) + width - 1) // width
    padded_len = height * width
    if len(data) < padded_len:
        data.extend([pad] * (padded_len - len(data)))

    # Prepend a filter-type byte (0 == None) to each scanline.
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += data[y * width:(y + 1) * width]

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", _zlib_stored(bytes(raw)))
        + _chunk(b"IEND", b"")
    ), width, height


def parse_region_from_dump(text, region):
    """Extract and de-fold a named region from an M10-S3 full-state dump.

    Reverses src/machine/debug_dump.cpp serialize_region(): a '*' line means the
    previous printed 16-byte line repeats until the next printed offset. Returns
    the reconstructed bytes (length == the region's declared `size`).
    """
    header = "[" + region + "] size="
    lines = text.split("\n")
    start = None
    size = None
    for idx, line in enumerate(lines):
        if line.startswith(header):
            size = int(line[len(header):].strip())
            start = idx + 1
            break
    if start is None:
        raise ValueError("region [{}] not found in dump".format(region))

    buf = bytearray(size)
    prev_off = None
    prev_bytes = None
    star = False
    for line in lines[start:]:
        if not line:
            continue
        if line.startswith("[") or line == "[END]":
            break
        if line == "*":
            star = True
            continue
        tokens = line.split()
        off = int(tokens[0], 16)
        row = [int(t, 16) for t in tokens[1:] if len(t) == 2 and t != "  "]
        if star:
            fill = prev_off + 16
            while fill < off:
                end = min(fill + 16, size)
                buf[fill:end] = prev_bytes[:end - fill]
                fill += 16
            star = False
        end = min(off + len(row), size)
        buf[off:end] = bytes(row[:end - off])
        prev_off = off
        prev_bytes = bytes(row)
    return bytes(buf)


def load_buffer(args):
    if args.region:
        with open(args.input, "r", encoding="ascii", errors="replace") as handle:
            return parse_region_from_dump(handle.read(), args.region)
    with open(args.input, "rb") as handle:
        return handle.read()


def run_self_check():
    """Hermetic determinism self-check. Returns 0 on success, 1 on failure."""
    ok = True
    ramp = bytes(range(256))  # Fixed 256-byte 0..255 ramp.

    png_a, w, h = encode_png_gray8(ramp, 16)
    png_b, _, _ = encode_png_gray8(ramp, 16)

    def check(label, cond):
        nonlocal ok
        print("  [{}] {}".format("PASS" if cond else "FAIL", label))
        if not cond:
            ok = False

    check("reproducible: two encodes byte-identical", png_a == png_b)
    check("signature is the 8-byte PNG magic", png_a[:8] == PNG_SIGNATURE)
    # IHDR payload begins at offset 16 (8 sig + 4 len + 4 'IHDR').
    ihdr_w, ihdr_h, depth, ctype = struct.unpack(">IIBB", png_a[16:26])
    check("IHDR width == 16", ihdr_w == 16)
    check("IHDR height == 16 (256/16)", ihdr_h == 16 and h == 16 and w == 16)
    check("IHDR bit depth == 8", depth == 8)
    check("IHDR color type == 0 (grayscale)", ctype == 0)
    check("ends with IEND chunk", png_a[-12:] == b"\x00\x00\x00\x00IEND\xaeB`\x82")

    digest = hashlib.sha256(png_a).hexdigest()
    print("  self-check PNG sha256 = {}".format(digest))
    print("  self-check PNG bytes  = {}".format(len(png_a)))
    check("golden sha256 matches", digest == SELF_CHECK_SHA256)

    # Round-trip the S3 folded-hex region parser against a synthetic fixture that
    # mirrors src/machine/debug_dump.cpp serialize_region() (including a '*' fold).
    fixture = (
        "HBF1XV-STATE-DUMP v1\n[SRAM] size=64\n"
        "00000000 AA BB CC DD 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "00000010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "*\n"
        "00000030 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 11\n"
        "[END]\n"
    )
    expected = bytearray(64)
    expected[0:4] = b"\xAA\xBB\xCC\xDD"
    expected[63] = 0x11
    got = parse_region_from_dump(fixture, "SRAM")
    check("S3 region parse (with '*' fold) round-trips", got == bytes(expected))

    print("SELF-CHECK: {}".format("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description="Deterministic memory -> grayscale PNG.")
    parser.add_argument("input", nargs="?", help="raw binary buffer, or an S3 dump with --region")
    parser.add_argument("-o", "--out", help="output PNG path")
    parser.add_argument("--width", type=int, default=256, help="pixels per row (default 256)")
    parser.add_argument("--pad", default="00", help="hex pad byte for the final row (default 00)")
    parser.add_argument("--region", help="extract this region from an M10-S3 dump text file")
    parser.add_argument("--self-check", action="store_true", help="run determinism self-check")
    args = parser.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()
    if not args.input or not args.out:
        parser.error("input and -o/--out are required (or use --self-check)")

    pad = int(args.pad, 16) & 0xFF
    buffer = load_buffer(args)
    png, width, height = encode_png_gray8(buffer, args.width, pad)
    with open(args.out, "wb") as handle:
        handle.write(png)

    digest = hashlib.sha256(png).hexdigest()
    sys.stderr.write(
        "mem-to-png: bytes_in={} image={}x{} png_bytes={} sha256={} out={}\n".format(
            len(buffer), width, height, len(png), digest, args.out
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
