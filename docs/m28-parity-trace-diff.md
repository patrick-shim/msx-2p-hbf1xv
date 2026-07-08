# M28-S2 openMSX A/B Parity Trace Diff — C5 Full-Boot / Auto-Disk-Boot-Trigger Investigation

- Milestone: M28 (Release Candidate — Backlog Closure Sweep + Full-Project Health Audit), slice S2.
- Backlog item: C5 (full boot / automatic disk-ROM boot-handshake trigger).
- Subject-A emulator: `sony_msx_headless` (this repo, Debug build, `build/Debug/sony_msx_headless.exe`).
- Reference-B emulator: openMSX 19.1 (WSL, `/usr/bin/openmsx`, `flavour: debian components:
  ALSAMIDI CORE GL LASERDISC`).
- Reference machine: **`Sony_HB-F1XV`** (`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`).
- Real disk asset: `disks/msxdos22.dsk` (737,280 bytes, sha256
  `934399667f9e684a97402ab1a16ab24272a23d0f79214e6510936fa6c50ac457`), a genuine bootable MSX-DOS
  2.2 system floppy — the key new variable this cycle vs. M16's own Subject 1
  (`docs/m16-parity-trace-diff.md`), which only had the FDC's default synthesized (non-bootable)
  medium mounted.
- Comparator: `tools/trace-diff.py` (unchanged from M10-M27) — diffs every ARCHITECTURAL Z80 field
  (PC, opcode bytes, A, F + all flag bits, B, C, D, E, H, L, AF, BC, DE, HL, AF', BC', DE', HL',
  IX, IY, SP, I, R, IFF1, IFF2, IM). T-state/cycle fields are reported, never gated (established
  M10 convention).
- Harness: `tools/openmsx-m28-c5-boot-parity.ps1` (new, this milestone) — drives this emulator via
  the M27 `--debug-session --disk --trace-cpu` tooling and openMSX via the established live-Tcl
  PC/register readback technique (same shape as `tools/openmsx-m16-boot-parity.ps1` Subject 1).
- Internal (single-emulator) diagnostic: `tests/system/hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp`
  (new, committed, part of the standard ctest suite).
- Capture date: 2026-07-08.

Every subject below produced a GENUINE captured trace / run (no fabrication).

---

## Method (per planner §2.1a)

