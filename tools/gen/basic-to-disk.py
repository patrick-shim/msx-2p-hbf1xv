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

"""M41-S1 deterministic FAT12 ASCII-BASIC disk injector
(docs/m41-production-qa-plan.md §5.2).

Places one or more files -- an ASCII (plain-text `SAVE`-format) MSX-BASIC
program and/or arbitrary named payloads -- onto a formatted-blank 720 KB
(2DD) FAT12 image, so both emulators can boot an identical self-running disk
with ZERO keyboard dependency. The blank image geometry is byte-identical to
tools/gen/format-blank-disk.ps1 (80x2x9x512, media 0xF9, 1 reserved sector, 2
FATs x 3 sectors @ LBA 1 and 4, 112-entry root dir @ LBA 7, data @ LBA 14,
2 sectors/cluster) -- reproduced here in Python so no PowerShell is needed.

ASCII BASIC convention: CRLF line endings + a trailing 0x1A (Ctrl-Z) EOF byte,
exactly as an MSX `SAVE"FILE.BAS"` (no ,A -> tokenised; here we use the ASCII
form Disk-BASIC `LOAD`/`RUN` accepts). Disk-BASIC auto-runs `AUTOEXEC.BAS` on
a non-DOS disk at cold boot (verified as Phase-0 assumption A-2).

Determinism: PURE FUNCTION of the input files + constants. All directory
date/time fields are fixed ZERO (never wall-clock), so two runs over the same
inputs produce a byte-identical image (auditable SHA256). Cross-checked SHA-
stable by --self-check.

Reader: --read NAME.EXT parses the FAT12 chain and writes/prints a file's
bytes -- used to verify SAVE round-trips (scenario H / assumption A-6) by
re-reading a `.dsk` written by either emulator.

Exit code: 0 = success / self-check pass; 1 = failure; 2 = usage error.
"""

import argparse
import hashlib
import os
import subprocess
import sys

# --- Geometry (mirrors tools/gen/format-blank-disk.ps1) -------------------------
SECTOR = 512
SECTORS_PER_TRACK = 9
SIDES = 2
TRACKS = 80
TOTAL_SECTORS = SECTORS_PER_TRACK * SIDES * TRACKS          # 1440
IMAGE_BYTES = TOTAL_SECTORS * SECTOR                        # 737280
MEDIA = 0xF9
RESERVED_SECTORS = 1
NUM_FATS = 2
SECTORS_PER_FAT = 3
ROOT_ENTRIES = 112
SECTORS_PER_CLUSTER = 2

ROOT_DIR_LBA = RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT           # 7
ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32 + SECTOR - 1) // SECTOR         # 7
DATA_START_LBA = ROOT_DIR_LBA + ROOT_DIR_SECTORS                      # 14
BYTES_PER_CLUSTER = SECTOR * SECTORS_PER_CLUSTER                      # 1024
TOTAL_CLUSTERS = (TOTAL_SECTORS - DATA_START_LBA) // SECTORS_PER_CLUSTER
FAT12_EOC = 0xFFF  # end-of-chain marker


def build_blank():
    """Byte-identical to tools/gen/format-blank-disk.ps1's empty formatted image."""
    img = bytearray(IMAGE_BYTES)
    # Boot sector + BPB at LBA 0.
    img[0] = 0xEB
    img[1] = 0xFE
    img[2] = 0x90
    img[3:11] = b"SONYMSX "
    img[11] = 0x00  # bytes/sector = 512
    img[12] = 0x02
    img[13] = 0x02  # sectors/cluster = 2
    img[14] = 0x01  # reserved sectors = 1
    img[15] = 0x00
    img[16] = 0x02  # FATs = 2
    img[17] = 0x70  # root entries = 112
    img[18] = 0x00
    img[19] = 0xA0  # total sectors = 1440
    img[20] = 0x05
    img[21] = MEDIA
    img[22] = 0x03  # sectors/FAT = 3
    img[23] = 0x00
    img[24] = 0x09  # sectors/track = 9
    img[25] = 0x00
    img[26] = 0x02  # heads = 2
    img[27] = 0x00
    img[28] = 0x00  # hidden sectors = 0
    img[29] = 0x00
    img[510] = 0x55
    img[511] = 0xAA
    # FAT copies at LBA 1 and LBA 4, seeded with the reserved cluster 0/1
    # entries (0xFF9 / 0xFFF) exactly as tools/gen/format-blank-disk.ps1.
    for fat_lba in (RESERVED_SECTORS, RESERVED_SECTORS + SECTORS_PER_FAT):
        base = fat_lba * SECTOR
        img[base + 0] = MEDIA
        img[base + 1] = 0xFF
        img[base + 2] = 0xFF
    return img


