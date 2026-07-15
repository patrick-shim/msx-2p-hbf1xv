# M23 Implementation Report — Z80 HALT-R Closure (C2) + VDP Access-Timing Foundation (C1/D4, narrowed)

- Milestone ID: M23
- Developer: MSX Developer Agent
- Planner package: `docs/m23-planner-package.md` (RESP-M23-001/REQ-M23-002)
- Status recorded here: developer implementation COMPLETE, evidence captured. NOT yet closed —
  QA sign-off + coordinator release decision are the remaining gates (per the standing M21-M23
  pre-authorization: proceed through the release-decision/tag step without an extra pause on a
  clean QA PASS; STOP and consult the human otherwise). Per this milestone's explicit instruction,
  the Status-column ledger flip (`agent-protocol/state/deferred-backlog.md`) and the `v1.0.23` tag
  are intentionally left to the coordinator at closure time, not applied here.

---

## 1. Milestone Target

Deliberately narrow, conservative slice (planner package §2, the "Critical Scope-Resolution
Section"): (a) close backlog **C2** (Z80 HALT-R increment, DEC-0004) **in full** — a small,
well-isolated, CPU-core-only fix; and (b) ship a real, tested, but strictly **NON-GATING** VDP
access-timing **foundation** for backlog **C1/D4** (raster-position derivation + the `Delta`
cost-unit contract + two clearly-separated, cited latency facts), explicitly declining the full
per-mode slot tables and any actual CPU-execution gating this cycle. This is the right,
honestly-scoped call, not a shortcut: M21's `VdpFrameRenderer` and M22's
`VdpCommandEngine`/`SpriteEngine` were both explicitly built as non-cycle-accurate
(pull-model/atomic) architectures specifically *because* cycle-accurate VDP timing was deferred to
this milestone, and the M21/M22 system-test CPU-program fixtures already contain back-to-back
`OUT (#98),A` writes with zero spacer instructions — naive real gating would risk silently
dropping bytes those tests' content assertions depend on (planner §2.3).

Build order followed exactly as scoped: **S1** (C2, HALT-R) → **S2** (VDP access-timing
foundation, strictly additive, no coupling to S1) → **S3** (backlog/documentation/evidence).

## 2. Code Changes

### Edited (surgical, additive only)

- **`src/devices/cpu/z80a_cpu.cpp`** — `Z80aCpu::step()`'s halted branch (previously
  `else if (state_.halted()) { tstates = 4; }`) now additionally calls the EXISTING
  `increment_refresh_register()` helper before returning. The `tstates` literal itself stays the
  bare, unchanged datasheet `4` (A-M23-1's architectural invariant: the CPU core keeps publishing
  pure datasheet T-states; only the machine layer's separate `datasheet + m1_wait` formula turns
  this into 5T). Grounded, cited in the added code comment:
  `references/openmsx-21.0/src/cpu/Z80.hh:19-21` (`HALT_STATES = 4 + WAIT_CYCLES`) and
  `CPUCore.cc:2508-2511` (`incR(narrow_cast<uint8_t>(advanceHalt(HALT_STATES, ...)))`) — the SAME
  `halts` computation drives both the R-register increment and the clock advance on real hardware;
  they are not separable mechanisms. This is the entire production behavior change for C2.
- **`src/machine/hbf1xv_machine.h` / `.cpp`** (additive-only) — one new private field
  `std::uint64_t last_vsync_cycle_ = 0;`; one new line inside the EXISTING `run_frame()`
  (`last_vsync_cycle_ = elapsed_cycles();`, immediately after the existing `vdp_.on_vsync()` call);
  two new public const accessors, `cycles_since_last_vsync()` and `vdp_cycle_position()` (a thin
  wrapper over `vdp_access_timing::vdp_cycle_within_line()`); one new `#include
  "devices/video/vdp_access_timing.h"`. Verified via `git diff v1.0.22` (§4 below):
  `step_cpu_instruction()`, `run_cycles()`, and `run_until_cycle()` are byte-for-byte unchanged —
  only `run_frame()` gains the one bookkeeping line, plus the new accessor bodies.
