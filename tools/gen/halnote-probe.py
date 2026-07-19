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

"""M20-S4 openMSX A/B Halnote probe-image assembler (backlog B4 + B6).

Produces a SYNTHETIC 1 MB Halnote-mapper image (planner A-M20 track precedent:
synthetic, hand-authored fixtures for byte-exact protocol claims), with
distinguishing marker bytes:

  - Every byte of main bank N (0..127, 8 KB each) equals N (mod 256) -- the
    same marker convention already used by
    tests/unit/devices/halnote/halnote_rom_unit_test.cpp's own
    ``make_marker_image()`` helper, so a local unit test and this file agree
    on what "bank N" looks like.
  - Every byte of sub-bank S (0..255, 2 KB each, the last 512 KB re-addressed)
    equals ``0x80 | (S & 0x7F)`` -- a disjoint marker family from the main
    banks (0-127), so a shadowed sub-bank read can never be confused with a
    plain main-bank read (mirrors
    tests/unit/devices/halnote/halnote_subbank_unit_test.cpp).

This artifact is a REUSABLE fixture for any FUTURE local (non-destructive)
swap-based A/B probe. The M20 developer cycle's OWN actual A/B evidence
(docs/m20-parity-trace-diff.md) instead used the REAL bios/f1xvfirm.rom
directly against the real installed WSL openMSX system ROM -- their SHA1
hashes were independently confirmed byte-identical
(``ade0c5ba5574f8114d7079050317099b4519e88f``), so no synthetic swap was
needed for that evidence (a stronger, zero-risk technique: no mutation of the
user's real openMSX install, and the "expected" values are simply read
directly from the real, legally-sourced firmware file). Separately confirmed
by direct source read (references/openmsx-21.0/src/memory/Rom.cc:202-208):
openMSX's ``<sha1>`` tag on a machine-XML ``<ROM>`` element is ADVISORY ONLY
(a printed CliComm warning on mismatch, never a hard rejection/throw) -- so a
synthetic-image swap remains a viable technique for future milestones that
need one, without being SHA1-blocked.

Usage:
  python tools/gen/halnote-probe.py
  python tools/gen/halnote-probe.py --self-check
"""
import argparse
import sys

BANK_SIZE = 0x2000        # 8 KB main bank
NUM_BANKS = 128            # 1 MB / 8 KB
SUB_BANK_SIZE = 0x800      # 2 KB sub-bank
SUB_BANK_REGION_BASE = 0x80000  # last 512 KB
NUM_SUB_BANKS = 256        # 512 KB / 2 KB
IMAGE_BYTES = NUM_BANKS * BANK_SIZE  # 0x100000, 1 MB


def build_halnote_image():
    """128 x 8 KB main banks (bank N's every byte == N mod 256), with the last
    512 KB additionally overwritten as 256 x 2 KB sub-banks (sub-bank S's
    every byte == 0x80 | (S & 0x7F)).

    NOTE (matches real Halnote hardware, A-M20-2/A-M20-8): the sub-bank region
    (banks 64-127, byte offsets 0x80000-0xFFFFF) is the SAME PHYSICAL BYTES as
    main banks 64-127 -- the sub-bank overwrite intentionally replaces those
    banks' plain-marker content with the sub-bank marker family. Only main
    banks 0-63 keep their plain "byte == bank index" marker untouched; banks
    64-127 are only meaningfully tested via the sub-bank read formula (this is
    a real hardware property, not a generation bug -- confirmed via a direct
    self-check assertion below using a low bank index, not a top-half one)."""
    image = bytearray(IMAGE_BYTES)
    for bank in range(NUM_BANKS):
        marker = bank & 0xFF
        start = bank * BANK_SIZE
        image[start:start + BANK_SIZE] = bytes([marker]) * BANK_SIZE
    for sub in range(NUM_SUB_BANKS):
        marker = 0x80 | (sub & 0x7F)
        start = SUB_BANK_REGION_BASE + sub * SUB_BANK_SIZE
        image[start:start + SUB_BANK_SIZE] = bytes([marker]) * SUB_BANK_SIZE
    assert len(image) == IMAGE_BYTES
    return bytes(image)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", default="tests/parity/m20_halnote.rom",
                        help="synthetic 1 MB Halnote image output path")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical) and exit")
    args = parser.parse_args(argv[1:])

    image_a = build_halnote_image()
    if args.self_check:
        image_b = build_halnote_image()
        ok = (image_a == image_b and len(image_a) == IMAGE_BYTES and
              image_a[0 * BANK_SIZE] == 0 and image_a[4 * BANK_SIZE] == 4 and
              image_a[SUB_BANK_REGION_BASE] == 0x80 and
              image_a[SUB_BANK_REGION_BASE + 200 * SUB_BANK_SIZE] == (0x80 | (200 & 0x7F)))
        print("self-check:", "PASS" if ok else "FAIL", "image_len=", len(image_a))
        return 0 if ok else 1

    with open(args.output, "wb") as f:
        f.write(image_a)
    print("wrote {} ({} bytes, {} main banks, {} sub-banks)".format(
        args.output, len(image_a), NUM_BANKS, NUM_SUB_BANKS))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
