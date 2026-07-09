#!/usr/bin/env python3
"""M32 split-screen A/B probe generator (docs/m32-planner-package.md section 2.7).

Emits the openMSX-side Z80 program for tools/openmsx-m32-split-ab.ps1: the
SAME synthetic split-screen protocol as
tests/system/hbf1xv_m32_split_screen_system_test.cpp (GRAPHIC4, IE0+IE1,
R#19=80/R#23=0, IM1 handler dispatching on S#1 FH: line path rewrites
R#23=128, VBlank path re-arms R#23=0 + R#19=80), adapted for injection into
a live MSX-BASIC environment at 0xC000 (page-0-to-RAM slot dance + IM1 hook
install + a Z80-side VRAM signature fill, since the reference side has no
debug_io_write seam).

VRAM signature: GRAPHIC4 page 0, row r byte c = (r + c) & 0xFF for all 256
rows -- every display row identifies its source row, and consecutive source
rows are 2-pixel left-shifts of each other (the property
tools/m32-split-ab-compare.py's palette-independent comparator keys on).

Variants:
  m32_split_basic.bin      -- the real arm (R#0 = 0x16: GRAPHIC4 | IE1)
  m32_split_basic_noie1.bin -- the adversarial arm (R#0 = 0x06, IE1 masked)

Zero game knowledge, zero game bytes; org 0xC000, handler at 0xC140,
counters at 0xC300/0xC301 (FH-seen / VBlank).
"""

import argparse
import pathlib

ORG = 0xC000
HANDLER = 0xC140
FH_COUNTER = 0xC300
VB_COUNTER = 0xC301
SPLIT_LINE = 80


def vdp_reg(reg, value):
    """LD A,value / OUT(99) / LD A,0x80|reg / OUT(99)."""
    return bytes([0x3E, value, 0xD3, 0x99, 0x3E, 0x80 | reg, 0xD3, 0x99])


