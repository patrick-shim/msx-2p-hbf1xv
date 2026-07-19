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

"""M41-S2 VIDEO parity cartridge assembler (docs/m41-production-qa-plan.md §7,
Subsystems 6 (screen modes) + 7 (sprites)).

Extends the M38 hand-encoded-Z80 cartridge pattern
(tools/gen/vdp-scroll-cart.py -- imported below to REUSE its `Asm`
assembler + VDP macro helpers, never duplicated) with one INIT-autostart
16 KB cart per screen-mode / sprite scenario. Each cart programs the V9958
through a FIXED deterministic OUT (#98/#99/#9A) sequence -- set the mode, draw
a DETERMINISTIC scene (rulers / ramps / tiled patterns / sprites -- the cart
analogue of BASIC LINE/CIRCLE/PSET, since no assembler nor a bootable
Disk-BASIC disk is available per debug/m41/ab/phase0-findings.md), then settle
(DI + JR $) into a static frame. The SAME .rom loads bit-identically in this
project's headless build (--cart1) and in openMSX 19.1 (-cart), so a pixel A/B
is a pure function of the VDP decode -- no ~24 s boot logo, no disk-boot
blocker.

Grounding (mode/register/table semantics), all read (never copied):
  * src/devices/video/vdp_mode.h decode_vdp_mode(): M5..M1 base byte + R#25
    YJK(bit3)/YAE(bit4).
  * src/devices/video/vdp_frame_renderer.cpp width()/height()/name_table_base()/
    pattern_table_base()/color_table_base()/render_text1().
  * src/devices/video/sprite_engine.cpp: mode-1 flat 4-byte attribute table
    (Y,X,pattern,color; Y-sentinel 208); mode-2 attribute addressing
    (baseMask=(R#11<<15)|(R#5<<7)|0x7F, Y/X/pattern sub-table at index
    512+sprite*4, per-line color at sprite*16+line; Y-sentinel 216); pattern
    base R#6<<11; size16=R#1 bit1, mag=R#1 bit0.
  * references/openmsx-21.0/src/video/DisplayMode.hh (behavior reference only).

Usage:
  python tools/gen/video-cart.py -o debug/m41/carts
  python tools/gen/video-cart.py --self-check
"""
import argparse
import importlib.util
import os
import sys

# --- Reuse the M38 assembler + VDP macro helpers (import, never duplicate). ---
_M38 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vdp-scroll-cart.py")
_spec = importlib.util.spec_from_file_location("genm38", _M38)
genm38 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(genm38)

Asm = genm38.Asm
set_reg = genm38.set_reg
set_write_addr = genm38.set_write_addr
palette = genm38.palette
RAINBOW16 = genm38.RAINBOW16
new_asm = genm38.new_asm
spin = genm38.spin
finalize_rom = genm38.finalize_rom
fill_const = genm38.fill_const
fill_g4_hruler = genm38.fill_g4_hruler
fill_g7_hruler = genm38.fill_g7_hruler
fill_yjk_yramp = genm38.fill_yjk_yramp


# ---------------------------------------------------------------------------
# Small additional emitters (on top of the reused Asm helper set).
# ---------------------------------------------------------------------------
def write_bytes(a, addr, data):
    """Set the VRAM write address then OUT each literal byte via #98."""
    set_write_addr(a, addr)
    for b in data:
        a.out_a(0x98, b & 0xFF)


def fill_run(a, addr, count, value, tag):
    """Write `count` copies of `value` from VRAM `addr` (16-bit counted loop)."""
    set_write_addr(a, addr)
    a.ld_hl(count)
    a.label('fr_%s' % tag)
    a.out_a(0x98, value)
    a._db(0x2B)          # DEC HL
    a.ld_a_h()
    a.or_l()
    a.jr_nz('fr_%s' % tag)


def fill_ramp(a, addr, count, mask, tag):
    """Write byte(i) = i & mask for i in [0, count) from VRAM `addr`.
    A deterministic per-cell/per-pixel ramp exercising the mode decode."""
    set_write_addr(a, addr)
    a.ld_bc(0)
    a.ld_hl(count)
    a.label('rmp_%s' % tag)
    a.ld_a_c()
    a.and_n(mask)
    a.out_n_a(0x98)
    a._db(0x03)          # INC BC
    a._db(0x2B)          # DEC HL
    a.ld_a_h()
    a.or_l()
    a.jr_nz('rmp_%s' % tag)


