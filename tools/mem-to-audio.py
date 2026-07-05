#!/usr/bin/env python3
"""Deterministic memory-buffer -> WAV serializer (M10-S5).

Interprets a dumped MSX memory buffer as raw PCM samples and emits a canonical
WAV file so a region can be auditioned offline. This is an INERT raw-buffer
serialization only: source bytes are treated directly as PCM samples. It does NOT
synthesize PSG (AY-3-8910) or YM2413/OPLL audio and models no sound chip at all
(that is a separate audio-device milestone). The WAV is a faithful, reversible
container around the raw bytes and nothing more.

Input contract (one of):
  * Raw binary buffer  : the default. A file of raw bytes (e.g. a region extracted
                         from a dump, or any committed memory image such as
                         tests/parity/z80_parity_checkpoint.bin).
  * M10-S3 region dump : pass --region NAME to parse the full-state dump text
                         emitted by src/machine/debug_dump.cpp and extract the
                         named region ([DRAM]/[SRAM]/[VRAM]); the folded canonical
                         hex ('*' repeat marker) is expanded losslessly to bytes.

Output: a canonical PCM WAV (RIFF/WAVE, 44-byte header + data). Defaults: mono,
8-bit unsigned PCM, 8000 Hz. --bits 16 reads the buffer as little-endian signed
16-bit samples (odd trailing byte padded with 0x00); --channels and --rate are
configurable. Sample interpretation is exactly the raw bytes -- no gain, no
filtering, no resampling.

Determinism (required, guaranteed by construction):
  * The 44-byte header is hand-assembled with struct in fixed field order; no
    LIST/INFO/fact/cue chunks, no creation time, no software tag -- WAV carries no
    timestamp, so identical input -> byte-identical output on every platform.
  * Sample bytes are copied verbatim (with at most a single 0x00 pad for 16-bit
    odd length). ASCII tags, little-endian per the RIFF spec.

Usage:
  python tools/mem-to-audio.py <input> -o <out.wav> [--rate HZ] [--bits 8|16] [--channels N]
  python tools/mem-to-audio.py <dump.txt> --region DRAM -o dram.wav
  python tools/mem-to-audio.py --self-check
"""
import argparse
import hashlib
import struct
import sys
import zlib

# Golden SHA-256 of the --self-check reference WAV (256-byte ramp, 8-bit mono 8000 Hz).
SELF_CHECK_SHA256 = "360911aa5e193cf2adcc660b619ee6faa0d72cae0f36a41dc539fb88be63bef9"


def encode_wav(pcm, sample_rate=8000, channels=1, bits=8):
    """Assemble a canonical PCM WAV byte string. Deterministic; no metadata."""
    if channels < 1:
        raise ValueError("channels must be >= 1")
    if bits not in (8, 16):
        raise ValueError("bits must be 8 or 16")
    data = bytearray(pcm)
    if bits == 16 and (len(data) % 2) != 0:
        data.append(0x00)  # Pad to a whole 16-bit sample.
    data_size = len(data)
    block_align = channels * bits // 8
    byte_rate = sample_rate * block_align
    header = b"RIFF" + struct.pack("<I", 36 + data_size) + b"WAVE"
    header += b"fmt " + struct.pack(
        "<IHHIIHH", 16, 1, channels, sample_rate, byte_rate, block_align, bits
    )
    header += b"data" + struct.pack("<I", data_size)
    return bytes(header) + bytes(data)


