#!/usr/bin/env python3
"""M29-S6 openMSX A/B KonamiSCC + SCC probe assembler (backlog G1).

Produces TWO deterministic artifacts (the M19 gen-m19-cartridge-probe.py
pattern, synthetic fixtures only -- A-M29-4):

  1. `tests/parity/m29_scc.rom` -- a SYNTHETIC KonamiSCC cartridge image:
     8 banks of 8 KB; bank N's every byte equals N.

  2. `tests/parity/m29_scc_probe.bin` -- a real Z80 driver program run at
     0xC000 on BOTH sides. Every `LD A,(addr)` readback lands in AF, so the
     per-instruction trace diff (tools/trace-diff.py) is simultaneously the
     architectural AND the content-bearing parity proof (M19 mechanism).

     IMPORTANT DESIGN NOTE (BIOS-boot insensitivity): the openMSX side boots
     the real BIOS for several seconds before register forcing, and the MSX
     BIOS's own RAM search may write into 0x8000-0xBFFF of the cartridge
     slot -- which on a KonamiSCC cart would bank-switch pages 4/5 and could
     even flip the SCC enable latch. The driver therefore NORMALIZES the
     full mapper state itself (explicit writes to all four bank registers,
     with a disabling value at 0x9000) BEFORE any sensitive readback --
     nothing below relies on reset-default bank state surviving the boot.

     Probe sequence (expected AF-visible readbacks in comments):
       OUT (#A8),0xD5           ; pages 0/1/2 -> primary slot 1 (the cart),
                                ; page 3 stays primary slot 3 RAM (program).
                                ; 0xD5 = 1|(1<<2)|(1<<4)|(3<<6), this
                                ; project's own SlotBus 2-bits/page layout.
       LD (0x9000),2            ; bank4=2 AND SCC disabled (2&0x3F != 0x3F)
       LD (0xB000),3            ; bank5=3
       LD (0x5000),0            ; bank2=0
       LD (0x7000),1            ; bank3=1   -- state now fully normalized
       LD A,(0x4000)            ; -> 00 (bank2)
       LD A,(0x0000)            ; -> 02  KonamiSCC-specific mirror: window
                                ;        slot 0 tracks PAGE 4 (R-M29-1)
       LD A,(0x2000)            ; -> 03  window slot 1 tracks PAGE 5
       LD (0x5400),5            ; mid-window bank write (0x800-wide decode)
       LD A,(0x4000)            ; -> 05
       LD (0x5800),1            ; OUTSIDE the decode window -> ignored
       LD A,(0x4000)            ; -> 05 (unchanged)
       LD (0x9000),0x3F         ; SCC enable AND bank4=0x3F&7=7 (both-effects)
       LD A,(0x8000)            ; -> 07
       LD A,(0x0000)            ; -> 07 (mirror tracks the live switch)
       32x LD (HL),0x55 fill    ; ch1 waveform 0x9800-0x981F all 0x55
       LD A,(0x9800)            ; -> 55 (wave readback)
       LD A,(0x9900)            ; -> 55 (the 0x9900 window mirror, A-M29-1;
                                ;        fMSX would return ROM here, s9.5)
       LD (0x9920),0x66         ; write VIA the mirror -> ch2 wave[0]
       LD A,(0x9820)            ; -> 66 (landed in the base map)
       LD A,(0x9880)            ; -> FF (freq/vol block is write-only)
       LD A,(0x98A0)            ; -> FF (dead region 0xA0-0xDF)
       LD (0x9000),0x3E         ; disable AND bank4=6
       LD A,(0x9800)            ; -> 06 (ROM again -- window gone)
       LD (0x9000),0xBF         ; MASKED enable compare: 0xBF&0x3F==0x3F
                                ; enables too (A-M29-2; fMSX requires exact
                                ; 0x3F, s9.6) AND bank4=7
       LD A,(0x9800)            ; -> 55 (SCC window back)
       LD A,(0x9FE0)            ; -> FF AND acts as deform:=0xFF (Pazos
                                ;        read-as-write; fires via readMem on
                                ;        both sides -- a REAL CPU read, not
                                ;        a debugger peek)
       LD (0x9805),0x77         ; blocked: wave RAM read-only under 0xC0
       LD A,(0x9805)            ; -> 55 (uniform ch1 wave, so the readback
                                ;        is rotation-shift-INDEPENDENT --
                                ;        deliberately timing-safe on both
                                ;        sides)
       HALT

Run side A via `--parity-trace tests/parity/m29_scc_probe.bin C000 400 <out>
--cart1 tests/parity/m29_scc.rom --cart1-type KonamiSCC`; side B via
tools/openmsx-m29-scc-parity.ps1 (`-carta <rom> -romtype KonamiSCC`, the
A-M29-3-verified forcing syntax).

Usage:
  python tools/gen-m29-scc-probe.py
  python tools/gen-m29-scc-probe.py --self-check
"""
import argparse
import sys