# A tiny deterministic 8x8 "font": glyph G row r pattern byte = (G + r) rotated
# into a diagonal stripe, so distinct name codes render as distinct, visually
# separable cells. Both engines read the same pattern-generator VRAM, so this is
# a pure decode A/B -- it does NOT depend on any BIOS-loaded font.
def write_diag_font(a, pattern_base, glyphs):
    """Write `glyphs` 8-byte diagonal-stripe patterns at pattern_base."""
    set_write_addr(a, pattern_base)
    for g in range(glyphs):
        for r in range(8):
            # diagonal bar whose position depends on the glyph index -> each
            # glyph is a distinct, deterministic 8x8 bitmap.
            byte = (0x80 >> ((g + r) & 7)) | (0x01 << ((g * 3 + r) & 7))
            a.out_a(0x98, byte & 0xFF)


# ---------------------------------------------------------------------------
# Screen-mode scenario builders (Subsystem 6: M0,M1,M2,M4,M5,M6,M7,M8 + YJK).
# ---------------------------------------------------------------------------
def build_m0_text1():
    """SCREEN 0 (TEXT1, 40x24, 240x192). Deterministic font + name ramp."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 8, 0x0A)           # SPD=1 (text has no sprites anyway), VR=1
    set_reg(a, 0, 0x00)           # M3/M4/M5 = 0
    set_reg(a, 1, 0x50)           # display on (b6) + M1 (b4) -> TEXT1
    set_reg(a, 2, 0x00)           # name table @ 0x0000
    set_reg(a, 4, 0x01)           # pattern gen @ 0x0800
    set_reg(a, 7, 0xF4)           # fg=15 (white) bg=4 (blue)
    set_reg(a, 9, 0x00)           # 192 lines
    write_diag_font(a, 0x0800, 64)          # 64 deterministic glyphs
    fill_ramp(a, 0x0000, 40 * 24, 0x3F, 't1')  # name cell = (index & 0x3F)
    spin(a)
    return finalize_rom(a)


def build_m1_g1():
    """SCREEN 1 (GRAPHIC1, 32x24 tiles, 256x192). Font + name ramp + colors."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 0, 0x00)           # GRAPHIC1
    set_reg(a, 1, 0x40)           # display on
    set_reg(a, 2, 0x06)           # name table @ 0x1800
    set_reg(a, 3, 0x80)           # color table @ 0x2000
    set_reg(a, 4, 0x00)           # pattern gen @ 0x0000
    set_reg(a, 7, 0x04)           # backdrop = 4
    set_reg(a, 9, 0x00)
    write_diag_font(a, 0x0000, 64)
    # G1 color table: 32 bytes, one per group of 8 chars; fg/bg per group.
    write_bytes(a, 0x2000, [((g % 15 + 1) << 4) | 0x01 for g in range(32)])
    fill_ramp(a, 0x1800, 32 * 24, 0x3F, 'g1')  # name cell = (index & 0x3F)
    spin(a)
    return finalize_rom(a)


def build_m2_g2():
    """SCREEN 2 (GRAPHIC2, 256x192). Reuse the proven M38 s10 baseline recipe."""
    return genm38.build_s10_g2_baseline()


