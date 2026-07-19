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

"""M21-S7 openMSX A/B VDP-render probe assembler (backlog D1/D5/D6/D7).

Produces four small, flat-RAM Z80 programs under `tests/parity/`, each
exercising ONE of the M21 A/B subjects named in docs/m21-planner-package.md
§2.7:

  m21_vdp_render_palette_probe.bin  -- 16-color palette expansion sweep
                                        (writes palette entries 0-7 to
                                        r=g=b=index via #9A, covering all 8
                                        A-M21-3 boundary values).
  m21_vdp_render_planar_probe.bin   -- GRAPHIC6 (planar) mode + a CPU-port
                                        write at logical address 0/1 -- the
                                        D7 CPU-port planar-transform subject
                                        (references/openmsx-21.0/src/video/
                                        VDP.cc:849-857).
  m21_vdp_render_graphic7_probe.bin -- GRAPHIC7 mode + the GGGRRRBB
                                        byte-order boundary case
                                        (A-M21-4: green in the TOP 3 bits).
  m21_vdp_render_yjk_probe.bin      -- SCREEN12 (YJK) mode + a 4-pixel group
                                        chosen to hit the negative,
                                        non-multiple-of-4 pre-clamp B-channel
                                        numerator (A-M21-5's rounding risk).

Each program writes ONLY via the real #98/#99/#9A ports (the same M14 port
contract), then HALTs. Run via this emulator's `--vdp-render-parity` mode
and, on the openMSX side, via tools/openmsx/vdp-render-parity.ps1 (raw
VRAM/palette/register byte comparison -- the derived-value/computed-color
comparison is this project's own engine output, cross-checked offline
against the SAME cited formulas; see docs/m21-parity-trace-diff.md for the
full, honest disposition of what IS and is NOT live-cross-engine-comparable).

Usage:
  python tools/gen/vdp-render-probe.py [-o tests/parity]
  python tools/gen/vdp-render-probe.py --self-check
"""
import argparse
import sys


def _ld_a(value):
    return [0x3E, value & 0xFF]


def _out(port):
    return [0xD3, port & 0xFF]


def _out_a(port, value):
    """LD A,value ; OUT (port),A"""
    return _ld_a(value) + _out(port)


def _set_register(reg, value):
    """#99 two-write register-select protocol: data first, then 0x80|reg."""
    return _out_a(0x99, value) + _out_a(0x99, 0x80 | (reg & 0x3F))


def _set_write_address(addr):
    """#99 two-write address-setup protocol (write mode, bit6 set)."""
    low = addr & 0xFF
    high = 0x40 | ((addr >> 8) & 0x3F)
    return _out_a(0x99, low) + _out_a(0x99, high)


def _vram_write(value):
    """#98 VRAM data write (auto-increments the pointer)."""
    return _out_a(0x98, value)


def _palette_write(r3, g3, b3):
    """#9A two-write palette protocol: byte1 = 0 R2R1R0 0 B2B1B0,
    byte2 = 0 0 0 0 0 G2G1G0 (R#16 pointer auto-increments)."""
    byte1 = ((r3 & 0x7) << 4) | (b3 & 0x7)
    byte2 = g3 & 0x7
    return _out_a(0x9A, byte1) + _out_a(0x9A, byte2)


def _halt():
    return [0x76]


def build_palette_probe():
    """R#16=0, then 8 palette writes (entries 0-7, r=g=b=index) -- sweeps
    all 8 A-M21-3 boundary values in one deterministic program."""
    program = bytearray()
    program.extend(_set_register(16, 0x00))
    for i in range(8):
        program.extend(_palette_write(i, i, i))
    program.extend(_halt())
    return bytes(program)


def build_planar_probe():
    """GRAPHIC6 (base 0x0A -> M3+M5, planar) + a CPU-port write at logical
    address 0 (even -> bank0) and 1 (odd -> bank1) -- the D7 CPU-port
    transform (VDP.cc:849-857), read back by the display path's
    planar_row_spans for row0."""
    program = bytearray()
    program.extend(_set_register(0, 0x0A))
    program.extend(_set_write_address(0x0000))
    program.extend(_vram_write(0xF0))  # logical0 -> bank0[0]: pixel0=15,pixel1=0
    program.extend(_vram_write(0x0A))  # logical1 -> bank1[0]: pixel2=0,pixel3=10
    program.extend(_halt())
    return bytes(program)


def build_graphic7_probe():
    """GRAPHIC7 (base 0x0E -> M3+M4+M5) + the GGGRRRBB boundary byte pair
    (A-M21-4): green=7/red=0/blue=0 at logical0 (bank0), red=7/green=0/
    blue=0 at logical1 (bank1)."""
    program = bytearray()
    program.extend(_set_register(0, 0x0E))
    program.extend(_set_write_address(0x0000))
    program.extend(_vram_write(0b111_000_00))  # green=7 (TOP 3 bits), not red
    program.extend(_vram_write(0b000_111_00))  # red=7
    program.extend(_halt())
    return bytes(program)


def build_yjk_probe():
    """GRAPHIC7 base + R#25 YJK (SCREEN12) + a 4-pixel group chosen to hit
    A-M21-5's rounding risk: y=0,j=2,k=0 -> pre-clamp B numerator
    5*0-2*2-0+2 = -2 (negative, non-multiple-of-4). p0=0x00,p1=0x00 (k=0);
    p2=0x02 (y2=0, contributes 2 to j),p3=0x00 (j=2)."""
    program = bytearray()
    program.extend(_set_register(0, 0x0E))
    program.extend(_set_register(25, 0x08))  # YJK, no YAE
    program.extend(_set_write_address(0x0000))
    program.extend(_vram_write(0x00))  # p0 -> bank0[0]
    program.extend(_vram_write(0x00))  # p1 -> bank1[0]
    program.extend(_vram_write(0x02))  # p2 -> bank0[1]
    program.extend(_vram_write(0x00))  # p3 -> bank1[1]
    program.extend(_halt())
    return bytes(program)


PROBES = {
    "palette": build_palette_probe,
    "planar": build_planar_probe,
    "graphic7": build_graphic7_probe,
    "yjk": build_yjk_probe,
}


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-o", "--output-dir", default="tests/parity",
                        help="output directory (default tests/parity)")
    parser.add_argument("--self-check", action="store_true",
                        help="verify determinism (two assemblies byte-identical per probe) and exit")
    args = parser.parse_args(argv[1:])

    if args.self_check:
        ok = True
        for name, builder in PROBES.items():
            a = builder()
            b = builder()
            same = (a == b) and len(a) > 0
            print("self-check:", name, "PASS" if same else "FAIL", "length=", len(a))
            ok = ok and same
        return 0 if ok else 1

    for name, builder in PROBES.items():
        program = builder()
        path = "{}/m21_vdp_render_{}_probe.bin".format(args.output_dir, name)
        with open(path, "wb") as f:
            f.write(program)
        print("wrote {} ({} bytes)".format(path, len(program)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
