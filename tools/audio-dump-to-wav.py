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

"""Deterministic decoded-PSG-audio dump -> real playable WAV converter (M27-S5).

Converts a `frontend::write_psg_audio_dump()` output (format tag
"HBF1XV-AUDIO-DUMP v1", src/frontend/psg_audio_dump.{h,cpp}) into a REAL,
playable, canonical PCM WAV file. This is the NEW capability M27 ("Production
Hardening + Debug/Test Tooling", item 2) authorizes -- capturing genuinely
DECODED PSG (YM2149) audio samples (real square-wave/noise/envelope synthesis
via devices::audio::PsgYm2149::advance_cycles()/sample(), pumped through the
SAME frontend::PsgAudioPump wiring M26's live SDL3 audio path already uses)
to a file, then converting that to a real WAV -- NOT raw memory bytes.

`tools/mem-to-audio.py` (M10-S5) is explicitly insufficient for this: it
treats raw memory bytes as PCM samples verbatim and, by its own doc comment,
"does NOT synthesize PSG (AY-3-8910) or YM2413/OPLL audio and models no sound
chip at all." This script instead parses the ALREADY-DECODED signed 16-bit
stereo PCM payload frontend::psg_audio_dump.cpp already computed (via the
documented, linear psg_raw_sum_to_pcm16() scale grounded in the real [0,62]
PsgYm2149::sample() range, A-M27-4) and wraps it in a real WAV container --
mirroring tools/frame-to-png.py's exact "genuinely decoded, not raw bytes"
precedent, and its own separate-new-tool-rather-than-extend-the-old-one
choice (docs/m27-planner-package.md §2.3).

Determinism (required, guaranteed by construction -- mirrors every sibling
tool's own discipline exactly):
  * The 44-byte WAV header is hand-assembled with struct in fixed field
    order; no LIST/INFO/fact/cue chunks, no creation time, no software tag.
  * PCM sample bytes are copied verbatim from the parsed dump (already
    little-endian signed 16-bit stereo interleaved, per the dump format's own
    documented layout) -- no gain, no filtering, no resampling.
  * ASCII-only parsing, fixed field order.

Input contract: a text file produced by
frontend::write_psg_audio_dump()/serialize_psg_audio_dump(), format:
    HBF1XV-AUDIO-DUMP v1
    [AUDIO] sample_rate=<hz> channels=2 bits=16 samples=<N>
    [PCM] size=<N*4>
    <folded canonical hex dump, mirrors src/machine/debug_dump.cpp
     serialize_region(): 16 bytes/line, 8-hex-digit offset, '*' repeat fold>
    [END]

Usage:
  python tools/audio-dump-to-wav.py <dump.dump> -o <out.wav>
  python tools/audio-dump-to-wav.py --self-check
"""
import argparse
import hashlib
import struct
import sys

FORMAT_TAG = "HBF1XV-AUDIO-DUMP v1"

# Golden SHA-256 of the --self-check reference WAV (a small, fixed, synthetic
# audio dump -- the SAME oracle shape tests/unit/frontend/
# psg_audio_dump_unit_test.cpp's own Case 2 uses, independently re-expressed
# here in Python, mirroring frame-to-png.py's own cross-language fixture
# precedent).
SELF_CHECK_SHA256 = "57052ffff17a79d84433eb6fc78717e6b6743be39f0d99573d3814786e4513b4"


def encode_wav(pcm, sample_rate=44100, channels=2, bits=16):
    """Assemble a canonical PCM WAV byte string. Deterministic; no metadata.

    A small, already-proven encoder duplicated from tools/mem-to-audio.py's
    own encode_wav() (a zero-risk, well-understood ~15-line routine) rather
    than imported/shared, per docs/m27-planner-package.md §2.3's explicit
    "either is acceptable" framing -- keeps tools/mem-to-audio.py's own
    responsibilities completely untouched this cycle.
    """
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


def parse_audio_dump(text):
    """Parse a psg_audio_dump.cpp serialize_psg_audio_dump() text into
    (sample_rate, channels, bits, samples, pcm_bytes).

    Reverses debug_dump.cpp serialize_region()'s exact folded-hex format for
    the [PCM] section (a '*' line means the previous printed 16-byte line
    repeats until the next printed offset) -- independently re-expressed
    here in Python, mirroring tools/frame-to-png.py's own
    parse_frame_dump()/tools/mem-to-png.py's own parse_region_from_dump()
    precedent exactly (never copied from any external source).
    """
    lines = text.split("\n")
    if not lines or lines[0] != FORMAT_TAG:
        raise ValueError("not a HBF1XV-AUDIO-DUMP v1 file (missing/mismatched format tag)")

    sample_rate = channels = bits = samples = None
    pcm_start = None
    size = None
    for idx, line in enumerate(lines[1:], start=1):
        if line.startswith("[AUDIO] sample_rate="):
            parts = line[len("[AUDIO] "):].split()
            sample_rate = int(parts[0].split("=")[1])
            channels = int(parts[1].split("=")[1])
            bits = int(parts[2].split("=")[1])
            samples = int(parts[3].split("=")[1])
        elif line.startswith("[PCM] size="):
            size = int(line[len("[PCM] size="):].strip())
            pcm_start = idx + 1
            break

    if sample_rate is None or size is None:
        raise ValueError("missing [AUDIO] or [PCM] header")

    buf = bytearray(size)
    prev_off = None
    prev_bytes = None
    star = False
    for line in lines[pcm_start:]:
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

    return sample_rate, channels, bits, samples, bytes(buf)


