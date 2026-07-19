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

"""M16-S6 openMSX A/B FDC register-sequence probe assembler.

Produces `tests/parity/m16_fdc_probe.bin`: the EXACT SAME Z80 byte sequence as
`build_restore_read_sector_probe()` in
`tests/integration/machine/hbf1xv_m16_fdc_integration_test.cpp` (already proven,
via that passing test, to drive the Sony WD2793 FDC end-to-end over the real
M11 bus: page-1 -> slot(3,2), side/drive/motor latch, Type I Restore, Type II
Read Sector of LBA 0, streaming 512 DRQ bytes into RAM). Assembling it here
(rather than re-deriving it by hand) guarantees the openMSX-side probe is
byte-identical to the internally-tested program (planner Section 7 "Subject
sequence").

Run from a flat-RAM base (0xC000) via this emulator's `--parity-trace` mode
(which already calls `map_flat_ram()` first) and, on the openMSX side, via the
existing `tools/openmsx/trace-parity.ps1` harness convention, with the
IDENTICAL `tests/parity/m16_boot.dsk` (see tools/gen/boot-disk.py) mounted
as drive A so both emulators service the Read Sector from the same medium.

Usage:
  python tools/gen/fdc-probe.py [-o tests/parity/m16_fdc_probe.bin]
  python tools/gen/fdc-probe.py --self-check
"""
import argparse
import sys

KBASE = 0xC000
KBUFFER = 0xC200  # 512-byte landing zone, page-3 RAM


class Prog:
    def __init__(self):
        self.bytes = bytearray()

    def emit(self, values):
        self.bytes.extend(values)

    def here(self):
        return len(self.bytes)

    def emit_jr_back(self, opcode, target):
        self.bytes.append(opcode)
        next_pc = len(self.bytes) + 1
        disp = (target - next_pc) & 0xFF
        self.bytes.append(disp)


def build_restore_read_sector_probe():
    p = Prog()
    p.emit([0x3E, 0x08])          # LD A,0x08
    p.emit([0x32, 0xFF, 0xFF])    # LD (0xFFFF),A ; page1->slot3-2(FDC), page3 stays sub0
    p.emit([0x3E, 0x00])          # LD A,0x00
    p.emit([0x32, 0xFC, 0x7F])    # LD (0x7FFC),A ; side = 0
    p.emit([0x3E, 0x80])          # LD A,0x80
    p.emit([0x32, 0xFD, 0x7F])    # LD (0x7FFD),A ; drive-select=A, motor on
    p.emit([0x3E, 0x00])          # LD A,0x00
    p.emit([0x32, 0xF8, 0x7F])    # LD (0x7FF8),A ; Restore (Type I)
    restore_wait = p.here()
    p.emit([0x3A, 0xF8, 0x7F])    # LD A,(0x7FF8) ; status
    p.emit([0xE6, 0x01])          # AND 0x01      ; Busy
    p.emit_jr_back(0x20, restore_wait)  # JR NZ, restore_wait
    p.emit([0x3E, 0x00])          # LD A,0x00
    p.emit([0x32, 0xF9, 0x7F])    # LD (0x7FF9),A ; TR = 0
    p.emit([0x3E, 0x01])          # LD A,0x01
    p.emit([0x32, 0xFA, 0x7F])    # LD (0x7FFA),A ; SR = 1
    p.emit([0x3E, 0x80])          # LD A,0x80
    p.emit([0x32, 0xF8, 0x7F])    # LD (0x7FF8),A ; Read Sector (Type II)
    p.emit([0x21, KBUFFER & 0xFF, (KBUFFER >> 8) & 0xFF])  # LD HL,KBUFFER
    p.emit([0x01, 0x00, 0x02])    # LD BC,0x0200  ; 512
    read_wait = p.here()
    p.emit([0x3A, 0xFF, 0x7F])    # LD A,(0x7FFF) ; interface status
    p.emit([0xE6, 0x80])          # AND 0x80      ; !DTRQ (active-low)
    p.emit_jr_back(0x20, read_wait)  # JR NZ, read_wait (wait for DRQ)
    p.emit([0x3A, 0xFB, 0x7F])    # LD A,(0x7FFB) ; data register
    p.emit([0x77])                 # LD (HL),A
    p.emit([0x23])                 # INC HL
    p.emit([0x0B])                 # DEC BC
    p.emit([0x78])                 # LD A,B
    p.emit([0xB1])                 # OR C
    p.emit_jr_back(0x20, read_wait)  # JR NZ, read_wait (next byte)
    p.emit([0x76])                 # HALT
    return bytes(p.bytes)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", default="tests/parity/m16_fdc_probe.bin",
                        help="output path (default tests/parity/m16_fdc_probe.bin)")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical) and exit")
    args = parser.parse_args(argv[1:])

    program_a = build_restore_read_sector_probe()
    if args.self_check:
        program_b = build_restore_read_sector_probe()
        ok = program_a == program_b and len(program_a) > 0
        print("self-check:", "PASS" if ok else "FAIL", "length=", len(program_a))
        return 0 if ok else 1

    with open(args.output, "wb") as f:
        f.write(program_a)
    print("wrote {} ({} bytes)".format(args.output, len(program_a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
