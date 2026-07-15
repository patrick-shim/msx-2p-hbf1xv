# M28 Planner Package — Release Candidate: Backlog Closure Sweep + Full-Project Health Audit

- Milestone ID: M28
- Title: Release Candidate — Backlog Closure Sweep + Full-Project Health Audit
- Spec Owner: MSX Planner Agent
- Status: PLANNING ONLY — no production code in this package
- Request: REQ-M28-001 (`agent-protocol/channels/requests.md:1411-1441`)
- Authority: DEC-0005 (every planner package restates all backlog rows); the M15 decomposition
  precedent (`docs/m15-planner-package.md`); the M23 conservative-scoping precedent
  (`docs/m23-planner-package.md`); standing project memories `feedback_license_sensitive_scope.md`
  ("zero license-sensitive future work") and `feedback_slow_test_cadence.md` (slow-sweep gating rule).

> Grounding rule followed throughout: every behavior/timing claim below cites a concrete
> `references/openmsx-21.0/...` or `references/fact-sheets/...` path, independently read this
> cycle (not assumed from memory or from prior planner packages' summaries). openMSX source is the
> BEHAVIOR reference only (GPL) — never copied into `src/`.

---

## 1. Scope and Assumptions

### 1.1 Two combined objectives (per REQ-M28-001)

**(A) Backlog closure.** Consult the full 34-row `agent-protocol/state/deferred-backlog.md`.
23 rows are already `DONE`; 11 are `OPEN`/`IN-PROGRESS`: **C1/D4** (twin references to the same VDP
access-slot/command-timing remainder), **C5**, **C10**, **E1**, **E2**, **F1**, **F2**, **G1**,
**G2**, **G3**, **G4**. Every open row gets an explicit IN-M28 or DEFERRED recommendation with
justification (§2.1), and — mirroring the M15 precedent, which explicitly warned "'all pending
items' is too large for one deterministic milestone" — a deterministic decomposition into M28 plus
named follow-on milestones (§3).

**(B) Release-candidate health audit.** Independent of which backlog rows are IN, define and scope
a concrete audit deliverable: (1) component-by-component source-code health across every `src/`
subsystem; (2) integration-flow coherence (machine composition, bus/slot wiring, CLI, asset
loading — confirmed end-to-end); (3) feature-completeness cross-check against the Target Machine
Specification in `CLAUDE.md`; (4) a full, current accuracy pass over all 34 backlog rows. This work
happens regardless of the backlog IN/DEFERRED split (§2.4, §3.1).

### 1.2 In scope for M28 (the IN set — justified in full in §2)

1. **The release-candidate health audit** (`docs/m28-release-candidate-audit.md`) — mandatory,
   always in scope per REQ-M28-001(B).
2. **E2 — YM2413 register-write timing constraint** (≥12 master-cycle address-write / ≥84
   master-cycle data-write minimum spacing).
3. **C5 — full-boot / automatic disk-ROM boot-handshake trigger investigation** (best-effort,
   honestly-scoped; equipped with M27's `--disk`/`--input-script` tooling and the real
   `disks/msxdos22.dsk`/`msxdos23.dsk`/`msxdos24/` assets).

### 1.3 Explicitly out of scope for M28 (deferred, named follow-ons — §3)

**C1/D4** (ruled UNBUILDABLE-WITHOUT-FABRICATION this cycle, §2.2 — stays OPEN, no follow-on
milestone assigned pending a genuine independent source), **C10**, **E1**, **F1**, **F2**, **G1**
(each assigned a named follow-on milestone, §3), and **G2/G3/G4** (re-affirmed indefinite/on-demand,
matching their own original backlog scoping — no change this cycle).

### 1.4 Assumptions (each with a verification action)

- **A-M28-1 (the 11-row OPEN/IN-PROGRESS count in REQ-M28-001 is accurate).** Independently
  re-read `agent-protocol/state/deferred-backlog.md` in full this cycle (491 lines, confirmed via
  `wc -l` at coordinator review time — corrects this package's original "843 lines" self-report,
  a bookkeeping error in the read-tracking, not in the row-by-row analysis below, which the
  coordinator independently re-verified against the live file) and confirmed: 9 Section-A rows all
  `DONE`; Section B has B1-B9 `DONE`, C1 `IN-PROGRESS`,
  C2 `DONE`, C3 `DONE`, C4 `DONE`, C5 `IN-PROGRESS`, C6 `DONE`, C7 `DONE`, C8 `DONE`, C9 `DONE`,
  C10 `OPEN`; Section C (D1-D7) all `DONE`; Section D has E1 `OPEN`, E2 `OPEN`; Section E has F1
  `OPEN`, F2 `OPEN`; Section F has G1-G4 all `OPEN`. Total OPEN/IN-PROGRESS = 11 (C1/D4 counted
  once as the REQ instructs). *Verify:* re-run this count at implementation time in case a prior
  cycle's status changed between planning and execution (none is scheduled to).
- **A-M28-2 (the standing license-sensitive-scope directive is unconditional, not merely a
  preference).** `feedback_slow_test_cadence.md`'s companion memory,
  `feedback_license_sensitive_scope.md` (cited verbatim in REQ-M28-001 and in
  `agent-protocol/state/current-phase.md:20-23`), states: "never reproduce openMSX's own large
  numeric tables/arrays under `src/`, even 'independently re-derived'." This is treated as a hard
  constraint on every row assessed below, not merely on C1/D4 — §2.3 applies the identical scrutiny
  to E1, E2, C10, F1, F2, and G1. *Verify:* no numeric array >20 entries appears under `src/` as a
  result of this milestone's IN work (mirrors M23's own R-M23-4 self-check).
- **A-M28-3 (E2 and C5 both touch existing files under `src/devices/`, which mechanically
  triggers the slow-sweep rule).** `feedback_slow_test_cadence.md`'s rule: run
  `hbf1xv_m24_zexall_system_test` only if `git diff <baseline-tag> --name-only -- src/devices/
  src/peripherals/ src/core/` shows a touch to an EXISTING file, or any touch to
  `src/devices/cpu/`/`src/core/`. E2 edits the EXISTING `src/devices/audio/ym2413_opll.{h,cpp}`
  (shipped M17); C5's investigation is expected to touch EXISTING files under `src/devices/fdc/`
  and/or `src/machine/` (see §2.1 C5 row). Both trigger the rule mechanically. **This means the
  slow sweep MUST run at least once before M28 closes**, despite the human's "we can skip the
  slow-sweep unless it's really needed" framing in the REQ's own preamble — the framing is an
  aspiration, the memory file's git-diff test is the actual mechanical gate, and it fires here.
  *Verify:* the developer runs the literal `git diff` command against the `v1.0.27` baseline tag
  at the point E2/C5 changes land, confirms the touch, and runs the full (not `-LE`-filtered)
  `ctest` at least once before requesting QA.
