# Konami SCC / SCC-I — Emulator Implementation Fact-Sheet

*Companion to the Yamaha V9958/YM2413 fact-sheets. Target: behaviour-accurate MSX emulator in
C/C++. Commissioned for milestone M29 (backlog G1) per DEC-0035 and the M28 planner's own
precondition (`agent-protocol/state/deferred-backlog.md`, row G1).*

**Grounding discipline.** Every behavioural fact below is grounded in INDEPENDENT, public,
community-measurement sources — Jon De Schrijder's 2003 oscilloscope amplitude measurements,
Manuel Pazos's 2003 mode-register write-up, NYYRIKKI's 2005 MSX Resource Center post, and enen's
IRC power-on-value test — all of which are quoted VERBATIM as third-party citations in the
opening comment blocks of `references/openmsx-21.0/src/sound/SCC.cc` (lines 1–97 and 146–153).
These are the same class of independently-citable community reverse-engineering facts the YM2413
fact-sheet already treats as legitimate grounding ("andete" citations). openMSX 21.0 source is
used as the primary behaviour reference (structure/decode semantics) and fMSX 6.0
(`references/fmsx-60/source/EMULib/SCC.c`) as the independent second cross-reference; every
disagreement between the two is recorded explicitly in §9 (DEC-0030 arbitration discipline).
**No numeric table in this sheet is transcribed from either source** — see §11.

## TL;DR

- The **SCC** ("Sound Creative Chip") is Konami's custom MegaROM mapper + 5-channel wavetable
  sound generator used in late-1980s Konami MSX cartridges (Nemesis 2/3, King's Valley 2, Space
  Manbow, Solid Snake, Quarth, …). One chip provides both the 8 kB-bank ROM mapper and the sound
  generator; the sound registers appear **memory-mapped at 0x9800–0x9FFF** when enabled by a
  magic bank-register value. There is **no fixed ROM wavetable**: all five 32-byte waveforms are
  written at runtime by the game software.
- Each of the 5 channels plays a **32-byte, 8-bit signed waveform** through a **12-bit frequency
  divider** and a **4-bit volume**, gated by a 5-bit channel-enable register. On the plain SCC,
  **channel 5 has no waveform RAM of its own — it plays channel 4's waveform**.
