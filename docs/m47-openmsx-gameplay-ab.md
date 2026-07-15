# M47 — YS II save-corruption gameplay A/B (DEC-0072, FINAL kill-step)

**Status:** read-only investigation, 2026-07-13. No `src/` edits. WSL openMSX 19.1 + our
headless engine. All artifacts under `debug/m47repro/ab/`.

**Bottom line up front.** The corruption is pinned, with same-owner clean/corrupt lineage, to a
single +1 written into one 16-bit game stat at **frame 4158, PC 0x98C7**. But the *cycle-matched
openMSX A/B the task asks for is intractable with this recording* and I prove why: `owner_bad.script`
is timed to our **fast-disk** timeline, and openMSX (real WD2793 timing) boots ~2× slower, so
time-scheduled key injection never reaches the same game state (openMSX is still on the YS II
title screen while our engine is deep in gameplay). Our own engine confirms this — under
`--no-fast-disk` the identical script never reaches the 4158 event either. So I could not confirm
at the instruction level *what real hardware computes at 0x98C7*, and per the task rule I do **not**
propose a device fix that the A/B has not backed. I instead deliver: the fully-confirmed corruption
locus, a stronger-than-before clean-vs-corrupt confirmation, the openMSX A/B tooling, the proof of
intractability, a ranked root-cause hypothesis, and the concrete tractable A/B that *would* confirm it.

---

## 1. Confirmed corruption locus (our engine, fully deterministic)

Repro (exact task command, and the `--ram 64` stock variant) reproduces the owner's corrupt disk
**byte-identical** to `games/disks/ys2/d2_rec.dsk` (0 diffs in the whole file; 0 diffs in save-slots
LBA 9–16). RAM size is irrelevant: `--ram 64` and `--ram 512` runs are byte-identical.

The corrupt field is a **16-bit stable stat at save-record offset 2–3 == physical DRAM 0xC102–0xC103**.
Full write history over the run (`--watch-mem C102 C104`, `debug/m47repro/ab/our_c102_all.csv`):

| frame | PC | value @0xC102-3 | note |
|------:|----|----|------|
| 441/655/958 | 0x7664 | boot junk | BIOS/loader scratch |
| **2034** | 0xCAE4 / 0xC97B | **0x0631** (1585) | bulk LDIR load — establishes the CORRECT value |
| **4158** | **0x98C7** | **0x0632** (1586) | **the +1 corruption** |

It is written **exactly once** between frames 3000–5500 (event-driven, not a per-frame timer).
At the 0x98C7 store: `HL=0x0632, DE=0x0631, A=05, BC=0006, IX=0180, IY=0198, SP=DAFC` — a 16-bit
store of `HL`. `IX`/`IY` point at on-map **object records** (physical 0xC180/0xC198 carry
24-byte object entries: sprite-slot 0x80…, page E0/80/A0/C0 — matches the save's object-table
layout, `docs/m47-openmsx-setup-and-save-structure.md` §2.3). So 0x98C7 is an
**object/stat-processing routine** (consistent with YS II bump-combat resolution), and the +1
propagates through the save's prefix-sum encode to corrupt ~192 stored bytes (the amplification
already documented in DEC-0072).

Snapshot at frame 4157 (`debug/m47repro/ab/snapshot/f004158_*`): 0xC102 is still **0x31** and the
FDC is idle with `LAST_SYNC` ≈ frame 3137 — i.e. **no disk I/O at frame 4158**. Fast-disk is not
the direct cause of the 4158 event; it is only needed for the *recording* to stay in sync.

## 2. Clean-vs-corrupt lineage (the strongest confirmation — no openMSX needed)

A second same-owner recording, `debug/m47repro/owner_save_repro.script`, **saves earlier** — at
**frame 3013, before the 4158 event** — and produces a **clean** save: 0xC102 = **0x31**, first-16
`00 00 31 37 7C CC CF D4 …` (≈ the known-good `roms/fmpac.rom.sram`; only 41/512 tail bytes differ,
a slightly later moment). `owner_bad.script` saves **after** 4158 (frame 5471) → 0xC102 = **0x32** →
the owner's corrupt record.

