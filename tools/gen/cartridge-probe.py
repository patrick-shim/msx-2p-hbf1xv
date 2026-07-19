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

"""M19-S6 openMSX A/B cartridge-loading probe assembler (backlog B7).

Produces TWO deterministic artifacts:

  1. `tests/parity/m19_cartridge.rom` -- a SYNTHETIC `Generic8kB` cartridge
     image (planner A-M19-10 track 1 precedent: synthetic, hand-authored
     fixtures for every byte-exact protocol claim, never a real, provenance-
     uncertain file). 4 banks of 8 KB; bank N's every byte equals N, a
     clearly distinguishable marker per bank
     (references/openmsx-21.0/src/memory/RomGeneric8kB.cc:7-36).

  2. `tests/parity/m19_cartridge_probe.bin` -- a real Z80 driver program that:
       - `OUT (#A8),A` with A=0xF7, repointing CPU page 1 (0x4000-0x7FFF) at
         primary slot 1 while pages 0/2/3 stay at primary slot 3 RAM (this
         project's OWN established SlotBus bit layout,
         src/devices/chipset/slot_bus.cpp:47-49 -- 2 bits/page, page N at
         bits [2N,2N+1]: page0=3,page1=1,page2=3,page3=3 ->
         3|(1<<2)|(3<<4)|(3<<6) = 0xF7);
       - reads the reset-default marker at 0x4000 (bank 0, into A);
       - bank-switches slot 2 (CPU 0x4000, `slot = addr>>13`) to bank 3 via a
         plain `LD (0x4000),A` write (RomGeneric8kB.cc:24-27);
       - re-reads 0x4000 (now bank 3) and reads the UNAFFECTED 0x6000 (still
         bank 1) -- a genuine, content-bearing bank-switch + read-back
         sequence;
       - HALTs.
     Every `LD A,(addr)` instruction's result lands in the accumulator, so
     the EXISTING per-instruction CpuTraceSink trace (AF register field)
     captures the read-back byte VALUES directly -- no separate memory-dump
     subject is needed; an empty PC/register/flags diff across the whole
     trace (tools/analyze/trace-diff.py, exactly the M13/M16/M17/M18 mechanism) is
     simultaneously the architectural parity proof AND the content-bearing
     proof (planner §2.7: "both sides load the identical authored file, so
     expected bytes are fully known in advance").

Run via this emulator's `--parity-trace <probe.bin> C000 <max_steps> <out>
--cart1 <cartridge.rom> --cart1-type 8kB` and, on the openMSX side, via
tools/openmsx/cartridge-parity.ps1 (which mounts the SAME cartridge file
via `-carta`, empirically confirmed to land in this machine's primary slot 1 --
see docs/m19-parity-trace-diff.md for the live WSL Tcl slot-lettering probe
this finding is grounded on, R-M19-6).

Usage:
  python tools/gen/cartridge-probe.py
  python tools/gen/cartridge-probe.py --self-check
"""
import argparse
import sys

BANK_SIZE = 0x2000
NUM_BANKS = 4


def build_cartridge_image():
    """4 x 8 KB banks; bank N's every byte == N (A-M19-10 track 1 marker)."""
    image = bytearray()
    for bank in range(NUM_BANKS):
        image.extend(bytes([bank & 0xFF]) * BANK_SIZE)
    return bytes(image)


def build_driver_program():
    """OUT(#A8),0xF7 ; LD A,(0x4000) ; LD (0x4000),3 ; LD A,(0x4000) ;
    LD A,(0x6000) ; HALT. Returns the raw Z80 bytes."""
    program = bytearray()
    program.extend([0x3E, 0xF7])              # LD A, 0xF7
    program.extend([0xD3, 0xA8])              # OUT (#A8), A
    program.extend([0x3A, 0x00, 0x40])        # LD A, (0x4000)  -- reset default (bank0 marker)
    program.extend([0x3E, 0x03])              # LD A, 3
    program.extend([0x32, 0x00, 0x40])        # LD (0x4000), A  -- bank-switch slot2 -> bank3
    program.extend([0x3A, 0x00, 0x40])        # LD A, (0x4000)  -- now bank3 marker
    program.extend([0x3A, 0x00, 0x60])        # LD A, (0x6000)  -- unaffected, still bank1 marker
    program.append(0x76)                       # HALT
    return bytes(program)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cart-output", default="tests/parity/m19_cartridge.rom",
                        help="synthetic cartridge image output path")
    parser.add_argument("--probe-output", default="tests/parity/m19_cartridge_probe.bin",
                        help="Z80 driver program output path")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical) and exit")
    args = parser.parse_args(argv[1:])

    image_a = build_cartridge_image()
    program_a = build_driver_program()
    if args.self_check:
        image_b = build_cartridge_image()
        program_b = build_driver_program()
        ok = (image_a == image_b and program_a == program_b and
              len(image_a) == NUM_BANKS * BANK_SIZE and len(program_a) > 0)
        print("self-check:", "PASS" if ok else "FAIL",
              "image_len=", len(image_a), "program_len=", len(program_a))
        return 0 if ok else 1

    with open(args.cart_output, "wb") as f:
        f.write(image_a)
    print("wrote {} ({} bytes, {} banks)".format(args.cart_output, len(image_a), NUM_BANKS))

    with open(args.probe_output, "wb") as f:
        f.write(program_a)
    print("wrote {} ({} bytes)".format(args.probe_output, len(program_a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
