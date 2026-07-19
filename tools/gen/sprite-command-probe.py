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

"""M22-S8 openMSX A/B sprite + VDP-command-engine probe assembler
(backlog D2/D3, closes D7's remaining command-engine-path piece).

Produces small, flat-RAM Z80 programs under `tests/parity/`, each exercising
ONE representative M22 A/B subject named in docs/m22-planner-package.md
section 2.6:

  sprite_mode1_collision_probe.bin -- two fully-overlapping Sprite Mode 1
                                           sprites at (0,0) -> a genuine
                                           collision, read back via S#0/S#3-S#6.
  sprite_mode1_fifth_probe.bin     -- five overlapping Sprite Mode 1
                                           sprites on one line -> the 5th-
                                           sprite flag + number (S#0 bits 6/4-0).
  sprite_mode2_ninth_ic_probe.bin  -- nine overlapping Sprite Mode 2
                                           sprites (9th-sprite flag) where two
                                           of them carry IC=1 -> confirms NO
                                           collision is registered despite the
                                           overlap (A-M22-11).
  sprite_tp_color0_probe.bin       -- TP disabled (R#8 bit5 set) + two
                                           overlapping color-0 sprites -> DOES
                                           collide (A-M22-12's canSpriteColor0
                                           Collide interaction).
  cmd_atomic_probe.bin             -- one program, four sequential atomic
                                           commands: HMMV (fill), LMMM (XOR
                                           copy), LINE (Bresenham draw), SRCH
                                           (border-color search, sets BD).
  cmd_graphic6_planar_probe.bin    -- an HMMV issued in GRAPHIC6 (planar)
                                           at a byte index whose parity places
                                           it in the SECOND physical bank
                                           (0x10000+) -- cross-validates D7's
                                           closure.
  cmd_lmmc_transfer_probe.bin      -- an LMMC transfer of 4 pixels, each
                                           fed via a SEPARATE OUT (#99) write
                                           to R#44 -- the event-driven
                                           TR/CE handshake (R-M22-9).

Every program writes ONLY via the real #98/#99/#9B ports (the SAME M14 port
contract), then HALTs. Run via this emulator's `--sprite-cmd-parity` mode and,
on the openMSX side, via tools/openmsx/sprite-command-parity.ps1 (raw
VRAM/register/status byte comparison -- see docs/m22-parity-trace-diff.md for
the full, honest disposition of what IS and is NOT live-cross-engine-
comparable).

Usage:
  python tools/gen/sprite-command-probe.py [-o tests/parity]
  python tools/gen/sprite-command-probe.py --self-check
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


def _write_vram_at(addr, value):
    return _set_write_address(addr) + _vram_write(value)


def _set_cmd_reg(index, value):
    """R#32..R#46 (index 0..14), via the SAME #99 two-write register-select
    protocol -- change_register()'s dispatch extension (A-M22-1) routes these
    identically to R#0-31."""
    return _set_register(32 + index, value)


def _set_sx(v):
    return _set_cmd_reg(0, v & 0xFF) + _set_cmd_reg(1, (v >> 8) & 0xFF)


def _set_sy(v):
    return _set_cmd_reg(2, v & 0xFF) + _set_cmd_reg(3, (v >> 8) & 0xFF)


def _set_dx(v):
    return _set_cmd_reg(4, v & 0xFF) + _set_cmd_reg(5, (v >> 8) & 0xFF)


def _set_dy(v):
    return _set_cmd_reg(6, v & 0xFF) + _set_cmd_reg(7, (v >> 8) & 0xFF)


def _set_nx(v):
    return _set_cmd_reg(8, v & 0xFF) + _set_cmd_reg(9, (v >> 8) & 0xFF)


def _set_ny(v):
    return _set_cmd_reg(10, v & 0xFF) + _set_cmd_reg(11, (v >> 8) & 0xFF)


def _set_col(v):
    return _set_cmd_reg(12, v)


def _set_arg(v):
    return _set_cmd_reg(13, v)


def _set_cmd(v):
    return _set_cmd_reg(14, v)


def _halt():
    return [0x76]


def _display_enable():
    """R#1 bit6 (display enable, BLANK) -- gates SpriteEngine's
    spritesEnabledFast()-equivalent check (VDP.hh:313-319); a freshly reset
    VDP has this bit clear, so sprite checking would otherwise never run."""
    return _set_register(1, 0x40)