def parse_region_from_dump(text, region):
    """Extract and de-fold a named region from an M10-S3 full-state dump.

    Reverses src/machine/debug_dump.cpp serialize_region(): a '*' line means the
    previous printed 16-byte line repeats until the next printed offset.
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
        row = [int(t, 16) for t in tokens[1:] if len(t) == 2]
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

    wav_a = encode_wav(ramp, sample_rate=8000, channels=1, bits=8)
    wav_b = encode_wav(ramp, sample_rate=8000, channels=1, bits=8)

    def check(label, cond):
        nonlocal ok
        print("  [{}] {}".format("PASS" if cond else "FAIL", label))
        if not cond:
            ok = False

    check("reproducible: two encodes byte-identical", wav_a == wav_b)
    check("RIFF magic", wav_a[:4] == b"RIFF")
    check("WAVE form type", wav_a[8:12] == b"WAVE")
    check("fmt chunk id", wav_a[12:16] == b"fmt ")
    audio_format, ch, rate, byte_rate, block_align, bits = struct.unpack(
        "<HHIIHH", wav_a[20:36]
    )
    check("PCM format tag (1)", audio_format == 1)
    check("mono", ch == 1)
    check("sample rate 8000", rate == 8000)
    check("8 bits/sample", bits == 8)
    check("block align 1", block_align == 1)
    check("byte rate == rate*block_align", byte_rate == 8000)
    check("data chunk id", wav_a[36:40] == b"data")
    data_size = struct.unpack("<I", wav_a[40:44])[0]
    check("data size == 256", data_size == 256)
    check("total bytes == 44 + 256", len(wav_a) == 44 + 256)
    # Data samples are the raw bytes verbatim.
    check("first sample == 0x00", wav_a[44] == 0x00)
    check("last sample == 0xFF", wav_a[-1] == 0xFF)

    # 16-bit odd-length padding path.
    wav16 = encode_wav(b"\x01\x02\x03", sample_rate=8000, channels=1, bits=16)
    check("16-bit odd length padded to even", len(wav16) == 44 + 4)

    digest = hashlib.sha256(wav_a).hexdigest()
    print("  self-check WAV sha256 = {}".format(digest))
    print("  self-check WAV bytes  = {}".format(len(wav_a)))
    check("golden sha256 matches", digest == SELF_CHECK_SHA256)

    # Round-trip the S3 folded-hex region parser (mirrors debug_dump.cpp).
    fixture = (
        "HBF1XV-STATE-DUMP v1\n[DRAM] size=64\n"
        "00000000 10 20 30 40 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "00000010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "*\n"
        "00000030 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7F\n"
        "[END]\n"
    )
    expected = bytearray(64)
    expected[0:4] = b"\x10\x20\x30\x40"
    expected[63] = 0x7F
    got = parse_region_from_dump(fixture, "DRAM")
    check("S3 region parse (with '*' fold) round-trips", got == bytes(expected))

    _ = zlib  # keep import parity with sibling tool; unused here.
    print("SELF-CHECK: {}".format("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description="Deterministic memory -> PCM WAV.")
    parser.add_argument("input", nargs="?", help="raw binary buffer, or an S3 dump with --region")
    parser.add_argument("-o", "--out", help="output WAV path")
    parser.add_argument("--rate", type=int, default=8000, help="sample rate Hz (default 8000)")
    parser.add_argument("--bits", type=int, default=8, choices=(8, 16), help="8 or 16 (default 8)")
    parser.add_argument("--channels", type=int, default=1, help="channel count (default 1)")
    parser.add_argument("--region", help="extract this region from an M10-S3 dump text file")
    parser.add_argument("--self-check", action="store_true", help="run determinism self-check")
    args = parser.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()
    if not args.input or not args.out:
        parser.error("input and -o/--out are required (or use --self-check)")

    buffer = load_buffer(args)
    wav = encode_wav(buffer, sample_rate=args.rate, channels=args.channels, bits=args.bits)
    with open(args.out, "wb") as handle:
        handle.write(wav)

    frames = (len(wav) - 44) // (args.channels * args.bits // 8)
    duration = frames / float(args.rate) if args.rate else 0.0
    digest = hashlib.sha256(wav).hexdigest()
    sys.stderr.write(
        "mem-to-audio: bytes_in={} wav_bytes={} frames={} duration_s={:.6f} "
        "rate={} bits={} ch={} sha256={} out={}\n".format(
            len(buffer), len(wav), frames, duration, args.rate, args.bits,
            args.channels, digest, args.out
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