BANK_SIZE = 0x2000
NUM_BANKS = 8


def build_cartridge_image():
    """8 x 8 KB banks; bank N's every byte == N."""
    image = bytearray()
    for bank in range(NUM_BANKS):
        image.extend(bytes([bank & 0xFF]) * BANK_SIZE)
    return bytes(image)


def _ld_a(program, value):
    program.extend([0x3E, value & 0xFF])


def _st(program, addr):
    program.extend([0x32, addr & 0xFF, (addr >> 8) & 0xFF])


def _ld_from(program, addr):
    program.extend([0x3A, addr & 0xFF, (addr >> 8) & 0xFF])


def build_driver_program():
    """See the module docstring for the annotated listing."""
    p = bytearray()
    _ld_a(p, 0xD5)
    p.extend([0xD3, 0xA8])          # OUT (#A8),A

    # Normalize the full mapper state (BIOS-boot insensitivity).
    _ld_a(p, 0x02); _st(p, 0x9000)  # bank4=2, SCC disabled
    _ld_a(p, 0x03); _st(p, 0xB000)  # bank5=3
    _ld_a(p, 0x00); _st(p, 0x5000)  # bank2=0
    _ld_a(p, 0x01); _st(p, 0x7000)  # bank3=1

    _ld_from(p, 0x4000)             # -> 00
    _ld_from(p, 0x0000)             # -> 02 (mirror slot0 <- page4)
    _ld_from(p, 0x2000)             # -> 03 (mirror slot1 <- page5)

    _ld_a(p, 0x05); _st(p, 0x5400)  # mid-window decode
    _ld_from(p, 0x4000)             # -> 05
    _ld_a(p, 0x01); _st(p, 0x5800)  # outside window -> ignored
    _ld_from(p, 0x4000)             # -> 05

    _ld_a(p, 0x3F); _st(p, 0x9000)  # enable + bank4=7 (both effects)
    _ld_from(p, 0x8000)             # -> 07
    _ld_from(p, 0x0000)             # -> 07

    # Fill ch1 waveform with 0x55 (uniform => later reads shift-independent).
    p.extend([0x21, 0x00, 0x98])    # LD HL,0x9800
    p.extend([0x06, 0x20])          # LD B,0x20
    _ld_a(p, 0x55)
    p.extend([0x77, 0x23, 0x10, 0xFC])  # LD (HL),A / INC HL / DJNZ -4

    _ld_from(p, 0x9800)             # -> 55
    _ld_from(p, 0x9900)             # -> 55 (A-M29-1 mirror)
    _ld_a(p, 0x66); _st(p, 0x9920)  # write via mirror -> ch2 wave[0]
    _ld_from(p, 0x9820)             # -> 66
    _ld_from(p, 0x9880)             # -> FF (write-only block)
    _ld_from(p, 0x98A0)             # -> FF (dead region)

    _ld_a(p, 0x3E); _st(p, 0x9000)  # disable + bank4=6
    _ld_from(p, 0x9800)             # -> 06 (ROM)
    _ld_a(p, 0xBF); _st(p, 0x9000)  # masked enable (A-M29-2) + bank4=7
    _ld_from(p, 0x9800)             # -> 55

    _ld_from(p, 0x9FE0)             # -> FF; deform := 0xFF (read-as-write)
    _ld_a(p, 0x77); _st(p, 0x9805)  # blocked (read-only)
    _ld_from(p, 0x9805)             # -> 55

    p.append(0x76)                  # HALT
    return bytes(p)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cart-output", default="tests/parity/m29_scc.rom")
    parser.add_argument("--probe-output", default="tests/parity/m29_scc_probe.bin")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv[1:])

    image_a = build_cartridge_image()
    program_a = build_driver_program()
    if args.self_check:
        ok = (image_a == build_cartridge_image() and program_a == build_driver_program() and
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