def _self_check_dump_text():
    """A small, fixed, synthetic HBF1XV-AUDIO-DUMP v1 fixture: 3 stereo
    16-bit samples, sample_rate=44100, channels=2, bits=16 -- a
    hand-computable cross-language (C++ serializer / Python converter)
    fixture, independently re-derived here (never copied)."""
    sample_rate, channels, bits, sample_count = 44100, 2, 16, 3
    # (left, right) pairs, deliberately including the documented extremes
    # (psg_raw_sum_to_pcm16(0) == -32768, psg_raw_sum_to_pcm16(62) == 32767,
    # A-M27-4) plus one mid-range value.
    pairs = [(-32768, -32768), (32767, 32767), (-1, 12345)]
    pcm_bytes = bytearray()
    for left, right in pairs:
        pcm_bytes += struct.pack("<hh", left, right)

    def hex_line(offset, data):
        s = "{:08X}".format(offset)
        for i in range(16):
            if i < len(data):
                s += " {:02X}".format(data[i])
            else:
                s += "   "
        return s

    lines = [
        FORMAT_TAG,
        "[AUDIO] sample_rate={} channels={} bits={} samples={}".format(
            sample_rate, channels, bits, sample_count
        ),
        "[PCM] size={}".format(len(pcm_bytes)),
        hex_line(0, pcm_bytes[0:16]),
        hex_line(16, pcm_bytes[16:len(pcm_bytes)]),
        "[END]",
    ]
    text = "\n".join(lines) + "\n"
    return text, sample_rate, channels, bits, sample_count, bytes(pcm_bytes)


def run_self_check():
    """Hermetic determinism self-check. Returns 0 on success, 1 on failure."""
    ok = True

    def check(label, cond):
        nonlocal ok
        print("  [{}] {}".format("PASS" if cond else "FAIL", label))
        if not cond:
            ok = False

    text, rate, ch, bits, samples, expected_pcm = _self_check_dump_text()
    parsed_rate, parsed_ch, parsed_bits, parsed_samples, parsed_pcm = parse_audio_dump(text)
    check("parse_audio_dump: sample_rate matches", parsed_rate == rate)
    check("parse_audio_dump: channels matches", parsed_ch == ch)
    check("parse_audio_dump: bits matches", parsed_bits == bits)
    check("parse_audio_dump: samples matches", parsed_samples == samples)
    check("parse_audio_dump: pcm bytes match", parsed_pcm == expected_pcm)

    wav_a = encode_wav(parsed_pcm, sample_rate=parsed_rate, channels=parsed_ch, bits=parsed_bits)
    wav_b = encode_wav(parsed_pcm, sample_rate=parsed_rate, channels=parsed_ch, bits=parsed_bits)
    check("reproducible: two encodes byte-identical", wav_a == wav_b)
    check("RIFF magic", wav_a[:4] == b"RIFF")
    check("WAVE form type", wav_a[8:12] == b"WAVE")
    check("fmt chunk id", wav_a[12:16] == b"fmt ")
    audio_format, out_ch, out_rate, byte_rate, block_align, out_bits = struct.unpack("<HHIIHH", wav_a[20:36])
    check("PCM format tag (1)", audio_format == 1)
    check("stereo (2 channels)", out_ch == 2)
    check("sample rate 44100", out_rate == 44100)
    check("16 bits/sample", out_bits == 16)
    check("block align 4 (2ch * 16bit/8)", block_align == 4)
    check("byte rate == rate*block_align", byte_rate == 44100 * 4)
    check("data chunk id", wav_a[36:40] == b"data")
    data_size = struct.unpack("<I", wav_a[40:44])[0]
    check("data size == 12 (3 stereo 16-bit samples * 4 bytes)", data_size == 12)
    check("total bytes == 44 + 12", len(wav_a) == 44 + 12)

    # Hand-computed spot check: the first sample pair decodes back to
    # (-32768, -32768) as little-endian signed 16-bit.
    first_left, first_right = struct.unpack("<hh", wav_a[44:48])
    check("first sample pair decodes to (-32768, -32768)", (first_left, first_right) == (-32768, -32768))
    last_left, last_right = struct.unpack("<hh", wav_a[44 + 8:44 + 12])
    check("third sample pair decodes to (-1, 12345)", (last_left, last_right) == (-1, 12345))

    digest = hashlib.sha256(wav_a).hexdigest()
    print("  self-check WAV sha256 = {}".format(digest))
    print("  self-check WAV bytes  = {}".format(len(wav_a)))
    check("golden sha256 matches", digest == SELF_CHECK_SHA256)

    print("SELF-CHECK: {}".format("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", nargs="?", help="a frontend::write_psg_audio_dump() output file")
    parser.add_argument("-o", "--out", help="output WAV path")
    parser.add_argument("--self-check", action="store_true", help="run determinism self-check")
    args = parser.parse_args(argv[1:])

    if args.self_check:
        return run_self_check()
    if not args.input or not args.out:
        parser.error("input and -o/--out are required (or use --self-check)")

    with open(args.input, "r", encoding="ascii", errors="replace") as handle:
        text = handle.read()
    sample_rate, channels, bits, samples, pcm_bytes = parse_audio_dump(text)
    wav = encode_wav(pcm_bytes, sample_rate=sample_rate, channels=channels, bits=bits)
    with open(args.out, "wb") as handle:
        handle.write(wav)

    duration = samples / float(sample_rate) if sample_rate else 0.0
    digest = hashlib.sha256(wav).hexdigest()
    sys.stderr.write(
        "audio-dump-to-wav: samples={} rate={} channels={} bits={} duration_s={:.6f} "
        "wav_bytes={} sha256={} out={}\n".format(
            samples, sample_rate, channels, bits, duration, len(wav), digest, args.out
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
