# DEC-0072 — YS II disk-save corruption: FINAL verdict (inherent to the game/disk)

**Status:** CLOSED — root cause confirmed by an independent, decisive owner test (2026-07-14).

**FINAL one-line verdict:** The YS II 8-slot save→load crash is **INHERENT to YS II's own
save/load code as present on these disk images** — it is **NOT** an emulator defect (not the FDC,
not our CPU/VDP timing). The owner reproduced the identical crash on **local Windows openMSX**, an
independent accuracy-focused emulator, from a **fresh game on a vanilla disk**, across **multiple
game sources** — all fail identically. Two independent accurate emulators agreeing from a clean
slate ⇒ the fault is in the software on the disk. Our emulator is fully vindicated.

> **Correction to an intermediate hypothesis:** during the investigation this was tentatively
> reclassified as a "CPU/VDP timing-fidelity" bug because the `--vdp-cmd-bias` sweep showed the
> crash is timing-*sensitive*. That sensitivity is real but was **over-read**: openMSX runs at
> correct timing and still crashes, so correct timing does not avoid the crash — the game reaches
> the crashing save-state under accurate timing too. Timing was a distraction, not the cause. The
> device-timing findings below remain valid as independent correctness items; they are NOT the YS
> II root cause.

**Likely mechanism for "multiple sources all fail":** MSX `.dsk` images overwhelmingly descend from
a few original cracks; independent "sources" are usually the same crack redistributed, sharing the
same buggy save routine that mishandles a full 8-slot save area.

**Practical guidance:** use 1–2 disk save slots (confirmed working) or FM-PAC SRAM saves (confirmed
100% reliable) — do not fill all 8 disk slots on these images.

## How we got here (the earlier wrong turns, corrected)

Across prior sessions the corruption was blamed on the FDC write path (M45, M45b, M47) and then on
"deterministic YS II game logic." Both are now **disproven by direct measurement**:

1. **FDC write is byte-faithful.** An env-gated per-byte write logger (`SONY_MSX_FDCWLOG`) over the
   deterministic replay shows **0 substitutions across all 8 saves**, and the written byte stream is
   **byte-identical** to what lands on disk (8 Write Sector cmds × 512 bytes, no drops/holes/overrun).
   Independently confirmed by the WD2793 datasheet audit: *"No write-corruption defect exists in the
   FDC."* The disk diff shows corruption confined **exactly** to LBA 9–16 (the 8 save slots) — no
   FAT/directory/adjacent spill, so addressing is clean too.
2. **The saved data itself is bad.** Loading our produced disk (`02f54bae`) on **openMSX** (genuine
   `Sony_HB-F1XV`) **immediately crashes** — an independent, accurate emulator chokes on the same
   bytes. So the corruption is created **upstream** of the FDC, when the game computes the save
   record from its RAM state.

## The breakthrough: the crash is TIMING-sensitive

The deterministic replay (`debug/ys2_8slot.script`, from a `--record-input` capture of the owner's
live 8-slot save) reproduces the owner's corrupted disk **byte-for-byte** (`02f54bae`) and the crash.
Sweeping the VDP command-engine busy-window bias (`--vdp-cmd-bias`) over that replay:

| bias (T-states) | resulting disk | outcome |
|-----------------|----------------|---------|
| **0 (current)** | `02f54bae`     | **CRASH** (SP underflow → HALT) |
| +1000           | `e7f8e40d`     | no crash |
| +5000           | `ef7e5811`     | no crash |
| −1000           | `3566fbf0`     | **CRASH** |
| −5000           | `4d5e4956`     | **CRASH** |
| +20000          | `ea771f4d`     | no crash |

The save data — and whether it crashes — **changes with command-engine timing**. The outcome sits
on a **cycle-timing knife-edge**. That rules out pure game logic (which is timing-independent) and
rules out the FDC (byte-faithful). It is a **timing-fidelity** problem: our emulated cycle timing
differs slightly from real hardware, and for a game this timing-sensitive that difference is enough
to change the computed save.

### Crash mechanism (CPU trace, ring-buffer captured at the terminal halt)

The loader reads the (bad) save, computes a garbage pointer/length, and derails: `SP` collapses from
a sane `0xF202` to `0x4000` (an `LD SP,nn` executed on data), then a `RET` pops a garbage address
(`0xFD3D`) off the trashed stack → NOP-sled through RAM → `CALL M →0xFFFF →DI →0x0000 HALT`
(`final_pc=0x0001`, IFF1=0, terminal). This is the classic "invasion into the low vector page" the
owner described.

## Device-fidelity audit results (4 parallel specialists, grounded in datasheets + openMSX/fMSX + web)