- **A-M28-4 (the health audit is a developer-executed deliverable, not something the planner can
  pre-fill from static reading alone).** Confirming "no dangling stubs," "confirmed end-to-end,"
  and "every backlog row's citations still accurate" requires running the actual built executables,
  greping the live tree, and re-reading cited `references/...`/`src/...` line numbers at
  implementation time — work this package scopes and templates (§2.4, §3.1) but does not fabricate
  results for. *Verify:* the implementation report cites concrete command output (grep counts, test
  run output, executable invocations), not restated planning-time text.
- **A-M28-5 (two dead-code candidates already surfaced by a light planning-time check, not
  invented).** `rg "device_placeholder|peripheral_placeholder|DevicePlaceholder|PeripheralPlaceholder"`
  across the repo returns exactly 3 hits: `README.md`, `src/devices/device_placeholder.h`, and
  `src/peripherals/peripheral_placeholder.h` — i.e., these two M0-era scaffold files are referenced
  from nowhere else (no `tests/`, no `src/machine/hbf1xv_machine.cpp` inclusion, no CMake target
  consuming them beyond glob-style enumeration). This is exactly the kind of finding audit
  criterion (1) exists to surface, seeded here so the developer has a concrete starting example.
  *Verify:* the developer re-runs the identical `rg` command at implementation time and decides,
  in the audit report, whether to remove these two files or document them as intentionally-inert
  scaffolding (a small, safe, audit-triggered fix — see §2.4's fix-scope rule).

---

## 2. Spec Summary

### 2.1 Full 34-row backlog disposition (every row; DEC-0005)

**Section A (human-directive-tracked) — all DONE, re-affirmed unchanged, none touched by M28:**
B1 (M15), B2 (M15), B3 (M17), B4 (M20), B5 (M18), B6 (M20), B7 (M19), B8 (M16), B9 (M14).

**Section B (other known deferrals):**

| # | Item | Current status | M28 disposition | Justification |
|---|------|-----------------|------------------|----------------|
| C1/D4 | VDP access-slot/command timing remainder | IN-PROGRESS (M23 partial) | **DEFERRED — ruled UNBUILDABLE-WITHOUT-FABRICATION this cycle; stays OPEN, carried forward, no milestone number assigned** | §2.2. No independent, non-GPL source for the ~340-entry slot tables exists anywhere in this repo. |
| C2 | Z80 HALT-R | DONE (M23) | re-affirmed unchanged | M28 touches no CPU-core file. |
| C3 | ZEXALL/ZEXDOC sweep | DONE (M24) | re-affirmed unchanged | Not touched. |
| C4 | S1985 backup-RAM persistence | DONE (M15) | re-affirmed unchanged | Not touched. |
| **C5** | Full boot / auto-disk-boot trigger | IN-PROGRESS (M16 partial) | **IN-M28** | §2.1a below. Now equipped with real disk assets + M27 scripted-input tooling; investigative, not license-blocked. |
| C6 | Peripherals (keyboard/joystick) | DONE (M15) | re-affirmed unchanged | Not touched. |
| C7 | Printer + cassette contract | DONE (M18) | re-affirmed unchanged | Not touched (F1/F2 are the DEPTH rows, handled below). |
| C8 | Sony speed/PAUSE + Ren-Sha | DONE (M25) | re-affirmed unchanged | Not touched. |
| C9 | SDL3 frontend | DONE (M26) | re-affirmed unchanged | Not touched. |
| **C10** | FDC flux-level/DMK fidelity | OPEN | **DEFERRED → M31** (§2.3c, §3) | Not license-blocked (DMK is a public, non-openMSX-exclusive file format), but a large, self-contained image-format subsystem better sequenced after C5's investigation settles the FDC's boot-time behavior. |

**Section C (M14 VDP-depth deferrals):** D1 (M21), D2 (M22), D3 (M22), D5 (M21), D6 (M21), D7
(M22) all DONE, re-affirmed unchanged. D4 is C1's twin — see above.

**Section D (M17 YM2413 depth deferrals):**

| # | Item | M28 disposition | Justification |
|---|------|------------------|----------------|
| **E1** | YM2413 FM-synthesis DSP/audio depth | **DEFERRED → M30** (§2.3a, §3) | Not wholly license-blocked (log-sin/exp tables and the patch table are independently, formulaically groundable — §2.3a), but the full cycle-accurate 18-slot pipeline is individually milestone-sized (a real-time DSP engine) and the attack/EG-rate tables carry a genuine, specifically-located transcription risk (§2.3a) that a future milestone must scope around, not attempt wholesale. |
| **E2** | YM2413 register-write timing constraint | **IN-M28** | §2.1b below. Small, independently grounded (Yamaha Application Manual figures + andete measurements, both cited in the existing fact-sheet), low regression risk, mirrors the C2/M23 "small, well-scoped, hardware-grounded" precedent. |

**Section E (M18 printer/cassette depth deferrals):**

| # | Item | M28 disposition | Justification |
|---|------|------------------|----------------|
| **F1** | Cassette tape image-format/signal fidelity | **DEFERRED → M32** (§2.3d, §3) | Individually milestone-sized (a real audio-codec integration); also carries a genuine grounding-precision risk (openMSX's own empirically-tuned constants, §2.3d) that a future milestone must resolve via independent MSX-kernel documentation, not by copying openMSX's tuned values. |
| **F2** | Printer image/ESC-sequence rendering depth | **DEFERRED → M33** (§2.3e) | Not primarily a size problem — a genuine ASSET-PROVENANCE blocker: authentic dot-matrix glyph rendering needs a real printer font, and no independently-sourced font asset exists in this repo's `bios/`/`roms/`/`references/` (§2.3e). Fabricating one would violate the guardrails' asset-provenance rule. |

**Section F (M19 cartridge-mapper depth deferrals):**

| # | Item | M28 disposition | Justification |
|---|------|------------------|----------------|
| **G1** | KonamiSCC mapper + SCC/SCC+ wavetable chip | **DEFERRED → M29** (§2.3b, §3) | The LEAST risky of the deferred big items (mapper half reuses the existing M19 `RomKonami`-family pattern almost verbatim; SCC audio half is independently groundable from public forum-sourced hardware measurements, §2.3b) — sequenced first among the follow-ons, but still individually milestone-sized (a genuinely new audio device) and needs a dedicated `references/fact-sheets/` entry first, matching this project's own established practice (every other audio/video/FDC device got one before implementation). |
| **G2** | ROM-database/SHA1 auto-mappertype-detection | **DEFERRED — indefinite, on-demand** | Unchanged from its own original scoping ("only if a real need arises"); M28 introduces no new reason to schedule it. |
| **G3** | Full runtime cartridge hot-plug | **DEFERRED — indefinite, on-demand** | Unchanged; same reasoning. |
| **G4** | Long tail of ~80 other mapper types | **DEFERRED — indefinite, on-demand** | Unchanged; explicitly scoped in its own row as "only if a specific real cartridge is wanted." |

No row is silently dropped; every OPEN/IN-PROGRESS row has an explicit disposition and (where
DEFERRED) a named or explicitly-indefinite owner.

### 2.1a — C5 spec: full-boot / auto-disk-boot-trigger investigation (IN-M28)

**Current state (re-read this cycle, not assumed).** `agent-protocol/state/deferred-backlog.md`'s
C5 row: real-boot max PC reaches `0x7D6F` over 400,000 instructions (M16), architecturally
identical to openMSX over a 3000-instruction real-boot window, but "the automatic disk-ROM boot
handshake (DSKCHG → Restore → Read Sector LBA 0) is genuinely never observed within an unattended,
keyboard-less cold boot (confirmed up to a 20,000,000-instruction diagnostic run)." M27 added two
tools this investigation did not have before: a headless `--debug-session --disk` mode
(`src/main.cpp`) and a deterministic scripted-input mechanism (`src/machine/input_script.{h,cpp}`,
`src/peripherals/msx_key_names.{h,cpp}`, `--input-script`) — `docs/m27-planner-package.md`'s own
dispatch named this useful for a future C5 attempt. The human separately added real MSX-DOS system
disks (`disks/msxdos22.dsk`, `disks/msxdos23.dsk`, `disks/msxdos24/`, DEC-0021).

**Grounding for the investigation itself.** The FDC fact-sheet
(`references/fact-sheets/FDC for Sony HB-F1XV.md` §2) documents the real Disk BIOS entry points
(`0x4010` DSKIO, `0x4013` DSKCHG, `0x4016` GETDPB, `0x401C` DSKFMT) and states the standard MSX
disk driver is **polled, not interrupt-driven** (§6: "a tight polling loop... Interrupts are
disabled (DI) around the transfer"). This project already has the tools needed to observe *where*
that polling loop is or is not being entered on a real, keyboard-less cold boot: the M10 CPU
trace-export facility (`src/devices/cpu/z80a_trace.h`, wired since M10) and the M27
`--debug-session --dump-state --trace-cpu` combination. The investigation technique is: drive a
long, deterministic boot (with and without a scripted keypress sequence via `--input-script`) and
compare the resulting PC trace against a real openMSX boot to the SAME disk image, using the
established live-Tcl PC/register readback technique (already used and QA-verified in
`docs/m11-parity-trace-diff.md` through `docs/m24-parity-trace-diff.md`) to find the FIRST point of
divergence — i.e., empirically locate where this project's BIOS-driven boot takes a different
branch than real openMSX's, rather than disassembling GPL disk-ROM/BIOS source.

**Honest, non-forced acceptance (mirrors the D4/C8 "not force-closed" precedent).** C5's M28
outcome is judged honest, not by whether it reaches a DOS prompt:

- **Outcome (a) — full close:** a real auto-disk-boot sequence is observed (DSKCHG→Restore→Read
  Sector), the machine reaches a genuine MSX-DOS/BASIC prompt state, and a bounded A/B window
  around the boot-decision point shows architectural parity vs. openMSX using the same disk image.
  C5 → `DONE (M28)`.
- **Outcome (b) — investigation advances, does not close:** a concrete, evidenced finding is
  produced (e.g., "the real BIOS's boot-device scan requires condition X observed at PC Y, which
  this project's default idle-input cold boot never supplies, confirmed by comparing traces at the
  first divergence point Z") and recorded with the same rigor as M16's own honest residual
  disclosure. C5 stays `IN-PROGRESS (M28 partial)` with the new finding appended — not force-closed,
  not silently re-worded to sound closed.

Either outcome is an acceptable, honest M28 result per this project's established discipline
(mirrors D4/M23's "not force-closed" and C8/M25's "honestly BLOCKED, does not gate closure"
precedents). **Do not fabricate a "reached DOS prompt" claim without a genuine, reproduced trace.**

**Placement / regression risk:** any production change (if the investigation finds and fixes a
genuine defect, as opposed to merely observing behavior) lands under `src/devices/fdc/` and/or
`src/machine/hbf1xv_machine.cpp` — both already-shipped, QA-signed surfaces (M16, M11-M27
respectively). Per A-M28-3, any such touch is an EXISTING-file touch and triggers the slow-sweep
rule. If the investigation makes NO production change (pure observation, outcome (b) above), no
slow-sweep trigger applies from C5 alone — but E2 (below) triggers it regardless.

### 2.1b — E2 spec: YM2413 register-write timing constraint (IN-M28)

**Grounding (independently re-read this cycle).**
`references/fact-sheets/Yamaha YM2413 FM Chip.md` §8 ("Timing"): "after an **address write**, wait
**≥12 master cycles** (~3.36 µs); after a **data write**, wait **≥84 master cycles** (~23.52 µs)
before the next write... Violating the 84-cycle rule causes dropped/wrong register writes on real
hardware." The same section explicitly discloses: "openMSX (Nuked-OPLL core) currently has the
too-fast-access-timing emulation *disabled*." This is independently-sourced from the Yamaha
Application Manual and andete's hardware measurements (fact-sheet §2 "Key Findings"), not
transcribed from openMSX's own array data — there is no numeric TABLE risk here at all, only two
small scalar constants (12 and 84 master cycles), already cited in this project's own vetted
fact-sheet.

**Design.** Extend the existing `src/devices/audio/ym2413_opll.{h,cpp}` (shipped M17, QA-signed
`docs/m17-qa-signoff.md`) with a last-write-cycle tracker for each of the two ports (`#7C`
address-latch, `#7D` data), consulted read-only against `elapsed_cycles()` — mirroring the exact
M15 X4 pattern ("device time derives from `elapsed_cycles()` READ-ONLY; the CPU-timing formula in
`step_cpu_instruction` is NOT touched," `docs/m15-planner-package.md` §5 X4) so this cannot perturb
the M9/M12/M23 zero-tolerance CPU-timing oracles. A write arriving before the minimum spacing is
dropped (per the fact-sheet's documented real-hardware behavior) rather than silently accepted.

**Regression-critical pre-check (a real risk, not hypothetical — the developer MUST perform this
before enabling the gate).** The existing M17 device and its tests
(`tests/unit/devices/audio/ym2413_opll_unit_test.cpp`,
`tests/integration/...ym2413...integration_test.cpp`, per the M17 implementation report) may
contain back-to-back `OUT (#7C),A` / `OUT (#7D),A` write sequences with zero spacer instructions —
exactly the class of risk M23 identified for VDP access-timing gating (`docs/m23-planner-package.md`
§2.3, "the M21/M22 system-test CPU-program fixtures already exercise the exact scenario that would
make naive gating dangerous"). **Required action:** the developer must `rg` the existing YM2413 test
files for consecutive port-`#7C`/`#7D` writes before implementation and, if any exist with
insufficient spacing, either (a) insert deterministic spacer instructions into those specific test
programs (a disclosed, justified test-only edit, not a production workaround) so they continue to
exercise their ORIGINAL intent under the new, hardware-accurate constraint, or (b) if inserting
spacers would defeat the test's own purpose, default the gate to **off** (matching openMSX's own
documented default-disabled stance, fact-sheet §8) and expose it as an explicit, unit-tested,
disabled-by-default option — mirroring the fact-sheet's own Recommendation 3 ("Model timing
constraints as an option: enforce 12/84-cycle write waits (flag-gated, since openMSX disables
this)"). **Either resolution is acceptable; silently changing existing test behavior without this
check is not.**

**A/B evidence plan.** Because openMSX itself disables this emulation by default (fact-sheet §8),
a live A/B comparison of "does the write get dropped" would show a DIVERGENCE BY DESIGN, not a
defect — this is explicitly, honestly disclosed in the parity report as **N/A / not a comparable
claim**, mirroring the E2 fact-sheet's own framing and the project's established "openMSX disables
X" handling (same shape as D4/M23's own honest BLOCKED disposition, `docs/m23-parity-trace-diff.md`
§3.4). The GATE MECHANISM itself (does a write land at the right cycle boundary, does a too-fast
write get dropped) is independently unit-testable against the cited 12/84-master-cycle constants
without needing openMSX at all — this is the actual acceptance evidence, not a live A/B diff.

**Tests:** unit — address-write acceptance at exactly 12/at 11 (dropped) master cycles; data-write
acceptance at exactly 84/at 83 (dropped) master cycles; the gate defaults to the value the
regression pre-check determines (on if safe, off-with-documented-toggle if not); determinism (two
identical runs, byte-identical register state). Integration — CPU `OUT (#7C)`/`OUT (#7D)` sequences
over the M11 bus at varying spacing, confirming register writes land/drop per the constants.

### 2.2 — C1/D4 ruling: UNBUILDABLE-WITHOUT-FABRICATION this cycle (hard constraint applied)

**Re-verified independently this cycle, not assumed from the M23 package.**
`references/fact-sheets/Yamaha V9958 VDP.md` §8 ("Timing model...") gives per-command
per-pixel/per-line VDP-cycle COSTS (e.g. `HMMV: 48 W, 56/line`) and access-SLOT COUNTS (154
screen-off / 88 sprites-off / 31 sprites-on) — but explicitly does NOT give the ~340 individual
per-slot VDP-cycle-offset table entries themselves; the fact-sheet's own §7 gives only the coarse,
independently-documented "~29 Z80-cycle safe access" software convention (already modeled,
non-numerically-table-shaped, in `src/devices/video/vdp_access_timing.h` since M23). No other file
under `references/fact-sheets/` addresses VDP access-slot timing. The ONLY place the actual
per-slot numeric tables exist anywhere in this repository's reach is
`references/openmsx-21.0/src/video/VDPAccessSlots.cc:14-141` — GPL source, off-limits per
`feedback_license_sensitive_scope.md` and the guardrails' license-isolation rule, and explicitly,
already documented by M23 as containing "roughly 340 individual `int16_t` entries... a large,
exacting, multi-file undertaking" (`docs/m23-planner-package.md` §2.1 item 2).

**Ruling (per REQ-M28-001's explicit instruction):** the remaining C1/D4 depth — the five items
named in `docs/m23-planner-package.md` §2.4 (full per-mode slot tables; CPU-steals-command-engine-
slot interaction; command engine's real per-pixel/per-line cost gating; exact sub-frame IRQ raster
position; actual CPU-execution gating) — is ruled **UNBUILDABLE-WITHOUT-FABRICATION this cycle**.
No genuinely independent, non-GPL source for the exact ~340-entry numeric tables exists in this
repository as of this planning cycle (Yamaha datasheet text, a real-hardware logic-analyzer
measurement set, or a dedicated `references/fact-sheets/` entry — none present). This is NOT a
"too large" judgment (unlike E1/F1/G1 below) — it is a genuine **sourcing** blocker. C1/D4 stays
**OPEN / carried forward**, with this reasoning recorded here and (at implementation/closure time)
in `deferred-backlog.md`, and is **explicitly not scheduled to any named follow-on milestone number**
— unlike E1/C10/F1/F2/G1, which get named owners because they merely need MORE WORK, C1/D4 needs a
**new fact** (an independent source) before any future milestone could responsibly schedule it. If
a legitimate independent source (e.g. a Yamaha V9938/V9958 technical datasheet excerpt, or original
logic-analyzer measurements) becomes available in `references/fact-sheets/` in the future, a
follow-on milestone can be named at that time — not before.

### 2.3 — Grounding-risk scrutiny applied to every other candidate row (per REQ-M28-001's explicit ask)

REQ-M28-001 explicitly instructs: "check E1/E2's `YM2413NukeYKT.{hh,cc}` citation, C10's
`DMKDiskImage.*` citation, and F1's cassette-format citations for the same risk before committing
to IN." Each was independently read this cycle (not assumed):

**(a) E1 — YM2413 DSP depth.** Read `references/openmsx-21.0/src/sound/YM2413NukeYKT.cc:56-114`
this cycle. Finding: the log-sin (256-entry) and exp (256-entry) operator tables are **computed by
a `constexpr` lambda from closed-form log2/sin/exp2 formulas** (`cstd::round(-cstd::log2<8,3>(...))`
etc.), NOT hardcoded literal arrays — these ARE independently, formulaically re-derivable without
reading or transcribing openMSX's numbers at all (the formula itself is the classic Yamaha
logarithmic-operator construction, already described structurally in
`references/fact-sheets/Yamaha YM2413 FM Chip.md` §4). The 15-melody + 6-rhythm patch table
(`YM2413NukeYKT.cc:73-98`) is a literal array in openMSX's source, but this project's OWN, already
existing device (`src/devices/audio/ym2413_opll.{h,cpp}`, shipped M17, `docs/m17-qa-signoff.md`
PASS) already implements a 15+3-entry ROM patch table grounded in the fact-sheet's own
independently-published "community-standard" table (§4, "originally by-ear, later confirmed via
the FHB013 die shot / NukeYKT debug-mode dump" — a citation to a public, cross-source-corroborated
community table, not a copy of openMSX's specific encoding) — this precedent already stands and is
not re-litigated here. HOWEVER: `YM2413NukeYKT.cc:106-114` shows the EG **attack**/**release** rate
tables are pulled from a separate, large, **generated `#include "YM2413NukeYktTables.ii"`** file
(confirmed present in the vendored tree at
`references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii`) — openMSX's own comment there says
these tables "could all be initialized via some constexpr functions... the calculation isn't
difficult, but it's a bit long" and are pre-generated instead. The fact-sheet (§5) gives a genuine,
independently-usable **formula** for the decay/release portion ("`cycles = (rate<60) ?
(1<<(14-(rate/4)))·s[rate&3] : 63`, `s = {127,102,85,73}`"), reconstructable exactly — but does
**not** give an equivalent formula or table for the **attack**-curve portion ("Attack times are
separately tabulated" — the fact-sheet discloses this data exists but does not reproduce it). **This
is a real, specifically-located transcription risk**: a full cycle-accurate DSP implementation
following the recommended Nuked-OPLL reference would need attack-curve data this repo does not
independently possess. Conclusion: E1 is **not** as absolutely blocked as C1/D4 (most of its
structure — logsin/exp tables, patch table, decay/release-rate formula — is independently
groundable), but the FULL depth is genuinely large (a real-time 18-slot TDM DSP pipeline,
individually milestone-sized per REQ-M28-001's own framing) AND carries this one concretely-located
sourcing gap. **DEFERRED → M30**, with an explicit scoping instruction for that future planner:
build only the formulaically-derivable subset (logsin/exp tables computed via formula, the
already-precedented patch table, the decay/release-rate mechanism per the fact-sheet's own formula)
and name the attack-curve depth as an explicitly carried-forward remainder — mirroring the C1/D4
"safe foundation vs. named remainder" pattern, not attempted wholesale.

**(b) G1 — KonamiSCC + SCC/SCC+ chip.** Read `references/openmsx-21.0/src/sound/SCC.cc:1-80` and
`references/openmsx-21.0/src/memory/RomKonamiSCC.cc:1-60` this cycle. Finding: the mapper half
(`RomKonamiSCC`) is a small, straightforward bank-switch device (four 0x800-aligned bank
registers) — this project already has the identical pattern shipped and QA-signed as `RomKonami`
(M19, `src/devices/cartridge/cartridge_konami_rom.{h,cpp}`), so extending it is low-risk,
incremental work, not a new algorithm. The SCC audio half's behavioral grounding in openMSX's
source (`SCC.cc:1-80`) is **not** openMSX's own proprietary derivation — it is a block of **verbatim
quotes from public third-party technical forum posts** (Jon De Schrijder's 2003 oscilloscope
amplitude measurements, Manuel Pazos's 2003 mode-register write-up, NYYRIKKI's 2005 MRC forum post)
describing the real chip's `AmpOut = 640 + ΣAmpX` mixing formula and mode-register bit semantics —
i.e., the SAME category of independently-citable, community-reverse-engineered hardware fact this
project already treats as legitimate grounding for the YM2413 fact-sheet's own "andete" citations.
There is **no large numeric table** involved (SCC waveform data is supplied at runtime by
cartridge-software register writes, not a fixed ROM table, unlike YM2413's patches) — the license
risk here is genuinely low. The real gap is **process**, not sourcing: unlike every other
major device this project has built (VDP, YM2413, FDC, S1985), there is currently no dedicated
`references/fact-sheets/` entry for the SCC — this project's own established practice (every
audio/video/FDC device got one before implementation) should be followed rather than having a
future developer re-derive grounding directly from openMSX source comments each time. **DEFERRED →
M29** (least risky of the deferred items, sequenced first among the follow-ons; the M29 planner
should first commission/verify a dedicated SCC fact-sheet grounded in the same class of independent,
public forum-sourced measurements cited above, before implementation).

**(c) C10 — FDC flux-level/DMK fidelity.** Read
`references/openmsx-21.0/src/fdc/DMKDiskImage.hh:1-48` this cycle. Finding: **zero numeric tables**
of any kind — the DMK format is a documented, independent, third-party file-container format (the
header itself cites `http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/`, unrelated to
openMSX and to MSX specifically — DMK originates from the TRS-80 emulation community). No license
risk from DMK itself. Separately, this project's OWN `references/fact-sheets/FDC for Sony
HB-F1XV.md` §5 already gives a genuinely independent, fully-cited track-layout specification (Sony
HB-720 interface documentation + the grauw "Low-level disk storage" article — gap/sync/IDAM/ID/DAM
byte sequences, the `0xF5/0xF6/0xF7` special-encoding convention) sufficient to construct real
MFM track images without reading openMSX's FDC source at all. **C10 is not license-blocked** — it
is DEFERRED purely on **size and sequencing** grounds: a new disk-image-format subsystem
(`DmkDiskImage`-equivalent, weak-bit/copy-protection modeling) is substantial, self-contained work,
and is better sequenced *after* C5's investigation concludes (since C5 may surface findings about
the FDC's boot-time behavior that a flux-fidelity milestone would want to build on, not
contradict). **DEFERRED → M31.**

**(d) F1 — cassette tape image-format/signal fidelity.** Read
`references/openmsx-21.0/src/cassette/CasImage.cc:52-73` this cycle. Finding: the `.CAS` file
format's sync-marker constants (`CAS_HEADER = {0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74}`,
`ASCII_HEADER`/`BINARY_HEADER`/`BASIC_HEADER` = repeated `0xEA`/`0xD0`/`0xD3` bytes) are the
standard, publicly-documented MSX cassette file-format markers (small constants widely documented
across independent MSX technical references, not an openMSX invention) — low risk. **However**, the
FSK modulation baud rate openMSX actually encodes with (`BAUDRATE = 3744`) is accompanied by
openMSX's own comment disclosing it as an **empirically-tuned, not-fully-understood value** ("a
higher baudrate doesn't work anymore, but it is unclear why... 3744 seems to work for those as well
(we don't understand why yet)," `CasImage.cc:54-60`) — i.e., openMSX's own maintainers arrived at
this exact number through trial-and-error against specific real hardware/software combinations, not
from a published spec. Reusing this SPECIFIC tuned value would be adopting openMSX's own empirical
work product, not an independently-sourced hardware fact — a real, if narrower than C1/D4,
grounding-precision risk. The standard MSX kernel nominal cassette baud rates (1200/2400 baud) ARE
independently, publicly documented (MSX Technical Handbook / "Red Book" convention, outside this
repo's current `references/`) and would need to be the basis for any implementation, not openMSX's
3744 tuning constant. Combined with F1's REQ-M28-001-acknowledged large scope ("a real audio-codec
integration... FSK/UDF bit modulation"), **DEFERRED → M32**, with an explicit instruction for that
future planner to ground baud-rate/modulation constants in an independent MSX-kernel/BIOS
technical reference (to be added to `references/fact-sheets/` first) rather than openMSX's own
tuned value.

**(e) F2 — printer image/ESC-sequence rendering depth.** Not covered by REQ-M28-001's explicit
citation list, but scrutinized under the same standard per its own instruction ("apply the same
independent-source scrutiny... before recommending any of them IN"). Finding: F2's core
grounding risk is **not** a numeric-table/license risk at all — ESC/P-style dot-matrix printer
control codes are an old, generic, public standard (not openMSX- or Sony-specific IP). The REAL
risk is **asset provenance**: authentic "dot-matrix font rendering" requires actual glyph BITMAP
data for a real printer's built-in character set, and no such font asset exists anywhere in this
repo's `bios/`, `roms/`, or `references/` trees (confirmed: `references/fact-sheets/` has no
printer-font entry; this project's guardrails explicitly forbid fabricating BIOS/ROM asset content
or provenance). Building "dot-matrix rendering" without a real, sourced font would mean inventing
glyph bitmaps — the same category of fabrication risk the license-sensitive-scope directive exists
to prevent, just for a font asset instead of a GPL numeric table. **DEFERRED → M33**, with an
explicit note that a future planner must either (i) source a legitimately public-domain/free
bitmap dot-matrix font before scheduling implementation, or (ii) scope the deliverable down to a
simpler, honestly-labeled ASCII/text virtual "page" representation (not claimed as authentic
ESC/P glyph rendering) to avoid fabrication.

### 2.4 — Release-candidate health-audit design (mandatory, always in scope)

**Deliverable name:** `docs/m28-release-candidate-audit.md` (per REQ-M28-001(B)'s own suggested
name).

**Structure (four parts, matching REQ-M28-001(B) verbatim):**

**Part 1 — Component-by-component source-code health**, per `src/` subsystem
(`src/core/`, `src/devices/{cpu,video,audio,rtc,fdc,memory,chipset,cartridge,halnote,kanji}/`,
`src/peripherals/`, `src/machine/`, `src/frontend/`). For every `.h`/`.cpp` file (the current
inventory: 78 headers / 68 sources, enumerated this cycle via `Glob "src/**/*.h"` and
`"src/**/*.cpp"` — the developer should re-enumerate fresh at implementation time, not reuse this
count verbatim, since M28's own IN work adds/edits files), confirm: (a) it is exercised by at least
one test under `tests/unit/` or `tests/integration/` (a `rg` cross-check, not a visual scan); (b)
for devices/peripherals, it is actually attached in `src/machine/hbf1xv_machine.cpp`'s wiring
(`wire_bus`/constructor) — not merely compiled; (c) no unresolved `TODO`/`FIXME`/`STUB`/
`not implemented` markers remain without a tracked backlog row. **Seeded finding (A-M28-5):**
`src/devices/device_placeholder.h` and `src/peripherals/peripheral_placeholder.h` are M0-era
scaffolding referenced from nowhere but `README.md` — the audit must explicitly resolve this (fix
or document-as-intentional), a small, safe, audit-triggered change permitted under the fix-scope
rule below.

**Part 2 — Integration-flow coherence**, confirmed END-TO-END, not just unit-level: (a) machine
composition — a fresh read of `Hbf1xvMachine`'s constructor/`wire_bus`/`cold_boot` confirming every
DONE backlog device is actually reachable from a real boot, cross-referenced against the Target
Machine Specification's I/O port list; (b) bus/slot wiring — re-confirm the M11 primary/secondary
slot decode and the M13-M20 slot map still match `references/openmsx-21.0/share/machines/
Sony_HB-F1XV.xml`; (c) CLI — both `sony_msx_headless` (`src/main.cpp`) and `sony_msx_sdl3`
(`src/frontend/sdl3_main.cpp`) actually invoked as built executables (not merely `ctest`), covering
`--bios-dir`, `--disk`, `--cart1`/`--cart2`, `--debug-session`, `--input-script`, and (SDL3 build)
`--hidden-window`/`--max-frames`; (d) asset loading — `tools/validate-assets.ps1` re-run fresh
against the current `bios/`/`roms/`/`disks/` trees.

**Part 3 — Feature-completeness cross-check against the Target Machine Specification**
(`CLAUDE.md`'s table). A row-by-row table: NAME/MANUFACTURER/ORIGIN/YEAR (machine identity, not
code-mapped), KEYBOARD (`src/peripherals/keyboard_matrix.*`), CPU/SPEED (`src/devices/cpu/
z80a_cpu.*`), CO-PROCESSOR (`src/devices/video/v9958_vdp.*`), RAM (`src/devices/memory/
memory_mapper_ram.*`), VRAM (`src/devices/video/vdp_vram.*`), ROM set (`src/devices/memory/
rom_device.*` + slot map), BUILT-IN MEDIA (`src/devices/fdc/*`), TEXT/GRAPHIC MODES
(`src/devices/video/vdp_frame_renderer.*`/`vdp_mode.*`), COLORS (palette + YJK decode paths),
SOUND (`src/devices/audio/{psg_ym2149,ym2413_opll}.*`), I/O PORTS (cartridge slots →
`src/devices/cartridge/*`; RGB/Scart+NTSC+RF video → N/A, analog-output-only, correctly out of a
software emulator's scope; joystick ports → `src/peripherals/joystick.*`; printer port →
`src/peripherals/printer_port.*`; cassette interface → `src/peripherals/cassette_interface.*`).
Each row gets a status marker (Built / Partial-with-backlog-row / Gap) — any "Gap" found here that
is NOT already a tracked backlog row is a genuine, reportable finding requiring a new backlog
entry (via `decisions.md`, per Guardrails Scope Control), not a silent fix.

**Part 4 — Full 34-row backlog-accuracy audit.** For every row (not just the 11 touched by M28),
independently re-verify: (a) every cited `references/...`/`src/...` path still exists (a `Test-Path`
sweep); (b) every "DONE (Mxx)" row's named src/ files still exist and are still wired (spot-check,
not exhaustive re-derivation of already-QA-signed milestones); (c) the milestone-level status in
`agent-protocol/state/milestones.md` and `definition-of-done.yaml` agrees with `deferred-backlog.md`
(no drift, mirroring the "stale-status drift corrected same-cycle" pattern already seen at M14/M19).

**Fix-scope rule (bounds Part 1-4's "plus whatever concrete fixes... fall out of it," per
REQ-M28-001(B)):** the audit MAY apply small, safe, mechanically-verifiable fixes found along the
way (dead-file removal/documentation, stale citation-line-number corrections, doc-comment
staleness) WITHOUT a separate decision entry, mirroring the M14/M19/M25 precedent of QA-found
doc-only fixes applied same-cycle. Anything that would change RUNTIME BEHAVIOR of an already
QA-signed device, or that represents new SCOPE (a new feature, not a correction), is logged as a
NEW backlog row or decision candidate instead of being applied ad hoc — the health audit reports,
it does not silently re-open closed milestones' behavior.

---

## 3. Milestones (decomposition)

| Milestone | One-line objective | Backlog items | Sequencing note |
|-----------|--------------------|-----------------|-------------------|
| **M28** (this) | Release-candidate health audit + the two safely-IN backlog rows | E2, C5 (partial-or-full) | Current milestone. |
| **M29** | KonamiSCC mapper + SCC/SCC+ wavetable audio chip (needs a new SCC fact-sheet first) | G1 | Least risky deferred item; sequenced first. |
| **M30** | YM2413 FM-synthesis DSP/audio depth, scoped to the formulaically-derivable subset (logsin/exp/decay-release), attack-curve depth named as a further carried-forward remainder | E1 | Pairs conceptually with C9 (SDL3 audio, already DONE) and G1's own new audio device (M29). |
| **M31** | FDC flux-level/DMK disk fidelity (DMK container + Write-Track byte-stream/gap/ID/DAM construction per the FDC fact-sheet's own independent §5 grounding) | C10 | Sequenced after C5 (M28) so any FDC boot-time findings are incorporated, not contradicted. |
| **M32** | Cassette tape image-format/signal fidelity (real `.CAS`/`.WAV`/`.TSX`), with baud-rate/modulation constants re-grounded in an independent MSX-kernel reference (to be added to `references/fact-sheets/` first), not openMSX's own tuned `BAUDRATE=3744` | F1 | Needs a new fact-sheet or documented independent source before implementation. |
| **M33** | Printer image/ESC-sequence rendering depth, contingent on sourcing a legitimate public-domain dot-matrix font asset (or scoping down to a non-authentic text-page representation) | F2 | Blocked on an asset-provenance decision, not merely sequencing. |
| *(not scheduled)* | VDP access-slot/command-timing full depth | C1/D4 | Stays OPEN indefinitely pending a genuine independent numeric source (§2.2) — no milestone number assigned. |
| *(on-demand, unchanged)* | ROM-database auto-detection / cartridge hot-plug / long-tail mapper types | G2, G3, G4 | Re-affirmed indefinite per their own original scoping; no M28 change. |

### 3.1 M28 slice plan

| Slice | Goal | Primary files | Tests | Evidence gates |
|-------|------|----------------|-------|-----------------|
| **M28-S1** | E2 — YM2413 write-timing constraint | `src/devices/audio/ym2413_opll.{h,cpp}` (extend) | Unit (12/84-cycle boundary accept/drop, determinism); integration (CPU `OUT (#7C)/(#7D)` sequences at varying spacing over the M11 bus) | 4 standard gates + full (unfiltered) `ctest` per A-M28-3 |
| **M28-S2** | C5 — full-boot / auto-disk-boot-trigger investigation | Investigative; possible edits under `src/devices/fdc/*` and/or `src/machine/hbf1xv_machine.{h,cpp}` if a genuine defect is found | New investigation trace/report tooling as needed (reuse M10 trace-export + M27 `--debug-session`/`--input-script`); a new or extended system test capturing whichever outcome (a) or (b) (§2.1a) is reached | 4 standard gates + full `ctest`; live WSL openMSX A/B for the boot-divergence-point comparison |
| **M28-S3** | Release-candidate health audit | `docs/m28-release-candidate-audit.md` (new); small, safe fixes per the §2.4 fix-scope rule (candidate: resolve the two placeholder files) | N/A (documentation + mechanically-verified command output); any code fix gets its own minimal test if behavior-affecting | 4 standard gates; `tools/validate-assets.ps1` re-run; both executables (`sony_msx_headless`, and `sony_msx_sdl3` if the SDL3 config is available) actually launched, not merely built |
| **M28-S4** | Backlog/documentation closure | `agent-protocol/state/deferred-backlog.md` (E2→DONE; C5→ per S2's honest outcome; C1/D4 reasoning recorded; G1/E1/C10/F1/F2 → DEFERRED with named owners; G2/G3/G4 re-affirmed), `agent-protocol/state/milestones.md`, `definition-of-done.yaml`, implementation report | — | 4 standard gates |

Sequencing: S1 (E2, small and self-contained) and S2 (C5, investigative, larger effort budget) can
proceed in parallel or S1-first; S3 (health audit) can start independently at any point since it
depends only on the CURRENT tree, but its Part 4 (backlog-accuracy audit) should run LAST, after
S1/S2/S4 have updated the ledger, so the audit reflects the milestone's own final state rather than
a stale mid-milestone snapshot. S4 is the QA-readiness gate.

---

## 4. Acceptance Criteria

1. **Full 34-row backlog consultation completed** with an explicit IN/DEFERRED recommendation and
   justification for every one of the 11 currently-open rows (§2.1), and all 23 DONE rows
   re-affirmed unchanged.
2. **C1/D4's license-sensitive-scope risk is explicitly assessed and ruled on, not silently
   skipped**: ruled UNBUILDABLE-WITHOUT-FABRICATION this cycle with the concrete sourcing gap
   documented (§2.2); NOT attempted via openMSX table transcription in any form; NOT force-closed.
3. **The same independent-source scrutiny applied to E1, E2, C10, F1, F2, and G1** (§2.3), each
   with a citation-grounded finding (not an assumption) of whether a numeric-table/asset-provenance
   risk exists, before any IN/DEFERRED recommendation.
4. **E2 implemented** per §2.1b: minimum address/data write-spacing enforced against the cited
   Yamaha Application Manual / andete-measured 12/84-master-cycle constants; the regression
   pre-check on existing YM2413 tests performed BEFORE any gate defaults to "on"; deterministic
   unit + integration tests; the A/B disposition honestly reported as N/A per openMSX's own
   documented default-disabled stance (not fabricated as PARITY or DIVERGENCE).
5. **C5 investigated** per §2.1a using the M10 trace-export + M27 `--debug-session`/`--input-script`
   tooling and the real `disks/` assets; EITHER outcome (a) full close with A/B-verified auto-boot,
   or (b) honest, evidenced advancement without force-closing, is acceptable — a fabricated "reached
   DOS prompt" claim without a genuine reproduced trace is not.
6. **`docs/m28-release-candidate-audit.md` produced** covering all four parts of §2.4, with concrete,
   executed evidence (grep/build/test/executable-launch output cited, not restated planning text);
   any applied fix stays within the §2.4 fix-scope rule (safe/mechanical) or is logged as a new
   backlog/decision candidate instead of applied ad hoc.
7. **Zero regression across the full M1-M27 suite**, independently reproduced by the developer,
   coordinator, and QA from clean rebuilds; the M9/M12/M23 zero-tolerance CPU-timing-oracle suites
   confirmed unchanged via `git diff` against `v1.0.27`.
8. **The slow `hbf1xv_m24_zexall_system_test` runs at least once this cycle** (A-M28-3 — mechanically
   triggered by E2/C5 touching existing `src/devices/` files), to completion, with both suites'
   `error_markers == 0` hard assertions (added M24, hardened DEC-0022) still passing.
9. **Every backlog row touched (E2, C5, and the DEFERRED rows' new named owners) is status-updated
   in `deferred-backlog.md` in the same cycle**; C1/D4's carried-forward reasoning is recorded
   verbatim, not summarized away; all untouched rows re-affirmed with a one-line justification.
10. **Evidence gates green**: `tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile
    docs/asset-checksums.txt`; `cmake --build build --config Debug` (both headless and, if SDL3 is
    configured, the SDL3 build); `ctest --test-dir build -C Debug --output-on-failure` (full,
    unfiltered, per criterion 8); conditional openMSX A/B evidence for E2 (mechanism-only,
    honestly N/A for the drop-behavior comparison) and C5 (boot-divergence-point comparison).

---

## 5. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|----------------------|
| R-M28-1 | E2's write-spacing gate silently breaks an existing M17 YM2413 test that issues back-to-back writes without spacers | Developer runs the mandated `rg` pre-check (§2.1b) BEFORE enabling any default-on gate; if found, either add disclosed spacer instructions to the specific test or default the gate off-with-toggle; QA independently re-runs the same `rg` sweep before sign-off |
| R-M28-2 | C5's investigation is pressured (by the "release candidate" framing) into reporting a fabricated "boot reaches DOS prompt" result without genuine, reproduced evidence | §2.1a's explicit dual-outcome acceptance criterion (either honest outcome is acceptable) removes the incentive; QA independently reproduces whichever trace/A-B evidence is submitted, not merely trusts a narrative |
| R-M28-3 | The health audit becomes a rubber-stamp ("everything looks fine") rather than a genuine, evidence-producing pass | §2.4 requires concrete command output (grep counts, executable launches, `Test-Path` sweeps) cited in the report, not restated planning text; QA independently re-runs a sample of the audit's own checks before sign-off |
| R-M28-4 | A future implementer misreads §2.2's C1/D4 ruling and attempts the VDP slot tables anyway "since M28 already scoped it out, maybe now it's safe" | The ruling explicitly states NO follow-on milestone number is assigned and names the exact new-fact precondition required; `decisions.md` Scope Control requires an explicit entry before any deviation |
| R-M28-5 | E1/C10/F1/F2/G1's DEFERRED-with-named-owner dispositions get silently treated as "already scoped, just build it" by a future planner, skipping the scoping caveats this package attaches to each (e.g., E1's attack-curve carve-out, F2's font-provenance blocker) | §2.3's per-row findings are written into `deferred-backlog.md` at S4 verbatim (not summarized), so the caveats travel with the row, mirroring how C1/D4's own remainder has survived two prior cycles (M21→M23) without being lost |
| R-M28-6 | The mandatory slow-sweep run (A-M28-3/criterion 8) is skipped because the human's kickoff framing ("skip unless really needed") is read as overriding the mechanical git-diff rule | This package states explicitly, twice (§1.4 A-M28-3, §4 criterion 8), that the rule fires mechanically regardless of the aspirational framing; the developer's evidence must show the literal `git diff <baseline> --name-only -- src/devices/ ...` output plus the resulting slow-test run |
| R-M28-7 | The two placeholder files (`device_placeholder.h`/`peripheral_placeholder.h`) are removed without checking whether any external tool, doc, or CMake glob depends on their mere presence (not just inclusion) | Before removal, the developer greps `CMakeLists.txt`/`tests/CMakeLists.txt` for glob patterns that would silently change if the file count in `src/devices/`/`src/peripherals/` shrinks, and confirms the build still configures cleanly after removal |
| R-M28-8 | SCC's grounding (§2.3b) — quoted forum posts embedded in openMSX's `SCC.cc` comments — gets treated as "already fine to cite openMSX's file directly" by a future M29 developer, without producing an independent fact-sheet first | §2.3b/§3's M29 row explicitly instructs commissioning a dedicated `references/fact-sheets/` SCC entry FIRST, mirroring every prior audio/video/FDC device's process; this is a stated M29 precondition, not a suggestion |

---

## 6. Developer Handoff

- **Do NOT start coding on E1/C10/F1/F2/G1/C1/D4** — all are DEFERRED this cycle per §2 with named
  (or explicitly unnamed, for C1/D4) follow-on owners. Only **E2** and **C5** are IN-M28
  implementation work; the **health audit** is the third work item (documentation + small, bounded
  fixes only, per the §2.4 fix-scope rule).
- **Build order:** M28-S1 (E2) and M28-S2 (C5) can proceed in either order or in parallel (no
  coupling between them); M28-S3 (health audit) can start early but its Part 4
  (backlog-accuracy pass) must run LAST, after S1/S2/S4 land, so it reflects the milestone's final
  state; M28-S4 (backlog/ledger closure) is the QA-readiness gate.
- **Hard constraints (do not deviate without a `decisions.md` entry):**
  - Do NOT implement any part of C1/D4's remaining VDP-timing depth this cycle (§2.2) — no
    slot-table transcription in any form, "independently re-derived" framing included.
  - Do NOT begin E1/C10/F1/F2/G1 production code this cycle — each has an unmet precondition
    (§2.3) a future milestone must first satisfy (an SCC fact-sheet for G1; an independent
    MSX-kernel baud-rate source for F1; a sourced font asset or scope-down for F2; the E1
    attack-curve carve-out named explicitly for M30's own planner to restate).
  - Perform the E2 regression pre-check (§2.1b, R-M28-1) BEFORE enabling any default-on write-gate
    behavior.
  - C5's acceptance is dual-outcome and evidence-gated (§2.1a, R-M28-2) — do not force a "closed"
    narrative without a genuine, reproduced trace/A-B artifact.
  - Run the FULL, unfiltered `ctest` (not the `-LE m24_slow_full_sweep`-filtered fast subset) at
    least once this cycle per A-M28-3/criterion 8 — this is mechanically required, not optional,
    despite the kickoff note's "skip unless needed" framing.
- **Files to create:** `docs/m28-release-candidate-audit.md`, `docs/m28-implementation-report.md`,
  new E2 unit/integration test files under `tests/unit/devices/audio/` and
  `tests/integration/devices/audio/` (or the existing YM2413 test files extended — developer's
  choice per `src/CLAUDE.md`/`tests/CLAUDE.md` conventions), a new or extended C5 investigation
  test/tooling artifact (placement per the developer's judgment, likely `tests/system/` for any new
  boot-trace assertion), and (if C5's outcome (a) is reached) a `docs/m28-parity-trace-diff.md`.
- **Files to edit (surgical, additive-first):** `src/devices/audio/ym2413_opll.{h,cpp}` (E2);
  possibly `src/devices/fdc/*` and/or `src/machine/hbf1xv_machine.{h,cpp}` (C5, only if a genuine
  defect is found — pure investigation requires no production edit); `agent-protocol/state/
  deferred-backlog.md`, `agent-protocol/state/milestones.md`, `agent-protocol/state/
  definition-of-done.yaml`, `agent-protocol/state/current-phase.md` (ledger closure, S4).
- **Grounding is mandatory:** cite the fact-sheet section + the openMSX file (behavior reference
  only, never copied) for every behavior claim; no A/B parity claim without a genuine captured
  diff; no "audit passed" claim in `docs/m28-release-candidate-audit.md` without cited command
  output.
- **Ledger discipline (DEC-0005):** every backlog row touched status-updated in the same cycle;
  the C1/D4 ruling and the E1/C10/F1/F2/G1 scoping caveats travel into `deferred-backlog.md`
  verbatim, not summarized.

---

Prepared for coordinator/human review. No production code produced in this package. On approval,
proceed to M28-S1/S2/S3 per §3.1.
