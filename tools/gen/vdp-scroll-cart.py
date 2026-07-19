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

"""M38 Phase-A V9958 horizontal-scroll repro cartridge assembler.

Produces a family of small deterministic MSX cartridge ROMs (16 KB each, with a
standard "AB" cartridge header + INIT autostart), one per VDP-scroll SCENARIO.
Each ROM programs the V9958 through a FIXED, deterministic OUT (#98/#99/#9A)
sequence -- set a screen mode, draw a labelled RULER reference pattern into
VRAM, then program the horizontal/vertical scroll registers (R#23/R#25/R#26/
R#27) -- and spins forever (DI + JR $) so the displayed frame is a static,
reproducible scene.

The SAME .rom loads identically in this project's headless build (via
`--cart1`) AND in openMSX (via `-cart`), so a pixel A/B can be taken without a
30-second game-disk boot. This is Phase A (repro + evidence only) of the M38
horizontal-scroll investigation: it does NOT fix anything.

Z80 is hand-encoded (no assembler toolchain in this environment) via the small
`Asm` helper below, following the same technique as
tools/gen/vdp-render-probe.py. It is NEVER copied from any reference source.

Ruler design
------------
Bitmap H-ruler (256-wide G4/G7): pixel colour = ((x>>2)+phase)&0x0F packed as a
4bpp byte -- 4-pixel-wide colour bands cycling through all 16 palette entries,
identical on every row. Any horizontal scroll shifts the bands; a 1-pixel error
is unambiguous at every band boundary. `phase` lets a second VRAM page carry a
DISTINCT (phase-shifted) ruler so a multi-page-scroll wrap is visible.

Bitmap V-ruler (256-wide G4): colour = (y>>3)&0x0F, constant across each row --
horizontal bands so a vertical scroll (R#23) shift is unambiguous.

Usage:
  python tools/gen/vdp-scroll-cart.py -o debug/m38/carts
  python tools/gen/vdp-scroll-cart.py --self-check
"""
import argparse
import os
import sys


# ---------------------------------------------------------------------------
# Minimal hand-encoding Z80 assembler with label/fixup support.
# ---------------------------------------------------------------------------
class Asm:
    def __init__(self, org):
        self.org = org
        self.buf = bytearray()
        self.labels = {}
        self.fixups = []  # (pos, label, kind) kind in {'rel8','abs16'}

    def pc(self):
        return self.org + len(self.buf)

    def label(self, name):
        self.labels[name] = self.pc()

    def _db(self, *bs):
        for b in bs:
            self.buf.append(b & 0xFF)

    # 8-bit immediate loads
    def ld_a(self, n): self._db(0x3E, n)
    def ld_b(self, n): self._db(0x06, n)
    def ld_c(self, n): self._db(0x0E, n)
    def ld_h(self, n): self._db(0x26, n)
    def ld_l(self, n): self._db(0x2E, n)
    # 16-bit immediate loads
    def ld_hl(self, nn): self._db(0x21, nn & 0xFF, (nn >> 8) & 0xFF)
    def ld_de(self, nn): self._db(0x11, nn & 0xFF, (nn >> 8) & 0xFF)
    def ld_bc(self, nn): self._db(0x01, nn & 0xFF, (nn >> 8) & 0xFF)
    # register moves
    def ld_a_b(self): self._db(0x78)
    def ld_a_c(self): self._db(0x79)
    def ld_a_d(self): self._db(0x7A)
    def ld_a_e(self): self._db(0x7B)
    def ld_a_h(self): self._db(0x7C)
    def ld_a_l(self): self._db(0x7D)
    def ld_l_a(self): self._db(0x6F)
    def ld_h_a(self): self._db(0x67)
    def ld_b_a(self): self._db(0x47)
    def ld_c_a(self): self._db(0x4F)
    def ld_e_a(self): self._db(0x5F)
    # ALU
    def add_a_a(self): self._db(0x87)
    def add_a_n(self, n): self._db(0xC6, n)
    def and_n(self, n): self._db(0xE6, n)
    def or_l(self): self._db(0xB5)
    def or_e(self): self._db(0xB3)
    def or_c(self): self._db(0xB1)
    def cp_n(self, n): self._db(0xFE, n)
    def srl_a(self): self._db(0xCB, 0x3F)
    def inc_c(self): self._db(0x0C)
    def inc_e(self): self._db(0x1C)
    def dec_de(self): self._db(0x1B)
    # I/O
    def out_n_a(self, port): self._db(0xD3, port)
    def out_a(self, port, value): self.ld_a(value); self.out_n_a(port)
    # control
    def di(self): self._db(0xF3)
    def ei(self): self._db(0xFB)
    def halt(self): self._db(0x76)
    def djnz(self, label):
        self._db(0x10, 0x00)
        self.fixups.append((len(self.buf) - 1, label, 'rel8'))
    def jr(self, label):
        self._db(0x18, 0x00)
        self.fixups.append((len(self.buf) - 1, label, 'rel8'))
    def jr_nz(self, label):
        self._db(0x20, 0x00)
        self.fixups.append((len(self.buf) - 1, label, 'rel8'))
    def jr_z(self, label):
        self._db(0x28, 0x00)
        self.fixups.append((len(self.buf) - 1, label, 'rel8'))
    def jp(self, label):
        self._db(0xC3, 0x00, 0x00)
        self.fixups.append((len(self.buf) - 2, label, 'abs16'))

    def resolve(self):
        for pos, label, kind in self.fixups:
            tgt = self.labels[label]
            if kind == 'rel8':
                rel = tgt - (self.org + pos + 1)
                if not (-128 <= rel <= 127):
                    raise ValueError("rel8 out of range to %s: %d" % (label, rel))
                self.buf[pos] = rel & 0xFF
            elif kind == 'abs16':
                self.buf[pos] = tgt & 0xFF
                self.buf[pos + 1] = (tgt >> 8) & 0xFF
        return bytes(self.buf)