| Device | Verdict | Key finding |
|--------|---------|-------------|
| **WD2793 FDC** | Faithful / exonerated | Write path byte-faithful; all divergences deliberate & *safer* than hardware. Cannot corrupt. |
| **V9958 VDP** | ⚠ **KEY GAP** | Command-engine CE duration is a **coarse, atomic per-mode approximation** (not slot-accurate); **LMCM/LMMC/HMMC transfer-ready (TR) is never paced**. Command/transfer-heavy code consumes a *different* cycle count than hardware → shifts which instruction runs at the frame boundary → cumulative state divergence. |
| **Z80A CPU** | Excellent, 1 real bug | Maskable-interrupt acceptance billed **14T instead of 13T** (IM2 20 vs 19) — the S1985 M1 wait was wrongly applied to the interrupt-acknowledge cycle. Fires on **every** VBlank/line interrupt → accumulating phase drift. (Also: NMI didn't tick R; `EI;EI` edge case.) |
| **RTC/PSG/mapper/bus** | Very faithful, 1 real bug | PSG Tone/Noise counter **zeroed instead of clamped to period-1** on a period-shrink write (phase loss vs openMSX). RTC Block-2 "valid" marker over zero-config is a lower-priority nuance to verify. |

## Fixes: applied + verified

> **Update (2026-07-14):** the two candidate device-timing fixes below were subsequently
> APPLIED and verified this session, recorded as **DEC-0074** and governed by the
> **reference-precedence directive (DEC-0073)**. The openMSX cycle A/B that this section gated the
> CPU change on was run and **confirmed 13T** (tier-2 confirmation of the tier-1 signal-level
> reasoning), so the `tick_refresh_only()` split was applied and the 4 oracles updated to
> **IM1 = 13T / IM2 = 19T / NMI = 11T** (`references/fact-sheets/Zilog Z80A CPU.md` §5 corrected).
> The V9958 command-engine timing under "Remaining work" was also addressed in part (LINE
> 88→112, per-line end-of-line overhead, and LMCM/LMMC/HMMC TR per-unit pacing); only the full
> access-slot CONTENTION model (154/88/31 slots + CPU↔command stealing) remains DEFERRED (HIGH
> risk + the license-sensitive ~340-entry tables, the standing C1/D4 ban). Full suite 228/228
> including the ZEXALL/ZEXDOC sweep after the CPU work. **None of this changes the verdict above:
> these are independent correctness wins, NOT the YS II crash cause (which is inherent to the
> game/disk).** The original wording is retained below for the investigation record.

1. **PSG Tone/Noise counter phase** (`src/devices/audio/psg_ym2149.cpp`) — **APPLIED, tests green.**
   `set_period` now clamps the live up-counter to `period-1` instead of resetting to 0 (matches
   openMSX `Generator::setPeriod` *and* our own `Envelope::set_period` — no reference disagreement).
   Audio-fidelity only; no game-logic impact.

2. **CPU interrupt/NMI acknowledge timing (IM1 14T→13T, IM2 20→19, +NMI R-tick)** — **CANDIDATE,
   REVERTED, not applied.** Prototyped this session but reverted because it is a genuine *reference
   disagreement* without definitive real-hardware proof: our `references/fact-sheets` guessed 14T
   (S1985 +1 M1 wait applied to the interrupt-acknowledge M1) *but explicitly deferred to openMSX*;
   openMSX bills 13T (no wait on the ack M1). 4 deterministic tests
   (`interrupt_ack`, `cpu_parity`, `cpu_bootstrap_im1`, `m23_halt_r`) hardcode the 14T oracle. Per the
   guardrails a behavior-affecting timing change of this breadth requires an **openMSX cycle A/B**
   before it (and the 4 test oracles) are changed — which cannot be run cleanly here (any timing
   change desyncs the cycle-stamped YS II recording). **Recommendation: run the openMSX A/B; if it
   confirms 13T, re-apply the `tick_refresh_only()` split (ack ticks R but not `m1_cycle_count_`) and
   update the 4 oracles to 13T/19T.** This is the single most systematic timing drift (fires on every
   VBlank/line interrupt) and the top follow-up.

## Remaining work (the big one)

The **VDP command-engine slot-accurate CE/TR model** (the M44 Phase-2b item) is the largest timing
gap and the strongest suspect for the YS II state divergence. Our CE duration is a per-mode-scaled
underestimate and TR transfers are unpaced; the fix is the openMSX access-slot model
(154/88/31 slots-per-line by sprite state, per-line overhead, CPU↔command slot contention). This is
a substantial change and should be A/B-verified against openMSX.

## How to verify the verdict / next steps

- **Owner live re-test:** after the timing fixes, replay the *live* 8-slot YS II save (a fresh
  session, NOT the old recording — any timing change desyncs the cycle-stamped recording). If the
  crash is gone, the timing drift was the cause.
- **Fresh openMSX 8-slot save** (staged in `~/ys2clean/`, genuine disks, blank SRAM): if openMSX
  (accurate timing) never corrupts a fresh 8-slot save, that confirms the fault was our timing.
- **openMSX cycle A/B** of the interrupt-timing and CE/TR changes (guardrail for behavior-affecting
  timing) before release.

## Reproduction assets (scratchpad + debug/)

- `debug/ys2_8slot.script` — the deterministic recording (replays to `02f54bae` + crash).
- Baseline disks: `recstart-d1.dsk` (cc099603), `recstart-d2.dsk` (pristine ef7e5811).
- Diagnostic env knobs (temporary, to be reverted before commit): `SONY_MSX_FDCWLOG` (FDC write log),
  `SONY_MSX_TRACE_RING=<N>` (bounded CPU trace tail), `SONY_MSX_HALT_BREAK=1` (stop at terminal halt),
  `--vdp-cmd-bias <N>` (CE busy-window bias — this one is a pre-existing committed flag).
