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

"""M17-S5 openMSX A/B YM2413 (OPLL) register-write probe assembler.

Produces `tests/parity/ym2413_probe.bin`: a real Z80 program that drives
the two-port write protocol (`OUT (#7C),reg ; OUT (#7D),value`) for a
representative address set covering user-patch ($00-$07), F-Num/Block/Key-on
($10-$18/$20-$28), instrument/volume ($30-$38), and rhythm ($0E, $36-$38) --
exactly the address families named in docs/m17-planner-package.md §2.5 / §3
M17-S5.

Also writes a sidecar text file (`tests/parity/ym2413_probe_regs.txt`,
lines `ADDR=xx VALUE=yy` in uppercase hex) listing the EXACT (address, value)
pairs the program writes, in program order, so the PowerShell A/B harness
(tools/openmsx/ym2413-parity.ps1) can compare both emulators' register state
at those addresses without re-deriving the value formula in a second
language (avoids Python/PowerShell formula drift).

Run from a flat-RAM base (0xC000) via this emulator's `--parity-trace` /
`--ym2413-parity` modes and, on the openMSX side, via
tools/openmsx/ym2413-parity.ps1.

Usage:
  python tools/gen/ym2413-probe.py [-o tests/parity/ym2413_probe.bin]
  python tools/gen/ym2413-probe.py --self-check
"""
import argparse
import sys


def probe_addresses():
    """The representative address set (program order), per planner §2.5/§3."""
    addrs = list(range(0x00, 0x08))   # user patch
    addrs.append(0x0E)                # rhythm control
    addrs.extend(range(0x10, 0x19))   # F-Num low, channels 0-8
    addrs.extend(range(0x20, 0x29))   # sustain/key/block/F-Num MSB, channels 0-8
    addrs.extend(range(0x30, 0x39))   # instrument/volume (incl. $36-$38 rhythm volumes)
    return addrs


def probe_value(addr):
    """Deterministic, address-dependent test value (register-accurate contract
    only -- not a real instrument patch; values just need to be distinct and
    reproducible so a write/readback mismatch is unambiguous)."""
    return (addr * 5 + 0x11) & 0xFF


def build_ym2413_write_probe():
    """Assembles: for each (addr,value): LD A,addr ; OUT (0x7C),A ; LD A,value ;
    OUT (0x7D),A. HALT at the end. Returns (program_bytes, [(addr,value), ...])."""
    program = bytearray()
    pairs = []
    for addr in probe_addresses():
        value = probe_value(addr)
        program.extend([0x3E, addr, 0xD3, 0x7C])   # LD A,addr  ; OUT (0x7C),A
        program.extend([0x3E, value, 0xD3, 0x7D])  # LD A,value ; OUT (0x7D),A
        pairs.append((addr, value))
    program.append(0x76)  # HALT
    return bytes(program), pairs


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", default="tests/parity/ym2413_probe.bin",
                        help="output path (default tests/parity/ym2413_probe.bin)")
    parser.add_argument("--regs-output", default=None,
                        help="sidecar regs-list path (default <output>_regs.txt "
                             "next to -o, or tests/parity/ym2413_probe_regs.txt)")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical) and exit")
    args = parser.parse_args(argv[1:])

    program_a, pairs_a = build_ym2413_write_probe()
    if args.self_check:
        program_b, pairs_b = build_ym2413_write_probe()
        ok = program_a == program_b and pairs_a == pairs_b and len(program_a) > 0
        print("self-check:", "PASS" if ok else "FAIL", "length=", len(program_a),
              "addresses=", len(pairs_a))
        return 0 if ok else 1

    with open(args.output, "wb") as f:
        f.write(program_a)
    print("wrote {} ({} bytes, {} register writes)".format(
        args.output, len(program_a), len(pairs_a)))

    regs_output = args.regs_output or "tests/parity/ym2413_probe_regs.txt"
    with open(regs_output, "w", encoding="ascii", newline="\n") as f:
        for addr, value in pairs_a:
            f.write("ADDR={:02X} VALUE={:02X}\n".format(addr, value))
    print("wrote {} ({} entries)".format(regs_output, len(pairs_a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