# ---------------------------------------------------------------------------
# VDP helper macro emitters (on top of Asm).
# ---------------------------------------------------------------------------
def set_reg(a, reg, value):
    """#99 two-write register-select: data first, then 0x80|reg."""
    a.out_a(0x99, value)
    a.out_a(0x99, 0x80 | (reg & 0x3F))


def set_write_addr(a, addr):
    """17-bit VRAM write address: R#14 = A16..A14, then #99 low + (0x40|high6)."""
    set_reg(a, 14, (addr >> 14) & 0x07)
    a.out_a(0x99, addr & 0xFF)
    a.out_a(0x99, 0x40 | ((addr >> 8) & 0x3F))


def palette(a, entries):
    """Set R#16=0, then write RGB triples (3-bit each) via #9A."""
    set_reg(a, 16, 0x00)
    for (r, g, b) in entries:
        a.out_a(0x9A, ((r & 7) << 4) | (b & 7))
        a.out_a(0x9A, g & 7)


RAINBOW16 = [
    (0, 0, 0), (7, 0, 0), (0, 7, 0), (0, 0, 7),
    (7, 7, 0), (7, 0, 7), (0, 7, 7), (7, 7, 7),
    (4, 0, 0), (7, 4, 0), (4, 7, 0), (0, 7, 4),
    (0, 4, 7), (4, 0, 7), (4, 4, 4), (7, 4, 4),
]


def fill_g4_hruler(a, phase, rows=212):
    """Fill a 256-wide 4bpp bitmap page (write addr must be pre-set).
    byte(col) = (((col>>2)+phase)&0x0F)*0x11, identical on every row."""
    a.ld_de(rows)
    a.label('hr_row')
    a.ld_c(0)          # C = byte/col index 0..127
    a.ld_b(128)        # inner count
    a.label('hr_byte')
    a.ld_a_c()
    a.srl_a()
    a.srl_a()          # A = C>>2
    if phase:
        a.add_a_n(phase)
    a.and_n(0x0F)
    a.ld_l_a()         # L = nibble
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()        # A = nibble<<4
    a.or_l()           # A = nibble*0x11
    a.out_n_a(0x98)
    a.inc_c()
    a.djnz('hr_byte')
    a.dec_de()
    a.ld_a_d()
    a.or_e()
    a.jr_nz('hr_row')