def fat_offsets():
    return [RESERVED_SECTORS * SECTOR, (RESERVED_SECTORS + SECTORS_PER_FAT) * SECTOR]


def fat_set(img, cluster, value):
    """Write a 12-bit FAT entry into BOTH FAT copies (keeps them consistent)."""
    off = cluster + (cluster >> 1)  # cluster * 3 // 2
    for base in fat_offsets():
        i = base + off
        if cluster & 1:  # odd
            img[i] = (img[i] & 0x0F) | ((value << 4) & 0xF0)
            img[i + 1] = (value >> 4) & 0xFF
        else:            # even
            img[i] = value & 0xFF
            img[i + 1] = (img[i + 1] & 0xF0) | ((value >> 8) & 0x0F)


def fat_get(img, cluster):
    off = fat_offsets()[0] + cluster + (cluster >> 1)
    lo, hi = img[off], img[off + 1]
    if cluster & 1:
        return (lo >> 4) | (hi << 4)
    return lo | ((hi & 0x0F) << 8)


def next_free_cluster(img, start):
    for c in range(start, TOTAL_CLUSTERS + 2):
        if fat_get(img, c) == 0:
            return c
    raise RuntimeError("disk full: no free cluster")


def split_83(name):
    name = name.upper()
    if "." in name:
        base, ext = name.rsplit(".", 1)
    else:
        base, ext = name, ""
    if len(base) > 8 or len(ext) > 3 or not base:
        raise ValueError(f"not an 8.3 name: {name}")
    return base.ljust(8), ext.ljust(3)


def find_free_dir_entry(img):
    base = ROOT_DIR_LBA * SECTOR
    for e in range(ROOT_ENTRIES):
        i = base + e * 32
        first = img[i]
        if first in (0x00, 0xE5):
            return i
    raise RuntimeError("root directory full")


