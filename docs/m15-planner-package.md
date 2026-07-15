# M15 Planner Package — Device Integration Completion & S1985 Chipset Full Wiring

- Milestone ID: M15
- Title: Device Integration Completion & S1985 Chipset Full Wiring (PSG/RTC real devices + keyboard/joystick input)
- Spec Owner: MSX Planner Agent
- Status: PLANNING ONLY — implementation HELD for human review per DEC-0008
- Requirement: REQ-M15-002
- Authority: DEC-0008 (north-star: "complete device integrations with the S1985 chipset fully wired"), DEC-0005 (backlog disposition discipline), DEC-0002 (S1985 thin-layer + PSG/RTC deferred as separate milestones)

> This package is presented to the human for review BEFORE any code is written. It contains NO
> production code. It converts the human directive "address all the pending items" into a
> deterministic, reviewable decomposition: a focused M15 plus an ordered follow-on milestone
> sequence, with every deferred-backlog row explicitly disposed.

---

## 0. Grounding: current state of the "seams"

The north-star calls M15 the replacement of the M11 chipset SEAMS "PSG at #A0-A2, RTC at #B4/B5"
with real devices. Reading the current wiring establishes the precise starting point:

- `src/machine/hbf1xv_machine.cpp:32-107` (`wire_bus`) attaches I/O devices ONLY on: `#98-#9B`
  (+`#9C-#9F` mirror) V9958 VDP; `#A8` (+`#AC` mirror) `PpiSlotSelect`; `#A9-#AB` mirrors
  registered but with NO base device; `#FC-#FF` `MapperIo`; `#40-#4F` `SwitchedIoController`
  (S1985 backup RAM ID `0xFE`).