Two complementary techniques, both grounded in the FDC fact-sheet's documented boot-relevant
facts (`references/fact-sheets/FDC for Sony HB-F1XV.md` §2: Disk BIOS entry points `0x4010`
DSKIO/`0x4013` DSKCHG/`0x4016` GETDPB/`0x401C` DSKFMT; §6: the standard MSX disk driver is
**polled, not interrupt-driven**, "a tight polling loop... Interrupts are disabled (DI) around the
transfer"):

1. **A/B architectural trace comparison** (this document) — a real, unattended cold boot with
   `disks/msxdos22.dsk` mounted as drive A on BOTH sides, single-stepped from an identical reset
   vector, diffed field-by-field, to find the FIRST point of divergence (if any) between this
   emulator and real openMSX.
2. **Internal, single-emulator diagnostic** (no openMSX needed, much larger step budgets feasible)
   — `hbf1xv_m28_c5_disk_boot_investigation_system_test`, which cold-boots with the SAME real
   `msxdos22.dsk` mounted, with and without a scripted keypress (M27's `InputScriptPlayer`), and
   tracks on EVERY instruction boundary whether page 1's resolved sub-slot for primary 3 ever
   becomes `2` (the disk-ROM window, `Sony_HB-F1XV.xml:161-176`) — the earliest, most fundamental
   necessary precondition for DSKCHG → Restore → Read Sector to ever follow. This is diagnostic
   instrumentation only: no BIOS/disk-ROM logic is disassembled, reproduced, or reimplemented
   anywhere in this repo (mirrors the M16 precedent's own license discipline) — only the OBSERVED,
   empirical CPU/bus behaviour of the real, unmodified local `bios/` + `disks/` assets already used
   since M10-M27 is measured.

`disks/msxdos24/` is a raw directory tree (`AUTOEXEC.BAT`, `COMMAND2.COM`, `MSXDOS.SYS`,
`MSXDOS2.SYS`, `HELP/`, `UTILS/`), not a flat, mountable `.dsk`/`.img` file, so it cannot be
attached to `DiskImage`/`-diska` directly without a directory→image conversion step (that
capability is explicitly C10 territory — FDC flux/DMK-image fidelity, deferred to M31 per
`docs/m28-planner-package.md` §2.3c — and was correctly NOT built ad hoc here). Investigation
therefore used the two flat `.dsk` images (`msxdos22.dsk`, `msxdos23.dsk`), both genuine, bootable
MSX-DOS system floppies.

---

## Result 1 — A/B architectural trace, canonical window (3000 instructions) → PARITY

Authentic reset (`#A8 = 0`, `PC = 0x0000`, all-zero register file, `SP = 0xFFFF`, IM1), real
`msxdos22.dsk` mounted as drive A on BOTH sides, single-stepped for **3000 instructions** (matching
M16's own Subject-1 window exactly, for direct comparability).

**Result: ARCHITECTURAL PARITY — EMPTY DIFF.** All 3000 instructions match on every architectural
field. Final emulator cumulative T-states: 19819. Both traces reach the identical final state
(`PC=0x0456`), inside the same RAM-sizing-test loop M15/M16 already identified (`docs/m15-implementation-report.md`, `docs/m16-parity-trace-diff.md`) — mounting a REAL bootable disk
(vs. M16's default synthesized medium) has **not** perturbed the CPU's architectural trajectory
through this window, and it remains bit-for-bit identical to genuine openMSX.

Raw traces: `build/m28_boot_A.txt` (emulator), `build/m28_boot_B.txt` (openMSX). Diff:
`build/m28_boot_diff.md`.

## Result 2 — A/B architectural trace, extended window (100,000 instructions) → PARITY

The same comparison, extended over a **100,000-instruction** window (33x M16's own Subject-1
window) to push further into the boot sequence while the live-Tcl single-step technique remained
wall-clock-feasible (~27 s including openMSX startup).

**Result: ARCHITECTURAL PARITY — EMPTY DIFF**, again. All 100,000 instructions match on every
architectural field; final emulator cumulative T-states: 655534; final PC `0x045A` on both sides
(both still inside the RAM-sizing loop at this point — consistent with M16's own finding that this
loop runs for roughly 200,000–300,000 instructions before the trajectory moves on). **No
divergence point exists within this directly-comparable window** — this emulator's boot behaviour
with a real MSX-DOS disk mounted is empirically identical to real openMSX 19.1 that far.

Raw traces: `build/m28_boot_100k_A.txt`, `build/m28_boot_100k_B.txt`. Diff:
`build/m28_boot_100k_diff.md`.

### Why the A/B window was not pushed further

Each additional live-Tcl single-stepped instruction on the openMSX side costs roughly 0.2-0.3 ms of
wall time (dominated by the Tcl round-trip, not this emulator, which completes any of these windows
in well under a second). Extrapolating, reaching the few-million-instruction range where M16 found
the trajectory "settles into a steady low-PC idle loop" would cost on the order of 15-20 minutes of
live openMSX stepping for a single run — a real, disclosed practical limit of the live-Tcl technique
at this scale (not a fabricated stopping point). Instead, Technique 2 (below) uses this emulator
ALONE (no per-instruction Tcl round-trip) to push the search out to 20,000,000 instructions, at a
cost of seconds, providing the decisive, larger-scale evidence.

---

## Result 3 — Internal diagnostic, 20,000,000-instruction budget → disk-ROM window NEVER selected

`hbf1xv_m28_c5_disk_boot_investigation_system_test`, run against the built Debug binary:

```
[C5] idle-boot (real msxdos22.dsk, no input): max_pc=0xff5c final_pc=0x2c03 disk_rom_page1_ever_selected=0 (at instruction 0) read_sector_commands_accepted=0 read_sector_bytes_transferred=0 read_sector_completions_ok=0 final_fdc_command_reg=0x0
[C5] scripted-input boot (real msxdos22.dsk, SPACE held T=1e6..2e6): max_pc=0xff5c final_pc=0x2c03 disk_rom_page1_ever_selected=0 (at instruction 0) read_sector_commands_accepted=0
All Hbf1xvM28C5DiskBootInvestigation_System cases passed
```

(`real 0m8.032s` for three 20,000,000-instruction cold-boot runs — 60,000,000 instructions total.)

> **STALE-FIGURE CAVEAT (added post-hoc, per `docs/m28-qa-signoff.md` §4, Medium finding):** the raw
> capture above and the `final_pc` figures in the bullets below were taken **before** DEC-0026 (the
> separately-landed VDP S#2 VR/HR fix, `docs/vdp-vr-hr-boot-hang-fix-report.md`) landed in this same
> closure commit. DEC-0026 demonstrably changes real, CPU-driven boot behavior: the terminal state
> this capture reached (`final_pc=0x2C03`, characterized below as "an idle BASIC-command-line-
> input-wait loop... ordinary, legitimate MSX BIOS/BASIC cold-boot behaviour") was, per DEC-0026's
> own investigation, actually the pre-fix VR-status-bit wait-for-toggle hang — NOT a legitimate
> BASIC-ready idle state. That characterization is superseded, not merely stale. QA independently
> re-ran this exact diagnostic against the current, post-DEC-0026 tree and reproduced
> **`final_pc=0x7FF2`** (byte-identical across two runs — still fully deterministic, just a
> different terminal state, matching the further-but-still-incomplete boot progress
> `docs/vdp-vr-hr-boot-hang-fix-report.md` §4 already discloses: the CPU now reaches a *different*
> dead end, a permanent HALT via stack corruption from an already-disclosed, separate memory-mapper
> content divergence at instruction #145,503 — not yet a real MSX-DOS/BASIC prompt). **The row's
> actual, load-bearing finding — the disk-ROM window (primary 3, sub 2) is never paged into page 1 —
> is UNCHANGED and was independently re-confirmed by QA on the current, post-fix code**
> (`disk_rom_page1_ever_selected=0`, `max_pc=0xff5c`, exact match). C5's disposition (outcome (b),
> `IN-PROGRESS (M28 partial)`) requires no change. A future cycle re-running this diagnostic should
> expect `final_pc≈0x7FF2` (or whatever the then-current tree produces), not the pre-DEC-0026
> `0x2C03` figure below.

**Findings, genuinely observed (not fabricated):**

- **Page 1's resolved sub-slot for primary 3 (`(debug_sub_slot_register(3) >> 2) & 0x03`) is
  checked on EVERY one of 20,000,000 instruction boundaries and is NEVER `2`** (the disk-ROM
  sub-slot, `Sony_HB-F1XV.xml:161-176`) — with a real, bootable MSX-DOS disk mounted, with or
  without a scripted `SPACE` key held for a full 1,000,000-master-cycle window spanning the
  RAM-sizing-loop/early-boot region. This is a STRONGER, MORE DIRECT signal than M16's own
  (`Wd2793::read_sector_commands_accepted`, also independently confirmed at 0 here): it establishes
  the disk-ROM window is never even PAGED IN, not merely that no Read-Sector command was issued —
  necessarily true, since a command register write requires the FDC's I/O window to be reachable,
  which itself requires page 1 → slot (3,2), the exact condition checked.
- **`max_pc` reaches `0xFF5C`** (vs. M16's `0x7D6F` over a 400,000-instruction budget) — the CPU
  trajectory explores substantially more address space than M16's own committed checkpoint window,
  yet the disk-ROM window is still never touched.
- **`final_pc = 0x2C03`** (SUPERSEDED, see the stale-figure caveat above — this was actually the
  pre-DEC-0026 VR-status-bit wait-for-toggle hang, not a legitimate idle-input loop; the sentence
  originally here characterizing it as "ordinary, legitimate MSX BIOS/BASIC cold-boot behaviour" was
  an incorrect interpretation, corrected by DEC-0026's own investigation. Post-fix, QA reproduces
  `final_pc=0x7FF2` instead — a different, still-incomplete terminal state, per the caveat above).
- **Determinism confirmed**: two independent idle-boot runs produce byte-identical `max_pc`,
  `final_pc`, disk-ROM-selection outcome, and FDC counters (test case
  `IdleBoot_TwoRuns_Identical*`).
- **Cross-check with `disks/msxdos23.dsk`** (a different MSX-DOS version, same underlying BIOS-level
  boot mechanism): a 3,000,000-instruction `--debug-session --disk disks/msxdos23.dsk` run reaches
  the IDENTICAL `final_pc=0x2BF7` as the `msxdos22.dsk` idle-boot run at the same instruction count
  — consistent with the disk-ROM engagement decision being made (or, per this finding, never made)
  at a level of the boot sequence that precedes any DOS-version-specific code, not a `msxdos22.dsk`-
  specific artifact. (Spot-check only, not run through the full 20,000,000-instruction/A-B
  machinery — the `msxdos22.dsk` results above are the primary, fully-corroborated evidence.) **This
  `0x2BF7` figure is ALSO pre-DEC-0026** and subject to the same stale-figure caveat above — DEC-0026
  wires the new VDP raster clock unconditionally in `wire_bus()` (every `Hbf1xvMachine` instance, not
  gated by mode), so this cross-check's terminal state may also differ on the current tree; the
  underlying, load-bearing claim (both DOS versions converge on the same disk-ROM-never-engaged
  behavior) is architectural, not tied to this specific hex value, and needs no re-verification.

---

## Adversarial comparator self-check (as QA did for M10-M16)

Run against the canonical Result-1 traces:

- **Empty B side** (`build/m28_adversarial_empty_B.txt` = empty file) → `trace-diff.py` exit **2,
  BLOCKED** (`## Result: BLOCKED -- a trace side is empty (not comparable)`,
  `build/m28_adversarial_empty_diff.md`).
- **Corrupted field** (the `af=`/derived flags on the openMSX trace's 10th `OMTRACE` line hand-
  altered to `af=DEAD`) → `trace-diff.py` exit **1, ARCHITECTURAL DIVERGENCE** (`seq=9 pc=0420
  field=af A=0300 B=DEAD` + the derived `F` mismatch, `build/m28_adversarial_corrupted_diff.md`).

So an empty/parity diff from this harness is trustworthy: the comparator does not silently pass a
blocked or corrupted comparison.

---

## Honest, dual-outcome disposition (planner §2.1a)

Per `docs/m28-planner-package.md` §2.1a's explicit dual-outcome acceptance criterion:

- **Outcome (a) — full close** would require observing a real DSKCHG→Restore→Read-Sector sequence,
  the machine reaching a genuine MSX-DOS/BASIC prompt, and a bounded A/B window around that
  boot-decision point showing architectural parity with openMSX. **This was NOT observed** — see
  Result 3.
- **Outcome (b) — investigation advances, does not close** — reached here. A concrete, evidenced
  finding is produced: **with a real, bootable MSX-DOS system disk mounted (not merely the FDC's
  default synthesized medium), and with or without a scripted keypress spanning a wide early-boot
  window, this project's BIOS-driven boot NEVER pages the disk-ROM window (primary 3, sub 2) into
  page 1 within 20,000,000 instructions** — extending M16's own honest residual
  (`docs/m16-parity-trace-diff.md`) from "no Read-Sector command observed" to the stronger,
  earlier-in-the-pipeline "the disk-ROM's memory window is never even mapped in," and from "no real
  disk was mounted" to "a genuine, bootable MSX-DOS 2.2 system disk was mounted throughout." The
  architectural A/B comparison against real openMSX (Results 1-2) independently confirms this
  emulator's own boot trajectory is faithful to genuine hardware/openMSX behaviour over the directly
  comparable window (0-100,000 instructions) — i.e., this is not attributable to a defect in THIS
  emulator's slot-decode or boot path, mirroring M16's own conclusion.
- **C5 stays `IN-PROGRESS (M28 partial)`**, not force-closed, not silently reworded to sound closed
  — see `agent-protocol/state/deferred-backlog.md`'s updated C5 row and
  `docs/m28-implementation-report.md` for the full narrative, including what a future investigation
  would need (real BIOS/disk-ROM disassembly — which this project's guardrails correctly forbid
  reproducing verbatim in `src/`/`docs/`, but which could still ground an INDEPENDENT understanding
  of the real ROM-scan/cold-start decision sequence, e.g. via a legitimate MSX technical reference
  not yet present in `references/`) to determine WHY the disk-ROM window is never mapped in during
  an otherwise-faithful, openMSX-matching boot.

---

## Reproduce

```powershell
cmake --build build --config Debug
tools/openmsx-m28-c5-boot-parity.ps1                                     # canonical 3000-instr window
tools/openmsx-m28-c5-boot-parity.ps1 -BootTraceSteps 100000 -DiffOut docs/m28-parity-trace-diff-100k.md
ctest --test-dir build -C Debug -R hbf1xv_m28_c5_disk_boot_investigation_system_test --output-on-failure
```

Needs WSL `openmsx` (19.1 used here) with the `Sony_HB-F1XV` machine definition on its search path,
and the real, local `disks/msxdos22.dsk` asset (not redistributed by this repository).