def _pin_sprite_regs(mode_reg0, r8=0x00):
    """Explicitly (over)write EVERY register the sprite-check algorithm
    consults, rather than relying on any "default" value. This matters
    specifically for the openMSX A/B side: unlike this engine's own
    `--sprite-cmd-parity` harness (a truly cold, BIOS-less boot), the real
    openMSX side runs the ACTUAL BIOS for several real-time seconds before
    this probe's own code executes, and BIOS reprograms MANY VDP registers
    (mode, sprite table bases, scroll, TP) away from their power-on-reset
    values as part of its own screen setup. Pinning every consulted register
    here (R#0 mode, R#1 display-enable + size/mag, R#5/R#11 attrib base,
    R#8 TP/SPD, R#23 vertical scroll) makes the probe's OWN sprite-table
    placement and semantics independent of whatever state BIOS left behind,
    on BOTH engines."""
    out = []
    out += _set_register(0, mode_reg0)
    out += _set_register(1, 0x40)        # display enable; size=8x8, mag=off
    out += _set_register(5, 0x00)        # attrib_base low half -> 0
    out += _set_register(11, 0x00)       # attrib_base high half -> 0
    out += _set_register(8, r8)          # TP/SPD
    out += _set_register(23, 0x00)       # vertical scroll = 0
    return out


def _write_sprite_m1(index, y, x, pattern_nr, color_attrib):
    """Sprite Mode 1 flat 4-byte-per-sprite table at attrib_base + index*4
    (attrib_base defaults to 0)."""
    base = index * 4
    out = []
    out += _write_vram_at(base + 0, y)
    out += _write_vram_at(base + 1, x)
    out += _write_vram_at(base + 2, pattern_nr)
    out += _write_vram_at(base + 3, color_attrib)
    return out


def _write_sentinel_m1(index):
    return _write_vram_at(index * 4, 208)


def build_sprite_mode1_collision_probe():
    program = bytearray()
    program += bytes(_pin_sprite_regs(0x00))  # GRAPHIC1 -> sprite mode 1
    program += bytes(_set_register(6, 0x01))  # pattern_base = 0x0800
    program += bytes(_write_sprite_m1(0, 0, 0, 0, 1))
    program += bytes(_write_sprite_m1(1, 0, 0, 0, 2))
    program += bytes(_write_sentinel_m1(2))
    program += bytes(_write_vram_at(0x0800, 0xFF))  # pattern row0: all set
    program += bytes(_halt())
    return bytes(program)


def build_sprite_mode1_fifth_probe():
    program = bytearray()
    program += bytes(_pin_sprite_regs(0x00))
    program += bytes(_set_register(6, 0x01))
    for i in range(5):
        program += bytes(_write_sprite_m1(i, 0, i * 20, 0, i + 1))
    program += bytes(_write_sentinel_m1(5))
    program += bytes(_write_vram_at(0x0800, 0xFF))
    program += bytes(_halt())
    return bytes(program)


def build_sprite_mode2_ninth_ic_probe():
    """GRAPHIC4 (base 0x0C) -> sprite mode 2. Y/X/pattern sub-table at +512,
    per-line color/attrib table at +0 (A-M22-15). Nine sprites at Y=0
    (9th-sprite flag); sprites 0 and 1 carry IC=1 (0x20) and fully overlap --
    confirms NO collision despite the overlap (A-M22-11)."""
    program = bytearray()
    program += bytes(_pin_sprite_regs(0x06))  # GRAPHIC4 -> sprite mode 2
    program += bytes(_set_register(6, 0x02))  # pattern_base = 0x1000
    for i in range(9):
        yxp_base = 512 + i * 4
        program += bytes(_write_vram_at(yxp_base + 0, 0))       # Y
        program += bytes(_write_vram_at(yxp_base + 1, 0))       # X (all overlap)
        program += bytes(_write_vram_at(yxp_base + 2, 0))       # pattern index
        color = (0x20 | (i + 1)) if i < 2 else (i + 1)
        program += bytes(_write_vram_at(i * 16 + 0, color))     # per-line color/attrib
    program += bytes(_write_vram_at(512 + 9 * 4, 216))          # sentinel
    program += bytes(_write_vram_at(0x1000, 0xFF))              # pattern row0: all set
    program += bytes(_halt())
    return bytes(program)


def build_sprite_tp_color0_probe():
    """Sprite Mode 1, TP disabled (R#8 bit5 SET -> canSpriteColor0Collide()
    true): two overlapping color-0 sprites DO collide (A-M22-12)."""
    program = bytearray()
    program += bytes(_pin_sprite_regs(0x00, r8=0x20))  # GRAPHIC1, TP disabled
    program += bytes(_set_register(6, 0x01))
    program += bytes(_write_sprite_m1(0, 0, 0, 0, 0))
    program += bytes(_write_sprite_m1(1, 0, 0, 0, 0))
    program += bytes(_write_sentinel_m1(2))
    program += bytes(_write_vram_at(0x0800, 0xFF))
    program += bytes(_halt())
    return bytes(program)