- Digital output (Jon De Schrijder's scope measurement): `AmpOut = 640 + AmpA+AmpB+AmpC+AmpD+AmpE`
  where `AmpX = ((SampleValue*VolX) AND #7FF0) div 16` (signed multiply, low 4 bits dropped
  BEFORE summation); disabled channel ⇔ `VolX = 0`; `AmpOut` range `[+40 … +1235]` (11-bit
  positive).
- The **SCC-I** (community name "SCC+", used in Konami's RAM-based Sound Cartridge for
  Snatcher/SD-Snatcher) adds an independent channel-5 waveform, a RAM-mapper mode, and an
  alternative register window at **0xB800–0xBFFF**; a mode register at 0xBFFE/0xBFFF selects
  SCC-compatible vs SCC+ mode and per-region RAM mapping.

## Key Findings

1. **Two devices in one.** Emulating "an SCC cartridge" means emulating (a) the KonamiSCC
   MegaROM mapper (four 8 kB bank registers, distinctive mirroring, an SCC-enable latch) and
   (b) the wavetable generator behind it. The mapper half is an incremental sibling of the plain
   Konami mapper already shipped in this project (M19, `src/devices/cartridge/
   cartridge_konami_rom.*`) — but with **different mirroring** and a **wider write-decode
   range** (§2).
2. **All quantitative sound behaviour reduces to formulas, not tables.** Frequency = divider
   formula (§4); amplitude = De Schrijder's measured formula (§5); waveform data is
   software-supplied at runtime. This is why the SCC carries none of the license-sensitivity
   that blocked C1/D4.
3. **The deformation/test register is real, measured behaviour** (Manuel Pazos, 2003): frequency
   bit-masking modes, restart-on-frequency-write, and waveform-rotation modes — including the
   nasty fact that **reading** the deformation-register address range acts as **writing 0xFF**
   to it (§6).

## Details

### 1. Chip identification & context

| Property | Value |
|---|---|
| Device | Konami SCC (Sound Creative Chip); successor **SCC-I** (community name "SCC+") |
| Function | MegaROM bank mapper + 5-channel wavetable sound generator, one package |
| Clock | 3.579545 MHz MSX master clock (all timing below derives from it) |
| Sound channels | 5, each: 32×8-bit signed waveform, 12-bit frequency divider, 4-bit volume |
| Waveform RAM | 4×32 bytes on SCC (ch5 shares ch4); 5×32 bytes on SCC-I |
| Host cartridges (SCC) | Konami MegaROMs: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow, Solid Snake, Quarth, … (`references/openmsx-21.0/src/memory/RomKonamiSCC.cc:1-6`) |
| Host cartridge (SCC-I) | Konami Sound Cartridge (RAM-based) for Snatcher / SD-Snatcher; 64 kB or 128 kB RAM variants (`references/openmsx-21.0/src/sound/MSXSCCPlusCart.cc:22-66`) |
| Naming | "the SCC+ is actually called SCC-I" — NYYRIKKI, 2005 (as quoted in `SCC.cc:96`) |

Assumption (contextual only, not load-bearing): community documentation commonly identifies the
parts as Konami custom chips 051649 (SCC) and 052539 (SCC-I). No in-repo file states these part
numbers; nothing in the emulation depends on them.

Canonical mapper-type vocabulary: openMSX calls the mapper `KonamiSCC` (8 kB block size), with
historical aliases `2`, `SCC`, `KONAMI5` (`references/openmsx-21.0/src/memory/RomInfo.cc:24,
127-129`); fMSX calls the same scheme `MAP_KONAMI5` (`references/fmsx-60/source/fMSX/MSX.c:1526`).

### 2. KonamiSCC MegaROM mapper (system integration, plain SCC)

Grounded in `references/openmsx-21.0/src/memory/RomKonamiSCC.cc` (primary) and
`references/fmsx-60/source/fMSX/MSX.c:1526-1537` (cross-check; disagreements in §9).

- **Geometry:** 8 kB banks; four switchable CPU regions at 0x4000–0x5FFF, 0x6000–0x7FFF,
  0x8000–0x9FFF, 0xA000–0xBFFF. Real mapper chips support at most **512 kB** (64 banks — a 6-bit
  bank latch; openMSX prints a warning, not an error, for larger images,
  `RomKonamiSCC.cc:28-34`).
- **Bank-select registers** (`RomKonamiSCC.cc:8-12`, decode `(addr & 0x1800) == 0x1000` at
  `RomKonamiSCC.cc:116-119`):
  - region 0x4000: write to **0x5000–0x57FF** (games use 0x5000)
  - region 0x6000: write to **0x7000–0x77FF** (0x7000 used)
  - region 0x8000: write to **0x9000–0x97FF** (0x9000 used)
  - region 0xA000: write to **0xB000–0xB7FF** (0xB000 used)
- **Reset/power-up bank layout:** banks 0,1,2,3 in the four regions; SCC disabled
  (`RomKonamiSCC.cc:60-68`).
- **Mirroring** (explicitly DIFFERENT from the plain Konami mapper — `RomKonamiSCC.cc:48`):
  the 0x4000–0x7FFF regions are mirrored into **0xC000–0xFFFF**, and the 0x8000–0xBFFF regions
  are mirrored into **0x0000–0x3FFF** (`RomKonamiSCC.cc:44-58`). Contrast plain Konami
  (`RomKonami.cc` / this project's M19 `CartridgeKonamiRom`): there 0x4000–0x7FFF mirrors into
  0x0000–0x3FFF and 0x8000–0xBFFF mirrors into 0xC000–0xFFFF. Easy to get backwards; needs a
  dedicated test.
- **SCC enable latch:** a write anywhere in 0x9000–0x97FF with `(value & 0x3F) == 0x3F` enables
  the SCC; any other value disables it (`RomKonamiSCC.cc:108-115`). Rationale: the bank latch is
  6 bits wide, so "bank 63 selected in the 0x8000 region" is the enable condition — consistent
  with the 512 kB/64-bank ceiling above. **The same write still performs the bank switch too**
  (both effects; openMSX falls through from the enable check to the page-selection check,
  `RomKonamiSCC.cc:108-123`).
- **SCC register window:** when enabled, reads AND writes in **0x9800–0x9FFF** go to the sound
  generator instead of ROM (`RomKonamiSCC.cc:70-107`); the 256-byte register map (§3) is
  addressed as `addr & 0xFF`, i.e. mirrored 8× across the 2 kB window. (fMSX decodes only
  0x9800–0x98FF — disagreement §9.5.) The ROM **mirror pages do NOT carry the SCC window**: at
  0x1800–0x1FFF (the mirror of 0x9800–0x9FFF) openMSX serves the mirrored ROM bank, never the
  SCC — the SCC override is keyed on the raw CPU address range 0x9800–0x9FFF only (derived from
  the structure of `RomKonamiSCC.cc:79-86`, where only that literal range is intercepted).
- **Write-decode range:** writes below 0x5000 or at/above 0xC000 are ignored entirely
  (`RomKonamiSCC.cc:98-101`). (Plain Konami ignores writes below 0x6000.)

### 3. SCC register map (plain SCC mode, base = 0x9800)

Grounded in `references/openmsx-21.0/src/sound/SCC.cc:209-250,288-340` (Mode::Real paths),
cross-checked against `references/fmsx-60/source/EMULib/SCC.c:46-74` and the community posts.

| Offset (from 0x9800) | Function | R/W |
|---|---|---|
| 0x00–0x1F | Channel 1 waveform, 32 signed bytes | R/W |
| 0x20–0x3F | Channel 2 waveform | R/W |
| 0x40–0x5F | Channel 3 waveform | R/W |
| 0x60–0x7F | Channel 4 waveform — **also channel 5's waveform** (a write lands in both channels' playback; there is no separate ch5 RAM on SCC) | R/W |
| 0x80–0x8F | Frequency / volume / enable block (below) | W (reads return 0xFF) |
| 0x90–0x9F | **Mirror** of 0x80–0x8F ("region is visible twice", `SCC.cc:376`; fMSX mirrors identically via `R` and `R+0x10`, `SCC.c:96`) | W |
| 0xA0–0xDF | No function (reads 0xFF, writes ignored) | — |
| 0xE0–0xFF | Deformation/test register (§6). **A READ of this range acts as a WRITE of 0xFF** (Pazos: "Reading Mode Setting Register, is equivalent to write #FF to it", `SCC.cc:57`; openMSX implements at `SCC.cc:196-207`), and returns 0xFF | (W) |

Frequency/volume/enable block layout (offsets within 0x80–0x8F; `SCC.cc:374-414`):

| Offset | Function |
|---|---|
| +0x0 / +0x1 | Ch1 period: low 8 bits / high 4 bits (12-bit total; high write's bits 4–7 ignored) |
| +0x2 / +0x3 | Ch2 period low/high |
| +0x4 / +0x5 | Ch3 period low/high |
| +0x6 / +0x7 | Ch4 period low/high |
| +0x8 / +0x9 | Ch5 period low/high |
| +0xA … +0xE | Ch1…Ch5 volume, 4 bits each (upper nibble ignored) |
| +0xF | Channel enable: bit0=ch1 … bit4=ch5 (1 = enabled); bits 5–7 unused |

Wave-RAM readback: offsets 0x00–0x7F are readable (channels 1–4); in the rotation modes of §6
the read is phase-shifted by the accumulated rotation.

### 4. Channel generator model (frequency / stepping / latching)

- **Step rate.** Each channel steps its 5-bit waveform position once per `(period + 1)` master
  cycles (period = the 12-bit register value). A full 32-byte waveform therefore repeats at
  `f = 3,579,545 / (32 × (period + 1))` Hz. Grounding: Manuel Pazos's independently-measured
  rotation-speed formula "Rotation speed = 3.58MHz / (channel i frequency + 1)" (`SCC.cc:46-47`)
  fixes the `+1` divisor; openMSX's generator (`SCC.cc:466-500` with `INPUT_RATE = 3579545/32`,
  `SCC.cc:116`, and `incr = 32`) realises exactly this rate; fMSX's `SCC_BASE = 111861`
  (= round(3579545/32), `SCC.h:16`) confirms the divide-by-32 structure but omits the `+1`
  (disagreement §9.1).
- **Low-period cutoff.** Writing a period value **less than 9** stops the channel's sample
  counter entirely (the current output byte is held): NYYRIKKI's 2005 post describes stopping
  the SCC from reading sample memory "by writing value less than 9 to frequency"
  (`SCC.cc:85-90`); openMSX implements as `incr = (per <= 8) ? 0 : 32` (`SCC.cc:393`). fMSX
  only silences period 0 (disagreement §9.2).
- **Frequency-write restart.** Writing either half of a period register resets the channel's
  intra-byte counter — "writing value to frequency causes current byte to be started again"
  (NYYRIKKI, `SCC.cc:78-81`) — and the output value is refreshed immediately from the current
  waveform position (`SCC.cc:394,402`). Repeatedly hammering the frequency registers therefore
  freezes the sample position (the basis of real software sample-playback techniques described
  in the same post).
- **Volume-change latency.** A volume write does NOT retroactively change the currently-playing
  output byte: "change of volume is not implemented immediately in SCC. Normally it is changed
  when next byte from sample memory is played" (NYYRIKKI, `SCC.cc:77-79`). openMSX models this
  by recomputing the volume-adjusted waveform immediately but only refreshing the held output
  at the next position step (or at a frequency write).
- **Waveform-write latency (same mechanism):** a wave-RAM write to the currently-playing byte
  position likewise reaches the output at the next position step.
- **Disabled channels:** contribute 0 to the mix (equivalent to `VolX = 0`, De Schrijder,
  `SCC.cc:20`) but their phase counters keep running (openMSX advances phase for muted
  channels, `SCC.cc:490-498`).

### 5. Amplitude & mixing (Jon De Schrijder, 2003-02-24 scope measurement)

Verbatim measured model (`SCC.cc:3-31`):

```
AmpOut = 640 + AmpA + AmpB + AmpC + AmpD + AmpE          (11-bit positive, range +40 … +1235)
AmpX   = ((SampleValue * VolX) AND #7FF0) div 16          (SampleValue in [-128,+127], VolX in [0,15])
```

- The multiply is **signed**; the low 4 bits of the 16-bit product are dropped (AND #7FF0 with
  sign preserved ⇔ arithmetic shift right by 4) **before** the five channel values are summed —
  proven by the measurement `SampleValue=+1, VolX=15 ⇒ AmpOut=640` (each channel's `15` product
  truncates to 0 before addition, `SCC.cc:25-30`).
- Enable bit cleared ⇔ that channel's term is 0.
- 640 is the DC centre; the AC swing per channel is `[-(128·15)>>4 … +(127·15)>>4]` = [-120,
  +119], five channels ⇒ total AC swing ≈ ±600 around 640 (consistent with the measured
  [+40…+1235] envelope).
- fMSX does not model this formula (it feeds a linear `255*(V&0x0F)/15` volume into its generic
  host mixer, `SCC.c:118-124`) — an output-stage approximation, not a register-semantics
  disagreement (§9.3).

### 6. Deformation / test register (Manuel Pazos, 2003-04-14; offsets 0xE0–0xFF in SCC mode)

Bit semantics as measured (`SCC.cc:34-70`):

| Bit | Function |
|---|---|
| 0 | 4-bit frequency mode: divider uses only the period's top 4 bits (`period >> 8`); low 8 bits ignored |
| 1 | 8-bit frequency mode: divider uses only the period's low 8 bits; bits 8–11 ignored. Setting both bit0 and bit1 ⇔ bit1 alone (openMSX "Additions", `SCC.cc:62`) |
| 2–4 | No documented function |
| 5 | Waveform restarts from position 0 whenever a channel's frequency is changed (also resynchronises the rotation timer — "confirmed by test based on Artag's SCC sample player", `SCC.cc:395-399`) |
| 6 | Rotate ALL waveforms continuously; wave RAM becomes read-only. Rotation step rate = `3.58MHz / (period_i + 1)` per channel; one rotation step is `waveData[0:31] = waveData[1:31].waveData[0]` (`SCC.cc:46-47,63-66`). On plain SCC, channels 4 and 5 rotate at **channel 5's** period (channel 4's own period is ignored for rotation, `SCC.cc:65-66`; openMSX `SCC.cc:257-262`) |
| 7 | Rotate channel 4's (=5's) wave data only; that wave RAM becomes read-only. **Plain SCC only — no effect on SCC-I** (`SCC.cc:48-49`; openMSX masks bit7 off in non-Real modes, `SCC.cc:429-431`) |