def fill_g4_vruler(a, rows=212):
    """Fill a 256-wide 4bpp bitmap page with horizontal bands.
    row byte = ((row>>3)&0x0F)*0x11 for all 128 bytes of the row."""
    a.ld_c(0)          # C = row index
    a.label('vr_row')
    a.ld_a_c()
    a.srl_a()
    a.srl_a()
    a.srl_a()          # A = row>>3
    a.and_n(0x0F)
    a.ld_l_a()
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()
    a.or_l()
    a.ld_h_a()         # H = row byte
    a.ld_b(128)
    a.label('vr_byte')
    a.ld_a_h()
    a.out_n_a(0x98)
    a.djnz('vr_byte')
    a.inc_c()
    a.ld_a_c()
    a.cp_n(rows & 0xFF)
    a.jr_nz('vr_row')


CART_INIT = 0x4010


def cart_header(init=CART_INIT):
    """16-byte MSX ROM cartridge header at 0x4000."""
    h = bytearray(16)
    h[0] = ord('A')
    h[1] = ord('B')
    h[2] = init & 0xFF
    h[3] = (init >> 8) & 0xFF
    # STATEMENT/DEVICE/BASIC/reserved all zero
    return bytes(h)


def finalize_rom(a, size=0x4000):
    body = a.resolve()
    rom = bytearray(cart_header())
    # code starts at 0x4010 == org(0x4000)+16; header occupies [0,16)
    assert a.org == 0x4000
    rom += body
    if len(rom) > size:
        raise ValueError("ROM overflow: %d > %d" % (len(rom), size))
    rom += bytes(size - len(rom))  # pad with 0x00
    return bytes(rom)


def new_asm():
    a = Asm(0x4000)
    # reserve the 16-byte header region; code emitted after starts at 0x4010
    a.buf.extend(bytes(16))
    a.label('init')
    a.di()
    return a


def spin(a):
    a.label('spin')
    a.jr('spin')


# ---------------------------------------------------------------------------
# Scenario builders. Each returns a 16 KB .rom image.
# ---------------------------------------------------------------------------
def disable_sprites(a, attr_addr=None):
    """Hide the boot-logo sprite residue via R#8 SPD (bit1) while PRESERVING
    the VR bit (bit3, 64K/128K VRAM addressing -- clearing it corrupts G6/G7
    planar reads). openMSX's Sony_HB-F1XV boot leaves R#8=0x08 (VR=1, TP=0),
    confirmed by a live `vdpreg 8` query; R#8=0x0A keeps VR=1 and adds SPD=1.
    `attr_addr` is accepted/ignored (call-site compatibility)."""
    _ = attr_addr
    set_reg(a, 8, 0x0A)


def setup_g4(a, r2=0x1F):
    """SCREEN5 / GRAPHIC4: R#0=0x06, R#1=0x40 (display on), R#2 selects page."""
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x06)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, r2)
    disable_sprites(a, 0x7600)


def setup_g7(a, r2=0x1F):
    """SCREEN8 / GRAPHIC7 (planar 256-colour)."""
    # GRAPHIC7 uses the fixed 256-colour decode; palette still set for sprites.
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x0E)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, r2)
    disable_sprites(a, 0x7600)


def fill_const(a, addr, count, value):
    """Write `count` bytes of `value` starting at VRAM `addr` (16-bit count)."""
    set_write_addr(a, addr)
    a.ld_hl(count)
    a.label('fc_%X' % addr)
    a.out_a(0x98, value)
    a._db(0x2B)          # DEC HL
    a.ld_a_h()
    a.or_l()
    a.jr_nz('fc_%X' % addr)


def fill_name_ramp(a, addr, count):
    """Name table: byte(index) = index & 0x1F (each column -> its own tile)."""
    set_write_addr(a, addr)
    a.ld_bc(0)
    a.ld_hl(count)
    a.label('fn_%X' % addr)
    a.ld_a_c()
    a.and_n(0x1F)
    a.out_n_a(0x98)
    a._db(0x03)          # INC BC
    a._db(0x2B)          # DEC HL
    a.ld_a_h()
    a.or_l()
    a.jr_nz('fn_%X' % addr)