- Therefore ports **`#A0-#A2` (PSG) and `#B4/#B5` (RTC) are currently UNMAPPED** — `IoBus::io_read`
  returns open-bus `0xFF` for them (`src/devices/chipset/io_bus.cpp:34-40`). The "seams" are
  conceptual placeholders that DEC-0002 explicitly deferred ("Full PSG/YM2149 and RTC/RP5C01
  device internals are their own later milestones ... M11 provides only the bus/engine seams they
  will plug into").
- The residual S1985 layer that DOES exist (`src/devices/chipset/s1985_engine.*`) is the 16-byte
  backup RAM (ID `0xFE`), the mapper-readback pattern `100xxxxx`, and the +1 M1 wait — all correct
  and QA-signed at M11 (`docs/m11-parity-trace-diff.md`). M15 does NOT rewrite these; it plugs the
  real PSG/RTC/PPI/keyboard/joystick devices into the existing `IoBus` dispatch fabric alongside
  them, and completes the S1985 by adding `.sram` persistence (backlog C4).

Implication: "replace the seam" means **attach real devices on the currently-unmapped ports** and
**expand `PpiSlotSelect` into a full i8255 PPI** so the keyboard rows (`#A9/#AA/#AB`) become live.
The dispatch fabric (`IoBus`, `SwitchedIoController`, mirrors) is already in place and unchanged.

---

## 1. Scope and Assumptions

### 1.1 Scope (IN-M15)

M15 completes the **S1985 chipset's own constituent devices** plus the **human-input path**, because
those are (a) the devices the north-star names, (b) coherent, self-contained, and deterministically
verifiable in one milestone, and (c) the set that unblocks further BIOS boot progress (backlog C5):

- **B1 — PSG / YM2149** register model, tone/noise/envelope/mixer, YM2149 5-bit envelope + register
  readback, stereo panning (A=Center, B=Left, C=Right), joystick ports via R14/R15, on `#A0/#A1/#A2`.
- **B2 — RTC / RP5C01** four register blocks × 13 regs + mode/control regs 13-15, BCD clock, Block-2
  CMOS boot config (reg0 = `0x0A` valid marker), gated by system-control port `#F5` bit 7, on `#B4/#B5`.
- **C4 — S1985 backup-RAM `.sram` persistence** (16 bytes, switched-I/O ID `0xFE`) — completes the
  S1985 engine device already present as volatile.
- **C6 — Keyboard matrix + joystick input**: full i8255 PPI (`#A8` slot-select preserved, `#A9`
  keyboard-row read, `#AA` port C row-select/LED/cassette/click, `#AB` control bit-set/reset), an
  11×8 keyboard matrix peripheral, and a joystick peripheral feeding PSG port A.
- **C5 — Boot past first device read** (partial): advance the deterministic boot checkpoint now that
  real PSG/RTC/keyboard/VDP answer the BIOS init reads. Full boot to a BASIC/Disk prompt remains
  gated on the FDC (deferred) — see §5.

### 1.2 Assumptions (each with a verification action)

- **A-M15-1 (RTC determinism).** The RTC must NOT read the host wall clock (would break the
  determinism non-negotiable). Assumption: seed the RP5C01 from a FIXED emulated epoch and advance
  only from the deterministic scheduler clock (emulated seconds), mirroring openMSX's use of emulated
  `getCurrentTime()` rather than real time (`references/openmsx-21.0/src/MSXRTC.cc:10`,
  `getCurrentTime()`). Verification: unit test asserts two identical runs produce byte-identical RTC
  register reads; confirm against `references/openmsx-21.0/src/RP5C01.cc` update cadence.
- **A-M15-2 (`#F5` gating owner).** Fact-sheet §5 states the RTC is gated by `#F5` bit 7 (CLOCK-IC
  enable). openMSX's `MSXRTC` attaches `#B4/#B5` unconditionally and handles `#F5` elsewhere
  (`references/openmsx-21.0/src/MSXRTC.cc:24-46` shows no `#F5` check). Assumption: model a small
  system-control register on `#F5` (Sony non-inverted `#F4` polarity per fact-sheet §9) and gate RTC
  data access on bit 7. Verification: read `references/openmsx-21.0/src/` for the `#F4/#F5` handler
  (candidate `MSXPSG`/`MSXCPUInterface`/machine XML) before implementing; SCOPE QUESTION Q3.
- **A-M15-3 (PSG mirrored register mask).** HB-F1XV integrates the PSG in the S1985, so registers are
  mirrored (address mask `0x0F`). Assumption: use `addressMask = 0x0F` per
  `references/openmsx-21.0/src/sound/MSXPSG.cc:38` (`mirrored_registers` default true). Verification:
  confirm the HB-F1XV machine XML `mirrored_registers` value.
- **A-M15-4 (PSG sample generation depth).** The PSG is an audio device but M15 has no SDL3 audio sink
  (C9 deferred). Assumption: M15 implements the DETERMINISTIC numeric sample/output-level model
  (tone/noise/envelope counters → per-channel level → stereo mix) as unit-testable integer state, and
  leaves only the analog presentation to the frontend milestone. Verification: unit tests assert
  deterministic sample sequences; no floating-point non-determinism.
- **A-M15-5 (keyboard default state).** No key pressed at boot; matrix reads all-ones inverted
  (0=pressed, so idle = `0xFF`) per fact-sheet §3. Verification: A/B vs openMSX idle keyboard read.
- **A-M15-6 (assets present).** `bios/` holds the required images (`f1xvbios.rom`, `f1xvext.rom`,
  `f1xvkdr.rom`, `f1xvdisk.rom`, `f1xvmus.rom`, `f1xvkfn.rom`, `f1xvfirm.rom`) verified via `Glob`.
  M15 adds no new asset requirement. Verification: `tools/validate-assets.ps1`.

### 1.3 Non-goals (explicitly OUT of M15)

FM synthesis DSP (OPLL), FM-PAC mapper/SRAM handshake, Kanji-font I/O, Halnote firmware, cartridge
loading, FDC drive mechanics, printer/cassette tape I/O, VDP rendering/sprite/command depth, exact
cycle/T-state timing, ZEXALL, Sony speed/pause + Ren-Sha, SDL3 frontend. All are dispositioned in §2.

---

## 2. Full-Backlog Disposition Table (mandatory — every row, DEC-0005)

Source of truth: `agent-protocol/state/deferred-backlog.md`. Every row B1-B9, C1-C9, D1-D7 disposed.

| Item | Disposition | One-line justification |
|------|-------------|------------------------|
| **B1** PSG/YM2149 internals | **IN-M15** | Named chipset device; replaces the `#A0-A2` seam; unblocks BIOS PSG init (R7) and joystick reads. Grounded `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §2. |
| **B2** RTC/RP5C01 internals | **IN-M15** | Named chipset device; replaces the `#B4/B5` seam; BIOS reads Block-2 CMOS at boot. S1985 fact-sheet §5. |
| **B3** FM-PAC/OPLL YM2413 internals | **DEFER → M16 (MSX-MUSIC/FM-PAC)** | Heavy 18-slot FM DSP + patch ROM + EG tables; a separate chip, not S1985; write-only, not boot-blocking. `references/fact-sheets/Yamaha YM2413 FM Chip.md`. |
| **B4** MSX-JE 16 KB SRAM | **DEFER → M16 (with FM-PAC)** *(or M20 MSX-JE/Halnote — see Q4)* | Battery SRAM store tied to FM-PAC/MSX-JE; pairs with its owning firmware; not boot-blocking. S1985 fact-sheet §9; YM2413 sheet §2 (8 KB FM-PAC SRAM). |
| **B5** Kanji-font I/O `#D8-DB` | **DEFER → M17 (Peripheral I/O)** | Self-contained font-ROM window; deterministic but not chipset/boot-critical; groups with printer/cassette. `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`; S1985 §9. |
| **B6** Halnote/MSX-JE firmware | **DEFER → M20 (MSX-JE/Halnote)** | 1 MB Halnote mapper + SRAM; large, self-contained firmware slot 0-3; independent of chipset wiring. |
| **B7** Cartridge loading | **DEFER → M19 (Cartridge/slot-manager)** | External slots 1&2 + mapper-type detection; independent subsystem; not needed for internal boot. Target Spec (2 cartridge slots). |
| **B8** FDC drive mechanics (MB89311/WD2793) | **DEFER → M18 (FDC)** | Very heavy: WD2793 core, disk images, DRQ/INTRQ timing, motor-off; own milestone. `references/fact-sheets/FDC for Sony HB-F1XV.md`; `references/openmsx-21.0/src/fdc/WD2793.*`. |
| **B9** VRAM/V9958 VDP | **DONE (M14)** | Closed at v1.0.14 (DEC-0007). |
| **C1** Exact cycle/T-state timing parity | **DEFER → M23 (Timing-fidelity)** | Cross-emulator wait-state/VDP-recovery parity; orthogonal to device wiring; groups with C2/D4. Zilog Z80A sheet §6/§8; S1985 §8. |
| **C2** Z80 HALT-R increment | **DEFER → M23 (with C1)** | Per DEC-0004 accepted-deferred; alters signed cpu_step oracle; belongs to timing-fidelity. Zilog Z80A §8. |
| **C3** ZEXDOC/ZEXALL sweep | **DEFER → M24 (CPU-validation)** | Needs a legally-sourced ZEX binary (unavailable offline in M12); independent verification milestone. |
| **C4** S1985 backup-RAM `.sram` persistence | **IN-M15** | Completes the S1985 engine device already present (volatile); small, deterministic; part of "chipset fully wired". S1985 §6. |
| **C5** Full boot past first device read | **IN-M15 (partial)** | Real PSG/RTC/keyboard/VDP let boot advance; M15 advances the checkpoint. Full boot to prompt needs FDC (M18) — residual tracked. `docs/m13-parity-trace-diff.md`. |
| **C6** Keyboard matrix + joystick | **IN-M15** | Named input path; PPI port B/C rows + PSG port A/B; completes human-input integration. S1985 §2/§3. |
| **C7** Printer `#90/91` + cassette | **DEFER → M17 (Peripheral I/O)** | Printer device + cassette tape-image I/O are their own peripheral surface. NOTE: the register-level bits that PPI port C (motor/click) and PSG R14 (cassette input) expose ARE modeled in M15 as deterministic inert bits; the printer device and tape transport are M17. S1985 §8. |
| **C8** Sony speed/pause (MB670836) + Ren-Sha Turbo | **DEFER → M25 (HB-F1XV specifics)** | HB-F1XV-specific companion-chip behavior; independent of the standard device set. S1985 §9; Zilog Z80A §6. |
| **C9** SDL3 frontend | **DEFER → M26 (Frontend)** | Presentation layer; consumes PSG audio + VDP video + keyboard/joystick input built by M14/M15/M21. `references/sdl3/`. |
| **D1** Pixel-accurate raster rendering | **DEFER → M21 (Video-rendering)** | VDP display DEPTH deferred by M14; large per-mode pipeline. fact-sheet §3/§5; `references/openmsx-21.0/src/video/`. |
| **D2** Sprite rendering + collision | **DEFER → M22 (Sprites, after M21)** | Sprite pipeline depends on D1 raster. fact-sheet §6; `SpriteChecker.cc`. |
| **D3** VDP command engine | **DEFER → M22/M21 group (Command-engine)** | R#32-46 blitter; depends on raster/VRAM path. fact-sheet §8; `VDPCmdEngine.cc`. |
| **D4** Cycle-accurate VDP access-slot/command timing | **DEFER → M23 (Timing-fidelity, with C1)** | VDP cycle timing groups with the CPU/system timing milestone. fact-sheet §7/§8; `VDPAccessSlots.cc`. |
| **D5** YJK/YAE color decode + DAC | **DEFER → M21 (with D1)** | Color decode is rendering-time; pairs with the raster pipeline. fact-sheet §5. |
| **D6** Horizontal scroll/interlace/blink/superimpose | **DEFER → M21 (with D1)** | Display consequences of already-stored register bits; rendering-time. fact-sheet §1/§3/§7/§9. |
| **D7** G6/G7 VRAM planar interleave | **DEFER → M21/M22 (with D1/D3)** | Observable only once raster/command paths exist. fact-sheet §2; `VDP.cc:851-857`. |

Nothing dropped. Every open row is either IN-M15 or assigned a named follow-on with justification.

---

## 3. Proposed Decomposition (M15 + ordered follow-on sequence)

"All pending items" cannot be one deterministic milestone. Recommended M15 scope is the coherent
**chipset-completion + input** set (§1.1). The remaining backlog is proposed as this ordered
follow-on sequence (IDs are proposals for the human to confirm; substance/order matter more than the
numbers):

| Milestone | One-line objective | Backlog items |
|-----------|--------------------|---------------|
| **M15** (this) | Complete the S1985 chipset's own devices (PSG, RTC, backup-RAM persistence) + human input (PPI keyboard/joystick), advancing deterministic boot | B1, B2, C4, C6, C5(partial) |
| **M16** | MSX-MUSIC / FM-PAC: YM2413 (OPLL) FM synthesis + FM-PAC mapper/SRAM + I/O `#7C/#7D` and memory-mapped access | B3, B4 |
| **M17** | Peripheral I/O: Kanji-font ROM I/O `#D8-DB`, printer `#90/91`, cassette tape-image interface | B5, C7 |
| **M18** | FDC: MB89311/WD2793 core + Sony memory-mapped decode (`7FF8-7FFF`) + 720 KB disk-image transport → boot to Disk BASIC | B8 |
| **M19** | Cartridge loading + external primary slots 1&2 + mapper-type detection | B7 |
| **M20** | MSX-JE / Halnote firmware mapping (slot 0-3, 1 MB, Halnote mapper) | B6 |
| **M21** | V9958 video-rendering pipeline: per-mode raster (D1), YJK/YAE decode (D5), scroll/interlace/blink (D6), planar interleave display path (D7) | D1, D5, D6, D7 |
| **M22** | V9958 sprites + collision (D2) and command engine (D3) + planar command path (D7) | D2, D3, D7 |
| **M23** | Timing-fidelity: exact cycle/T-state wait-state parity (C1), HALT-R (C2), VDP access-slot/command timing (D4) | C1, C2, D4 |
| **M24** | CPU validation: ZEXDOC/ZEXALL full sweep (needs legal ZEX binary) | C3 |
| **M25** | Sony HB-F1XV specifics: speed-controller + hardware PAUSE (MB670836), Ren-Sha Turbo autofire | C8 |
| **M26** | SDL3 frontend: video/audio/input presentation | C9 |

Rationale for M15 = {B1, B2, C4, C6, C5}:
1. **Finishes the chipset.** PSG and RTC are the S1985's own integrated devices (fact-sheet §2/§5);
   C4 completes the already-present backup-RAM device. This directly satisfies the north-star
   "S1985 chipset fully wired".
2. **Enables boot progress (C5).** The BIOS's earliest device interactions are PSG R7 init, RTC
   Block-2 CMOS read for screen config, and the keyboard boot-key scan. Making these real is exactly
   what lets boot advance past the M13 first-device-read boundary.
3. **Deterministically verifiable.** Register/state models + input matrices are integer-exact,
   unit-testable, and openMSX-A/B-comparable without heavy DSP or flux-level modeling.
4. **Coherent unit.** PPI (port A slot-select already present) + keyboard + joystick + PSG ports form
   one input path; splitting them would create awkward half-wired seams.

Deferring the heavy items plainly: **FDC drive mechanics (B8)**, **full OPLL FM synthesis DSP (B3)**,
and **VDP rendering (D1-D7)** are each large enough to be their own milestone(s) and are called out
here as deferred with named owners (M18, M16, M21/M22) rather than crammed into M15.

---

## 4. Per-IN-Device Spec

All device behavior below is grounded in the cited fact-sheet section and the openMSX file to consult
(behavior reference only — GPL, never copied into `src/`, per guardrails). Source placement follows
`src/CLAUDE.md` folder responsibilities.

### 4.1 PSG — YM2149 (B1)

- **Placement:** `src/devices/audio/psg_ym2149.{h,cpp}` (new `src/devices/audio/` folder per
  `src/CLAUDE.md` "devices/ = chip implementations ... PSG"). Implements `core::IoDevice`.
- **Ports / access model (fact-sheet §2; `references/openmsx-21.0/src/sound/MSXPSG.cc:60-86`):**
  address latch on `#A0` (write, `latch = value & 0x0F` mirrored-register mask, A-M15-3), data write
  on `#A1`, data read on `#A2` (`port & 0x03 == 2`). Register file R0-R15.
- **Register semantics (fact-sheet §2 table):** R0-R5 12-bit tone periods A/B/C; R6 5-bit noise; R7
  mixer/IO-enable (bit6 must read/behave port-A-input, bit7 port-B-output — mask-preserve per §2
  "Critical R7 note"); R8-R10 amplitude+envelope-enable; R11/R12 16-bit envelope period; R13 4-bit
  envelope shape; R14 = port A read; R15 = port B write.
- **YM2149 specifics (fact-sheet §2 "YM2149 vs AY-3-8910"):** 5-bit (32-step) envelope counter;
  **register readback returns written values** (unlike AY masking-to-0) — this is behavior-affecting
  and A/B-testable. `/SEL` clock-halving is not wired on MSX (constant divider).
- **Stereo (fact-sheet §2 "Stereo on the S1985"):** channel B → Left, C → Right, A → both. Modeled as
  a deterministic 3-channel → 2-channel numeric mix (A-M15-4); no analog stage in M15.
- **Timing:** tone = fc/(16·TP), fc = 1.7897725 MHz; deterministic sample generation advanced from
  the scheduler clock. Must NOT alter CPU T-state accounting (see §6 regression note).
- **Joystick/keyboard-layout/cassette via R14 (fact-sheet §2 "Joystick ports";
  `references/openmsx-21.0/src/sound/MSXPSG.cc:90-107`):** R14 read = `joystick | keyLayout |
  cassetteInput`; bit6 keyboard layout (JIS/ANSI), bit7 cassette input (inert/low in M15, C7). R15
  write (`MSXPSG.cc:109-120`) selects joystick port (bit6), KANA LED (bit7).
- **Wiring / seam replacement:** `IoBus::attach(0xA0, &psg_); attach(0xA1,&psg_); attach(0xA2,&psg_)`
  in `wire_bus`. Replaces the current open-bus behavior on `#A0-A2`. The joystick peripheral (§4.4)
  is injected so PSG `readA` returns real port state.
- **Tests:** unit — address-latch mask `0x0F`; tone/noise period decode; 5-bit envelope stepping +
  shape table; R7 mixer/IO-dir mask; YM register readback; stereo mix (B→L, C→R, A→both); R14/R15
  joystick select + KANA/layout bits. System-integration — CPU `OUT (#A0)/(#A1)` then `IN (#A2)`
  over the M11 bus returns written value; idle joystick read. A/B — `tools/openmsx-io-parity.ps1`
  register-write/readback + port-A read vs openMSX `Sony_HB-F1XV`.

### 4.2 RTC — RP5C01 (B2)

- **Placement:** `src/devices/rtc/rp5c01.{h,cpp}` (new `src/devices/rtc/`). Implements `core::IoDevice`.
- **Ports (fact-sheet §5; `references/openmsx-21.0/src/MSXRTC.cc:24-46`):** `#B4` = register/address
  latch (`latch = value & 0x0F`); `#B5` = 4-bit data. Read: even port (`#B4`) → `0xFF`; odd (`#B5`) →
  `data | 0xF0` (upper nibble floats to 1, per `MSXRTC.cc:26`).
- **Structure (fact-sheet §5; `references/openmsx-21.0/src/RP5C01.*`):** 4 blocks × 13 4-bit regs +
  shared control regs 13 (mode: block-select bits0-1, alarm-en bit2, timer-en bit3), 14 (test), 15
  (reset). Regs 14-15 not readable.
  - Block 0: BCD time/date.
  - Block 1: alarm/timer, 12/24h, leap counter.
  - Block 2: system init CMOS — **reg0 must read `0x0A`** to signal valid CMOS (else BIOS defaults +
    Block-3 clear). Boot config: screen mode/width/colors/key-click/printer/region.
  - Block 3: title/password/prompt ASCII.
- **`#F5` gating (fact-sheet §5; A-M15-2, Q3):** RTC data access gated by `#F5` bit 7 (CLOCK-IC
  enable). Model a system-control register on `#F5`; Sony non-inverted `#F4` polarity (fact-sheet §9).
- **Determinism (A-M15-1):** seed from a FIXED emulated epoch; advance only from the scheduler clock;
  never read host time. openMSX uses emulated `getCurrentTime()` (`MSXRTC.cc:10`).
- **Persistence:** RTC CMOS (`4*13` nibbles) is battery-backed; openMSX saves an SRAM
  (`MSXRTC.cc:9`, `sram(... 4*13 ...)`). SCOPE QUESTION Q2: persist RTC CMOS in M15 or defer? Default
  recommendation: model in-memory deterministic CMOS in M15; add file persistence with C4 in the same
  slice for symmetry (small).
- **Wiring:** `IoBus::attach(0xB4,&rtc_); attach(0xB5,&rtc_)`; `#F5` system-control device gates data.
  Replaces open-bus on `#B4/B5`.
- **Tests:** unit — address latch `0x0F`; block-select via mode reg 13; BCD roll-over (deterministic
  epoch → seconds/minutes); Block-2 reg0 = `0x0A` valid marker; regs 14-15 unreadable; `#B5` read
  upper-nibble = 1; `#F5` bit7 gate. System-integration — CPU writes latch+data over M11 bus, reads
  back; BIOS-style REDCLK/WRTCLK sequence. A/B — `tools/openmsx-io-parity.ps1` RTC register access
  vs openMSX (note fact-sheet §10 `#B4` read-instability caveat — compare `#B5` data reads, not `#B4`).

### 4.3 Full i8255 PPI + keyboard-row plumbing (C6)

- **Placement:** expand `src/devices/chipset/ppi_slot_select.{h,cpp}` into a full PPI —
  recommend rename to `src/devices/chipset/ppi_8255.{h,cpp}` (i8255 is S1985-integrated glue → stays
  in `chipset/`), holding ports A/B/C/control; the keyboard matrix itself lives in
  `src/peripherals/` (see §4.5) and is injected. **This is a required M1-M14 change — see §5.**
- **Ports (fact-sheet §3; `references/openmsx-21.0/src/I8255.*`, `MSXPPI.*`):**
  - `#A8` port A (r/w) — primary slot select, 2 bits/page → drives `SlotBus`. **Behavior preserved
    byte-for-byte** from current `PpiSlotSelect` (M11/M13 regression-critical).
  - `#A9` port B (read) — keyboard row: returns 8 column bits of the selected matrix row, inverted
    (0 = pressed).
  - `#AA` port C (r/w) — bits0-3 keyboard row select (0-10); bit4 cassette motor (inert, C7); bit5
    cassette write (inert, C7); bit6 CAPS LED; bit7 key-click (deterministic, no audio in M15).
  - `#AB` control — i8255 bit-set/reset mode (bit0 value, bits1-3 port-C bit index, bit7=0).
- **Mirrors:** `#AC-#AF` already mirror `#A8-#AB` in `wire_bus` — once base devices answer `#A9-#AB`,
  the mirror is automatically live (fact-sheet §3/§10; MSX-ENGINE detection `IN(#AC)==IN(#A8)`).
- **Wiring:** attach the PPI on `#A9/#AA/#AB` in addition to `#A8` (currently only `#A8` is attached).
- **Tests:** unit — port A slot-select unchanged (reuse existing M11 assertions verbatim); port C row
  select drives which matrix row port B returns; port B inversion; control-register bit-set/reset
  toggling port C bits; CAPS/KANA LED state bits. System-integration — CPU selects a row via `#AA`
  and reads `#A9`. A/B — idle keyboard read vs openMSX.

### 4.4 Joystick peripheral (C6)

- **Placement:** `src/peripherals/joystick.{h,cpp}` (per `src/CLAUDE.md` "peripherals/ = ... joystick").
- **Behavior (fact-sheet §2):** two ports; R14 read bits: 0=up,1=down,2=left,3=right,4=trigA,5=trigB
  (0=pressed). R15 bit6 selects which port feeds R14. Injected into the PSG (§4.1), which owns R14/R15.
- **Determinism:** default = no direction/button pressed → idle read all-high. Deterministic; input
  events are a frontend concern (C9).
- **Tests:** unit — R15 bit6 port select routes the correct port to R14; idle read. Integration via PSG.

### 4.5 Keyboard matrix peripheral (C6)

- **Placement:** `src/peripherals/keyboard_matrix.{h,cpp}`.
- **Behavior (fact-sheet §3/§10):** 11×8 matrix, inverted (0=pressed). Selected row (PPI port C
  bits0-3) chooses which 8-bit column vector PPI port B returns. Ghosting NOT modeled in M15 (fact-
  sheet §10: "most emulators inject the NEWKEY buffer directly" — deterministic direct-injection is
  the M15 model; hardware ghosting is an optional later refinement).
- **Determinism:** idle = no keys → all rows read `0xFF`. Key state set by test/API; live input is C9.
- **Tests:** unit — set a key in matrix, select its row, read inverted column bit; row-select bounds.

### 4.6 S1985 backup-RAM `.sram` persistence (C4)

- **Placement:** extend `src/devices/chipset/s1985_engine.{h,cpp}` (device already present) + a small
  persistence path in `src/machine/` (reuse the M13 asset/file conventions).
- **Behavior (fact-sheet §6; the existing device already models the volatile registers):** on
  cold_boot, load 16 bytes from a `.sram` file if present (deterministic default = zero-fill when
  absent, matching current `reset()`); write-back on a defined flush point. Address/data/rotating-
  pattern/color1/color2 register behavior unchanged (M11-verified).
- **Determinism / provenance:** the `.sram` file is a local, never-fabricated artifact under the
  debug/asset root (guardrails: no fabricated provenance). Absent file → deterministic zero state
  (identical to today), so no regression to M11 tests.
- **Tests:** unit — write bytes via switched-I/O ID `0xFE`, flush, reload → same bytes; absent file →
  zero state (M11 golden unchanged). Determinism — two runs byte-identical.

---

## 5. Required M1-M14 Changes (DEC-0008 authorizes; ledger-tracked when executed)

Each change below will be recorded in the ledger (decisions/requests/responses/milestones/backlog)
when implemented, per DEC-0005/DEC-0008. None is a rewrite; all are additive or tightly-scoped.

| # | Change | Why | Regression risk & mitigation |
|---|--------|-----|------------------------------|
| **X1** | Expand `src/devices/chipset/ppi_slot_select.*` into a full i8255 PPI (ports A/B/C/control), recommend rename → `ppi_8255.*` | Keyboard rows (`#A9-#AB`) must become live; current device is port-A-only | **Med.** `#A8` slot-select is M11/M13-critical (drives boot). Mitigation: preserve `#A8` read/write path byte-for-byte; reuse existing M11 slot-select tests unchanged as a locked regression guard; add ports B/C/control as new surface only. |
| **X2** | `src/machine/hbf1xv_machine.*` `wire_bus`: attach PSG `#A0-A2`, RTC `#B4/B5`, `#F5` system-control gate, PPI `#A9/#AA/#AB`; construct new device members; inject keyboard+joystick | Plug real devices into the currently-unmapped seam ports | **Med.** Adds members + attachments; existing attachments untouched. Mitigation: additive only; existing `#98-9B/#A8/#FC-FF/#40-4F` wiring and mirrors unchanged; M11-M14 io tests must stay green. |
| **X3** | `cold_boot` reset ordering: reset PSG, RTC, PPI, keyboard, joystick alongside existing devices | New devices need deterministic power-on state | **Low.** Ordering additions; existing reset sequence preserved. Mitigation: reset new devices after existing ones; assert deterministic boot state. |
| **X4** | Advance PSG envelope + RTC time from the scheduler clock WITHOUT changing CPU T-state accounting | Deterministic device time must not perturb the M9/M12 signed `cpu_step` timing oracle or `step_cpu_instruction` T-state math | **High (if done wrong).** Mitigation: device time derives from `elapsed_cycles()` READ-ONLY; the `datasheet_tstates + m1_wait` computation in `step_cpu_instruction` (`hbf1xv_machine.cpp:269-272`) is NOT touched. Regression guard: all M9/M11/M12 timing oracles must recompute identically. |
| **X5** | Joystick path shared between PSG port A and the S1985 joystick pins | Fact-sheet §2: joystick pins are wired to the S1985 but READ THROUGH PSG R14; the joystick peripheral connects to the PSG, not to `S1985Engine` | **Low.** Clean ownership: peripheral → PSG. No change to `S1985Engine`. |
| **X6** | Optional: relocate/rename nothing else — the M11 `S1985Engine` residual (backup RAM, mapper readback, M1 wait) stays put; C4 extends it in place | Avoid gratuitous refactor (scope control) | **Low.** In-place extension only. |

The `#F5` system-control register (X2) is a NEW small device, not a change to prior code, but it is
noted here because its polarity (Sony non-inverted, fact-sheet §9) interacts with BIOS boot behavior
(Q3).

---

## 6. Boot-Boundary (C5) Assessment

**Current boundary (M13/M14):** `docs/m13-parity-trace-diff.md` verified lockstep boot up to the
first VDP/PSG/RTC read; PC advances `0x0000 → ~0x043C` through real BIOS. Beyond that, the
open-bus `0xFF` on `#A0-A2` (PSG) and `#B4/B5` (RTC) and the dead keyboard rows diverge from real
hardware.

**Expected progress once the M15 IN set exists:**
- **PSG init:** BIOS writes R7 (mixer/IO-dir) and clears amplitudes early; with a real PSG these
  writes land and read back correctly (YM readback), removing a divergence source.
- **RTC Block-2 CMOS read:** BIOS reads Block-2 reg0; if it reads `0x0A` (valid CMOS) BIOS applies the
  stored screen/width/color/region config, else loads defaults and clears Block 3 (fact-sheet §5).
  Either path is DETERMINISTIC and A/B-matchable — we choose one (recommend: valid `0x0A` with a
  fixed deterministic Block-2 config) and match openMSX configured identically (Q3).
- **Keyboard boot-key scan:** BIOS scans the matrix for boot modifiers (e.g. CTRL to skip disk, SHIFT,
  etc.); idle matrix (`0xFF`) is deterministic → BIOS takes the no-modifier path.
- **VDP init:** already real (M14) — screen mode set, VBlank IRQ path live.

**How far boot can go:** with PSG/RTC/keyboard/VDP real, boot should progress through the standard
MSX2+ init sequence up to the **slot scan for the Disk ROM / MSX-MUSIC**. The DISK-ROM init handshake
requires the FDC (deferred, M18); with no FDC responding, boot will either take the "no disk" path
(toward BASIC) or stall at the disk handshake — **this is the C5 residual**, explicitly owned by M18.

**Boot checkpoint = M15 acceptance signal:** define a deterministic instruction-count / PC + machine-
state checkpoint reached after PSG+RTC+keyboard+VDP init, captured in lockstep against openMSX
`Sony_HB-F1XV` for N instructions (N to be fixed during implementation by the first divergence, per
the M13 method). Acceptance = empty A/B architectural-state diff to the checkpoint, reproduced by QA
(no fabricated parity — guardrails, DEC-0001 precedent). The checkpoint advances strictly beyond the
M13 `~0x043C` boundary; the exact PC is derived, not asserted here.

**SCOPE QUESTION Q1:** Should M15's C5 acceptance be "advance the deterministic A/B checkpoint past
the PSG/RTC/keyboard init reads" (recommended, achievable without FDC), or must it reach a specific
target (e.g. BASIC prompt)? The latter likely needs FDC (M18) and/or cartridge (M19).

---

## 7. Slice Plan (M15-S1 … M15-S6)

Each slice is independently buildable, testable, and evidence-gated. Evidence gates per slice:
`tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`;
`cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure`.
Behavior-affecting slices additionally run the relevant `tools/openmsx-*-parity.ps1` A/B harness.

| Slice | Goal | Primary files | Unit tests | System-integration tests | Evidence gates |
|-------|------|---------------|------------|--------------------------|----------------|
| **M15-S1** | PSG YM2149 device (registers, tone/noise/5-bit envelope/mixer, stereo mix) on `#A0-A2` | `src/devices/audio/psg_ym2149.*`; `wire_bus` attach | latch mask, period decode, envelope steps+shape, R7 dir-mask, YM readback, stereo mix | CPU `OUT #A0/#A1` → `IN #A2` over M11 bus | all 4 gates + `tools/openmsx-io-parity.ps1` (PSG regs) |
| **M15-S2** | Joystick peripheral + PSG port A/B (R14/R15) integration | `src/peripherals/joystick.*`; PSG wiring | R15 bit6 port-select, idle R14, cassette/layout bits | idle joystick read via PSG over bus | 4 gates + io-parity (port A) |
| **M15-S3** | RTC RP5C01 device + `#F5` gate on `#B4/B5`, deterministic epoch | `src/devices/rtc/rp5c01.*`; `#F5` system-control; `wire_bus` | latch mask, block-select, BCD rollover, Block-2 reg0=`0x0A`, regs14-15 unreadable, `#B5` hi-nibble, `#F5` gate | CPU REDCLK/WRTCLK-style sequence over bus | 4 gates + io-parity (RTC data reads) |
| **M15-S4** | Full i8255 PPI (expand `ppi_slot_select`→`ppi_8255`) + keyboard matrix on `#A9/#AA/#AB` | `src/devices/chipset/ppi_8255.*`; `src/peripherals/keyboard_matrix.*`; `wire_bus` | **port-A slot-select unchanged (reuse M11 asserts)**, port C row-select, port B inversion, control bit-set/reset, LED bits | select row via `#AA`, read `#A9` over bus; `IN(#AC)==IN(#A8)` mirror | 4 gates + io-parity (idle keyboard) |
| **M15-S5** | S1985 backup-RAM `.sram` persistence (C4) | `src/devices/chipset/s1985_engine.*`; machine persistence path | write via ID `0xFE` → flush → reload same; absent file → zero (M11 golden intact); determinism | switched-I/O `#40-4F` round-trip over bus | 4 gates |
| **M15-S6** | System integration + boot-checkpoint advance (C5) + full A/B trace-diff; zero regression M1-M14 | `hbf1xv_machine.*` cold_boot ordering; parity report | — | boot-to-checkpoint lockstep vs openMSX; full regression sweep | 4 gates + `tools/openmsx-trace-parity.ps1` → `docs/m15-parity-trace-diff.md` |

Sequencing: S1→S2 (PSG then its input), S3 (RTC), S4 (PPI/keyboard), S5 (persistence), S6
(integration + boot + regression). S6 is the QA sign-off gate for the milestone.

---

## 8. Acceptance Criteria (milestone-level)

- PSG (B1), RTC (B2), full PPI + keyboard + joystick (C6), and S1985 backup-RAM persistence (C4) are
  implemented to production spec, each grounded in its fact-sheet section + openMSX reference, and
  wired into the M11 `IoBus` so the previously-unmapped `#A0-A2` / `#B4/B5` / `#A9-#AB` ports answer
  with real device behavior (the "seam" is replaced).
- The S1985 chipset presents its real constituent devices — no seam-only stub remains for the IN set.
- Deterministic unit + system-integration tests cover every device; two identical runs produce
  byte-identical device state (RTC/PSG determinism explicitly asserted).
- Real openMSX A/B trace-diff evidence captured for behavior-affecting devices
  (`docs/m15-parity-trace-diff.md`), QA-reproduced, no fabricated parity.
- Boot checkpoint (C5) advanced past the M13 first-device-read boundary, A/B-matched to a fixed
  checkpoint (§6 / Q1).
- ZERO regression across M1-M14 suites; QA sign-off; human release decision.
- Every backlog row touched (B1, B2, C4, C5, C6) status-updated in `deferred-backlog.md` in the same
  cycle; all deferred rows re-affirmed with their named follow-on owner (§2).
- Evidence gates green: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile
  docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug
  --output-on-failure`.

---

## 9. Risks and Assumptions (each with a verification action)

| ID | Risk / Assumption | Verification action |
|----|-------------------|---------------------|
| R-1 | RTC pulls host wall-clock → non-determinism | Seed fixed emulated epoch, advance from scheduler only; unit test byte-identical across runs (A-M15-1) |
| R-2 | Expanding PPI breaks the M11/M13-critical `#A8` slot-select path | Preserve `#A8` byte-for-byte; run existing M11 slot-select tests unchanged as a locked guard (X1) |
| R-3 | Device time-advance perturbs the signed M9/M12 `cpu_step` timing oracle | Device time is READ-ONLY off `elapsed_cycles()`; do NOT touch `step_cpu_instruction` T-state math; recompute all timing oracles identically (X4) |
| R-4 | `#F5` gate polarity wrong (Sony non-inverted) → BIOS mis-boots | Read the openMSX `#F4/#F5` handler + HB-F1XV XML before implementing; A/B boot checkpoint (A-M15-2, Q3) |
| R-5 | PSG `mirrored_registers` mask wrong (`0x0F` vs `0xFF`) | Confirm HB-F1XV XML `mirrored_registers`; ground on `MSXPSG.cc:38` (A-M15-3) |
| R-6 | `#B4` read-instability (fact-sheet §10) makes A/B noisy | Compare `#B5` data reads, not `#B4`; document the caveat in the parity report |
| R-7 | RTC Block-2 config choice diverges from openMSX default | Configure openMSX identically (valid `0x0A` + fixed Block-2), or take the defaults path; fix in S3 (Q3) |
| R-8 | Boot cannot reach a clean checkpoint without the FDC | Define the checkpoint short of the disk handshake; residual owned by M18 (Q1) |
| R-9 | New `src/devices/audio/` + `src/devices/rtc/` folders need CMake wiring | Add to the build in S1/S3; build gate catches omissions |
| R-10 | Keyboard ghosting omitted may mismatch exotic software | Deterministic direct-injection matrix (fact-sheet §10 sanctioned); ghosting is an optional later refinement, not M15 |

---

## 10. Developer Handoff

- **Do NOT start coding** — this package awaits human review per DEC-0008. On approval, the developer
  executes M15-S1…S6 in order (§7), each with its evidence gates.
- **IN-M15 set:** B1 (PSG), B2 (RTC), C4 (S1985 `.sram` persistence), C6 (keyboard+joystick+full PPI),
  C5 (boot-checkpoint advance, partial).
- **Placement (per `src/CLAUDE.md`):** `src/devices/audio/psg_ym2149.*`,
  `src/devices/rtc/rp5c01.*`, `src/devices/chipset/ppi_8255.*` (expanded from `ppi_slot_select.*`),
  `src/peripherals/{keyboard_matrix,joystick}.*`, extend `src/devices/chipset/s1985_engine.*`, wire in
  `src/machine/hbf1xv_machine.*`.
- **Seam replacement is additive:** attach real devices on the currently-unmapped `#A0-A2` / `#B4/B5`
  and expand PPI to answer `#A9-#AB`; leave the existing `IoBus`/`SwitchedIoController`/mirror fabric
  and the `S1985Engine` residual (backup RAM / mapper readback / M1 wait) unchanged.
- **Hard regression guards:** M9/M11/M12 timing oracles unchanged (do not touch `step_cpu_instruction`
  T-state math); M11 `#A8` slot-select tests reused verbatim; M13 boot golden must still pass or be
  re-derived with a documented, QA-audited reason.
- **Grounding is mandatory:** cite the fact-sheet section + the openMSX file (behavior reference only,
  never copied — GPL) for every behavior; no A/B parity claim without a genuine captured diff.
- **Ledger discipline (DEC-0005/0008):** every M1-M14 change in §5 recorded when executed; every
  backlog row touched status-updated same-cycle.

### SCOPE QUESTIONS for the human at review

- **Q1 (C5 target):** Is "advance the deterministic A/B boot checkpoint past PSG/RTC/keyboard init"
  the right M15 bar, or must M15 reach a specific target (BASIC prompt)? The latter likely needs the
  FDC (M18) and/or cartridge (M19).
- **Q2 (RTC persistence):** Persist RTC CMOS to a `.sram`-style file in M15 (symmetric with C4), or
  keep RTC CMOS in-memory-deterministic and defer its file persistence?
- **Q3 (`#F5` + Block-2 config):** Confirm modeling `#F5` bit-7 CLOCK-IC gating in M15 and the choice
  of RTC Block-2 boot config (valid `0x0A` + fixed config vs defaults path) to match openMSX for A/B.
- **Q4 (B4 owner):** Assign the MSX-JE 16 KB SRAM to M16 (with FM-PAC) or to M20 (with MSX-JE/Halnote
  firmware)? The backlog text ("FM-PAC/MSX-JE SRAM store") is ambiguous.
- **Q5 (follow-on numbering/order):** Confirm the proposed M16-M26 sequence (§3), especially whether
  FDC (M18) should precede or follow the FM-PAC/peripheral milestones given its boot importance.
- **Q6 (PSG audio depth):** Confirm that M15 delivers the deterministic NUMERIC PSG sample model only,
  with analog/audio presentation deferred to the SDL3 frontend (M26 / C9).

---

Prepared for human review. No production code produced. On approval, proceed to M15-S1.