- bits6+7 both set: only channels 1–3 rotate; ALL wave RAM is read-only; Pazos additionally
  reports audible data-bus noise corruption in this configuration on the plain SCC ("execute
  DIR command on DOS. You will hear noise"), fixed in SCC-I (`SCC.cc:51-55`) — treat as a
  documented hardware anomaly, not required emulation behaviour.
- **Reading the deformation-register address range = writing 0xFF** (§3). Register cannot be
  read back.
- fMSX does not model the deformation register at all (writes to offsets ≥0xE0 are discarded,
  `SCC.c:66-67`) — a modeling gap, recorded in §9.4.

### 7. Power-on / reset state (enen, IRC #openmsx measurement)

As quoted in `SCC.cc:146-153`: at power-on, deformation = 0, volumes = full (15), channel
enable = 0, periods "under 8" (i.e. channels stopped, §4), waveform RAM reads mostly 0xFF
("filled with $FF, some bits cleared but that seems random"). openMSX deterministically fills
waveforms with 0xFF and assumes position counters start at 0 (`SCC.cc:158-173`). A Konami
soft reset (mapper `reset()`) clears the SCC-enable latch, the deformation register, and the
channel-enable register (`SCC.cc:176-184`, `RomKonamiSCC.cc:60-68`); openMSX leaves
waveforms/volumes/periods untouched by soft reset (they are power-up state).

### 8. SCC-I ("SCC+") differences and the Sound Cartridge mapper

The SCC-I chip is register-compatible with the SCC ("Compatible" mode) and adds a "Plus" mode.
Grounded in `references/openmsx-21.0/src/sound/SCC.cc:209-340` (Compatible/Plus mode paths) and
`references/openmsx-21.0/src/sound/MSXSCCPlusCart.cc` (the Sound Cartridge around it).

Chip-level differences:

| Aspect | SCC (Real mode) | SCC-I Compatible mode | SCC-I Plus mode |
|---|---|---|---|
| Window | 0x9800–0x9FFF | 0x9800–0x9FFF | **0xB800–0xBFFF** |
| Ch5 waveform | shares ch4 (write 0x60–0x7F hits both) | shares ch4 for WRITES; ch5 wave READABLE at offsets 0xA0–0xBF (writes there ignored) | **independent** ch5 wave RAM, R/W at 0x80–0x9F |
| Freq/vol/enable block | 0x80–0x9F (×2 mirror) | 0x80–0x9F | 0xA0–0xBF |
| Deform register | 0xE0–0xFF | **0xC0–0xDF** | 0xC0–0xDF |
| Deform bit7 | rotates ch4/5 | no effect | no effect |

Sound Cartridge (SCC-I host) facts (`MSXSCCPlusCart.cc`):

- RAM-based, not ROM: 64 kB (Snatcher / SD-Snatcher subtypes) or 128 kB ("expanded"); same
  four-region/8 kB-bank layout and the same 0x5000/0x7000/0x9000/0xB000 bank registers
  (`MSXSCCPlusCart.cc:184-194`).
- **Mode register at 0xBFFE–0xBFFF** (write-only; `(address | 1) == 0xBFFF`,
  `MSXSCCPlusCart.cc:163-167,124`): bit4 = map ALL four regions as writable RAM; bit5 = SCC+
  mode select (also switches the chip's Compatible/Plus mode); bit0/bit1 = RAM-enable for the
  0x4000/0x6000 regions; bit2 (only together with bit5) = RAM-enable for the 0x8000 region; the
  0xA000 region is never RAM-mapped outside bit4 mode (`MSXSCCPlusCart.cc:251-274`).
- Sound-generator enable: in SCC+ mode (bit5=1), the 0xB000-region bank register's **bit7**
  exposes the SCC+ window at 0xB800–0xBFFF; in compatible mode (bit5=0) the classic
  `(bank3 & 0x3F) == 0x3F` rule exposes 0x9800–0x9FFF (`MSXSCCPlusCart.cc:276-285`).
- RAM-mode read note (Sean Young, quoted at `MSXSCCPlusCart.cc:171-177`): with regions in RAM
  mode, the SCC(+) registers can still be read but not written (openMSX marks this unverified).

### 9. openMSX vs fMSX disagreements (recorded per DEC-0030 discipline)

| # | Topic | openMSX 21.0 | fMSX 6.0 | Arbitration |
|---|---|---|---|---|
| 9.1 | Frequency divisor | step per `(period+1)` master cycles (`SCC.cc:475`) | `Freq = 111861 / period` — no `+1` (`SCC.c:113`) | **openMSX**: the `+1` is independently fixed by Pazos's measured rotation formula "3.58MHz/(freq+1)" (`SCC.cc:46-47`) |
| 9.2 | Low-period behaviour | period ≤ 8 stops the counter (`SCC.cc:393`) | only period = 0 silences (`SCC.c:113`) | **openMSX**: NYYRIKKI's measured "write value less than 9 … stop SCC" (`SCC.cc:85-90`) |
| 9.3 | Output/mix stage | De Schrijder formula: signed product, low-4-bit drop BEFORE summation, +640 DC (`SCC.cc:3-31,347-353`) | linear `255·vol/15` into a host mixer (`SCC.c:118-124`) | **openMSX**: direct oscilloscope measurement; fMSX's is an output approximation, not a semantics claim |
| 9.4 | Deformation/test register | fully modeled per Pazos (`SCC.cc:417-464`) | not modeled; writes ≥0xE0 dropped (`SCC.c:66-67`) | **openMSX** (fMSX gap, not a counter-claim) |
| 9.5 | SCC window width | full 0x9800–0x9FFF, 256-byte map mirrored 8× via `addr & 0xFF` (`RomKonamiSCC.cc:79-86`) | 0x9800–0x98FF only (decode `(A & 0xDF00) == 0x9800`, `MSX.c:1456`) | **openMSX** preferred (primary reference). Assumption: the mirror follows from the cartridge decoding only A11–A15 for the window and feeding A0–A7 to the chip (A8–A10 unconnected). No in-repo independent measurement decides this; an A/B register probe at 0x9900 vs 0x9800 is the named verification action (M29 A/B plan) |
| 9.6 | SCC-enable compare | `(value & 0x3F) == 0x3F` — e.g. 0xBF also enables (`RomKonamiSCC.cc:110`) | `V == 0x3F` exact (`MSX.c:1531`) | **openMSX**: the 6-bit-bank-latch rationale (512 kB ceiling, §2) makes the masked compare the hardware-consistent reading; include 0xBF-enables in the A/B probe |
| 9.7 | Bank/enable write decode width | 0x800-wide windows: 0x5000–0x57FF etc., enable anywhere in 0x9000–0x97FF (`RomKonamiSCC.cc:108-119`) | exact single addresses 0x5000/0x7000/0x9000/0xB000, enable at 0x9000 only (`MSX.c:1528-1531`); fMSX's generic-8kB variant instead enables on ANY 0x8000–0x9FFF write (`MSX.c:1493-1497`) | **openMSX**: matches its own documented address-range comment (`RomKonamiSCC.cc:8-12`); fMSX's two mutually-inconsistent decodes suggest pragmatic shortcuts |

Agreements worth noting (double-grounded facts): ch4→ch5 shared waveform on plain SCC
(`SCC.cc:355-372` / `SCC.c:69-71`); the freq/vol block's ×2 visibility (`SCC.cc:376` /
`SCC.c:96`); 12-bit period assembly with high-nibble masking (`SCC.cc:380-383` /
`SCC.c:109`); wave region read-back below the freq/vol block, 0xFF above it (`SCC.cc:209-250` /
`SCC.c:46-58`); reset banks 0,1,2,3 and the 0x3F-family enable value at the 0x9000 register.

### 10. Emulator implementation recommendations

1. Model the generator cycle-exactly at the master-clock level: per channel, a counter that
   steps the 5-bit position once per `(period+1)` master cycles (freeze when period ≤ 8); hold
   the current output byte's volume-adjusted value between steps (§4's latching semantics then
   fall out naturally). openMSX's 32-cycle output granularity is its own resampler artifact,
   not chip semantics — do not copy it.