def build(ie1: bool) -> bytes:
    image = bytearray(0x200)

    main = bytearray()
    main += bytes([0xF3])                    # DI
    main += bytes([0xED, 0x56])              # IM 1
    main += bytes([0x31, 0x80, 0xF3])        # LD SP,0xF380
    # Page 0 -> primary slot 3, sub-slot 0 (the RAM mapper; mapper register
    # #FC's BASIC default segment). Page 3 is already slot 3 in BASIC, so
    # 0xFFFF is the slot-3 sub-slot register (inverted readback).
    main += bytes([0x3A, 0xFF, 0xFF])        # LD A,(0xFFFF)
    main += bytes([0x2F])                    # CPL
    main += bytes([0xE6, 0xFC])              # AND 0xFC   (page0 sub -> 0)
    main += bytes([0x32, 0xFF, 0xFF])        # LD (0xFFFF),A
    main += bytes([0xDB, 0xA8])              # IN A,(0xA8)
    main += bytes([0xE6, 0xFC])              # AND 0xFC
    main += bytes([0xF6, 0x03])              # OR 0x03    (page0 -> slot 3)
    main += bytes([0xD3, 0xA8])              # OUT (0xA8),A
    # Install the IM1 hook: JP HANDLER at 0x0038.
    main += bytes([0x3E, 0xC3])              # LD A,0xC3
    main += bytes([0x32, 0x38, 0x00])        # LD (0x0038),A
    main += bytes([0x21, HANDLER & 0xFF, HANDLER >> 8])  # LD HL,HANDLER
    main += bytes([0x22, 0x39, 0x00])        # LD (0x0039),HL
    # Zero the observation counters.
    main += bytes([0xAF])                    # XOR A
    main += bytes([0x32, FH_COUNTER & 0xFF, FH_COUNTER >> 8])
    main += bytes([0x32, VB_COUNTER & 0xFF, VB_COUNTER >> 8])
    # VDP setup: GRAPHIC4 (no IE1 yet), sprites off (R#8 SPD) WITH the VR
    # bit (bit3) KEPT SET -- R#8 = 0x0A, not 0x02: VR selects the 64K-DRAM
    # VRAM organization and openMSX remaps VRAM addressing when VR = 0
    # (references/openmsx-21.0/src/video/VDP.cc, the R#8 VR handling);
    # clobbering it blanks the real reference's display even though the
    # register/VRAM state reads back correctly. SCREEN5 page-0 name base
    # (R#2 = 0x1F canonical), R#14 = 0, R#23 = 0.
    main += vdp_reg(0, 0x06)
    main += vdp_reg(8, 0x0A)
    main += vdp_reg(2, 0x1F)
    main += vdp_reg(14, 0x00)
    main += vdp_reg(23, 0x00)
    # VRAM write address 0.
    main += bytes([0x3E, 0x00, 0xD3, 0x99])  # addr low
    main += bytes([0x3E, 0x40, 0xD3, 0x99])  # addr high | write
    # Signature fill: 256 rows x 128 bytes, byte = (row + col) & 0xFF.
    main += bytes([0x16, 0x00])              # LD D,0        ; row
    row_loop = len(main)
    main += bytes([0x5A])                    # LD E,D
    main += bytes([0x06, 0x80])              # LD B,0x80
    col_loop = len(main)
    main += bytes([0x7B])                    # LD A,E
    main += bytes([0xD3, 0x98])              # OUT (0x98),A
    main += bytes([0x1C])                    # INC E
    main += bytes([0x10, (col_loop - (len(main) + 2)) & 0xFF])  # DJNZ col
    main += bytes([0x14])                    # INC D
    main += bytes([0x7A])                    # LD A,D
    main += bytes([0xB7])                    # OR A
    main += bytes([0x20, (row_loop - (len(main) + 2)) & 0xFF])  # JR NZ,row
    # Arm the split and spin.
    main += vdp_reg(19, SPLIT_LINE)
    main += vdp_reg(0, 0x16 if ie1 else 0x06)
    main += vdp_reg(1, 0x60)                 # BL | IE0
    main += bytes([0xFB])                    # EI
    main += bytes([0x18, 0xFE])              # JR $

    handler = bytearray()
    handler += bytes([0xF5, 0xC5])           # PUSH AF / PUSH BC
    handler += vdp_reg(15, 1)
    handler += bytes([0xDB, 0x99])           # IN A,(0x99)   ; S#1 (clears FH)
    handler += bytes([0x47])                 # LD B,A
    handler += vdp_reg(15, 0)
    handler += bytes([0x78])                 # LD A,B
    handler += bytes([0xE6, 0x01])           # AND 1
    jrz_at = len(handler)
    handler += bytes([0x28, 0x00])           # JR Z,vblank (patched below)
    # Line-interrupt path: count + R#23 = 128 (the split).
    handler += bytes([0x3A, FH_COUNTER & 0xFF, FH_COUNTER >> 8])
    handler += bytes([0x3C])
    handler += bytes([0x32, FH_COUNTER & 0xFF, FH_COUNTER >> 8])
    handler += vdp_reg(23, 128)
    handler += bytes([0xC1, 0xF1, 0xFB, 0xED, 0x4D])  # POP BC/POP AF/EI/RETI
    vblank_at = len(handler)
    handler[jrz_at + 1] = (vblank_at - (jrz_at + 2)) & 0xFF
    # VBlank path: read S#0 (clears F), re-arm R#23 = 0 AND R#19 = 80 (the
    # (R#19 - R#23) & 0xFF relation makes the re-arm mandatory), count.
    handler += bytes([0xDB, 0x99])           # IN A,(0x99)   ; S#0
    handler += vdp_reg(23, 0)
    handler += vdp_reg(19, SPLIT_LINE)
    handler += bytes([0x3A, VB_COUNTER & 0xFF, VB_COUNTER >> 8])
    handler += bytes([0x3C])
    handler += bytes([0x32, VB_COUNTER & 0xFF, VB_COUNTER >> 8])
    handler += bytes([0xC1, 0xF1, 0xFB, 0xED, 0x4D])

    assert len(main) <= HANDLER - ORG, f"main too long: {len(main)}"
    image[0:len(main)] = main
    image[HANDLER - ORG:HANDLER - ORG + len(handler)] = handler
    return bytes(image)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--out-dir", default="build/m32-ab")
    args = parser.parse_args()
    out = pathlib.Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for name, ie1 in (("m32_split_basic.bin", True), ("m32_split_basic_noie1.bin", False)):
        data = build(ie1)
        (out / name).write_bytes(data)
        print(f"gen-m32-split-probe: wrote {out / name} ({len(data)} bytes, org 0x{ORG:04X})")


if __name__ == "__main__":
    main()