Raw-field diff of the two saves: they differ in 84/512 fields (different game moments), but in the
**stable early header they differ at exactly one byte — offset 2** (clean `00 00 31 06 45 50`,
corrupt `00 00 32 06 45 50`; offsets 3,4,5 identical). This is decisive: **a save of this
playthrough taken before frame 4158 is valid; taken after, it carries the +1 and is the reported
corruption.** The +1 at 0x98C7 *is* the corruption.

## 3. openMSX A/B — tooling built, and proof the cycle-match is intractable

Built and verified (all under `debug/m47repro/ab/`):
- **Headless openMSX scripting** works: `smoke.tcl` boots `Sony_HB-F1XV -ext fmpac`, advances
  emulated time, reads memory, exits (SRAM staged byte-identical per the setup doc).
- **Debuggable map** (`omsx_debuggables.txt`): openMSX HB-F1XV has **"Main RAM" size=65536 (64 KB,
  stock)**; the game buffer must be read by **physical mapper offset** via `debug read "Main RAM"
  0xC100` (banking-independent) — reading logical `debug read memory 0xC102` returns 0xFF because
  page 3 is bank-switched away at the async read instant.
- **Input→openMSX replay generator** (`gen_replay2.py` → `replay_hdr2.tcl`): converts
  `owner_bad.script` `T`-states → seconds (`T/3579545`), maps our key names → MSX matrix rows
  (RETURN=7/0x80, F1=6/0x20, arrows=row 8) via `keymatrixdown/up`, schedules the disk swap at
  `frame1108·59736/3579545` and RAM dumps at 14 checkpoints with `after time`.

**Why it cannot cycle-match (proven, not assumed):**
- openMSX with **real WD2793 timing boots far slower** than our fast-disk engine. Screenshot at
  emulated t=20 s shows openMSX **still on the YS II title screen** (`omsx_t20.png`), whereas our
  fast-disk engine has been in gameplay since ~t=11 s. The replay's `0xC100` stayed the openMSX
  power-on pattern `06 D1 06 D1 …` for the whole run (`omsx_hdr2.csv`) — the game **never reached
  the state** where the buffer is populated.
- **Independent confirmation from our own engine:** running `owner_bad.script` with
  `--no-fast-disk` (accurate FDC timing, i.e. openMSX-like) produces **no 0x98C7 event and never
  reaches the save** (`our_nofast_*`). The recording is locked to the fast-disk timeline; any
  slower/real disk timing desyncs it.

Therefore a matched-state, per-frame RAM diff (openMSX vs ours) around frames 4000–5500 is not
achievable from this recording, and I did not fabricate one.

## 4. Timing character (measured)

- **Raster-phase robust:** `--input-shift 0..29868` (up to ½ frame) keeps the value at 0x32; the
  event merely slides to a later frame (`sh_*.csv`). So it is **not** a "save-build lands on the
  wrong scanline" artifact.
- **RAM-size independent** (§1).
- **Extreme timing sensitivity:** `--no-fast-disk` *or* `--vdp-cmd-bias ±500` **desyncs the whole
  replay** (the 4158 event never fires) — the playthrough's outcome depends on precise cycle timing
  of device interactions across the ~4000 frames leading to the event.

## 5. Root-cause hypothesis (labeled — NOT A/B-confirmed)

The +1 is produced by an **object-processing routine** (IX/IY → monster/object records) that
"reads RAM/IY state" (no device read at 0x98C7 itself); the diverging input is an object's state
set **earlier**, and the outcome is highly sensitive to device timing. Ranked candidates from the
task's list:

1. **VDP sprite-collision / coincidence status (S#0 bit 5 / bit 6).** *Leading hypothesis.* YS II
   is bump-combat: the player walks into enemy **sprites**; the routine that awards a stat reads
   object records (IX/IY) and, plausibly, the VDP sprite-collision flag. A timing-dependent
   collision/coincidence result would register one extra (or one differently-resolved) hit → +1 to
   a combat stat. Fits: object-record inputs, "combat-like" one-shot event, timing sensitivity.
2. **VDP VBlank interrupt timing / raster-driven state.** History flags this as fragile
   (MEMORY: M36 "Bug B = VDP IE0-write /INT bug"; M41 interrupt fixes). A one-count interrupt/frame
   drift over 4000 frames would shift an interrupt-driven counter by 1. Fits the ±500-T-state
   VDP-bias fragility.
3. **CPU/instruction or PSG/RTC edge.** Least likely (ZEXALL/ZEXDOC pass; no RTC/PSG dependence seen).

## 6. What would actually confirm it (recommended, tractable)

Because the full-replay A/B is blocked by the fast-disk timeline, do one of:

- **(A) Re-capture a replayable reproducer.** Record the owner's session (or a short scripted
  reproducer that reaches the 4158-equivalent combat) **under `--no-fast-disk`** so its timeline
  matches openMSX's real FDC, *then* run the cycle-matched A/B with the tooling already built here
  (`replay_hdr2.tcl` + `debug read "Main RAM"` at physical 0xC100). This is the clean way to obtain
  real-hardware's value at 0x98C7 and to bisect the first-diverging RAM byte before 4158.
- **(B) Isolated device A/B (fast).** Test the leading hypothesis directly with an isolated
  sprite-collision timing probe (extend the existing `--sprite-cmd-parity` harness / an openMSX Tcl
  A/B): position two sprites like YS II's player+enemy and read **S#0 collision (bit 5) / 5th-sprite
  (bit 6)** across a raster sweep on both engines. If our collision-flag *timing* diverges from
  openMSX, that confirms candidate #1 and points to the exact fix (sprite-collision raster timing in
  `src/devices/video/`), *with A/B data behind it*.

Only after (A) or (B) yields A/B evidence should a `src/` fix be written; re-run the repro and
expect the save to come out with 0xC102 = 0x31 (clean, loads without the SP-underflow at 0xAA75).

## 7. Artifacts (`debug/m47repro/ab/`)

- `FINDINGS.md` — condensed data sheet.
- `our_c102_all.csv`, `our_c102_ram64.csv`, `our_hdr_writes.csv`, `our_stat_traj.csv` — 0xC102
  trajectory + 16-byte header reconstruction (our engine).
- `our_savelog.csv`, `our_savelog_ram64.csv`, `osr_savelog.csv` — per-frame save-slot detector
  (corrupt @5471 vs clean @3013).
- `sh_*.csv`, `vb_*.csv`, `our_nofast_*` — input-shift / vdp-cmd-bias / no-fast-disk sensitivity
  sweeps.
- `snapshot/f004158_*` — full machine snapshot at frame 4157 (DRAM, mapper, FDC idle, etc.);
  `our_dram.bin` — extracted 512 KB physical DRAM.
- openMSX tooling: `gen_replay2.py`, `replay_hdr2.tcl`, `smoke.tcl`, `list.tcl`,
  `omsx_debuggables.txt`, `omsx_hdr2.csv` (all-`06D1` = never reached state),
  `omsx_t20/35/55/70.png` (title screen at t=20 s).

## Sources
- Corruption structure / prior tracing: `docs/m47-openmsx-setup-and-save-structure.md`,
  `debug/m47repro/evidence/corrupt_save/`.
- Engine debug flags: `src/main.cpp` (`--watch-mem`, `--snapshot`, `--input-shift`,
  `--vdp-cmd-bias`, `--no-fast-disk`), key matrix `src/peripherals/msx_key_names.cpp`,
  fast-disk = timing-only `src/devices/fdc/wd2793.h:156-305`.
- openMSX: machine `Sony_HB-F1XV`, ext `fmpac`, `debug read "Main RAM"`, `keymatrixdown/up`,
  `after time`, `screenshot` (openMSX 19.1, WSL).
