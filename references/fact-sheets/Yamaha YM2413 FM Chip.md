# FM-PAC & Yamaha YM2413 (OPLL) — Emulator Implementation Fact-Sheet

*Companion to the V9958 VDP fact-sheet. Target: cycle/behaviour-accurate MSX emulator in C/C++.*

## TL;DR
- The **YM2413 (OPLL)** is a cost-reduced 2-operator FM chip (9 melody voices, or 6 melody + 5 rhythm) whose 15 melody instruments are hard-coded in ROM (only 1 user patch); it runs at 3.579545 MHz, produces ~49716 Hz output via a 9-bit time-division-multiplexed DAC, and is the sound core of Panasonic's **FM-PAC** cartridge and the MSX-MUSIC standard.
- For accuracy, base the DSP core on **Nuked-OPLL** (die-shot-derived, cycle-accurate; adopted by openMSX 16.0) or emu2413; model the 18-slot round-robin pipeline, the 128-step logarithmic EG with the global-counter rate mechanism, the two-waveform (full/half sine) log-sin+exp operator, and the ROM patch table dumped from the FHB013/YM2413B die.
- The FM-PAC wraps the OPLL in a 64 kB ROM (16 kB MSX-MUSIC BIOS + 48 kB firmware) mapped at 4000h–7FFFh with a bank register at 7FF7h, an 8 kB battery-backed SRAM (enable via 4Dh→5FFEh, 69h→5FFFh), OPLL access via I/O ports 7Ch/7Dh or memory-mapped 7FF4h/7FF5h, and an I/O-enable flag at 7FF6h bit 0.

## Key Findings

1. **YM2413 = "OPL Light."** Per MSX Wiki, it is "a low-cost OPL2, thus named OPLL, for 'OPL Light'... its core is a stripped-down version of the YM3812 (OPL2)." Yamaha announced the chip on 3 June 1986 (Dempa Shimbun Daily, 4 June 1986). It removed most per-operator registers, replacing them with a 15-entry ROM instrument table plus one user patch; reduced waveforms from 4 (OPL2) to 2; removed additive synthesis and the 6-bit carrier TL (carriers get 4-bit/15-level volume instead); and replaced the summing adder with a time-division-multiplexed 9-bit DAC.
2. **Two documents anchor accuracy:** the Yamaha *YM2413 Application Manual* (the register/timing bible, with known transcription errors), and **andete's** multi-year hardware reverse-engineering (envelope tables, sin/exp ROMs, AM/VIB, feedback, rhythm/noise). Nuked-OPLL and IKAOPLL are die-shot-based.
3. **The FM-PAC is the reference MSX-MUSIC cartridge** — per MSX Wiki, the Panasoft SW-M004 "was introduced by Panasonic in 1988... the only official MSX-MUSIC cartridge expansion... released under the Panasoft brand" (Japan, ¥7,800). Emulate its mapper, SRAM handshake, and dual (I/O + memory-mapped) OPLL access to run the MSX-MUSIC catalogue and PAC-save games.

## Details

### 1. Chip Identification & Context

| Property | Value |
|---|---|
| Part number | Yamaha **YM2413**, family name **OPLL** ("FM Operator Type-LL") |
| Manufacturer | Yamaha Corporation; announced 3 June 1986 |
| Process | Si-gate **NMOS** (original); YM2413B / FHB013 later variants use CMOS with reduced DAC crossover distortion |
| Package | 18-pin DIP (also 24-pin SOP variants) |
| Supply | Single +5 V, ~40–80 mA |
| Clock | 3.579545 MHz typical (NTSC colorburst); operable 2–4 MHz. AM/VIB/EG rates calibrated to 3.58 MHz |
| Output | 2 analog current outputs (MO = melody, RO = rhythm), source-follower; ~1.6 Vpp; needs external buffer/LPF/amp |
| DAC | Built-in, 9-bit, time-division multiplexed |