def build_m4_g3():
    """SCREEN 4 (GRAPHIC3, character mode, 256x192). Same render path as G2
    (render_graphic2_or_3) but base 0x08 + sprite mode 2. Identical table
    layout to the PROVEN G2 baseline (setup_g2: R#3=0x80 colour @ 0x2000,
    mask-invariant 8-fold colour ramp) so this is a pure G2-vs-G3 mode-decode
    A/B, NOT confounded by the R#3 colour-table-window mask simplification."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x04)           # M4 -> base 0x08 (GRAPHIC3)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x06)           # name @ 0x1800
    set_reg(a, 3, 0x80)           # colour table @ 0x2000 (same as setup_g2)
    set_reg(a, 4, 0x00)           # pattern @ 0x0000
    set_reg(a, 10, 0x00)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 9, 0x00)           # 192 (character mode, 24 rows)
    fill_const(a, 0x0000, 0x1800, 0xFF)   # all tiles solid
    # per-tile colour ramp folded to 8 (mask-invariant, mirrors gen-m38 note).
    genm38.fill_color_ramp(a, 0x2000, 0x1800)
    genm38.fill_name_ramp(a, 0x1800, 0x0300)
    spin(a)
    return finalize_rom(a)


def build_m5_g4():
    """SCREEN 5 (GRAPHIC4, 256x212). H-ruler over the full 212 active lines."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x06)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_write_addr(a, 0x0000)
    fill_g4_hruler(a, phase=0)    # 212 rows of 4bpp colour bands
    spin(a)
    return finalize_rom(a)


def build_m6_g5():
    """SCREEN 6 (GRAPHIC5, 512x212, 4 colours). 2bpp packed ruler."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 0, 0x08)           # M5 -> base 0x10 (GRAPHIC5)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_write_addr(a, 0x0000)
    # 512 px, 2bpp -> 128 bytes/row; byte = 4 pixels. byte(col) = (col & 0xFF)
    # gives a repeating 4-colour ramp; identical every row -> vertical bands.
    a.ld_de(212)
    a.label('g5_row')
    a.ld_c(0)
    a.ld_b(128)
    a.label('g5_byte')
    a.ld_a_c()
    a.out_n_a(0x98)
    a.inc_c()
    a.djnz('g5_byte')
    a.dec_de()
    a.ld_a_d()
    a.or_e()
    a.jr_nz('g5_row')
    spin(a)
    return finalize_rom(a)


def build_m7_g6():
    """SCREEN 7 (GRAPHIC6, 512x212, 16 colours). 4bpp planar ruler."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 0, 0x0A)           # M5+M3 -> base 0x14 (GRAPHIC6)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_write_addr(a, 0x0000)
    # 512 px, 4bpp -> 256 bytes/row; byte = 2 pixels, nibble ramp.
    # byte(col) = col & 0xFF -> repeating 16-colour bands, identical each row.
    a.ld_de(212)
    a.label('g6_row')
    a.ld_c(0)
    a.ld_b(0)            # 256 iterations
    a.label('g6_byte')
    a.ld_a_c()
    a.out_n_a(0x98)
    a.inc_c()
    a.djnz('g6_byte')
    a.dec_de()
    a.ld_a_d()
    a.or_e()
    a.jr_nz('g6_row')
    spin(a)
    return finalize_rom(a)


def build_m8_g7():
    """SCREEN 8 (GRAPHIC7, 256x212, 256 colours). Fixed GRRRB-BB decode ruler."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 0, 0x0E)           # base 0x1C (GRAPHIC7)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_write_addr(a, 0x0000)
    fill_g7_hruler(a)             # byte = col -> GGGRRRBB gradient
    spin(a)
    return finalize_rom(a)


def build_m10_yjkyae():
    """SCREEN 10/11 (YJK + YAE, 256x212). Same YJK Y-ramp; YAE attribute bit
    routes low-3-bit=0 pixels through the RGB-palette path when set. This cart
    keeps J=K=0 so it is the YJK+YAE analogue of the s09 SCREEN12 ramp -- the
    KD-YJK luma delta is the measured quantity."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x0E)           # GRAPHIC7 base
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_reg(a, 25, 0x18)          # YJK (b3) + YAE (b4)
    set_write_addr(a, 0x0000)
    fill_yjk_yramp(a)
    spin(a)
    return finalize_rom(a)