def fill_color_ramp(a, addr, count):
    """Colour table: fg nibble = (index>>3)&0x07 (per-tile colour), bg=0.
    Deliberately 8 colours (mask-invariant): SCREEN2's colour-table VRAM-window
    mask (R#3 low bits) folds char codes >=8 back onto lower codes in openMSX,
    which this project's renderer does NOT model (raw R#3<<6, a documented
    simplification in vdp_frame_renderer.h). Making the table content repeat
    every 8 tiles keeps the baseline char scene identical on BOTH engines, so
    the char-mode H-scroll A/B is not confounded by that non-scroll nuance."""
    set_write_addr(a, addr)
    a.ld_bc(0)
    a.ld_hl(count)
    a.label('fcr_%X' % addr)
    a.ld_a_c()
    a.srl_a()
    a.srl_a()
    a.srl_a()            # A = index>>3 (bits 3-6 of index live in C)
    a.and_n(0x07)
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()          # fg nibble
    a.out_n_a(0x98)
    a._db(0x03)          # INC BC
    a._db(0x2B)          # DEC HL
    a.ld_a_h()
    a.or_l()
    a.jr_nz('fcr_%X' % addr)


def setup_g2(a):
    """SCREEN2 / GRAPHIC2 char/tile mode. Solid tiles (pattern all 1s) so each
    name cell shows one flat colour from the colour table; name cell = column
    index -> a per-column colour ruler."""
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x02)   # M3 -> base 0x04 (GRAPHIC2)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x06)   # name table 0x1800
    set_reg(a, 3, 0x80)   # colour table 0x2000
    set_reg(a, 4, 0x00)   # pattern table 0x0000
    set_reg(a, 10, 0x00)
    disable_sprites(a, 0x3800)
    fill_const(a, 0x0000, 0x1800, 0xFF)   # all tiles solid (3 quarters)
    fill_color_ramp(a, 0x2000, 0x1800)    # per-tile fg colour
    fill_name_ramp(a, 0x1800, 0x0300)     # 24 rows x 32 cols, cell = column


def fill_g7_hruler(a, rows=212):
    """GRAPHIC7: one byte per pixel (GGGRRRBB fixed decode). Ruler colour byte
    = a smooth ramp of the pixel column so bands are visible: byte = (col&0xFF)
    which yields a repeating GGGRRRBB gradient every 256 px."""
    # 256 bytes/row (256 px, 1 byte/px). addr pre-set. Use C for col 0..255.
    a.ld_de(rows)
    a.label('g7_row')
    a.ld_c(0)
    a.ld_b(0)          # 0 -> 256 iterations via DJNZ
    a.label('g7_byte')
    a.ld_a_c()
    a.out_n_a(0x98)
    a.inc_c()
    a.djnz('g7_byte')
    a.dec_de()
    a.ld_a_d()
    a.or_e()
    a.jr_nz('g7_row')


def build_s01_g4_baseline():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    spin(a)
    return finalize_rom(a)


def build_s02_g4_coarse():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    set_reg(a, 26, 0x03)   # coarse: 3 * 8 = 24px left
    spin(a)
    return finalize_rom(a)


def build_s03_g4_fine():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    set_reg(a, 27, 0x03)   # fine only
    spin(a)
    return finalize_rom(a)


def build_s04_g4_coarse_fine():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    set_reg(a, 26, 0x03)
    set_reg(a, 27, 0x03)
    spin(a)
    return finalize_rom(a)


def build_s05_g4_msk():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    set_reg(a, 26, 0x03)
    set_reg(a, 25, 0x02)   # MSK: extend left border by 8px
    spin(a)
    return finalize_rom(a)


def build_s06_g4_multipage():
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x06)
    set_reg(a, 1, 0x40)
    disable_sprites(a, 0x7600)
    # Fill page 0 (0x0000): phase 0 ruler
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)
    # Fill page 1 (0x8000): phase 8 ruler (distinct/phase-shifted)
    set_write_addr(a, 0x8000)
    fill_g4_hruler(a, phase=8)
    # Display page 1 (odd) + enable multi-page scroll + coarse scroll.
    set_reg(a, 2, 0x3F)    # (0x3F>>5)&3 = 1 -> page 1 (odd)
    set_reg(a, 25, 0x01)   # multi-page scroll enable
    set_reg(a, 26, 0x08)   # coarse 8*8 = 64px -> wraps toward page 0
    spin(a)
    return finalize_rom(a)