**OPL-family relationships (crucial for the emulator's shared DSP core):**

| Chip | Name | Ops/ch | Voices | Waveforms | Notes |
|---|---|---|---|---|---|
| YM3526 | OPL | 2 | 9 (or 6+5 rhythm) | 1 (sine) | External YM3014B DAC |
| Y8950 | MSX-AUDIO (OPL1+ADPCM) | 2 | 9 | 1 | Adds ADPCM |
| YM3812 | OPL2 | 2 | 9 | 4 | AdLib/SoundBlaster |
| **YM2413** | **OPLL** | **2** | **9 (or 6+5)** | **2 (full/half sine)** | **ROM patches; built-in DAC; MSX-MUSIC** |
| YMF262 | OPL3 | 2 or 4 | 18 (or 4-op pairs) | 8 | Stereo |
| YM2414 | OPZ | 4 | 8 | multiple | TX81Z (OPM/OPZ lineage, not OPL) |

Note: YM2414 is **OPZ** (a YM2151/OPM-lineage 4-op chip), not an OPL-family part; the task brief's "YM2414/OPLB" label is a misnomer — there is no "OPLB."

**Derivatives to be aware of:** YM2420 (OPLL2, some register bits bit-reversed to deter piracy), YM2423 (OPLL-X, different patches), YMF281 (OPLLP, different patches), YM2413B/YMF281B (low-power CMOS, better DAC), FHB013 (CMOS; the only chip with a public die shot — its patch ROM differs from the YM2413's), Konami **VRC7/DS1001** (6 channels, no rhythm DAC, different patches; used only in *Lagrange Point*, released in Japan on 26 April 1991), and pin-adapter clones UMC U3567/UM3567 and K-663A.

**On-cartridge block diagram (FM-PAC):**
```
MSX cartridge bus (slot, page 1 4000h-7FFFh, I/O 7Ch/7Dh)
        |
   [glue logic / mapper] --7FF7h ROM bank, 7FF6h I/O-enable, 5FFE/5FFF SRAM handshake
    |          |            |
 [64kB ROM]  [YM2413] --XIN/XOUT 3.579545MHz xtal
 (BIOS+fw)     | MO+RO
 [8kB SRAM]   [reconstruction filter R7/R8=4.7k, C12/C13=0.015uF] --> [buffer/amp] --> mono out (3-way volume switch)
   |
 [CR2025/BR2025 3V battery] -> SRAM backup
```

### 2. FM-PAC Cartridge Specifics (Panasonic/Matsushita SW-M004, "FM Pana Amusement Cartridge," 1988)

- **Contents:** YM2413 + 64 kB ROM (16 kB MSX-MUSIC BIOS + MSX-MUSIC BASIC, plus 48 kB firmware/demo) + 8 kB battery-backed SRAM + 3-position volume switch, mono output. It is the only official MSX-MUSIC cartridge (Panasoft brand). Minimum requirement: MSX1 with 8 kB RAM, one slot with ±12 V.
- **ROM mapping / bank switching:** ROM occupies **page 1 only, 4000h–7FFFh**, one 16 kB bank at a time (64 kB = 4 banks). **Bank register = 7FF7h**, read/write, only **bits 0–1** used (bits 2–7 read 0). Per BiFi (msxnet.org): "The ROM is in memory area 4000h-7FFFh. By writing to address 7FF7h, you can switch 16kB blocks... This address is read/write but since there are only 4 pages, bits 2-7 are always zero." This is a dedicated "FMPAC" mapper (not generic ASCII-16). Bank 0 holds the MSX-MUSIC BIOS (with the ID string; active at reset); the 48 kB firmware lives in banks 1–3, paged in by `CALL FMPAC`/`_FMPAC`.
- **BIOS/ID detection:** an 8-byte ID at **4018h–401Fh**. External FM-PAC = `"PAC2OPLL"` (`PAC2` at 4018h, `OPLL` at 401Ch); internal MSX-MUSIC and many clones = `"APRLOPLL"`. MSX-MUSIC BASIC adds `CALL MUSIC`; the firmware adds `CALL FMPAC` (demo, virtual keyboard, SRAM backup/restore).
- **FM-BIOS entry points** (in BIOS bank, page 1): WRTOPL #4110 (A=reg, E=data), INIOPL #4113, MSTART #4116, MSTOP #4119, RDDATA #411C (read instrument data), OPLDRV #411F (hook into H.TIMI).
- **8 kB SRAM:** enable by writing **4Dh to 5FFEh AND 69h to 5FFFh** → SRAM appears at **4000h–5FFFh** (the two magic bytes are then readable at 5FFE/5FFF). Any other value at either address disables SRAM (restores ROM); commonly 0 is written to disable. When SRAM is active, 6000h–7FFFh reads FFh **except** the live 7FF6h/7FF7h registers. SRAM is organised as **8 pages × 1024 bytes** (page1 0000h–03FFh … page8 1C00h–1FFFh; page 8 has 1022 usable bytes because 7FF6/7FF7 alias). Save files use a 16-byte header `"PAC2 BACKUP DATA"` + 1FFEh bytes. Battery: non-rechargeable **CR2025 or BR2025 Lithium 3V** coin cell (SRAM loses data without it). The identical SRAM (without FM) is the "PAC" cartridge; ROM-less PAC cannot be detected by string-matching, so some games (e.g. Dragon Slayer 6) only support the FM-PAC.
- **OPLL access (two methods):**
  - **I/O ports:** **7Ch** = register/address (write-only), **7Dh** = data (write-only). On the external FM-PAC these ports are gated by **7FF6h bit 0** (must be set to 1 to enable them). Do NOT set 7FF6h bit 0 when an internal "APRLOPLL" device is present: per MSX Wiki, "In Panasonic A1WX and A1WSX seting bit 0 in any address between #7FF0 and #7FFF will disable the MSX-MUSIC ROM completely."
  - **Memory-mapped:** **7FF4h** = YM2413 register/address port (write-only), **7FF5h** = YM2413 data port (write-only); always available on external MSX-MUSIC (clones may lack this).
  - 7FF6h: bit 0 = OPLL I/O enable; bit 4 writable (unknown function); other bits always 0.
- **Detection protocol (emulator must reproduce):** Per MSX Wiki (MSX-MUSIC programming): "There is text 'APRLOPLL' in the ROM an internal device at 4018h~401Fh... There is text 'OPLL'... external device at 401Ch~401Fh (Panasoft FM-PAC). If the first is found, you can directly use the OPLL on the I/O ports." Scan all slots/subslots for `"APRLOPLL"` at 4018h first (internal takes precedence); only if absent, match `"OPLL"` at 401Ch and enable I/O via 7FF6h bit 0 (or use memory-mapped I/O). Wrong order makes the FM-PAC double up with internal MSX-MUSIC.

### 3. YM2413 Register Map

Access model: two logical ports — **address latch** then **data**. On the MSX these are I/O ports 7Ch (address) / 7Dh (data), or FM-PAC memory-mapped 7FF4h/7FF5h. The chip is **write-only** (no register readback, no status/busy flag). Internally the register file is 271 bits.

**Master register map:**

| Addr | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | Function |
|---|---|---|---|---|---|---|---|---|---|
| $00 | AM | VIB | EG-TYP | KSR | MUL[3:0] | | | | User-patch modulator |
| $01 | AM | VIB | EG-TYP | KSR | MUL[3:0] | | | | User-patch carrier |
| $02 | KSL[1:0] | | TL[5:0] | | | | | | User modulator KSL + total level |
| $03 | — | DC | DM | FB[2:0] | | | | | User carrier/mod waveform + feedback |
| $04 | AR[3:0] | | | | DR[3:0] | | | | User modulator attack/decay |
| $05 | AR[3:0] | | | | DR[3:0] | | | | User carrier attack/decay |
| $06 | SL[3:0] | | | | RR[3:0] | | | | User modulator sustain-level/release |
| $07 | SL[3:0] | | | | RR[3:0] | | | | User carrier sustain-level/release |
| $0E | — | — | R (rhythm en) | BD | SD | TOM | T-CY | HH | Rhythm control |
| $0F | TEST | | | | | | | | Test register (keep 0) |
| $10–$18 | F-Number[7:0] | | | | | | | | Per-channel F-Num low (ch 0–8) |
| $20–$28 | — | — | SUS | KEY | BLOCK[2:0] | | | F-Num[8] | Per-channel sustain/key-on/block/F-Num MSB |
| $30–$38 | INST[3:0] | | | | VOL[3:0] | | | | Per-channel instrument select + volume |

**Bit-field semantics:**
- **MUL[3:0]** ($00/$01): frequency multiple. Table: 0→½, 1→1, 2→2 … 9→9, A→10, B→10, C→12, D→12, E→15, F→15.
- **KSR** (D4): key-scale-of-rate. Effective envelope RATE = 4·R + Rks, where Rks is a block/F-num offset (KSR=1 gives the full offset 0–15; KSR=0 gives 0–3).
- **EG-TYP** (D5): 0 = percussive (envelope releases after decay/at key-off using RR), 1 = sustained (holds at sustain level while keyed).
- **VIB** (D6): vibrato enable (pitch mod, ~6.1 Hz measured / 6.4 Hz per datasheet, ≈14 cent depth).
- **AM** (D7): amplitude modulation / tremolo enable (~3.7 Hz, 4.8 dB depth).
- **TL[5:0]** ($02): modulator total level, 0.75 dB/step, range to 47.25 dB (modulation index control).
- **KSL[1:0]** ($02 D7-D6): key-scale level attenuation: 00→0, 10→1.5, 01→3.0, 11→6.0 dB/oct.
- **DC / DM** ($03 D4/D3): carrier / modulator waveform select — 0 = full sine, 1 = half-rectified sine (this is the OPLL's only "waveform" control; two waveforms total vs OPL2's four).
- **FB[2:0]** ($03): modulator self-feedback; modulation index 0, π/16, π/8, π/4, π/2, π, 2π, 4π (exponential — it is a shift amount).
- **AR/DR/SL/RR** ($04–$07): 4-bit attack rate, decay rate, sustain level (SL 3 dB/step from top), release rate.
- **F-Number** ($1x + $2x bit0): 9-bit; **BLOCK** ($2x D3-D1): octave 0–7; **KEY** ($2x D4): key-on; **SUS** ($2x D5): when 1, release rate becomes 5 at key-off.
- **INST[3:0] / VOL[3:0]** ($3x): instrument 0 (user) or 1–15 (ROM); carrier volume 3 dB/step (15 levels; 0 = loudest, F = quietest — it is attenuation).

**Melody vs Rhythm mode:** $0E bit 5 = 0 → 9 melody channels (each ch = modulator+carrier). =1 → channels 0–5 melody, channels 6–8 become 5 percussion sounds; $0E bits 0–4 key the individual drums, and $26/$27/$28 key-on bits must be 0.

### 4. FM Synthesis Architecture

- **2 operators/channel**, one modulator → one carrier (single fixed algorithm; phase modulation of carrier by modulator). Feedback path only on the modulator (melody channels). Sines are generated via a **log-sin table** (256 entries, 1st quadrant, 12-bit) fed to an **exp table** (256 entries, 10-bit) — the classic Yamaha logarithmic operator; output ≈ `expTable[ logsin[phase] + 128·volume + 16·envelope ]`.
- **Difference from OPL2:** OPL2 exposes all operator parameters per channel in registers. OPLL exposes only channel 0's operators (the "user patch," $00–$07) plus a 4-bit instrument selector; instruments 1–15 are hard-coded ROM parameter sets. Any patch (user or ROM) can be used by any number of channels simultaneously. Waveforms cut to 2; carrier volume cut to 4 bits.
- **The 15 ROM melody patches** (per Application Manual naming): 1 Violin, 2 Guitar, 3 Piano, 4 Flute, 5 Clarinet, 6 Oboe, 7 Trumpet, 8 Organ, 9 Horn, 10 Synthesizer, 11 Harpsichord, 12 Vibraphone, 13 Synthesizer Bass, 14 Acoustic Bass, 15 Electric Guitar; instrument 0 = user/custom.

**ROM patch parameter dump** (8 bytes = regs $00–$07: mod $00/$02/$04/$06, car $01/$03/$05/$07). This is the community-standard set (originally by-ear, later confirmed via the FHB013 die shot / NukeYKT debug-mode dump):

| # | Name | $00 | $01 | $02 | $03 | $04 | $05 | $06 | $07 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Violin | 71 | 61 | 1E | 17 | D0 | 78 | 00 | 17 |
| 2 | Guitar | 13 | 41 | 1A | 0D | D8 | F7 | 23 | 13 |
| 3 | Piano | 13 | 01 | 99 | 00 | F2 | C4 | 11 | 23 |
| 4 | Flute | 31 | 61 | 0E | 07 | A8 | 64 | 70 | 27 |
| 5 | Clarinet | 32 | 21 | 1E | 06 | E0 | 76 | 00 | 28 |
| 6 | Oboe | 31 | 22 | 16 | 05 | E0 | 71 | 00 | 18 |
| 7 | Trumpet | 21 | 61 | 1D | 07 | 82 | 81 | 10 | 07 |
| 8 | Organ | 23 | 21 | 2D | 14 | A2 | 72 | 00 | 07 |
| 9 | Horn | 61 | 61 | 1B | 06 | 64 | 65 | 10 | 17 |
| 10 | Synthesizer | 41 | 61 | 0B | 18 | 85 | F7 | 71 | 07 |
| 11 | Harpsichord | 13 | 01 | 83 | 11 | FA | E4 | 10 | 04 |
| 12 | Vibraphone | 17 | C1 | 24 | 07 | F8 | F8 | 22 | 12 |
| 13 | Synth Bass | 61 | 50 | 0C | 05 | C2 | F5 | 20 | 42 |
| 14 | Acoustic Bass | 01 | 01 | 55 | 03 | C9 | 95 | 03 | 02 |
| 15 | Electric Guitar | 61 | 41 | 89 | 03 | F1 | E4 | 40 | 13 |

Rhythm patches (3 entries, shared): BD `01 01 18 0F DF F8 6A 6D`; SD/HH `01 01 00 00 C8 D8 A7 48`; TOM/TC `05 01 00 00 F8 AA 59 55`. Note: these are the "canonical" values; emu2413's separately-estimated YM2413B table differs slightly (e.g. Piano `13 01 99 00 F2 C4 21 23`), and Nuked-OPLL stores decoded per-field patches from the die. VRC7 patches are entirely different, and its snare byte $07 is `68` vs YM2413's `48`.

### 5. Envelope Generator & Operators

- **EG:** 4-phase ADSR, **128 levels over 48 dB** → **0.375 dB/step** (the datasheet's "0.325 dB" is a confirmed typo; measured 48/128). Logarithmic (attenuation) domain. Attack rises exponentially; decay/sustain/release are linear-in-dB. Attack ends at 0 dB → decay; decay→sustain at SL; sustain→release at key-off.
- **Rate mechanism (andete, matches Burczynski/openMSX):** effective 6-bit rate = 4·R + Rks. Rates 0–3 = no change; 60–63 = advance 2 levels/sample. Otherwise: `eg_shift = 13 - (rate/4)`, `eg_select = rate & 3`. A single **global counter shared by all 18 operators** increments each sample; an operator advances only when the low `eg_shift` bits of the counter are zero, and by the amount from one of four 8-entry select tables ({0,1,0,1,0,1,0,1}=4/8, {0,1,0,1,1,1,0,1}=5/8, {0,1,1,1,0,1,1,1}=6/8, {0,1,1,1,1,1,1,1}=7/8). Consequence: the first decay segment after key-on is typically shorter (global counter not zero at phase start) — audible and worth emulating. andete found rates 52–59 deviate from the naive algorithm (needs 16-entry select tables) — a real-hardware quirk.
- **Operator parameters:** MUL (multiple), KSL (key scale level, per octave), TL (total level, modulator only, 0.75 dB), KSR (key scale of rate), 2 waveforms (DC/DM), AM/VIB flags, EG-TYP (percussive/sustained), FB (feedback). Carrier level is the 4-bit channel VOL (3 dB steps), not a 6-bit TL.
- **Feedback:** based on the **average of the modulator's last two output samples** (requires a 2-deep delay chain per channel — confirmed on the die: a 2×9×12-bit region).
- **AM/VIB LFOs:** AM triangle, 14 levels (low bit of the standard OPL2 AM table dropped), ~3.7 Hz, 4.8 dB depth. VIB via an 8×8 PM table indexed by top 3 bits of F-Num, ~6.1 Hz, ≈14 cent. Both LFOs are global.

**Envelope decay/release timing (0%→100%, ms @ 3.579545 MHz), Application Manual Table III-7, reconstructable exactly** with `cycles = (rate<60) ? (1<<(14-(rate/4)))·s[rate&3] : 63`, s = {127,102,85,73}, ×(72/3579545 s):

| RATE | +0 | +1 | +2 | +3 |
|---|---|---|---|---|
| 4 | 20926.6 | 16807.2 | 14006.0 | 12028.7 |
| 8 | 10463.3 | 8403.6 | 7003.0 | 6014.3 |
| 12 | 5231.6 | 4201.8 | 3501.5 | 3007.2 |
| 16 | 2615.8 | 2100.9 | 1750.8 | 1503.6 |
| 20 | 1307.9 | 1050.5 | 875.4 | 751.8 |
| 24 | 654.0 | 525.2 | 437.7 | 375.9 |
| 28 | 327.0 | 262.6 | 218.8 | 187.9 |
| 32 | 163.5 | 131.3 | 109.4 | 94.0 |
| 36 | 81.7 | 65.7 | 54.7 | 47.0 |
| 40 | 40.9 | 32.8 | 27.4 | 23.5 |
| 44 | 20.4 | 16.4 | 13.7 | 11.7 |
| 48 | 10.2 | 8.2 | 6.8 | 5.9 |
| 52 | 5.11 | 4.10 | 3.42 | 2.94 |
| 56 | 2.55 | 2.05 | 1.71 | 1.47 |
| 60 | 1.27 | 1.27 | 1.27 | 1.27 |

(Attack times are separately tabulated; rates 0–3 = ∞/no change, 60–63 ≈ 0.)

### 6. Rhythm Mode

Enabled by $0E bit 5. Channels 6–8 (slots 12–17 in 0-based / 13–18 in the manual's 1-based numbering) become 5 fixed drums. Slot/operator assignment:
- **Bass Drum (BD):** channel 6, both operators (mod6+car6), normal 2-op FM.
- **Hi-Hat (HH):** channel 7 modulator (mod7). Uses noise + special phase generation.
- **Snare Drum (SD):** channel 7 carrier (car7). 1-bit noise + square, phase-generator override.
- **Tom-Tom (TOM):** channel 8 modulator (mod8). Essentially 1-op.
- **Top Cymbal (T-CY):** channel 8 carrier (car8). Special synthesis.

HH+SD share channel-7 frequency; TOM+TC share channel-8 frequency. Yamaha's recommended fixed writes: $16=20h, $17=50h, $18=C0h (F-Num/block), $26=05h, $27=05h, $28=01h (block, key-off). The optimal HH/SD/TC frequency ratio is 3:1 (f_ch7 = 3·f_ch8), fed to a white-noise generator (LFSR). **$0E bits 0–4** = individual drum key-on (BD/SD/TOM/T-CY/HH); volumes: $36 = BD; $37 = HH(hi nibble)/SD(lo); $38 = TOM(hi)/T-CY(lo).

**Quirks:** (1) Each rhythm sound is **output twice** per DAC cycle (the manual: "the same percussion sounds are output twice") — effectively +6 dB after LPF; andete verified the 2nd output is identical even if params change mid-cycle. (2) You cannot disable individual drums to free their operators for melody — the 6-operator block is committed while rhythm mode is on. (3) $26/$27/$28 key-on bits must stay 0 or they fight the $0E drum keys. (4) VRC7 lacks the rhythm DAC entirely, so rhythm streams are silent there.

### 7. Output & Mixing

- **Time-division multiplexing:** one hardware operator cell serves all 18 slots; it is traversed 18× per sample. Each slot occupies **4 master-clock cycles**, so a full channel scan = 18×4 = 72 cycles → sample rate = 3.579545 MHz / 72 = **49716 Hz**. Rhythm sounds are emitted twice within the cycle. There is **no internal adder**; the 9-bit DAC plays short segments of each channel in sequence (like the later YM2612), producing high-frequency multiplex components (>1 MHz) that require external low-pass filtering.
- **DAC:** 9-bit, ~2/5·Vcc max swing, ~-65 dB noise floor. Output levels are non-linear: carrier volume steps are 3 dB (logarithmic), and the DAC's per-code levels are unequally spaced (andete measured 511 distinct values, level -256 missing; peak amplitudes per volume 0–15 = 255,180,127,90,63,45,31,22,15,11,7,5,3,2,1,1). Two analog pins: **MO** (melody) and **RO** (rhythm), each a source follower.
- **FM-PAC analog path:** MO+RO summed, through a reconstruction low-pass filter (R7/R8 = 4.7 kΩ, C12/C13 = 0.015 µF), buffer/amp, then mono out with a 3-position volume switch. Note: sd_snatcher discovered the Yamaha Application Manual's reference filter has a decimal-point error giving a ~2.26 kHz cutoff (should be ~20 kHz) — many real machines/cartridges inherit this muffled response; emulators applying a naive 20 kHz LPF will sound brighter than some real hardware.
- **PSG mixing:** the OPLL output is mixed with the MSX's internal AY-3-8910/YM2149 PSG externally (on the cartridge / mainboard), not inside the chip. The blend ratio varies by machine and is a known source of "sounds different" complaints across FM-PAC vs internal MSX-MUSIC vs turboR.

### 8. Timing

- **Sample clock:** input / 72 = 49716 Hz @ 3.579545 MHz. Phase accumulator: 9-bit F-Num → phase increment `ΔP = (F-Num · 2^BLOCK · MUL)` in a ~18–19-bit accumulator; upper 10 bits index the sine (top 2 bits mirror, next 8 index the quadrant table); no interpolation.
- **Register write timing (real hardware constraint):** after an **address write**, wait **≥12 master cycles** (~3.36 µs); after a **data write**, wait **≥84 master cycles** (~23.52 µs) before the next write. On a stock 3.58 MHz Z80 MSX these correspond to the 12/84-cycle waits (an `OUT` address then `OUT` data back-to-back satisfies the 12-cycle rule). Violating the 84-cycle rule causes dropped/wrong register writes on real hardware.
- **No busy flag / no readback:** unlike OPM/OPN, the OPLL has no status register and no busy bit; software must time waits blindly. There is no interrupt/timer on the OPLL, so MSX drivers slave tempo to the VDP 50/60 Hz interrupt.
- **Emulation note:** openMSX (Nuked-OPLL core) currently has the too-fast-access-timing emulation *disabled*, so it does not reproduce the corruption from sub-84-cycle writes; a behaviour-accurate emulator should optionally model the 12/84-cycle constraints. VRC7 (Lagrange Point) enforced even longer 24/122-cycle delays.

### 9. Known Hardware Quirks, Undocumented Behaviour, Emulation Pitfalls

- **No real YM2413 die shot exists publicly** — all die-based work uses the **FHB013** (a CMOS OPLL derivative with a *different* patch ROM). So patch-ROM contents were reverse-engineered by ear/recording (andete, emu2413) and later by NukeYKT's debug-mode serial dump; treat the patch table as "best known," not datasheet-authoritative.
- **EG datasheet typos:** "0.325 dB" step (really 0.375), and several transcription errors in the Table III-7 rate columns. The Burczynski-derived cores originally used 256 EG levels (inherited from OPL2/3); real OPLL has **128**.
- **EG rate 52–59 anomaly:** real hardware transitions differ from the simple 8-entry-table algorithm; needs 16-entry select tables to match measurements.
- **Global-counter phase dependence:** because one counter drives all EGs and LFOs, the exact moment of key-on shifts the first envelope segment length and can shift peak amplitudes — deterministic but easy to get wrong.
- **Attenuation non-linearity:** volume/TL tables and DAC codes are logarithmic and unequally spaced; use measured tables, not linear scaling.
- **Rhythm double-output** (+6 dB) and the fixed operator commitment (see §6).
- **Reference cores & how they differ:**
  - **emu2413** (Mitsutaka Okazaki): widely ported, fast, table-based; historically used estimated patches; good but not fully cycle-exact.
  - **Nuked-OPLL** (Alexey "NukeYKT" Khokholov): die-shot-based (VRC7/FHB013), **cycle-accurate**, models the 18-slot pipeline and internal signals; the current gold standard; GPL-2.0. Adopted by openMSX 16.0 ("Oh Shucks," which the release notes describe as "a super accurate YM2413 emulation (originally written by NukeYKT)").
  - **MAME/Burczynski (ym2413.c)**: older, some values marked "until we find what it really is on real chip"; the 256-vs-128 EG-level issue.
  - **IKAOPLL** (Verilog, die-shot-based, BSD-2): FPGA-oriented cycle-accurate core; 9-bit output + built-in mixer.
  - **VM2413** (VHDL): FPGA clone (1chipMSX/MiSTer).
  - FPGA OPLLs (1chipMSX, Carnivore2, GR8NET) are reverse-engineered and, per Grauw, historically *less* accurate than Nuked-OPLL for edge cases like PCM-via-DAC playback.
- **Real vs naive emulation deltas:** correct 20 kHz LPF vs the FM-PAC's inherited ~2.26 kHz filter error; TDM/DAC nonlinearity and quantization; NMOS DAC crossover distortion (reduced on YM2413B/CMOS parts); the too-fast register-write corruption most emulators skip.

### 10. Comparison Notes (OPLL vs OPL2, and why consumer devices chose it)

- **OPL2 (YM3812):** every operator fully programmable per channel (multiple TLs, 4 waveforms, additive/FM select), needs an external YM3014B DAC, 9 channels — flexible but more registers/pins/cost and harder to program.
- **OPLL (YM2413):** fixed ROM patches (1 user patch), 2 waveforms, 4-bit carrier volume, built-in 9-bit DAC and oscillator, 18-pin DIP — much cheaper, tiny board footprint, and trivially easy to drive (pick an instrument number, set note/volume). The trade-off is limited timbral flexibility (only one custom voice at a time).
- **Why FM-PAC / SMS FM unit / VRC7 chose OPLL:** cost and integration. Panasonic wanted a cheap FM add-on for MSX (curiously choosing the OPL2-incompatible OPLL over the Y8950-compatible YM3526); Sega's Mark III/Japanese Master System FM unit and Konami's VRC7 (*Lagrange Point*, 1991) likewise valued the single-chip, low-cost, self-contained DAC design. The ROM-patch approach that saved silicon is exactly what makes the chip's "sound" instantly recognizable and what emulators must reproduce faithfully.

## Recommendations

1. **Base the DSP core on Nuked-OPLL** (if GPL-2.0 is acceptable) for behaviour accuracy, or emu2413 (permissive) if you need a license-compatible, lighter core; if writing from scratch, implement the **18-slot round-robin pipeline at 49716 Hz**, the log-sin(256,12b)+exp(256,10b) operator, 2-deep feedback averaging, the 128-level EG with the global-counter/eg_shift/eg_select mechanism, and the measured patch/volume/DAC tables. Benchmark against real-hardware VGM recordings.
2. **Implement the FM-PAC mapper exactly:** ROM at 4000h–7FFFh with bank register 7FF7h (bits 0–1); SRAM handshake (4Dh→5FFEh, 69h→5FFFh) exposing 8 kB at 4000h–5FFFh with battery persistence to a `.pac`-style file; both OPLL access paths (I/O 7Ch/7Dh gated by 7FF6h bit 0, and memory-mapped 7FF4h/7FF5h); the `APRLOPLL`/`PAC2OPLL` ID strings at 4018h; and the internal-first detection order.
3. **Model timing constraints as an option:** enforce 12/84-cycle write waits (flag-gated, since openMSX disables this); slave nothing to a nonexistent OPLL interrupt.
4. **Get the analog stage right:** offer both a correct ~20 kHz reconstruction LPF and an optional FM-PAC-authentic (~2.26 kHz) filter; provide adjustable OPLL:PSG mix. **Threshold to revisit:** if A/B tests against real FM-PAC/A1WX/turboR recordings reveal timbre mismatch, first suspect the LPF cutoff and PSG mix ratio, then the patch table variant (YM2413 vs YM2413B), then EG rate 52–59 handling.
5. **Pick the patch table deliberately:** use the canonical YM2413 table for MSX FM-PAC; keep VRC7 and YMF281 tables separate and switchable if you later emulate those systems.

## Caveats
- No public YM2413 die shot exists; patch ROM values are empirically derived (FHB013 die + by-ear/recording estimates) and vary slightly between the canonical table, emu2413's YM2413B estimates, and Nuked-OPLL's decoded fields. Treat exact patch bytes as ~99% but not datasheet-certain.
- The Application Manual contains confirmed typos (EG step size; Table III-7 columns) and awkward translations; cross-check against andete's measurements.
- The FM-PAC reset-bank fact (bank 0 = BIOS active at power-on) is inferred from the detection protocol and emulator behaviour rather than a verbatim datasheet line; verify against openMSX `RomFmPac.cc` if bit-exact boot behaviour matters.
- Analog/DAC characteristics differ between NMOS YM2413 and CMOS YM2413B/FHB013 (crossover distortion, DAC output), and among pin-adapter clones (U3567/UM3567/K-663A) — "accurate" depends on which physical part you target.
- "YM2414/OPLB" in the source brief is a mislabel: YM2414 is OPZ (OPM lineage), and no "OPLB" exists; the closest OPL-adjacent Yamaha parts are YM3526/OPL, YM3812/OPL2, YMF262/OPL3, YMF278/OPL4.