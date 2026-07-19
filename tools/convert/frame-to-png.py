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

"""Deterministic decoded-FrameBuffer -> real color PNG converter (M26-S4).

Converts a `Hbf1xvMachine::write_frame_dump()` output (format tag
"HBF1XV-FRAME-DUMP v1", src/machine/frame_dump.{h,cpp}) into a REAL, decoded,
truecolor PNG (color type 2). This is the ONE new debug/testing capability
M26 (SDL3 frontend, backlog C9) authorizes: capturing the *decoded*
V9958 VDP FrameBuffer (RGB555 pixels, from the M21 VdpFrameRenderer) -- NOT
raw VRAM bytes.

`tools/convert/mem-to-png.py` (M10-S5) is explicitly insufficient for this: it
visualizes raw memory bytes as 8-bit GRAYSCALE noise with zero VDP-mode/
palette awareness (its own doc comment: "It does NOT perform any real V9958
rendering, tile/sprite decoding, or palette lookup"). This script instead
parses the ALREADY-DECODED RGB555 pixel buffer and expands each 5-bit
R/G/B component to 8 bits via standard bit-replication ((v<<3)|(v>>2)),
emitting a genuine, human-viewable color picture.

Determinism (required, guaranteed by construction -- mirrors tools/convert/mem-to-png.py's
own discipline exactly):
  * No timestamps, no tEXt/tIME/pHYs/gAMA or other metadata chunks.
  * The IDAT zlib stream is emitted as DEFLATE *stored* (uncompressed)
    blocks, hand-assembled here rather than via zlib.compress() -- byte-
    identical on every platform/zlib build. zlib is used ONLY for the
    CRC-32 (chunk checksums) and Adler-32 (zlib trailer), fixed algorithms.
  * ASCII-only parsing, fixed field order, big-endian per the PNG spec.

Input contract: a text file produced by Hbf1xvMachine::write_frame_dump()
(or serialize_frame_dump()), format:
    HBF1XV-FRAME-DUMP v1
    [FRAME] width=<w> height=<h> border=<HHHH>
    [PIXELS] size=<n>
    <folded canonical hex dump, mirrors src/machine/debug_dump.cpp
     serialize_region(): 16 bytes/line, 8-hex-digit offset, '*' repeat fold>
    [END]

Usage:
  python tools/convert/frame-to-png.py <dump.frame> -o <out.png>
  python tools/convert/frame-to-png.py --self-check
"""
import argparse
import hashlib
import struct
import sys
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
FORMAT_TAG = "HBF1XV-FRAME-DUMP v1"