def add_file(img, name, data):
    """Allocate a cluster chain, write data, add an 8.3 root-dir entry."""
    base83, ext83 = split_83(name)
    size = len(data)
    n_clusters = max(1, (size + BYTES_PER_CLUSTER - 1) // BYTES_PER_CLUSTER)
    # Allocate the chain deterministically (lowest free clusters first).
    chain = []
    search_from = 2
    for _ in range(n_clusters):
        c = next_free_cluster(img, search_from)
        # Reserve immediately (mark EOC) so the next search skips it.
        fat_set(img, c, FAT12_EOC)
        chain.append(c)
        search_from = c + 1
    # Link the chain (each -> next, last stays EOC).
    for idx in range(len(chain) - 1):
        fat_set(img, chain[idx], chain[idx + 1])
    # Write the data into the data area.
    for idx, c in enumerate(chain):
        lba = DATA_START_LBA + (c - 2) * SECTORS_PER_CLUSTER
        off = lba * SECTOR
        chunk = data[idx * BYTES_PER_CLUSTER:(idx + 1) * BYTES_PER_CLUSTER]
        img[off:off + len(chunk)] = chunk
    # Root-directory entry (all date/time = 0 for determinism).
    d = find_free_dir_entry(img)
    for k in range(32):
        img[d + k] = 0
    img[d:d + 8] = base83.encode("ascii")
    img[d + 8:d + 11] = ext83.encode("ascii")
    img[d + 11] = 0x20  # archive attribute
    first_cluster = chain[0]
    img[d + 26] = first_cluster & 0xFF
    img[d + 27] = (first_cluster >> 8) & 0xFF
    img[d + 28] = size & 0xFF
    img[d + 29] = (size >> 8) & 0xFF
    img[d + 30] = (size >> 16) & 0xFF
    img[d + 31] = (size >> 24) & 0xFF


def read_file(img, name):
    """Return a named file's bytes by walking its FAT12 cluster chain."""
    base83, ext83 = split_83(name)
    want = base83.encode("ascii") + ext83.encode("ascii")
    base = ROOT_DIR_LBA * SECTOR
    for e in range(ROOT_ENTRIES):
        i = base + e * 32
        if img[i] in (0x00, 0xE5):
            continue
        if bytes(img[i:i + 11]) == want:
            size = int.from_bytes(img[i + 28:i + 32], "little")
            cluster = img[i + 26] | (img[i + 27] << 8)
            out = bytearray()
            guard = 0
            while 2 <= cluster < 0xFF0 and guard <= TOTAL_CLUSTERS:
                lba = DATA_START_LBA + (cluster - 2) * SECTORS_PER_CLUSTER
                off = lba * SECTOR
                out += img[off:off + BYTES_PER_CLUSTER]
                cluster = fat_get(img, cluster)
                guard += 1
            return bytes(out[:size])
    return None


def to_ascii_basic(text_bytes):
    """Normalize to CRLF line endings + trailing 0x1A EOF (MSX ASCII SAVE)."""
    # Decode leniently, normalize newlines, re-encode latin-1 (MSX is 8-bit).
    s = text_bytes.decode("latin-1")
    s = s.replace("\r\n", "\n").replace("\r", "\n")
    s = s.replace("\n", "\r\n")
    data = s.encode("latin-1")
    if not data.endswith(b"\x1a"):
        data += b"\x1a"
    return data


def parse_add(spec):
    if "=" not in spec:
        raise ValueError(f"--add/--ascii expects NAME.EXT=path, got: {spec}")
    name, path = spec.split("=", 1)
    return name.strip(), path


def build_image(ascii_specs, raw_specs, autoexec_path):
    img = build_blank()
    if autoexec_path:
        with open(autoexec_path, "rb") as f:
            add_file(img, "AUTOEXEC.BAS", to_ascii_basic(f.read()))
    for spec in ascii_specs:
        name, path = parse_add(spec)
        with open(path, "rb") as f:
            add_file(img, name, to_ascii_basic(f.read()))
    for spec in raw_specs:
        name, path = parse_add(spec)
        with open(path, "rb") as f:
            add_file(img, name, f.read())
    return img


def sha256(data):
    return hashlib.sha256(bytes(data)).hexdigest()


def self_check(headless, bios, workdir):
    ok = True
    os.makedirs(workdir, exist_ok=True)

    # (1) SHA-stable: build the same disk twice from fixed content.
    prog = b'10 SCREEN 2\r\n20 CIRCLE(128,96),40\r\n30 GOTO 30\r\n'
    ascii_prog = to_ascii_basic(prog)
    src = os.path.join(workdir, "sc_autoexec.bas")
    with open(src, "wb") as f:
        f.write(prog)
    img1 = build_image([], [], src)
    img2 = build_image([], [], src)
    h1, h2 = sha256(img1), sha256(img2)
    print(f"self-check: build#1 sha256={h1}")
    print(f"self-check: build#2 sha256={h2}")
    if h1 != h2:
        print("self-check FAIL: two builds differ (non-deterministic)")
        ok = False
    else:
        print("self-check PASS: SHA-stable (deterministic)")

    # (2) Writer/reader round-trip at the tool level ("reads the file back").
    back = read_file(img1, "AUTOEXEC.BAS")
    if back == ascii_prog:
        print("self-check PASS: FAT12 read-back byte-identical to written file")
    else:
        print(f"self-check FAIL: read-back mismatch (wrote {len(ascii_prog)}, "
              f"read {len(back) if back is not None else None})")
        ok = False

    # (3) FDC-LOAD boot probe (HONEST, per the M41-S1 Phase-0 finding).
    #
    # The tool-level read-back in (2) already proves the injected file is
    # retrievable from the FAT12. This step additionally BOOTS the disk headless
    # and reports the decoded frame's distinct-colour count. It deliberately does
    # NOT claim "AUTOEXEC ran": the format-blank-disk blank image is NOT a
    # bootable Disk-BASIC disk -- AUTOEXEC.BAS does not auto-run and our headless
    # HANGS (DI:HALT disk-ROM pc=0x4005) at a solid-blue (1-colour) screen, while
    # openMSX sits at blue too. A boot-based load proof requires a genuinely
    # bootable Disk-BASIC image (S2 tool task). See debug/m41/ab/phase0-findings.md.
    dsk = os.path.join(workdir, "sc_disk.dsk")
    with open(dsk, "wb") as f:
        f.write(img1)
    if headless and os.path.isfile(headless) and bios and os.path.isdir(bios):
        droot = os.path.join(workdir, "sc_run")
        os.makedirs(os.path.join(droot, "frames"), exist_ok=True)
        # Windows subprocess needs an absolute exe path (a forward-slash
        # relative path is not resolved against cwd for the image name).
        # M46 (DEC-0071): --stock keeps this deterministic disk-fixture authoring
        # on the accurate (non-fast) FDC timing the disk-write cadence depends on
        # (the emulator's implicit default is now fast-disk on).
        cmd = [os.path.abspath(headless), "--debug-session", os.path.abspath(bios),
               "99999999", "--stock", "--disk", os.path.abspath(dsk), "--frames", "600",
               "--debug-root", os.path.abspath(droot), "--dump-frame", "frame"]
        try:
            subprocess.run(cmd, capture_output=True, timeout=300, check=False)
            colors = frame_color_count(os.path.join(droot, "frames", "frame"))
            if colors > 1:
                print(f"self-check PASS: boot probe -- {colors} distinct colours "
                      f"(drawn content: this blank image booted to a program)")
            else:
                print(f"self-check INFO: boot probe -- {colors} distinct colour "
                      f"(the known blank-disk blue hang; blank image is NOT "
                      f"bootable Disk-BASIC -- see Phase-0 findings, expected)")
        except (OSError, subprocess.SubprocessError) as e:
            print(f"self-check WARN: boot probe could not run: {e}")
    else:
        print("self-check SKIP: boot probe (headless exe / bios not given)")
    return ok


def frame_color_count(frame_path):
    """Distinct DECODED colours in the frame (1 = uniform screen, e.g. the blank-
    disk blue hang; >1 = actual drawn content). Decodes via tools/convert/frame-to-png.py
    so a solid fill correctly counts as 1 (the raw dump's RGB555 word has 2 bytes,
    which naively looks 'non-blank' -- this avoids that false positive)."""
    if not os.path.isfile(frame_path):
        return -1
    png = frame_path + ".probe.png"
    try:
        subprocess.run([sys.executable,
                        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "convert", "frame-to-png.py"),
                        os.path.abspath(frame_path), "-o", os.path.abspath(png)],
                       capture_output=True, timeout=60, check=False)
        from PIL import Image
        im = Image.open(png).convert("RGB")
        cols = im.getcolors(maxcolors=1 << 20)
        return len(cols) if cols is not None else -1
    except Exception:  # noqa: BLE001 -- best-effort probe, never crashes self-check
        return -1


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", help="output .dsk path")
    p.add_argument("--autoexec", help="host .bas written as ASCII AUTOEXEC.BAS")
    p.add_argument("--ascii", action="append", default=[],
                   help="NAME.EXT=path -- add host file as ASCII BASIC (CRLF+0x1A)")
    p.add_argument("--add", action="append", default=[],
                   help="NAME.EXT=path -- add host file raw (BSAVE/binary payload)")
    p.add_argument("--read", help="read NAME.EXT out of --out (or --image) and dump it")
    p.add_argument("--image", help="input .dsk for --read (defaults to --out)")
    p.add_argument("--read-out", help="write --read bytes to this path (else hexdump summary)")
    p.add_argument("--self-check", action="store_true", help="determinism + FDC-LOAD self-check")
    p.add_argument("--headless", default="build/Debug/sony_msx_headless.exe",
                   help="headless exe for the self-check FDC-LOAD proof")
    p.add_argument("--bios", default="bios", help="bios dir for the self-check FDC-LOAD proof")
    p.add_argument("--workdir", default="debug/m41/injector", help="self-check scratch dir")
    args = p.parse_args(argv[1:])

    if args.self_check:
        return 0 if self_check(args.headless, args.bios, args.workdir) else 1

    if args.read:
        img_path = args.image or args.out
        if not img_path:
            print("error: --read needs --image or --out", file=sys.stderr)
            return 2
        with open(img_path, "rb") as f:
            img = bytearray(f.read())
        data = read_file(img, args.read)
        if data is None:
            print(f"error: file not found on image: {args.read}", file=sys.stderr)
            return 1
        if args.read_out:
            with open(args.read_out, "wb") as f:
                f.write(data)
            print(f"read {len(data)} bytes of {args.read} -> {args.read_out} "
                  f"sha256={sha256(data)}")
        else:
            print(f"{args.read}: {len(data)} bytes sha256={sha256(data)}")
            preview = data[:120].decode("latin-1").replace("\r", "\\r").replace("\n", "\\n")
            print(f"  preview: {preview}")
        return 0

    if not args.out:
        print("error: --out required (or use --read / --self-check)", file=sys.stderr)
        return 2
    if not (args.autoexec or args.ascii or args.add):
        print("error: nothing to add (use --autoexec / --ascii / --add)", file=sys.stderr)
        return 2

    img = build_image(args.ascii, args.add, args.autoexec)
    outdir = os.path.dirname(args.out)
    if outdir:
        os.makedirs(outdir, exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(img)
    print(f"wrote {args.out} ({len(img)} bytes) sha256={sha256(img)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