- **`src/main.cpp`** (additive, backward-compatible; **a disclosed, reasoned scope note** — see
  below) — the existing `--parity-trace` CLI mode's `run_parity_trace()` gains one new, OPTIONAL
  trailing parameter, `halt_idle_extra_steps` (default `0`). The main stepping loop itself is
  completely unchanged (still stops exactly at the halt boundary, the established safe convention
  every other call site in this project uses). When the new parameter is `> 0` AND the CPU is
  halted after that unchanged loop, `run_parity_trace()` steps that many MORE times while already
  halted, recording every one of those steps via the SAME existing `CpuTraceSink` mechanism (which
  records every `step()` call unconditionally, regardless of halted state). Absent the new
  argument (every prior invocation, including `tools/openmsx-cpu-parity.ps1` and
  `tools/openmsx-trace-parity.ps1`), behavior is byte-identical to pre-M23.
  **Scope disclosure**: this file is not named in the planner package's own "files to edit" list.
  It was added because the milestone's own acceptance criteria (§6 item 9) require a genuine,
  attempted openMSX A/B check for C2, and the existing `--parity-trace` mode structurally cannot
  observe halted-idle stepping (its loop stops the instant `halted()` becomes true, mirroring the
  safe "stop at halt" pattern every other test/tool in this project deliberately uses). This
  mirrors the established M19 precedent of extending this SAME `--parity-trace` mode additively
  for a new milestone's specific A/B need (the `--cart1`/`--cart2` flags, `main.cpp`'s own existing
  doc comment at the call site). It does not touch any CPU-execution-gating path, any hard
  constraint named in the developer handoff, or any of the 10 zero-tolerance oracle test files.

### New (production)

- **`src/devices/video/vdp_access_timing.h`** (header-only, pure functions/constants over explicit
  parameters; no VDP device/bus/slot knowledge of its own):
  - `enum class VdpAccessDelta : int { D0=0, D1=1, D16=16, D24=24, D28=28, D32=32, D40=40, D48=48,
    D64=64, D72=72, D88=88, D104=104, D120=120, D128=128, D136=136 };` — the 15 named VDP-cycle
    offsets openMSX's own access-slot calculator is keyed on, independently re-derived from the
    enum's OWN labels (`references/openmsx-21.0/src/video/VDPAccessSlots.hh:17-33`), NOT from the
    ~340-entry numeric slot tables (`VDPAccessSlots.cc:14-141`), which remain explicitly out of
    scope (R-M23-4: no large (>20-entry) numeric array was added under `src/`).
  - `kVdpCyclesPerLine = 1368` (`VDP.hh:76`), `kCpuTstatesPerVdpCycleRatio = 6` (fixed CPU:VDP
    clock ratio, Z80A/V9958 fact-sheets §1), `kCpuTstatesPerLine = 228` (exact, no remainder).
  - `cpu_tstate_within_line()` / `vdp_cycle_within_line()` — the raster-position derivation
    (CPU T-states since the last VSync → position within the current scanline).
  - `minimum_request_latency_tstates(VdpAccessDelta)` — a lower-bound-only `ceil(delta/6)`
    conversion of the V99x8 `Delta::D16` CPU-VRAM-access-scheduler floor (`VDP.cc:842-844`),
    explicitly NOT slot-table-refined.
  - `kDocumentedSafeAccessGapTStates = 29` — the V9958 fact-sheet §2's SEPARATE, coarser
    "~29 Z80-cycle safe access" software-discipline convention. Per A-M23-8, kept as two
    separately-named, separately-cited, never-combined constants (enforced by a dedicated test,
    §3 below).

### New (tests)

