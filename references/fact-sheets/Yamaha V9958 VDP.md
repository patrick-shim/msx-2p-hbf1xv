# Yamaha V9958 VDP — Emulator Implementation Fact-Sheet (Sony HB-F1XV context)

## TL;DR
- The V9958 (Yamaha MSX-VIDEO, part of the "V99x8" family, ID#=2 in status register S#1 bits 1–5) is a minor superset of the V9938: it adds three write-only registers (R#25/26/27), hardware horizontal scrolling, a software /WAIT generator, command-engine-in-all-modes, and three YJK/YJK+YAE high-colour modes (SCREEN 10/11/12) with a 15-bit (5:5:5) DAC. Per the MAP/Grauw "The YJK screen modes" article, SCREEN 12 (YJK) shows up to **19,268 colours** and SCREEN 10/11 (YJK+YAE) shows up to **12,499 YJK colours + 16 palette colours (out of 512)**. Everything else — register map R#0–R#23/R#32–R#46, status S#0–S#9, sprites, command engine, timing — is identical to the V9938.
- In the Sony HB-F1XV the V9958 drives 128 KB VRAM, is fed a ~21.477 MHz crystal, outputs CPUCLK (~3.58 MHz) to the Zilog Z80A, and is glued to the CPU/PPI/PSG/RTC by a Yamaha S1985 MSX-ENGINE. It is NTSC/60 Hz (262 lines/frame). Composite/RGB conversion is done off-chip on a "HIC-1" hybrid daughterboard; the V9958 itself dropped the V9938's composite-video and mouse/lightpen functions.
- For behaviour-accurate emulation the hard parts are: command-engine VRAM-slot timing (openMSX's logic-analyzer model), YJK↔RGB rounding/clipping, the 1-pixel vertical sprite shift, VRAM address interleaving in G6/G7, and several undocumented bits/edge-cases. Treat the V9958 as a V9938 core plus a thin "MSX2+ feature layer."

## Key Findings

### Identity and lineage
- **Part**: Yamaha V9958, "MSX-VIDEO." Catalog No. 249958Y, data book dated 1988.12. Successor to V9938 (MSX2); predecessor of the cartridge-only V9990. Also sold as the "TI Image Maker" for the TI-99/4A.
- **Process/package**: N-channel silicon-gate MOS, 5 V single supply, linear RGB output. 64-pin shrink DIP (package drawing shows 64 pins, ~58 mm body). Typical current consumption 230 mA.
- **VDP ID**: readable at power-on in S#1 bits 1–5 — per the MSX Wiki "VDP Status Registers" page, `0`=V9938, `2`=V9958 (also `3`=V9968, `4`=V9978 for later/hypothetical chips). This is the canonical runtime way to detect the chip; emulators must return 2. On the V9958 the S#1 LPS and FL bits "always [read] value 0 on MSX2+ VDP."
- **Register count**: 39 write-only control registers (R#0–R#23, R#32–R#46) inherited from V9938, plus 3 new (R#25, R#26, R#27). R#24 does not exist (skipped). 10 read-only status registers S#0–S#9. Palette: 16 entries × 9-bit (3:3:3) → 512 colours.

### What the V9958 ADDS vs V9938 (the entire MSX2+ delta)
1. **R#25 (mode/feature register)** — new bits: `CMD`(b6) command-engine-in-all-modes, `VDS`(b5) pin-8 CPUCLK-vs-VDS select, `YAE`(b4), `YJK`(b3), `WTE`(b2) wait enable, `MSK`(b1) mask left 8 dots, `SP2`(b0) 2-page horizontal scroll.
2. **R#26/R#27 horizontal scroll** — R#26 holds H08–H03 (8-dot page units + which page), R#27 holds H02–H00 (fine 0–7 dot right shift). In G5/G6 the units are doubled (2-dot fine, 16-dot coarse).
3. **YJK / YJK+YAE colour modes** (SCREEN 12 and 10/11).
4. **Built-in software /WAIT generator** (WTE) to let the VDP stall the CPU on VRAM access — used only on a few turbo machines, not on HB-F1XV.
5. **Command engine usable in non-bitmap modes** via R#25 CMD bit. Per the Portar MSX Tech Doc: "Normally MSX2 Video Commands can be used in Screen 5-8 only. However, by setting the CMD bit Video Commands may be used in all other Screen modes ... accessing VRAM bytewise (as in Screen 8), therefore X/Y coordinates as in Screen 8 must be used."

### What the V9958 REMOVES vs V9938
Per §5.3 of the V9958 data book and the Portar MSX Tech Doc: "The V9958 does not output a Composite Video Signal, and does not include a Mouse/Lightpen Interface. In the result, the following bits have been removed (and should be set to zero): Bit 5 of VDP Reg 00h (IE2), Bit 6 and 7 of VDP Reg 08h (LP and MS), Bit 6 and 7 of VDP Status Reg 01h (LPS and FL)." Composite is regenerated off-chip.

## Details

### 1. Chip identification, pins & HB-F1XV integration

**Clocking.** External crystal XTAL1/XTAL2 at 20.26–22.55 MHz (typ 21.48 MHz; MSX uses 21.47727 MHz = 6× the 3.579545 MHz Z80 clock, itself 3× the NTSC colour subcarrier). Pin 8 `CPUCLK//VDS` outputs XTAL÷6 ≈ 3.58 MHz to clock the Z80A (spec 3.37–3.76 MHz). Dot clocks: `*DHCLK` ≈ 10.74 MHz (high-res/512), `*DLCLK` ≈ 5.37 MHz (low-res/256). The internal VDP master clock is the full ~21.48 MHz; the openMSX timing model counts "VDP cycles" at this rate (1368 cycles/line).

**Key pins (64-DIP):** CD0–CD7 CPU data bus; MODE0/MODE1 I/O port select (= A0/A1 from CPU); `*CSR`/`*CSW` CPU read/write strobes; RD0–RD7 VRAM data bus; AD0–AD7 multiplexed VRAM address; `*RAS`, `*CAS0`, `*CAS1`, `*CASX` (expansion), `R/*W` DRAM control; R/G/B analog outputs; `*YS` superimpose transparency switch; `*HSYNC`, `*CSYNC`; `*VRESET`/`*HRESET` (tri-level sync inputs, new); `*WAIT` output (new, pin 26); `*INT` open-drain interrupt; C0–C7 colour bus (digitize/external palette); CBDR colour-bus direction. Pin changes vs V9938: pin4 became `*VRESET` (was VDS out), pin5/6 HSYNC/CSYNC became outputs only, pin8 CPUCLK//VDS, pin21 became analog VDD/DAC (was VIDEO out), pin26 `*WAIT` (was LPS in), pin27 `*HRESET` (was LPD in).

**HB-F1XV motherboard.** MSX2+ that was, per the MSX Wiki and the thefoggiest.dev teardown, "the last MSX built and marketed by Sony," released in October 1989 purely for the Japanese market. Z80A = Zilog Z0840004SPC @ 3.58 MHz; MSX-ENGINE = Yamaha **S1985** (MSX-SYSTEMII: integrates YM2149 PSG, i8255-compatible PPI, DRAM/ROM control, printer port, RP5C01-compatible RTC). FDC = MB89311. 64 KB main RAM, 16 KB SRAM (MSX-JE), **128 KB VRAM**, YM2413 (OPLL/MSX-MUSIC). The S1985 mirrors VDP ports 98–9Bh to 9C–9Fh. Video/audio analog conditioning (RGBS→composite conversion, audio mux/mute) is on a hybrid "HIC-1" daughterboard (the SMD electrolytics on it are a known failure/leak point). The machine outputs RGB (21-pin) + composite (RCA); no S-Video (unlike some Panasonic 2+ models). Emulation-relevant Sony quirks: the RAM mapper is implemented in a separate Sony MB670836 (the S1985's mapper pins are unused); the HB-F1XV does NOT wire the V9958 /WAIT feature, so software must respect VRAM access timing itself.

### 2. Video RAM configuration

- The V9958 supports 16 KB, 64 KB, or 128 KB VRAM, plus an optional (non-standard) 64 KB expansion RAM (192 KB total addressable via a 17-bit counter). HB-F1XV = 128 KB, fixed (no internal expansion socket; can only be modded).
- **DRAM types:** 16Kx1, 16Kx4, 64Kx1, 64Kx4. R#8 `VR` bit selects refresh style (set for 64K-type chips). 256-row auto-refresh every 4 ms.
- **Address counter:** R#14 holds A16–A14 (page select), then two port-#1 bytes set A13–A0 with a read/write flag (bit 6 of 2nd byte: 0=read,1=write). Auto-increment on each port-#0 access; R#14 auto-increments on carry from A13 — except in G1/G2/MC/T1 modes where it doesn't. In legacy modes the low 14 bits wrap at 16 KB boundaries (TMS9918 compat); in new modes it counts full 128 KB.
- **Bank/expansion:** first 64 KB VRAM and 64 KB expansion share the same VDP address space, selected by R#45 bit 6. Expansion RAM can only be touched by commands, never displayed.
- **G6/G7 interleave (critical for accurate emulation):** in SCREEN 7/8 (G6/G7) the VDP reads two DRAM banks in parallel to double display bandwidth. The logical→physical mapping is `physical = (logical >> 1) | (logical << 16)` (17-bit rotate right): even logical addresses → bank0 (CAS0), odd → bank1 (CAS1). All other modes have logical==physical. This is visible when switching modes; functionally you can keep a flat 128 KB array and just apply the mapping in G6/G7 command/render address generation.
- **CPU↔VDP↔VRAM access:** CPU never sees VRAM directly; it reads/writes through port #98. The safe-access rule on real hardware is, per the openMSX timing writeup: "The general consensus seems to be 'at least 29 Z80 cycles between two accesses'. For example an OUT(#99),A instruction takes 12 cycles (on MSX), so you need 17 extra cycles before the [next access]." Too-fast access drops requests (see timing section).

### 3. Screen modes

Mode selected by M1–M5 (R#1 b4,b3 and R#0 b3,b2,b1) plus LN (R#9 b7, 192 vs 212 lines) plus the YJK/YAE bits (R#25). Full list:

| BASIC | VDP mode | Type | Resolution | Colours | Sprite mode | VRAM/page |
|---|---|---|---|---|---|---|
| SCREEN 0 (W40) | TEXT1 | text | 40×24 (6×8) | 2 of 512 | none | ~1.1 KB |
| SCREEN 0 (W80) | TEXT2 | text | 80×24 / 80×26.5 | 2 (4 w/ blink) | none | ~2.2 KB |
| SCREEN 1 | GRAPHIC1 (G1) | tile | 32×24 | 16/512 | 1 | — |
| SCREEN 2 | GRAPHIC2 (G2) | tile | 256×192 | 16/512 | 1 | — |
| SCREEN 3 | MULTICOLOR (MC) | block | 64×48 | 16/512 | 1 | — |
| SCREEN 4 | GRAPHIC3 (G3) | tile | 256×192 | 16/512 | 2 | — |
| SCREEN 5 | GRAPHIC4 (G4) | bitmap | 256×212 | 16/512 | 2 | 32 KB |
| SCREEN 6 | GRAPHIC5 (G5) | bitmap | 512×212 | 4/512 | 2 | 32 KB |
| SCREEN 7 | GRAPHIC6 (G6) | bitmap | 512×212 | 16/512 | 2 | 64 KB |
| SCREEN 8 | GRAPHIC7 (G7) | bitmap | 256×212 | 256 (fixed 3:3:2) | 2 | 64 KB |
| SCREEN 10/11 | G7+YJK+YAE | bitmap | 256×212 | 12,499 YJK + 16 palette | 2 (palette) | 64 KB |
| SCREEN 12 | G7+YJK | bitmap | 256×212 | 19,268 | 2 (palette) | 64 KB |

Notes: interlace (R#9 IL) doubles vertical resolution to ×424. In G6/G7 the horizontal scroll and mask units double. SCREEN 10 vs 11 differ only in how BASIC exposes drawing (10 = palette-view, editable like SCREEN 5; 11 = raw). The YJK modes are G7 with R#25 YJK set; G6/G7 selection in YJK only affects the command engine's coordinate width. Sprites in SCREEN 8 use fixed colours, but in the YJK modes sprites use the 16-colour palette.

### 4. Register map

**Mode registers (write-only):**
```
R#0  0  DG  IE2 IE1 M5  M4  M3  0      (DG digitize dir; IE2 lightpen int [dead on 9958]; IE1 hblank int; M3–M5 mode)
R#1  0  BL  IE0 M1  M2  0   SI  MAG    (BL blank; IE0 vblank int; M1,M2 mode; SI 16x16 sprites; MAG magnify)
R#8  MS LP  TP  CB  VR  0   SPD BW     (MS/LP dead on 9958; TP colour0 transparent; CB colour-bus dir; VR VRAM type; SPD sprite disable; BW b/w)
R#9  LN 0   S1  S0  IL  EO  *NT DC     (LN 212/192; S1,S0 simultaneous src; IL interlace; EO even/odd; NT ntsc/pal; DC dot-clock dir)
```
**Table base registers:** R#2 pattern name table; R#3+R#10 colour table; R#4 pattern generator; R#5+R#11 sprite attribute table; R#6 sprite pattern generator. (Many have "must-be-1" low bits acting as AND-masks on the active address — e.g. sprite attribute table base masks; getting these wrong produces mirroring, an easy emulator bug.)
**Colour registers:** R#7 text/backdrop colour; R#12 text/back blink colour; R#13 blink on/off timing.
**Display registers:** R#14 VRAM base A16–A14; R#15 status register pointer (S0–S3); R#16 palette pointer (C0–C3); R#17 control-register pointer (+AII auto-increment-inhibit bit7); R#18 display adjust (H/V position); R#19 interrupt line; R#20–22 colour-burst (default 00/3B/05); R#23 vertical scroll (display offset).
**Palette:** written via port #2 as two bytes: byte1 = `0 R2 R1 R0 0 B2 B1 B0`, byte2 = `0 0 0 0 0 G2 G1 G0` (9-bit, 3:3:3).
**New V9958 registers:**
```
R#25  0  CMD VDS YAE YJK WTE MSK SP2
R#26  0  0   H08 H07 H06 H05 H04 H03
R#27  0  0   0   0   0   H02 H01 H00
```
All three clear to 0 on RESET → V9938-compatible behaviour.

**Command registers R#32–R#46:** R#32/33 SX (source X, 9-bit), R#34/35 SY (10-bit), R#36/37 DX, R#38/39 DY, R#40/41 NX (number X), R#42/43 NY, R#44 CLR (colour/data), R#45 ARG (`MXD/MXS` mem select, `DIY/DIX` direction, bank), R#46 = command code (bits7–4) + logical op (bits3–0). Writing R#46 starts the command.

**Status registers (read-only, select via R#15):**
```
S#0  F   5S  C   [fifth-sprite number 4:0]           (F=vblank int flag, cleared on read; 5S=5th/9th sprite; C=collision)
S#1  FL  LPS [ID# 5:1]                       FH       (FL/LPS lightpen [dead=0 on 9958]; ID#=2 for V9958; FH=hblank int flag)
S#2  TR  VR  HR  BD  1   1   EO  CE                   (TR transfer-ready; VR vblank area; HR hblank area; BD search border found; EO field; CE command executing)
S#3  X7..X0   / S#4  1 1 1 1 1 1 1 X8         (column register — collision X, or SRCH/lightpen/mouse)
S#5  Y7..Y0   / S#6  1 1 1 1 1 1 EO Y8        (row register — collision Y)
S#7  colour register (POINT/LMCM result byte)
S#8/S#9  BX0..BX8 (SRCH border X coordinate)
```
S#0 and S#1 auto-reset their interrupt flags on read; the BIOS reads S#0 every VBlank. (Community docs disagree on S#2 bits 2–3: MSX wiki reads them as 1; some list 0.)

### 5. Colour palette & YJK decoding

- **Palette DAC:** V9938 palette is 9-bit (3 bits/component). The V9958 DAC is physically 15-bit (5 bits/component + transparency). Per Grauw's YJK article, the 3-bit palette values are expanded to 5-bit by the fixed table **{0→0, 1→4, 2→9, 3→13, 4→18, 5→22, 6→27, 7→31}** (≈ floor(4.5·c), equivalently `c<<2 | c>>1`), and "This applies to all screen modes." An accurate emulator should render palette colours through this 5-bit expansion, not a naive `c*36` scaling.
- **YJK packing (SCREEN 12):** 4 consecutive pixels share one J and one K (6-bit signed two's-complement, −32..31); each pixel has its own Y (5-bit unsigned). Byte layout per 4-pixel group: pixel1 = `Y1[7:3] K[low]`, pixel2 = `Y2 K[high]`, pixel3 = `Y3 J[low]`, pixel4 = `Y4 J[high]` (each byte's top 5 bits = that pixel's Y, low 3 bits contribute to the shared K/J nibbles).
- **YJK+YAE packing (SCREEN 10/11):** same J/K, but Y is 4-bit (even values only) because Y's LSB (bit3 of the byte) is repurposed as the attribute bit A. If A=1, the pixel's upper 4 Y bits index a palette colour (16 palette colours mixed into the YJK image); if A=0 it's a normal YJK pixel.
- **Decode (from the V9958 manual):**
  `R = clamp(Y + J, 0, 31)`
  `G = clamp(Y + K, 0, 31)`
  `B = clamp(floor((5·Y − 2·J − K + 2) / 4), 0, 31)`
  The +2 models the VDP's rounding; the clamp models out-of-gamut clipping. Per Grauw, "There are 16384 (2^14) un-clipped colours, and due to the clamping to the 0-31 range you get an additional 2884 unique colours totalling 19268." An emulator that omits rounding/clipping will produce visibly wrong colours on gradient/photographic images.
- **RGB→YJK (encode):** `Y = (4B + 2R + G)/8, J = R − Y, K = G − Y`. Note this weights Y toward blue (unlike Y′UV which weights green) — a documented Yamaha idiosyncrasy, not a bug to "fix."
- **Superimpose interaction:** in pure YJK (SCREEN 12) you cannot superimpose external video except the borders; in YJK+YAE you can, by setting A=1 with palette colour 0 in the Y bits (and R#0 TP must be 0).

### 6. Sprites

- **Sprite Mode 1** (G1/G2/MC, TMS9918-compatible): 32 sprites, max 4 per line, one colour each, 5th-sprite & collision flags in S#0.
- **Sprite Mode 2** (G3–G7 and all new modes): 32 sprites, **max 8 per line**, a colour per sprite line (via the sprite colour table), plus per-line CC (priority/OR-colour), IC (inhibit-collision) and EC (early-clock/32-px left shift) attribute bits. 5S bit means "≥5 (or ≥9 in G-modes) sprites on a line"; C bit = collision (any two "on" pixels overlap, even transparent-coloured, checked once/frame).
- **Sprite attribute table** (mode 2): Y, X, pattern number per sprite; the colour table (512 bytes, one 16-byte block per sprite giving per-line colour+CC+IC+EC) sits 512 bytes below the attribute table — the hardware masks bit 9, so if the base doesn't have bits A7–A9 set you get overlap/undefined behaviour (a real WebMSX/edge-case bug source).
- **Colour-0 pixels in a sprite pattern are always transparent** regardless of TP (a documented erratum in some guides that claim TP affects them).
- **Emulation pitfalls:** (a) the whole sprite subsystem always performs the same VRAM fetch pattern (32+ y-reads then 4 chunks of 2 sprites) even if fewer/no sprites are visible — this is why command execution slows identically whenever sprites are enabled; (b) sprite data for line N is fetched during line N−1; (c) there is a **1-pixel vertical shift on all MSX VDPs** — a sprite at Y=0 appears on the 2nd displayed line (subtract 1 from Y). V9958 horizontal scroll also introduces a ~2-pixel horizontal offset relationship that trips naive emulators.

### 7. Timing & synchronisation (NTSC / HB-F1XV)

- **Line:** 1368 VDP cycles. Breakdown (set-adjust 0, S1S0=0): sync [0–100), left-erase [100–202), left-border [202–258), display [258–1282) =1024, right-border [1282–1341), right-erase [1341–1368). (With R#9 S1,S0≠0 a line is 1365 cycles.)
- **Frame (NTSC, from V9938 Data Book Table 7-2):** total 262 lines. LN=0 (192): top-erase 13 + top-border 26 + active 192 + bottom-border 25 + bottom-erase 3 + vsync 3. LN=1 (212): 13 + 16 + 212 + 15 + 3 + 3. (PAL for comparison = 313 lines total; HB-F1XV is NTSC only.)
- **Interrupts:** VBlank interrupt fires at the start of the lower border (line 192 or 212), enabled by R#1 IE0 (on by default; disabling it hangs the MSX), flag = S#0 bit7 F (cleared on read). HBlank (line) interrupt: R#19 sets the line, enabled by R#0 IE1, flag = S#1 bit0 FH (cleared on read); can be re-armed multiple times per frame. `*INT` is open-drain, active-low.
- **VBlank VRAM-access quirk (real-HW vs emulators):** on V9938/V9958, in VBlank you still need ~2 NOPs between the last address-setup OUT and the data IN — unlike the TMS9918 which allows immediate read. openMSX historically emulated this like the TMS (a known inaccuracy, openMSX issues #1154/#1402).
- **Lightpen:** deleted on V9958 (S#1 FL/LPS read 0; the lightpen pins were repurposed to /WAIT and /HRESET).
- **/WAIT generator (WTE, R#25 b2):** when enabled the VDP holds the CPU in WAIT until VRAM access completes; asserted ~130 ns after /CSR or /CSW. Not wired on HB-F1XV — irrelevant for HB-F1XV emulation but must exist as a register bit.

### 8. VDP command engine

**Command set (R#46 bits 7–4):**
| Cmd | code | Dest | Src | Unit | Function |
|---|---|---|---|---|---|
| HMMC | 1111 | VRAM | CPU | byte | high-speed CPU→VRAM |
| YMMM | 1110 | VRAM | VRAM | byte | high-speed Y-only copy (same X) |
| HMMM | 1101 | VRAM | VRAM | byte | high-speed VRAM→VRAM copy |
| HMMV | 1100 | VRAM | VDP | byte | high-speed fill |
| LMMC | 1011 | VRAM | CPU | dot | logical CPU→VRAM |
| LMCM | 1010 | CPU | VRAM | dot | logical VRAM→CPU |
| LMMM | 1001 | VRAM | VRAM | dot | logical copy |
| LMMV | 1000 | VRAM | VDP | dot | logical fill |
| LINE | 0111 | VRAM | VDP | dot | Bresenham line |
| SRCH | 0110 | VRAM | VDP | dot | search colour along X |
| PSET | 0101 | VRAM | VDP | dot | plot point |
| POINT | 0100 | — | VRAM | dot | read point → S#7 |
| STOP | 0000 | — | — | — | abort command |

**Logical operation (R#46 bits 3–0):** IMP=0000, AND=0001, OR=0010, EOR/XOR=0011, NOT=0100; transparent variants TIMP=1000, TAND=1001, TOR=1010, TEOR/TXOR=1011, TNOT=1100. Codes 0101–0111 and 1101–1111 are undefined/unused. Operation semantics (DC=destination colour, SC=source colour): IMP `DC=SC`, AND `DC=SC·DC`, OR `DC=SC+DC`, EOR `DC=SC⊕DC`, NOT `DC=~SC`. For the T-variants, wherever the **source pixel colour = 0 the destination is left unchanged** (DC=DC); only non-zero source pixels apply the op — this is the masked-blit/animation mode. Non-T ops always write (source colour 0 overwrites). Codes triple-confirmed against the Konamiman MSX2 Technical Handbook (Table 4.6), the Komkon V9958 doc, and MSX BIOS constants.

**Handshake:** writing R#46 sets S#2 bit0 `CE`=1; software must poll CE=0 before issuing the next command (else the running one aborts — poll idiom `IF VDP(-2) AND 1 THEN loop`). For data-transfer commands (HMMC/LMMC/LMCM) poll S#2 bit7 `TR`=1 before each byte via R#44; CE=0 means the whole transfer finished (documented edge case: the transfer can end internally with CE=0 even though TR was 1 after the last byte).

**Timing model (openMSX 2013 logic-analyzer measurements by Joost Yervante Damad, Alex Wulms and Wouter Vermaelen, taken at the MSX fair in Nijmegen on a Philips NMS8250; per pixel / per line, in VDP cycles):**
| Cmd | per pixel | per line |
|---|---|---|
| HMMV | 48 W | 56 |
| YMMM | 40 R 24 W | 0 |
| HMMM | 64 R 24 W | 64 |
| LMMV | 72 R 24 W | 64 |
| LMMM | 64 R 32 R 24 W | 64 |
| LINE | 88 R 24 W | 32 (extra per minor Bresenham step) |
Rewritten (openMSX's model): the command engine effectively runs at 1/8 the master clock (all values are multiples of 8) and stalls 16 cycles per VRAM request. Actual speed depends on **access-slot availability**: 154 slots/line (screen off), 88 (sprites off), 31 (sprites on). CPU VRAM access takes priority over the command engine and steals slots — with sprites on, a HMMV can be cut ~2× by simultaneous `OUT (#98),A`. Commands execute line-by-line, so a 20×4 rect is faster than 4×20. On V9958 (unlike V9938) commands also run in pattern/text modes when R#25 CMD=1, using G7-style coordinates.

### 9. Digital/analog output & superimpose

- **DAC:** V9958 outputs linear analog RGB from a 15-bit (5:5:5) DAC; R/G/B swing ≈ 0.9 Vpp (black level ≈2.2 V, max ≈3.1 V), rise/fall ≈60 ns. This is the improvement over the V9938 (whose palette DAC was effectively 3 bits/component); the extra resolution is only reachable through YJK modes since the palette itself is still 9-bit.
- **Composite deleted on-chip:** unlike V9938, the V9958 has NO composite video output pin. `*HSYNC` and `*CSYNC` are outputs only (`*CSYNC` = composite sync). Machines regenerate composite/S-video externally (Sony CXA1145/CXA1645-class encoders). On HB-F1XV this and the RGBS→composite conversion happen on the HIC-1 hybrid board; the machine provides RGB (21-pin) and composite RCA, no S-Video.
- **Superimpose/digitize:** the `*YS` pin (transparency switch) and the C0–C7 colour bus support external video superimposition and digitizing. When R#0 TP=0 (colour 0 transparent) and an external sync is fed to `*VRESET`/`*HRESET`, external video shows through colour-0 pixels. YJK-mode superimpose is restricted as noted in §5. HB-F1XV is not a digitizer/superimpose model, so these are typically unused there.

### 10. Known quirks, undocumented behaviour & emulation pitfalls

- **VDP ID:** must be 2 in S#1. Software (and the MSX2+ BIOS) uses this plus the MSX version byte to decide whether to enable YJK.
- **Undocumented R#1 bit2 side effects** (openMSX issue #1091): beyond enabling VBlank interrupt, this bit apparently changes the blink/page-switch counter clock source from VSYNC to HSYNC and can make R#9 EO/IL irrelevant — rarely modelled.
- **Command engine "too fast" copy cache:** real hardware has a small read pipeline; a self-overlapping LMMM copy with a too-small width streaks on real HW but works in emulators — copies must respect a minimum overlap distance. (Documented on V9990 but the V99x8 command engine has analogous slot/pipeline behaviour.)
- **openMSX is/was too fast on VDP commands** (issue #2057) and models VBlank VRAM access like the TMS rather than requiring the 2-NOP gap (#1402/#1154) — cautionary examples that even the reference emulator isn't perfectly cycle-accurate; test against real-HW captures where possible.
- **Address-mask "must-be-1" bits** on table base registers (name table in TEXT2, sprite attribute table in mode 2, etc.) act as AND-masks on the live address, not simple base offsets — omitting them causes mirroring artifacts.
- **1-pixel vertical sprite shift** and the V9958 horizontal-scroll border behaviour (dots scrolled off-left are undefined unless MSK masks the left 8/16 dots) must be reproduced; games like Space Manbow, Sea Sardine, F1 Spirit 3D rely on exact scroll behaviour.
- **G6/G7 address interleave** (see §2) — mode switches expose it.
- **R#25 VDS bit must stay 0** on a real MSX: setting it turns pin-8 into a VDS output and stops clocking the Z80 (machine hangs). An emulator can ignore VDS functionally but should not treat it as a normal mode bit.
- **Command modifies coordinate/display registers:** modifying R#23 (vertical scroll) or R#18 during an active command corrupts a byte (documented in the application manual) — a subtle behaviour some tests check.

## Recommendations
1. **Architect the emulator as "V9938 core + MSX2+ layer."** Implement the full V9938 register/status/sprite/command model first; add R#25/26/27, YJK decode, horizontal scroll, and CMD-in-all-modes as a thin overlay. Gate YJK/scroll behind the ID=2 path.
2. **Adopt the openMSX VRAM-slot timing model** for the command engine and CPU access (slot tables for screen-off/sprites-off/sprites-on; 16-cycle request latency; CPU priority; command engine at 1/8 clock). This is the single highest-value accuracy investment and the only source with logic-analyzer-verified numbers. Benchmark against the LINE-speed octagon test and the CMD/CPU border-colour test (openMSX issue #2057) — if your octagon is a clean square, your slope timing is wrong.
3. **Implement YJK with rounding (+2) and clamping** exactly as the formulas in §5, and expand the 3-bit palette to 5-bit via the fixed table. Validate against a known SCREEN 12 image; a correct decoder reproduces 19,268 distinct colours including the 2,884 clipped-gamut ones.
4. **Model NTSC 262-line timing** (LN-dependent border sizes from Table 7-2), VBlank IRQ at line 192/212, and HBlank IRQ via R#19 — these drive raster-split effects and the BIOS. Do not add PAL for HB-F1XV.
5. **Reproduce the documented gotchas**: 1-pixel vertical sprite shift, table-base AND-masks, sprite fetch pattern always running when sprites enabled, G6/G7 interleave, and the 2-NOP VBlank read gap. Add a test-ROM harness comparing your output frame-by-frame with openMSX and, ideally, real-HW captures.
6. **Thresholds that change the plan:** if you only target games/demos, slot-accurate command timing and YJK rounding are sufficient and you can skip DRAM-level RAS/CAS emulation. If you target timing test-suites or copy-protection/raster tricks, you must implement the full slot model, the too-fast-CPU dropped-request behaviour, and the VBlank access gap. If you later emulate a turbo MSX2+ (not HB-F1XV), you must add the WTE /WAIT generator and the "+1 cycle on OUT to VDP" MSX-ENGINE behaviour.

## Caveats
- The command-engine and VRAM-timing numbers come from openMSX's 2013 logic-analyzer measurements **on a V9938** (Philips NMS8250). The team states the YJK/screen-11/12 modes were not measured but "highly likely" behave like SCREEN 8 from a VRAM standpoint; treat V9958-specific timing as inferred, not measured.
- Timing for PSET/POINT/SRCH per-pixel was explicitly left as "TODO" by openMSX; those command speeds are less certain than the copy/fill/line figures.
- The V9958 data book is a *delta* document ("added/modified/deleted vs V9938"); all unchanged behaviour must be read from the V9938 Technical Data Book / Application Manual. Some community docs disagree on unused S#2 bits 2–3 (read as 1 per MSX wiki, 0 per some others) and on minor naming (EOR vs XOR, TEOR vs TXOR — same codes).
- HB-F1XV-specific circuit details (HIC-1, MB670836 mapper, S1985) are from MSX Wiki and community teardown pages, not an official Sony schematic; the exact crystal value in this specific unit is the standard 21.47727 MHz per MSX NTSC practice but was not independently verified for HB-F1XV in this research.
- "Behaviour-accurate" here means matching documented + community-reverse-engineered behaviour; a handful of undocumented bits (R#1 b2 effects, exact command pipeline depth) remain incompletely characterised even in the best emulators.