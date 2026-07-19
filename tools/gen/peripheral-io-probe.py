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

"""M18-S5 openMSX A/B peripheral-I/O probe assembler (Kanji font ROM + printer
+ cassette interface, Sony HB-F1XV).

Produces `tests/parity/m18_peripheral_io_probe.bin`: ONE combined Z80 program
(mirrors the M15 "one combined multi-device A/B trace" precedent) exercising
all three M18-S5 subjects (docs/m18-planner-package.md §2.6) back-to-back:

  Section A -- Kanji font ROM, JIS1 half (#D8/#D9): select two representative
    block-aligned addresses (near the start and near the top of the 128 KB
    JIS1 half) and read each 4 times (exercising the auto-increment). The
    byte read into A on each `IN A,(#D9)` is directly visible in the
    per-instruction trace's `af=` field -- content parity falls straight out
    of the SAME architectural-trace diff used for the addressing protocol.
  Section B -- Kanji font ROM, JIS2 half (#DA/#DB): the same shape, at two
    representative addresses in the 128 KB JIS2 half.
  Section C -- Printer write path (#90/#91): OUT (#91),data / OUT (#90),
    strobe-pattern for a few representative bytes -- NEVER reads the status
    bit (#90 read), which is a deliberate, disclosed divergence from
    openMSX's *unplugged* default (A-M18-7) kept out of this probe by design.
  Section D -- Cassette idle-state + write path: `IN A,(#A2)` with PSG R14
    selected (both emulators default idle-high -- genuinely comparable,
    A-M18-11), then representative PPI port-C (#AA) bit4/bit5 writes,
    re-checked with a second `IN A,(#A2)` (write-sequence parity, mirrors the
    printer/OPLL write-only technique).

Run from a flat-RAM base (0xC000) via this emulator's `--parity-trace` and,
on the openMSX side, via tools/openmsx/peripheral-io-parity.ps1 (an
extension of the tools/openmsx/io-parity.ps1 mechanism -- NO new CLI mode was
needed since every read here lands in a CPU register already captured by the
architectural per-instruction trace).

Usage:
  python tools/gen/peripheral-io-probe.py [-o tests/parity/m18_peripheral_io_probe.bin]
  python tools/gen/peripheral-io-probe.py --self-check
"""
import argparse
import sys


def _ld_a(value):
    return [0x3E, value & 0xFF]


def _out(port, value):
    # LD A,value ; OUT (port),A
    return _ld_a(value) + [0xD3, port & 0xFF]


def _out_reg_a(port):
    # OUT (port),A -- reuses whatever is currently in A (no reload).
    return [0xD3, port & 0xFF]


def _in_a(port):
    return [0xDB, port & 0xFF]


def _select_adr1(addr):
    """#D8 (low, bits5-10) then #D9 (high, bits11-16) -- JIS1 half."""
    lo = (addr >> 5) & 0x3F
    hi = (addr >> 11) & 0x3F
    return _out(0xD8, lo) + _out(0xD9, hi)


def _select_adr2(addr):
    """#DA (low, bits5-10) then #DB (high, bits11-16) -- JIS2 half."""
    lo = (addr >> 5) & 0x3F
    hi = (addr >> 11) & 0x3F
    return _out(0xDA, lo) + _out(0xDB, hi)


# Representative addresses (block-aligned; the low 5 bits are cleared by
# either write per A-M18-3, so any value works -- these are chosen to span
# both halves including near-boundary offsets, matching the integration test
# tests/integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp).
KANJI_JIS1_ADDRESSES = (0x00020, 0x1FFE0)
KANJI_JIS2_ADDRESSES = (0x20020, 0x2FFE0)
KANJI_READS_PER_ADDRESS = 4

PRINTER_BYTES = (0x48, 0x49, 0x21)  # 'H', 'I', '!'


def build_probe():
    program = []

    # --- Section A: Kanji JIS1 half (#D8/#D9). ---
    for addr in KANJI_JIS1_ADDRESSES:
        program += _select_adr1(addr)
        for _ in range(KANJI_READS_PER_ADDRESS):
            program += _in_a(0xD9)

    # --- Section B: Kanji JIS2 half (#DA/#DB). ---
    for addr in KANJI_JIS2_ADDRESSES:
        program += _select_adr2(addr)
        for _ in range(KANJI_READS_PER_ADDRESS):
            program += _in_a(0xDB)

    # --- Section C: printer write path (#90/#91) -- write-only, NEVER reads
    #     the status bit (A-M18-7 divergence kept out of this probe). ---
    for byte in PRINTER_BYTES:
        program += _out(0x91, byte)   # data
        program += _out(0x90, 0x00)   # strobe low (falling edge)
        program += _out(0x90, 0x01)   # strobe high (idle)

    # --- Section D: cassette idle-state + PPI port-C write path. ---
    program += _out(0xA0, 0x0E)   # select PSG R14
    program += _in_a(0xA2)        # idle read BEFORE any PPI writes (bit7=1 both sides)
    program += _out(0xAA, 0x10)   # port C bit4=1 (motor off)
    program += _out(0xAA, 0x20)   # port C bit4=0,bit5=1 (motor on, output high)
    program += _out(0xAA, 0x00)   # port C all clear
    program += _out(0xA0, 0x0E)   # re-select PSG R14 (defensive; unchanged)
    program += _in_a(0xA2)        # idle read AFTER PPI writes (still bit7=1, A-M18-11)

    program.append(0x76)  # HALT
    return bytes(program)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", default="tests/parity/m18_peripheral_io_probe.bin",
                        help="output path (default tests/parity/m18_peripheral_io_probe.bin)")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical) and exit")
    args = parser.parse_args(argv[1:])

    program_a = build_probe()
    if args.self_check:
        program_b = build_probe()
        ok = program_a == program_b and len(program_a) > 0
        print("self-check:", "PASS" if ok else "FAIL", "length=", len(program_a))
        return 0 if ok else 1

    with open(args.output, "wb") as f:
        f.write(program_a)
    print("wrote {} ({} bytes)".format(args.output, len(program_a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