2. Compute the per-channel contribution as `(int8_wave × vol4) >> 4` (arithmetic shift = the
   measured AND-#7FF0-div-16), sum enabled channels; expose both the AC sum (mixing) and the
   `640 + sum` form (direct De Schrijder test oracles).
3. Implement the plain SCC ("Real" mode) for KonamiSCC cartridges: ch5 mirrors ch4's waveform;
   deform register at +0xE0 with read-as-write-0xFF. Keep the register-map dispatch mode-aware
   so SCC-I (Compatible/Plus) can be added later without restructuring.
4. Mapper: implement §2 exactly — including the KonamiSCC-specific mirroring (opposite of plain
   Konami), the both-effects 0x9000 write, and the masked 0x3F enable compare. Do NOT reuse the
   plain-Konami 256 kB block-mask override; KonamiSCC uses the image-derived default mask.
5. Reset (this project's single-deterministic-cold-state convention): implement §7's power-on
   state (wave RAM = 0xFF, volumes = 15, enable = 0, deform = 0, periods = 0, positions = 0) in
   `reset()`; the soft-reset-vs-power-cycle distinction openMSX draws is a disclosed
   simplification if collapsed.

### 11. License-hygiene note

The SCC domain contains **no large numeric tables** in either reference source: waveforms are
runtime-supplied by software, and frequency/volume/mixing behaviour is closed-form (§4–§5).
Every constant in this sheet is either a register address/bit position (small interface facts),
a measured formula from the cited community posts, or the master clock. Nothing here transcribes
GPL (openMSX) or non-commercial (fMSX) work product; both sources were read for understanding
and cross-checking only, per `agent-protocol/guardrails.md`.

## Sources

Independent community measurements (primary factual grounding; all quoted verbatim as
third-party citations inside `references/openmsx-21.0/src/sound/SCC.cc`):

1. **Jon De Schrijder**, 2003-02-24 — oscilloscope amplitude measurements: the
   `AmpOut = 640 + ΣAmpX` formula, per-channel truncate-before-sum proof, output range, enable ⇔
   vol-0 equivalence (as quoted at `SCC.cc:1-32`).
2. **Manuel Pazos**, 2003-04-14 — "Mode Setting Register" (deformation register) bit semantics,
   rotation behaviour and `3.58MHz/(freq+1)` rotation rate, read-equals-write-0xFF, SCC vs SCC+
   differences for bit7; plus the openMSX-recorded additions (bit0+bit1 precedence, rotation
   step definition, ch4/ch5 rotation-period sharing) and MRC thread
   `http://www.msx.org/forumtopicl7875.html` (as quoted at `SCC.cc:34-70`).
3. **NYYRIKKI**, 2005-09-09 MRC post — volume-change latching, frequency-write restart
   behaviour, the period<9 counter stop, "SCC+ is actually called SCC-I" (as quoted at
   `SCC.cc:73-97`).
4. **enen**, IRC #openmsx log — power-on values: deform=0, amplitude=full, channelEnable=0,
   period under 8, wave RAM ≈ 0xFF (as quoted at `SCC.cc:146-153`).
5. **Sean Young** — SCC-I RAM-mode read-but-not-write note (as quoted at
   `MSXSCCPlusCart.cc:171-177`).

Behaviour references (read-only; structure/decode grounding and cross-check — never copied):

6. openMSX 21.0: `references/openmsx-21.0/src/sound/SCC.cc`, `SCC.hh`,
   `references/openmsx-21.0/src/memory/RomKonamiSCC.cc`, `RomKonamiSCC.hh`,
   `references/openmsx-21.0/src/sound/MSXSCCPlusCart.cc`, `MSXSCCPlusCart.hh`,
   `references/openmsx-21.0/src/memory/RomInfo.cc` (naming).
7. fMSX 6.0 (independent second reference): `references/fmsx-60/source/EMULib/SCC.c`, `SCC.h`,
   `references/fmsx-60/source/fMSX/MSX.c` (mapper/window integration).

---
*Authored 2026-07-09 by the MSX Planner Agent for M29 (DEC-0035); every `references/...` line
cited above was read directly this cycle.*