def build_m12_yjk():
    """SCREEN 12 (YJK, no YAE, 256x212). GRAPHIC7 + R#25 YJK. Grayscale ramp."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x0E)
    set_reg(a, 1, 0x40)
    set_reg(a, 2, 0x1F)
    set_reg(a, 8, 0x0A)           # SPD=1, VR=1
    set_reg(a, 9, 0x80)           # LN -> 212 lines
    set_reg(a, 25, 0x08)          # YJK only
    set_write_addr(a, 0x0000)
    fill_yjk_yramp(a)
    spin(a)
    return finalize_rom(a)


# ---------------------------------------------------------------------------
# Sprite scenario builders (Subsystem 7: SP1 single, SP2 double, SP3 collision).
# ---------------------------------------------------------------------------
def _g2_blank_background(a, backdrop):
    """SCREEN2 background of a single flat backdrop colour so sprites stand out.
    char 0 pattern = all zeros (transparent -> backdrop); name table all 0;
    colour table char0 = fg/bg both backdrop."""
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x02)           # GRAPHIC2
    set_reg(a, 1, 0x40)           # display on, sprite size 8, mag off
    set_reg(a, 2, 0x06)           # name @ 0x1800
    set_reg(a, 3, 0x80)           # colour @ 0x2000
    set_reg(a, 4, 0x00)           # pattern gen @ 0x0000
    set_reg(a, 7, backdrop & 0x0F)
    set_reg(a, 9, 0x00)
    fill_run(a, 0x0000, 0x1800, 0x00, 'g2bg_pat')     # blank tile patterns
    fill_run(a, 0x2000, 0x1800, (backdrop << 4) | backdrop, 'g2bg_col')
    fill_run(a, 0x1800, 0x0300, 0x00, 'g2bg_name')    # every cell = char 0


def build_sp1_single():
    """SP1: SCREEN2 sprite mode 1, three 8x8 single-size sprites, distinct
    colours/positions. Attribute table @ 0x1B00 (R#5=0x36), sprite pattern
    generator @ 0x3800 (R#6=0x07)."""
    a = new_asm()
    set_reg(a, 8, 0x08)           # sprites ENABLED (SPD=0), VR=1
    _g2_blank_background(a, backdrop=4)
    set_reg(a, 5, 0x36)           # sprite attr @ 0x36<<7 = 0x1B00
    set_reg(a, 6, 0x07)           # sprite pattern @ 0x07<<11 = 0x3800
    set_reg(a, 1, 0x40)           # size 8, mag off (bits1,0 = 0)
    # sprite pattern 0 = solid 8x8 box; pattern 1 = hollow ring.
    write_bytes(a, 0x3800, [0xFF] * 8)
    write_bytes(a, 0x3808, [0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF])
    # attribute table: (Y, X, pattern#, colour). Y=208 terminates.
    write_bytes(a, 0x1B00, [
        40, 40, 0, 0x0F,      # sprite0: white solid box
        80, 90, 1, 0x0A,      # sprite1: green ring
        120, 150, 0, 0x06,    # sprite2: cyan solid box
        208, 0, 0, 0,         # Y-sentinel (mode 1)
    ])
    spin(a)
    return finalize_rom(a)


def build_sp2_double():
    """SP2: SCREEN5 sprite mode 2, four 16x16 double-size (MAG) sprites with
    distinct per-line colours. Attr @ R#5=0xEF -> baseMask region; pattern
    @ R#6=0x0F (0x7800)."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x06)           # GRAPHIC4
    set_reg(a, 2, 0x1F)
    set_reg(a, 8, 0x08)           # sprites ENABLED, VR=1
    set_reg(a, 9, 0x80)           # 212 lines
    set_reg(a, 5, 0xEF)           # attr baseMask (BIOS SCREEN5 convention)
    set_reg(a, 6, 0x0F)           # sprite pattern @ 0x7800
    set_reg(a, 1, 0x42)           # display on (b6) + size16 (b1); mag off
    # light-grey bitmap background so sprites are visible (fill first 0x6A00).
    fill_run(a, 0x0000, 0x6A00, 0x44, 'sp2bg')
    set_reg(a, 1, 0x43)           # + mag (b0) -> 16x16 doubled to 32x32
    # 16x16 pattern (pattern# 0): four 8x8 quadrants at +0,+8,+16,+24.
    # solid filled square outline.
    box16 = ([0xFF, 0xFF] + [0x80, 0x01] * 6 + [0xFF, 0xFF])          # left col-halves
    # pattern layout: bytes 0..7 = top-left 8 rows, 16..23 = top-right,
    # 8..15 = bottom-left, 24..31 = bottom-right (calculate_pattern reads
    # index+0 for high 8px and index+16 for the right 8px columns).
    tl = [0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF]  # rows 0-7 left cols
    tr = [0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF]  # rows 0-7 right cols
    bl = [0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF]  # rows 8-15 left cols
    br = [0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF]  # rows 8-15 right cols
    write_bytes(a, 0x7800, tl + bl + tr + br)              # +0,+8,+16,+24
    _ = box16
    # Mode-2 Y/X/pattern sub-table @ 0x7600 (index 512 combine); 4 sprites.
    write_bytes(a, 0x7600, [
        30, 30, 0, 0,
        30, 90, 0, 0,
        90, 30, 0, 0,
        90, 90, 0, 0,
        216, 0, 0, 0,         # Y-sentinel (mode 2)
    ])
    # Per-line colour bytes (offset 0-511): sprite*16 + line. Give each sprite
    # one flat colour across all 32 mag lines (write 32 bytes per sprite).
    write_bytes(a, 0x7400, [0x0F] * 32)     # sprite0 white
    write_bytes(a, 0x7410, [0x0A] * 32)     # sprite1 green
    write_bytes(a, 0x7420, [0x06] * 32)     # sprite2 cyan
    write_bytes(a, 0x7430, [0x08] * 32)     # sprite3 red
    spin(a)
    return finalize_rom(a)


def build_sp3_collision():
    """SP3: SCREEN5 sprite mode 2, two overlapping 16x16 sprites -> S#0 bit5 (C)
    collision, plus a line-coloured (COLOR SPRITE$ analogue: per-line colour)
    sprite. Visible A/B = the overlapping sprites; the collision flag is our-
    side status (not visible in the screenshot -- reported as state)."""
    a = new_asm()
    palette(a, RAINBOW16)
    set_reg(a, 0, 0x06)           # GRAPHIC4
    set_reg(a, 2, 0x1F)
    set_reg(a, 8, 0x28)           # sprites ENABLED, VR=1, TP=1 (enable colour-0
                                  # collide path too)
    set_reg(a, 9, 0x80)
    set_reg(a, 5, 0xEF)
    set_reg(a, 6, 0x0F)
    set_reg(a, 1, 0x42)           # size16, mag off (true 16x16)
    fill_run(a, 0x0000, 0x6A00, 0x11, 'sp3bg')
    # solid 16x16 pattern# 0 (all pixels set) so overlap is guaranteed.
    write_bytes(a, 0x7800, [0xFF] * 8 + [0xFF] * 8 + [0xFF] * 8 + [0xFF] * 8)
    # two overlapping sprites (dx=8,dy=8 overlap) + one separate line-colour one.
    write_bytes(a, 0x7600, [
        60, 60, 0, 0,
        68, 68, 0, 0,         # overlaps sprite0 -> collision
        100, 140, 0, 0,       # line-colour sprite (below)
        216, 0, 0, 0,
    ])
    # per-line colour: sprite0 flat white, sprite1 flat red, sprite2 a per-line
    # colour RAMP (the COLOR SPRITE$ line-colour analogue).
    write_bytes(a, 0x7400, [0x0F] * 16)                       # sprite0
    write_bytes(a, 0x7410, [0x08] * 16)                       # sprite1
    write_bytes(a, 0x7420, [((r % 15) + 1) for r in range(16)])  # sprite2 ramp
    spin(a)
    return finalize_rom(a)


SCENARIOS = {
    "m0_text1": build_m0_text1,
    "m1_g1": build_m1_g1,
    "m2_g2": build_m2_g2,
    "m4_g3": build_m4_g3,
    "m5_g4": build_m5_g4,
    "m6_g5": build_m6_g5,
    "m7_g6": build_m7_g6,
    "m8_g7": build_m8_g7,
    "m10_yjkyae": build_m10_yjkyae,
    "m12_yjk": build_m12_yjk,
    "sp1_single": build_sp1_single,
    "sp2_double": build_sp2_double,
    "sp3_collision": build_sp3_collision,
}


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--output-dir", default="debug/m41/carts")
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