def build_s07_g7_coarse():
    a = new_asm()
    setup_g7(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g7_hruler(a)
    set_reg(a, 26, 0x03)
    spin(a)
    return finalize_rom(a)


def build_s08_g4_vscroll():
    a = new_asm()
    setup_g4(a, r2=0x1F)
    set_write_addr(a, 0x0000)
    fill_g4_vruler(a)
    set_reg(a, 23, 0x10)   # vertical scroll by 16 lines
    spin(a)
    return finalize_rom(a)


def fill_yjk_yramp(a, rows=212):
    """SCREEN12 grayscale test: J=K=0, Y ramps with the column so every pixel
    is gray(Y). Byte layout per 4-pixel group: p0,p1 (K plane) and p2,p3 (J
    plane) low-3-bits = 0 -> J=K=0; each byte's high 5 bits = Y. So byte =
    (Y<<3) with the low 3 bits clear. Y = (col>>1)&0x1F ramps 0..31 across a
    64px span, repeating -> a clean grayscale ruler that MATCHES iff the YJK
    luma decode agrees. (256 bytes/row, 1 byte/pixel; addr pre-set.)"""
    a.ld_de(rows)
    a.label('yjk_row')
    a.ld_c(0)
    a.ld_b(0)            # 256 iterations
    a.label('yjk_byte')
    a.ld_a_c()
    a.srl_a()           # col>>1
    a.and_n(0x1F)       # Y in 0..31
    a.add_a_a()
    a.add_a_a()
    a.add_a_a()         # Y<<3, low 3 bits (J/K) = 0
    a.out_n_a(0x98)
    a.inc_c()
    a.djnz('yjk_byte')
    a.dec_de()
    a.ld_a_d()
    a.or_e()
    a.jr_nz('yjk_row')


def build_s09_yjk_baseline():
    a = new_asm()
    # SCREEN12 (YJK, no YAE): GRAPHIC7 base + R#25 bit3.
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x0E)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 25, 0x08)   # YJK
    disable_sprites(a, 0x7600)
    set_write_addr(a, 0x0000)
    fill_yjk_yramp(a)      # J=K=0 grayscale Y-ramp (a valid YJK reference)
    spin(a)
    return finalize_rom(a)


def build_s10_g2_baseline():
    a = new_asm()
    setup_g2(a)
    spin(a)
    return finalize_rom(a)


def build_s11_g2_coarse():
    a = new_asm()
    setup_g2(a)
    set_reg(a, 26, 0x03)   # char-mode coarse H-scroll (within one name half)
    spin(a)
    return finalize_rom(a)


SCENARIOS = {
    "s01_g4_baseline": build_s01_g4_baseline,
    "s02_g4_coarse": build_s02_g4_coarse,
    "s03_g4_fine": build_s03_g4_fine,
    "s04_g4_coarse_fine": build_s04_g4_coarse_fine,
    "s05_g4_msk": build_s05_g4_msk,
    "s06_g4_multipage": build_s06_g4_multipage,
    "s07_g7_coarse": build_s07_g7_coarse,
    "s08_g4_vscroll": build_s08_g4_vscroll,
    "s09_yjk_baseline": build_s09_yjk_baseline,
    "s10_g2_baseline": build_s10_g2_baseline,
    "s11_g2_coarse": build_s11_g2_coarse,
}


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--output-dir", default="debug/m38/carts")
    p.add_argument("--self-check", action="store_true",
                   help="verify determinism (two assemblies byte-identical per scenario) + valid header")
    args = p.parse_args(argv[1:])

    if args.self_check:
        ok = True
        for name, b in SCENARIOS.items():
            r1 = b()
            r2 = b()
            same = (r1 == r2) and len(r1) == 0x4000 and r1[0:2] == b"AB"
            print("self-check:", name, "PASS" if same else "FAIL",
                  "len=%d init=%04X" % (len(r1), r1[2] | (r1[3] << 8)))
            ok = ok and same
        return 0 if ok else 1

    os.makedirs(args.output_dir, exist_ok=True)
    for name, b in SCENARIOS.items():
        rom = b()
        path = os.path.join(args.output_dir, name + ".rom")
        with open(path, "wb") as f:
            f.write(rom)
        print("wrote %s (%d bytes)" % (path, len(rom)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