# Golden SHA-256 of the --self-check reference PNG (fixed 4x3 synthetic
# frame, the SAME make_known_frame() pixel values as
# tests/unit/machine/frame_dump_unit_test.cpp's own oracle). Stable across
# platforms because the encoder emits DEFLATE stored blocks only.
SELF_CHECK_SHA256 = "c4c52f8f418883178d7327ef9c1d4a5350f263d83ffce6019d9aa0f5dc0e5503"


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

    Identical algorithm to tools/convert/mem-to-png.py's own _zlib_stored (small,
    ~15-line, already-hermetically-tested helper -- duplicated here rather
    than factored into a shared module, per docs/m26-planner-package.md
    §2.5 point 2's explicit either-is-acceptable developer's-call).
    """
    out = bytearray([0x78, 0x01])  # zlib header: 32K window, FLEVEL=fastest, no dict.
    n = len(data)
    if n == 0:
        out += bytes([0x01, 0x00, 0x00, 0xFF, 0xFF])
    else:
        i = 0
        while i < n:
            block = data[i:i + 65535]
            i += len(block)
            bfinal = 1 if i >= n else 0
            out.append(bfinal)
            length = len(block)
            out += struct.pack("<H", length)
            out += struct.pack("<H", length ^ 0xFFFF)
            out += block
    out += struct.pack(">I", zlib.adler32(data) & 0xFFFFFFFF)
    return bytes(out)


def parse_frame_dump(text):
    """Parse a frame_dump.cpp serialize_frame_dump() text into
    (width, height, border_color, pixel_bytes).

    Reverses debug_dump.cpp serialize_region()'s exact folded-hex format for
    the [PIXELS] section (a '*' line means the previous printed 16-byte line
    repeats until the next printed offset) -- independently re-expressed
    here in Python, mirroring this same script's own parse_region_from_dump
    precedent in tools/convert/mem-to-png.py (never copied from any external source).
    """
    lines = text.split("\n")
    if not lines or lines[0] != FORMAT_TAG:
        raise ValueError("not a HBF1XV-FRAME-DUMP v1 file (missing/mismatched format tag)")

    width = height = border = None
    pixels_start = None
    size = None
    for idx, line in enumerate(lines[1:], start=1):
        if line.startswith("[FRAME] width="):
            parts = line[len("[FRAME] "):].split()
            width = int(parts[0].split("=")[1])
            height = int(parts[1].split("=")[1])
            border = int(parts[2].split("=")[1], 16)
        elif line.startswith("[PIXELS] size="):
            size = int(line[len("[PIXELS] size="):].strip())
            pixels_start = idx + 1
            break

    if width is None or size is None:
        raise ValueError("missing [FRAME] or [PIXELS] header")

    buf = bytearray(size)
    prev_off = None
    prev_bytes = None
    star = False
    for line in lines[pixels_start:]:
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

    return width, height, border, bytes(buf)


def border_geometry(active_width, active_height):
    """Presentation border-canvas geometry, mirroring
    src/frontend/border_composer.cpp border_geometry() EXACTLY (any change
    there must be mirrored here; both cite the same grounding: V9958
    fact-sheet §7 line/frame tick tables + openMSX SDLRasterizer.cc
    translateX()/lineRenderTop, verified against measured openMSX raw
    screenshots). Returns (canvas_w, canvas_h, x0, y0)."""
    canvas_w = 640 if active_width > 320 else 320
    canvas_h = 240
    x0_table = {240: 41, 256: 32, 480: 82, 512: 64}
    x0 = x0_table.get(active_width, max(0, (canvas_w - active_width) // 2))
    y0_table = {192: 24, 212: 14}
    y0 = y0_table.get(active_height, max(0, (canvas_h - active_height) // 2))
    return canvas_w, canvas_h, x0, y0


def compose_border_canvas(width, height, border, pixel_bytes):
    """Compose the presented canvas: border-color-filled canvas with the
    active area at its raster-true position (the same composition
    src/frontend/border_composer.cpp compose_border_canvas() performs for
    the SDL3 window). Returns (canvas_w, canvas_h, canvas_pixel_bytes)."""
    canvas_w, canvas_h, x0, y0 = border_geometry(width, height)
    lo = border & 0xFF
    hi = (border >> 8) & 0xFF
    canvas = bytearray(bytes((lo, hi)) * (canvas_w * canvas_h))
    copy_w = min(width, canvas_w - x0)
    copy_h = min(height, canvas_h - y0)
    for y in range(copy_h):
        src = y * width * 2
        dst = ((y + y0) * canvas_w + x0) * 2
        canvas[dst:dst + copy_w * 2] = pixel_bytes[src:src + copy_w * 2]
    return canvas_w, canvas_h, bytes(canvas)


def rgb555_to_rgb888(pixel):
    """Expand one native RGB555 uint16 (bits 14-10=R, 9-5=G, 4-0=B,
    frame_buffer.h's own documented layout) to 8-bit R,G,B via standard
    5-bit -> 8-bit bit-replication ((v << 3) | (v >> 2))."""
    r5 = (pixel >> 10) & 0x1F
    g5 = (pixel >> 5) & 0x1F
    b5 = pixel & 0x1F
    r8 = (r5 << 3) | (r5 >> 2)
    g8 = (g5 << 3) | (g5 >> 2)
    b8 = (b5 << 3) | (b5 >> 2)
    return r8, g8, b8


def encode_png_rgb8(width, height, pixel_bytes):
    """Encode a flat little-endian uint16 RGB555 pixel buffer (width*height
    entries, row-major -- frame_buffer.h's own layout) into a deterministic
    24-bit truecolor PNG (color type 2). Returns the full PNG byte string."""
    if width <= 0 or height <= 0:
        raise ValueError("width/height must be positive")
    expected_bytes = width * height * 2
    if len(pixel_bytes) != expected_bytes:
        raise ValueError(
            "pixel byte count {} does not match width*height*2={}".format(len(pixel_bytes), expected_bytes)
        )

    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0 (None)
        row_offset = y * width * 2
        for x in range(width):
            i = row_offset + x * 2
            pixel = pixel_bytes[i] | (pixel_bytes[i + 1] << 8)  # host-native little-endian uint16
            r8, g8, b8 = rgb555_to_rgb888(pixel)
            raw.append(r8)
            raw.append(g8)
            raw.append(b8)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # color type 2 = truecolor
    return (
        PNG_SIGNATURE
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", _zlib_stored(bytes(raw)))
        + _chunk(b"IEND", b"")
    )


def _self_check_frame_text():
    """The SAME 4x3 synthetic FrameBuffer as tests/unit/machine/
    frame_dump_unit_test.cpp's make_known_frame() / Case 1 oracle -- a fixed,
    hand-computable cross-language (C++ serializer / Python converter)
    fixture, independently re-derived here (never copied)."""
    width, height = 4, 3
    border = 0x1234
    pixel_count = width * height
    pixel_values = [((0x0100 * (i + 1) + i) & 0xFFFF) for i in range(pixel_count)]
    pixel_bytes = bytearray()
    for v in pixel_values:
        pixel_bytes.append(v & 0xFF)
        pixel_bytes.append((v >> 8) & 0xFF)

    # Assemble the folded-hex [PIXELS] section by hand (mirrors
    # debug_dump.cpp serialize_region() -- 24 bytes here, under 16/line
    # folding threshold entirely, so this is simply 2 verbatim lines).
    def hex_line(offset, data):
        s = "{:08X}".format(offset)
        for i in range(16):
            if i < len(data):
                s += " {:02X}".format(data[i])
            else:
                s += "   "
        return s

    lines = [FORMAT_TAG, "[FRAME] width={} height={} border={:04X}".format(width, height, border)]
    lines.append("[PIXELS] size={}".format(len(pixel_bytes)))
    lines.append(hex_line(0, pixel_bytes[0:16]))
    lines.append(hex_line(16, pixel_bytes[16:24]))
    lines.append("[END]")
    text = "\n".join(lines) + "\n"
    return text, width, height, bytes(pixel_bytes)


def run_self_check():
    """Hermetic determinism self-check. Returns 0 on success, 1 on failure."""
    ok = True

    def check(label, cond):
        nonlocal ok
        print("  [{}] {}".format("PASS" if cond else "FAIL", label))
        if not cond:
            ok = False

    text, width, height, expected_bytes = _self_check_frame_text()
    parsed_width, parsed_height, parsed_border, parsed_bytes = parse_frame_dump(text)
    check("parse_frame_dump: width matches", parsed_width == width)
    check("parse_frame_dump: height matches", parsed_height == height)
    check("parse_frame_dump: border matches", parsed_border == 0x1234)
    check("parse_frame_dump: pixel bytes match", parsed_bytes == expected_bytes)

    png_a = encode_png_rgb8(parsed_width, parsed_height, parsed_bytes)
    png_b = encode_png_rgb8(parsed_width, parsed_height, parsed_bytes)
    check("reproducible: two encodes byte-identical", png_a == png_b)
    check("signature is the 8-byte PNG magic", png_a[:8] == PNG_SIGNATURE)
    ihdr_w, ihdr_h, depth, ctype = struct.unpack(">IIBB", png_a[16:26])
    check("IHDR width == 4", ihdr_w == 4)
    check("IHDR height == 3", ihdr_h == 3)
    check("IHDR bit depth == 8", depth == 8)
    check("IHDR color type == 2 (truecolor)", ctype == 2)
    check("ends with IEND chunk", png_a[-12:] == b"\x00\x00\x00\x00IEND\xaeB`\x82")

    # A hand-computed spot check: pixel[0] raw value = 0x0100*1+0 = 0x0100.
    # RGB555 decode: R5=(0x0100>>10)&0x1F=0, G5=(0x0100>>5)&0x1F=8, B5=0.
    # 8-bit expand: G8=(8<<3)|(8>>2)=64|2=66.
    r0, g0, b0 = rgb555_to_rgb888(0x0100)
    check("rgb555_to_rgb888(0x0100) hand-computed oracle", (r0, g0, b0) == (0, 66, 0))

    digest = hashlib.sha256(png_a).hexdigest()
    print("  self-check PNG sha256 = {}".format(digest))
    print("  self-check PNG bytes  = {}".format(len(png_a)))
    check("golden sha256 matches", digest == SELF_CHECK_SHA256)

    print("SELF-CHECK: {}".format("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", nargs="?", help="a Hbf1xvMachine::write_frame_dump() output file")
    parser.add_argument("-o", "--out", help="output PNG path")
    parser.add_argument("--self-check", action="store_true", help="run determinism self-check")
    parser.add_argument(
        "--with-border",
        action="store_true",
        help="compose the presentation border canvas (320x240/640x240, the"
             " same geometry the SDL3 presenter shows) around the active area",
    )
    args = parser.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()
    if not args.input or not args.out:
        parser.error("input and -o/--out are required (or use --self-check)")

    with open(args.input, "r", encoding="ascii", errors="replace") as handle:
        text = handle.read()
    width, height, border, pixel_bytes = parse_frame_dump(text)
    if args.with_border:
        width, height, pixel_bytes = compose_border_canvas(width, height, border, pixel_bytes)
    png = encode_png_rgb8(width, height, pixel_bytes)
    with open(args.out, "wb") as handle:
        handle.write(png)

    digest = hashlib.sha256(png).hexdigest()
    sys.stderr.write(
        "frame-to-png: image={}x{} border={:04X} png_bytes={} sha256={} out={}\n".format(
            width, height, border, len(png), digest, args.out
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