- `tests/unit/devices/z80a_halt_r_unit_test.cpp` — direct-CPU-core tests (bypassing the machine):
  R increments by exactly 1 per halted `step()` call; R wraps `0x7F→0x00` with bit 7 preserved
  (mirrors the existing M12 `RLow7Wrap_Bit7Frozen` pattern in
  `z80a_parity_undocumented_unit_test.cpp:242-251`); `Z80aCpu::step()` called directly on an
  already-halted CPU still returns the bare `4` (A-M23-1's invariant); an interrupt arriving while
  halted still transitions out via the unchanged M12 IM1-ack path at its own bare `13`T (14T at
  the machine level), proving zero coupling between the HALT-R fix and interrupt-accept timing.
- `tests/integration/machine/hbf1xv_m23_halt_r_integration_test.cpp` — machine-level companion:
  several halted idle steps (R visibly incrementing by 1, 5T each at the machine level) followed
  by an interrupt wake at the SAME IM1-ack timing (14T) already established in M12.
  **Deliberately a NEW file**, not an edit to `hbf1xv_interrupt_ack_integration_test.cpp` — see
  §3/§6 for why.
- `tests/unit/devices/video_vdp_access_timing_unit_test.cpp` — `kCpuTstatesPerLine * 262 ==
  kFrameCycles` (A-M23-7's cross-check, via each side's own independently-declared constant, since
  the machine's `kFrameCycles` lives in an anonymous namespace); raster-position round-trips across
  a line boundary and a full-frame (262-line) boundary; the 15 `VdpAccessDelta` values match their
  literal labels exactly; `minimum_request_latency_tstates(D16) != kDocumentedSafeAccessGapTStates`
  and no simple multiple reconciles them (A-M23-8).
- `tests/integration/machine/hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp` — the
  concrete, checkable "non-gating" proof: re-runs the EXACT M21 (4 fixtures) and M22 (2 fixtures)
  system-test CPU-program byte sequences (copied verbatim from
  `tests/system/hbf1xv_m21_vdp_render_system_test.cpp` and
  `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp`) through
  `step_cpu_instruction()` and asserts the cumulative T-state total for each is a literal, captured
  golden constant (185 / 105 / 125 / 205 / 445 / 525 T-states respectively — each independently
  measured this cycle, not hand-derived, then hardcoded as the before/after regression oracle);
  separately, in the same test, exercises `vdp_cycle_position()`/`cycles_since_last_vsync()`/
  `minimum_request_latency_tstates()` and confirms sane, non-crashing, in-range values, including
  the documented "no vsync yet, relative to program start" boundary (R-M23-5).
- `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp` — the ONE deliberate golden
  update (§3).

### New (tooling / docs)

- `tools/openmsx-m23-halt-r-parity.ps1` — the C2 openMSX A/B harness (§5).
- `docs/m23-parity-trace-diff.md` — the captured A/B result (§5).
- `docs/asset-checksums.txt` — refreshed (§6).
- `docs/m23-implementation-report.md` — this file.

### CMake

- `tests/CMakeLists.txt` — registered the 4 new test executables (additive only).

## 3. Unit Test Results

Ran the audit the planner package's own risk R-M23-2 requires **before touching anything else**:

```
rg "\.halted\(\)" tests/
```

Result: **exactly the same 22 files** the planner's own audit found (§1.3 A-M23-3). Read the
call-site context of every match: 21 of the 22 use the safe `while/for (... &&
!machine.cpu().state().halted()) { machine.step_cpu_instruction(); }` stop-at-halt-boundary
pattern (confirmed individually, including the two "special" cases the planner flagged:
`hbf1xv_debug_dump_unit_test.cpp:168-180` only captures `halt_before`/never steps again;
`z80a_trace_export_unit_test.cpp:152` compares two PARALLEL runs' final `halted()` states for
equality, unaffected either way; `z80a_unprefixed_unit_test.cpp:67-70` exercises the HALT OPCODE'S
OWN fetch cost, not the halted-idle branch, unaffected). **The sole exception** — the only test
that steps one MORE time after already halting — is
`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp:62-73`, exactly as the planner
predicted. No new site was found that the planner's cycle did not already enumerate.

Unit test executables (new): `devices_z80a_halt_r_unit_test`,
`devices_video_vdp_access_timing_unit_test` — both **Passed**.

Full unit-test tier result (part of the full `ctest` run, §4/§6): 100% pass.

## 4. Integration Test Results

New integration executables: `machine_hbf1xv_m23_halt_r_integration_test`,
`machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test` — both **Passed**.

The ONE deliberate golden update, exactly as A-M23-4 recomputed it (re-verified by hand here, not
merely re-run and trusted): `LD A,0x2A` (7+1=8T) + `INC A` (4+1=5T) + `HALT` (4+1=5T, its own real
M1 fetch, unaffected) + the halted idle step, now correctly billed `4 (bare) + 1 (M1 wait, newly
applied) = 5T` (was `4`) = **8+5+5+5 = 23** (was 22). `t3` updated `4 → 5`;
`elapsed_cycles()` updated `22 → 23`.

### Full regression run (real, captured output)

```
$ cmake --build build --config Debug
... (build succeeded, only pre-existing C4819 code-page warnings, zero errors)

$ ctest --test-dir build -C Debug --output-on-failure
...
100% tests passed, 0 tests failed out of 121

Total Test time (real) =   3.36 sec
```

**Test count: 117 (pre-M23) → 121 (post-M23)**, exactly 4 new (`devices_z80a_halt_r_unit_test`,
`machine_hbf1xv_m23_halt_r_integration_test`, `devices_video_vdp_access_timing_unit_test`,
`machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test`). Zero failures, zero
regressions. Re-run identically after every slice (S1, then S1+S2, then S1+S2+S3).

### Named, zero-tolerance oracle suites — `git diff v1.0.22` confirmed ZERO changes

```
$ git diff v1.0.22 --stat -- \
    tests/unit/devices/z80a_unprefixed_unit_test.cpp \
    tests/unit/devices/z80a_trace_export_unit_test.cpp \
    tests/integration/machine/hbf1xv_cpu_parity_integration_test.cpp \
    tests/integration/machine/hbf1xv_m11_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_indexed_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_cb_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_ldir_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_interrupt_ack_integration_test.cpp

(no output — empty diff, confirmed by exit code 0 and an empty stat table)
```

All 10 named suites — `z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`,
`hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`,
`hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`,
`hbf1xv_parity_checkpoint_integration_test.cpp`, `hbf1xv_indexed_program_integration_test.cpp`,
`hbf1xv_cb_program_integration_test.cpp`, `hbf1xv_ldir_program_integration_test.cpp`,
`hbf1xv_interrupt_ack_integration_test.cpp` — pass, with confirmed byte-identical content to
`v1.0.22`. This is why the M23-S1 "extended integration test" requirement (proving several halted
idle steps + an interrupt wake at the same 14T IM1-ack timing) was implemented as a **brand-new**
file (`hbf1xv_m23_halt_r_integration_test.cpp`) rather than an edit to
`hbf1xv_interrupt_ack_integration_test.cpp`: that file is one of the 10 files this exact
zero-diff gate applies to, so editing it would have been self-contradictory.

### Full `git diff v1.0.22` summary (all of `src/` and `tests/`)

```
$ git diff v1.0.22 --stat -- src/
 src/devices/cpu/z80a_cpu.cpp   | 16 ++++++++++++++++
 src/machine/hbf1xv_machine.cpp | 16 ++++++++++++++++
 src/machine/hbf1xv_machine.h   | 22 ++++++++++++++++++++++
 src/main.cpp                   | 35 ++++++++++++++++++++++++++++++++---
 4 files changed, 86 insertions(+), 3 deletions(-)

$ git diff v1.0.22 --stat -- tests/
 tests/CMakeLists.txt                               | 29 ++++++++++++++++++++++
 .../machine/hbf1xv_cpu_step_integration_test.cpp   | 19 ++++++++++----
 2 files changed, 43 insertions(+), 5 deletions(-)

$ git diff v1.0.22 --stat -- \
    src/devices/video/vdp_frame_renderer.h src/devices/video/vdp_frame_renderer.cpp \
    src/devices/video/vdp_command_engine.h src/devices/video/vdp_command_engine.cpp \
    src/devices/video/sprite_engine.h src/devices/video/sprite_engine.cpp

(no output — confirmed genuinely untouched, A-M23-6)
```

`src/`: exactly the 4 files named/justified above (`z80a_cpu.cpp` — S1's one branch;
`hbf1xv_machine.{h,cpp}` — S2's one field/one line/two accessors; `main.cpp` — the disclosed,
additive `--parity-trace` extension for the A/B gate). `tests/`: exactly `CMakeLists.txt`
(additive test registrations) and the ONE deliberate golden
(`hbf1xv_cpu_step_integration_test.cpp`) — no other existing test file under `tests/` was touched;
every other change under `tests/` is a brand-new file (untracked, does not show in a `git diff`
against a tag).

## 5. Real openMSX A/B Evidence

### C2 (Z80 HALT-R): a genuine, feasible technique — CONFIRMED AVAILABLE, with an honestly-reported finding

Live WSL check performed before any claim: `wsl -e bash -lc "command -v openmsx"` → `/usr/bin/openmsx`
(openMSX 19.1, debian flavour). The `reg r` live R-register readback IS the SAME mechanism
`tools/openmsx-trace-parity.ps1` (M10) and `tools/openmsx-cpu-parity.ps1` (M12) already use
successfully — not a new, unverified technique.

`tools/openmsx-m23-halt-r-parity.ps1` drives an identical single-`HALT` (0x76) program on both
engines, then continues stepping N more times while already halted on each side (via the new
`main.cpp` `halt_idle_extra_steps` argument on the "A" side, and the SAME `emit`/`step` Tcl
mechanism the M10-M12 harnesses established on the "B", openMSX, side), comparing PC and R (not
the full architectural field set — see the scope note embedded in
`docs/m23-parity-trace-diff.md` for why the opcode-byte field is deliberately excluded, mirroring
the established DP-4/A-3/A-4 excluded-field precedent).

**Result (captured, `docs/m23-parity-trace-diff.md`): DIVERGENCE, with a grounded root-cause
analysis — not a fabricated PASS.** R and PC match EXACTLY for the HALT opcode's own M1 fetch and
the first two genuine halted-idle steps (seq 0/1/2: R=00/01/02 on both engines, PC constant at
`C001` on both). From seq 3 onward, a large (+0x29), non-monotonic jump appears on the openMSX
side only, reproduced at the identical ABSOLUTE seq index regardless of the total step count
requested (independently re-verified at `HaltIdleSteps` = 2/3/5 during this implementation — 2
stays PARITY, 3 and 5 both diverge starting at seq=3). This divergence-onset-tracks-real-time (not
step-count) pattern points to openMSX's own live-session `step()` command, while the CPU is
halted, not corresponding to a fixed one-halt-state-per-call quantum — grounded, per the SAME
source this milestone's own C2 change cites
(`references/openmsx-21.0/src/cpu/CPUClock.hh:53-59`, `advanceHalt`'s bulk ceiling-division over
however many scheduler ticks the CPU jumps forward to the NEXT scheduled sync point while halted),
not a defect in either engine's R-register correctness. This does **not** invalidate C2's closure:
the R-increments-by-exactly-1-per-halted-step claim is independently, deterministically proven by
direct citation of the SAME openMSX source and by this project's own deterministic unit/
integration tests, neither of which depends on the live session's real-time scheduling behavior.
Per the guardrails' explicit contract for uncertainty, this is labeled an **Assumption** with a
concrete verification action (recorded in the diff artifact itself), not asserted as certain fact.

### D4/C1 numeric VDP-access-wait cost: BLOCKED, not attempted (per the plan)

No feasible live-Tcl-debugger technique exists in this project's established methodology (M11-M22)
for comparing "exactly how many extra wait T-states did openMSX insert for a specific VRAM port
access" — no independently observable side effect of that wait exists without dedicated
instrumentation neither engine exposes. Since this milestone gates no CPU timing on this value
(§2.3 of the planner package), this sub-claim is honestly marked **BLOCKED / not attempted**,
consistent with the M21 planner's own precedent (`docs/m21-planner-package.md` §2.7, the
computed-pixel-color BLOCKED disposition) and this milestone's own explicit permission for this
outcome.

## 6. Known Issues

- **openMSX live-session A/B divergence beyond the first 2 halted-idle steps** (§5): reported as
  DIVERGENCE with a grounded, cited root-cause hypothesis, not swept under a PASS. A verification
  action for a future cycle is recorded in `docs/m23-parity-trace-diff.md` (a wall-clock-independent
  stepping technique, e.g. correlating each `step()` against `machine_info time`). Not required for
  C2's closure — the closure claim rests on the cited openMSX source + this project's own
  deterministic tests, which this finding does not contradict.
- **`src/main.cpp` was edited** — not in the planner's own "files to edit" list, but a small,
  additive, backward-compatible, fully-disclosed extension of the EXISTING `--parity-trace` mode
  (mirroring the established M19 `--cart1`/`--cart2` precedent for the SAME mode), needed to make
  the required C2 A/B evidence gate genuinely attemptable rather than trivially BLOCKED. Default
  behavior for every pre-M23 invocation is byte-identical (confirmed: the new parameter defaults to
  `0`, and the main stepping loop itself is untouched). Flagged here explicitly per the guardrails'
  "no scope beyond what was approved without a decision entry" discipline — recommend the
  coordinator/QA treat this as an accepted, disclosed, in-scope tooling addition rather than a
  silent scope expansion; no `decisions.md` entry was added by the developer (per the ledger
  instruction, that decision is left to the coordinator).
- **D4/C1's five-item remainder** stays explicitly OPEN, precisely named (§2.4 of the planner
  package, restated in `agent-protocol/state/deferred-backlog.md`'s new developer-readiness note):
  full per-mode slot tables + raster/`run_frame()` reconciliation; CPU-steals-command-engine-slot
  interaction; command-engine per-pixel/per-line VDP-cycle cost; exact sub-frame IRQ raster
  position; any actual CPU-execution gating (including "too-fast dropped-request" behavior). None
  of these were attempted this cycle, by design.
- No other residual risk identified. All 4 evidence-gate scripts (`validate-assets.ps1`,
  `checksum-assets.ps1`, `cmake --build`, `ctest`) ran clean; the conditional
  `openmsx-m23-halt-r-parity.ps1` → `m23-parity-trace-diff.md` gate ran and produced a real,
  honestly-labeled result (DIVERGENCE-with-root-cause, not PARITY, not fabricated).

## 7. Evidence Gate Outputs (captured, real)

```
$ tools/validate-assets.ps1
Asset validation result: True
BIOS directory: .../bios (7 files present: f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom,
  f1xvkdr.rom, f1xvkfn.rom, f1xvmus.rom)
ROM directory:  .../roms (2 files: aleste.rom, metalgear.rom)

$ tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to: docs/asset-checksums.txt   (SHA256, 7 BIOS + 2 ROM files, refreshed)

$ cmake --build build --config Debug
(succeeded; only pre-existing C4819 code-page warnings, zero errors)

$ ctest --test-dir build -C Debug --output-on-failure
100% tests passed, 0 tests failed out of 121

$ tools/openmsx-m23-halt-r-parity.ps1
M23 C2 A/B RESULT: DIVERGENCE. See docs/m23-parity-trace-diff.md
(exit code 1 -- an honest DIVERGENCE-with-root-cause disposition, not a harness failure: the
script's own exit-code contract treats "openMSX unreachable" (exit 2, BLOCKED) differently from
"openMSX reachable but fields diverge" (exit 1, DIVERGENCE) -- this run is the latter)
```

## 8. Backlog Disposition (developer-side readiness; ledger transition left to the coordinator)

- **C2 (Z80 HALT-R increment)** — ready to close **DONE (M23)**. Closed in full per §1.1/§2.2 of
  the planner package: R increments by exactly 1 (low 7 bits, bit 7 preserved) per halted `step()`
  call; the CPU core continues to return the bare datasheet `4`; the machine level correctly
  reports 5T via the pre-existing, unmodified M1-wait formula.
- **C1 (exact cycle/T-state timing parity)** — ready to advance **OPEN → IN-PROGRESS (M23
  partial)**. The HALT-refetch M1-wait sub-item closes via C2; the VDP-access-recovery-wait
  remainder is identical to D4's remainder (A-M23-5: no other independent wait-state source exists
  for this machine), carried forward as ONE item.