def build_cmd_atomic_probe():
    """GRAPHIC4. Four sequential atomic commands sharing one VDP instance:
    HMMV fill (byte index 5), LMMM XOR copy (byte 0 -> byte 10), LINE draw
    (X-major diagonal, GRAPHIC4 space), SRCH (finds a pre-placed target
    color, sets BD)."""
    program = bytearray()
    program += bytes(_set_register(0, 0x06))  # GRAPHIC4
    program += bytes(_set_arg(0x00))  # pin ARG (no DIX/DIY/EQ/MAJ) -- BIOS may have left a stale value

    # HMMV: fill byte index 5 (DX=10) with 0x5A.
    program += bytes(_set_dx(10))
    program += bytes(_set_dy(0))
    program += bytes(_set_nx(2))
    program += bytes(_set_ny(1))
    program += bytes(_set_col(0x5A))
    program += bytes(_set_cmd(0xC0))  # HMMV

    # LMMM: copy byte 0 (pre-written 0x0F) to byte 10, XOR against the
    # pre-existing 0xFF there.
    program += bytes(_write_vram_at(0, 0x0F))
    program += bytes(_write_vram_at(10, 0xFF))
    program += bytes(_set_sx(0))
    program += bytes(_set_sy(0))
    program += bytes(_set_dx(20))  # byte index 10
    program += bytes(_set_dy(0))
    program += bytes(_set_nx(2))
    program += bytes(_set_ny(1))
    program += bytes(_set_cmd(0x93))  # LMMM, XOR

    # LINE: X-major diagonal starting at (30,0), NX=NY=4.
    program += bytes(_set_dx(30))
    program += bytes(_set_dy(0))
    program += bytes(_set_nx(4))
    program += bytes(_set_ny(4))
    program += bytes(_set_col(0x0A))
    program += bytes(_set_arg(0x00))
    program += bytes(_set_cmd(0x70))  # LINE, IMP

    # SRCH: pixel6 (byte3 high nibble) pre-set to color 5; search from SX=6.
    program += bytes(_write_vram_at(3, 0x50))
    program += bytes(_set_sx(6))
    program += bytes(_set_sy(0))
    program += bytes(_set_col(0x05))
    program += bytes(_set_arg(0x02))  # EQ
    program += bytes(_set_cmd(0x60))  # SRCH

    program += bytes(_halt())
    return bytes(program)


def build_cmd_graphic6_planar_probe():
    """GRAPHIC6 (base 0x14, planar). HMMV at DX=2 (byte index 1, ODD -- bit1
    of X set) lands in the SECOND physical bank (0x10000+), cross-validating
    D7's closure (graphic6_command_address(2,0) == 0x10000)."""
    program = bytearray()
    program += bytes(_set_register(0, 0x0A))  # GRAPHIC6
    program += bytes(_set_arg(0x00))  # pin ARG
    program += bytes(_set_dx(2))
    program += bytes(_set_dy(0))
    program += bytes(_set_nx(2))
    program += bytes(_set_ny(1))
    program += bytes(_set_col(0x0D))
    program += bytes(_set_cmd(0xC0))  # HMMV
    program += bytes(_halt())
    return bytes(program)


def build_cmd_lmmc_transfer_probe():
    """GRAPHIC4. LMMC transfer of 4 pixels (2 bytes), each fed via a
    SEPARATE OUT (#99) write to R#44 (COL) -- the event-driven TR/CE
    handshake (R-M22-9): CE must stay 1 until the LAST write."""
    program = bytearray()
    program += bytes(_set_register(0, 0x06))  # GRAPHIC4
    program += bytes(_set_arg(0x00))  # pin ARG
    program += bytes(_set_dx(0))
    program += bytes(_set_dy(0))
    program += bytes(_set_nx(4))
    program += bytes(_set_ny(1))
    program += bytes(_set_cmd(0xB0))  # LMMC, IMP
    for pixel in (1, 2, 3, 4):
        program += bytes(_set_col(pixel))
    program += bytes(_halt())
    return bytes(program)


PROBES = {
    "sprite_mode1_collision": build_sprite_mode1_collision_probe,
    "sprite_mode1_fifth": build_sprite_mode1_fifth_probe,
    "sprite_mode2_ninth_ic": build_sprite_mode2_ninth_ic_probe,
    "sprite_tp_color0": build_sprite_tp_color0_probe,
    "cmd_atomic": build_cmd_atomic_probe,
    "cmd_graphic6_planar": build_cmd_graphic6_planar_probe,
    "cmd_lmmc_transfer": build_cmd_lmmc_transfer_probe,
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
        path = "{}/m22_{}_probe.bin".format(args.output_dir, name)
        with open(path, "wb") as f:
            f.write(program)
        print("wrote {} ({} bytes)".format(path, len(program)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