- **D4 (cycle-accurate VDP access-slot/command timing)** — ready to advance **OPEN →
  IN-PROGRESS (M23 partial)**. The raster-position derivation + `Delta` cost-unit contract + two
  separated, cited latency facts close this cycle as a real, tested, non-gating foundation; the
  full slot tables, CPU-priority-steals-command-engine-slot interaction, command-engine
  per-pixel/per-line VDP-cycle cost, exact sub-frame IRQ raster position, and any actual
  CPU-execution gating remain explicitly OPEN and precisely named, carried forward as ONE tracked
  row (mirrors the D7/C5 precedent — not force-closed, not split into a new letter).
- All other 31 backlog rows re-affirmed unchanged (no other row touched by this milestone) —
  see the developer-readiness note appended to `agent-protocol/state/deferred-backlog.md`.

## 9. QA Handoff

Please independently verify, from a clean rebuild:

1. `git diff v1.0.22 --stat` limited to `src/` and `tests/` matches exactly the file list in §4
   above (4 `src/` files, 2 `tests/` files + new untracked test/tooling files).
2. The 10 named zero-tolerance oracle files show a genuinely EMPTY `git diff v1.0.22` (re-run the
   command in §4 yourself; do not trust this report's claim alone).
3. `rg "\.halted\(\)" tests/` independently reproduces the same 22-file list, with
   `hbf1xv_cpu_step_integration_test.cpp` as the sole file that steps once more after halting.
4. Hand-recompute `8+5+5+5=23` and `t3=5` for the updated golden before accepting it (R-M23-1).
5. `ctest --test-dir build -C Debug --output-on-failure` reproduces 121/121 passed.
6. Read `docs/m23-parity-trace-diff.md` in full and assess whether the DIVERGENCE-with-root-cause
   disposition for C2's A/B evidence is a reasonable, honest characterization (not a hidden
   failure) — the raw trace files were regenerated fresh for this report and are reproducible via
   `tools/openmsx-m23-halt-r-parity.ps1` (requires WSL + `/usr/bin/openmsx`).
7. Assess the disclosed `src/main.cpp` scope note (§2/§6) — confirm the addition is genuinely
   additive/backward-compatible (default parameter, unchanged main loop) and does not touch any
   CPU-execution-gating path.
8. Confirm no file under `src/devices/video/vdp_frame_renderer.*`, `vdp_command_engine.*`, or
   `sprite_engine.*` was touched (§4's empty-diff command).
9. Review `agent-protocol/state/deferred-backlog.md`'s new developer-readiness note (§8 above) and
   confirm it accurately reflects what was and was not built this cycle before the ledger
   Status-column flip.

No scope beyond planner package §1.1/§1.2 was implemented; the one disclosed addition beyond the
planner's own file list (`src/main.cpp`) is called out explicitly above for QA/coordinator
judgment, not silently introduced.
