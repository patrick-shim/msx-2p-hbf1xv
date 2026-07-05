# M10 Planner Package (REQ-M10-002)

- Milestone ID: M10
- Title: Debug/Trace Infrastructure & openMSX Opcode-Trace Parity
- Spec Owner: MSX Planner Agent
- Source Request: REQ-M10-002
- Authority: `agent-protocol/project-baseline.md` (In-Scope debug/trace/parity clause),
  decision `DEC-0001` (2026-07-06), `agent-protocol/state/milestones.md` (M10),
  `agent-protocol/state/current-phase.md` (M10 planner-first, STOP-AND-ASK policy).
- Status: Planning artifact only. No production code, tests, or QA sign-off produced here.

This package decomposes M10 into ordered, dependency-sequenced implementable slices, defines the
exact R5 opcode-trace-parity acceptance test, maps evidence gates per slice, and — most
importantly — surfaces the prerequisites that likely require a SEPARATE milestone / human
decision so the coordinator can honor the human STOP-AND-ASK policy before any out-of-M10 work.

---

## 1. Scope and Assumptions

### 1.1 Non-negotiables carried into every slice

- Determinism: identical inputs produce identical trace/dump bytes across runs. Every new
  behavior carries a deterministic oracle (`tests/CLAUDE.md`). No wall-clock, no unseeded
  randomness, no environment-dependent formatting (fixed field order, fixed radix, fixed
  width, `\n`-normalized line endings for diffable artifacts).
- Timing fidelity: the CPU T-state model delivered in M9 is authoritative and must NOT be
  altered by M10. Trace records report T-states; they do not redefine them.
- Boundaries (`src/CLAUDE.md`): CPU-side hooks live in `src/devices/cpu/`; composition, memory
  regions, sinks, and output-path wiring live in `src/machine/`; no device logic in
  `src/core/`; no machine wiring in `src/devices/`; converters live in `tools/`.
- Anti-fabrication: NO parity result may be claimed without a genuine captured trace-diff
  artifact. If a faithful comparison cannot be produced, the slice reports BLOCKED with the
  real probe output and the coordinator STOPS AND ASKS.

### 1.2 Current baseline (verified in source this cycle)

- `src/machine/hbf1xv_machine.h`: the machine holds ONLY a flat `std::array<std::uint8_t,
  65536> memory_` behind `MachineBus`. There is NO slot/subslot register, NO memory mapper,
  NO SRAM region, NO VRAM region, NO VDP/PSG/RTC/FDC device wiring. Public surface includes
  `cold_boot()`, `run_frame/run_frames/run_cycles/run_until_cycle`, `step_cpu_instruction()`
  (returns per-instruction T-states), `load_memory(addr, bytes, size)`, `read_memory(addr)`,
  `cpu()`, `elapsed_cycles()`, `frame_count()`, `frame_cycles_per_frame()`.
- `src/devices/cpu/z80a_state.h` / `z80a_cpu.{h,cpp}`: full Z80A register state (main/shadow,
  IX/IY, I/R, IFF1/IFF2, IM0/1/2), full opcode families, full interrupt architecture — all
  QA-verified green (`ctest` 21/21, REQ-M9-010). No per-instruction trace-export exists.
- `tools/`: `validate-assets.ps1`, `checksum-assets.ps1`, `openmsx-ab-smoke.ps1` (already
  carries a real, non-fabricated R5 capability probe), `README.md`. No converters yet.
- openMSX 19.1 confirmed at WSL `/usr/bin/openmsx` (`docs/openmsx-ab-smoke.md`). Documented R5
  blocker reproduced: in the headless `-script` startup context `step` does not advance PC
  (`step_advanced=0`) and a flat write to `0xC000` reads back `0xFF` (unmapped at reset).

### 1.3 In scope for M10 (minimum, per DEC-0001)

- (a) Full-state debug dump — CPU registers/state, DRAM, SRAM, VRAM — plus execution-event
  logging under `debug/traces/` and `debug/logs/` (subfolders under `debug/` as needed).
- (b) Deterministic CPU trace-export facility (per-instruction, diff-friendly).
- (c) The MINIMUM machine slot/RAM/SRAM/VRAM wiring needed to run a comparable program to a
  defined checkpoint and to expose SRAM/VRAM as dumpable regions.
- (d) The real openMSX opcode-trace / per-instruction parity harness producing an actual
  captured trace-diff artifact (resolves R5).
- (e) Supporting `tools/` converters (memory->png, memory->audio) as needed.

### 1.4 Explicit non-goals (OUT of M10 unless a `decisions.md` entry approves)

- Real HB-F1XV BIOS boot and full slot/subslot/mapper behavioral fidelity.
- V9958 VDP device behavior (rendering pipeline, command engine, timing/contention). M10
  exposes an INERT, addressable VRAM byte region for dump/wiring only.
- FMPAC / SRAM device behavior and battery persistence semantics. M10 exposes an INERT,
  addressable SRAM byte region for dump/wiring only.
- MSX M1 wait-state / VDP-contention timing model (needed for EXACT cross-emulator cycle
  parity — see Section 4).
- Full I/O-port device bus (IN/OUT remain the M9 deterministic open-bus stubs).
- PSG/YM2413 audio synthesis (the memory->audio converter operates on dumped memory buffers,
  not on a synthesized audio pipeline).

These non-goals are the decision points enumerated in Section 4. They are flagged, not
assumed-resolved.

---

## 2. Spec Summary

M10 adds an observability and reference-comparison layer on top of the completed M9 CPU core,
plus the minimum memory-region wiring required for that layer to be meaningful and for a
controlled parity program to run to a checkpoint. The deliverables are:

1. A per-instruction CPU trace record + a deterministic text serialization suitable for
   line-by-line diffing.
2. A full-state debug dump (CPU + DRAM + SRAM + VRAM regions) and an execution-event log,
   written under `debug/`.
3. Minimum addressable DRAM/SRAM/VRAM regions on the machine so (2) has real content to dump
   and so a comparable program can be loaded and run to a checkpoint.
4. A parity harness that runs the SAME controlled program on this emulator and on openMSX,
   captures both per-instruction traces, and emits a REAL diff artifact — resolving R5.
5. `tools/` converters that turn dumped memory regions into PNG / audio artifacts.

Interfaces affected (all additive; no change to M9 CPU semantics):
`src/devices/cpu/z80a_cpu.{h,cpp}` (trace observer hook), `src/machine/hbf1xv_machine.{h,cpp}`
(memory regions + dump/trace sinks + output-path wiring), new `tools/` scripts, new `tests/`
suites, and new runtime output under `debug/`.

---

## 3. Slice Decomposition (M10-S1 .. M10-S5)

Ordering note (justified deviation from the request's suggested S2=dump / S3=wiring order):
the full-state dump's SRAM/VRAM coverage cannot exist until SRAM/VRAM regions exist, so the
MINIMUM memory-region wiring (S2 here) is sequenced BEFORE the complete dump (S3 here). DRAM
already exists; SRAM/VRAM do not. This is the only reordering and it is a dependency-correctness
call, not a scope change.

Dependency graph:

```
M10-S1 (CPU trace-export)                 -> depends on: M9 CPU (done)
M10-S2 (minimum DRAM/SRAM/VRAM regions)   -> depends on: none (machine-local)
M10-S3 (full-state dump + event log)      -> depends on: S1 (record reuse) + S2 (regions)
M10-S4 (openMSX opcode-trace parity)      -> depends on: S1 (our trace) + S2 (comparable run)
M10-S5 (tools converters memory->png/aud) -> depends on: S2 + S3 (dumped regions to convert)
```

### M10-S1 — Deterministic CPU trace-export facility

- Scope: add a per-instruction trace record capturing, at minimum: sequence index, PC (pre),
  opcode byte(s) fetched, AF/BC/DE/HL, AF'/BC'/DE'/HL', IX, IY, SP, I, R, IFF1, IFF2, IM,
  the decomposed flags (S,Z,Y,H,X,P/V,N,C), instruction T-state cost, and cumulative T-states.
  Provide a deterministic, fixed-layout text serialization (one line per instruction) and a
  thin observer hook so recording is opt-in and off by default (zero behavior change when off).
- Target files:
  - `src/devices/cpu/z80a_cpu.h` / `z80a_cpu.cpp` — a lightweight trace-observer callback
    invoked per completed instruction (no change to timing/semantics; observer is nullable).
  - `src/machine/hbf1xv_machine.h` / `hbf1xv_machine.cpp` — a concrete trace recorder/sink
    that formats records and writes to a caller-provided stream / `debug/traces/` path.
  - Boundary option to flag (see Section 4, minor): if a dedicated `src/debug/` module is
    preferred over housing the sink in `src/machine/`, that introduces a new top-level source
    folder not listed in `src/CLAUDE.md` and is a structural decision. Default recommendation:
    keep the CPU-side hook in `src/devices/cpu/` and the sink in `src/machine/` to avoid a
    structure decision.
- Tests:
  - Unit `tests/unit/devices/z80a_trace_export_unit_test.cpp` (suite
    `Devices_Z80ATraceExport_Unit`): a fixed small program produces a byte-identical trace
    string across two runs; every field present and correctly formatted; recording OFF yields
    zero records and identical CPU end-state vs a non-recorded run (proves no behavioral
    perturbation).
  - Integration `tests/integration/machine/hbf1xv_trace_program_integration_test.cpp` (suite
    `Machine_Hbf1xvTraceProgram_Integration`): run a known multi-instruction program through
    the machine boundary; assert the full trace text against a committed golden oracle and a
    hardcoded final cumulative T-state total.
- Acceptance criteria:
  - Trace serialization is deterministic and byte-stable across repeated runs (asserted).
  - Trace OFF is a no-op: identical CPU/machine end-state and cycle totals vs the M9 baseline
    (asserted).
  - Record includes every field listed above; format is documented in-file and fixed.
- Dependencies: M9 CPU core (done). None within M10.

### M10-S2 — Minimum machine DRAM/SRAM/VRAM region wiring

- Scope: introduce MINIMUM addressable memory regions on the machine so full-state dump has
  real content and a parity program can be loaded/run to a checkpoint. Specifically: keep the
  existing 64K DRAM; add an INERT SRAM byte region and an INERT VRAM byte region (fixed sizes,
  documented) with deterministic read/write accessors for dump/load. NO slot/subslot register
  behavior, NO mapper paging, NO VDP/FMPAC device semantics (those are Section 4 decision
  points). This is deliberately the "minimum wiring" of DEC-0001(c), not full machine fidelity.
- Target files:
  - `src/machine/hbf1xv_machine.h` / `hbf1xv_machine.cpp` — SRAM/VRAM arrays + accessors
    (`sram_size()`, `vram_size()`, region read/write/load helpers) alongside existing
    `load_memory`/`read_memory`. Region sizes and the fact they are inert must be documented.
- Tests:
  - Unit `tests/unit/machine/hbf1xv_memory_regions_unit_test.cpp` (suite
    `Machine_Hbf1xvMemoryRegions_Unit`): DRAM/SRAM/VRAM load+readback round-trips; reset/boot
    default contents are deterministic; region bounds are respected; regions are independent
    (a write to one does not alter another).
- Acceptance criteria:
  - DRAM, SRAM, and VRAM are each addressable, loadable, and dumpable with deterministic
    reset/boot defaults.
  - Regions are explicitly documented as INERT storage (no device behavior) pending the
    Section-4 device milestones.
  - No regression to existing machine stepping / cycle behavior (existing suites stay green).
- Dependencies: none within M10 (machine-local). Prerequisite for S3 and S4.
- SCOPE FLAG: if "minimum wiring" is interpreted to require real slot/subslot/mapper or real
  VDP/FMPAC behavior, that exceeds M10 — STOP AND ASK (Section 4, DP-1/DP-2/DP-3).

### M10-S3 — Full-state debug dump + execution-event logging

- Scope: implement a deterministic full-state dump (CPU registers/state + DRAM + SRAM + VRAM)
  and an execution-event log. Dumps write under `debug/traces/`; event logs under `debug/logs/`
  (subfolders under `debug/` for frames/events/audio may be added as needed). Reuse the S1
  record/formatter for the CPU portion.
- Target files:
  - `src/machine/hbf1xv_machine.h` / `hbf1xv_machine.cpp` — `dump_state(path)` (CPU + regions)
    and an event-log sink (e.g. instruction-retired / interrupt-serviced / HALT events).
  - Reuse S1 serialization in `src/devices/cpu/` for CPU register formatting.
- Tests:
  - Unit `tests/unit/machine/hbf1xv_debug_dump_unit_test.cpp` (suite
    `Machine_Hbf1xvDebugDump_Unit`): dump of a known state is byte-identical across runs and
    matches a committed golden; CPU/DRAM/SRAM/VRAM sections all present and correctly framed.
  - Integration `tests/integration/machine/hbf1xv_debug_event_log_integration_test.cpp`
    (suite `Machine_Hbf1xvDebugEventLog_Integration`): run a known program; assert the
    event-log sequence (e.g. instruction/interrupt/HALT events) against a golden oracle.
- Acceptance criteria:
  - Dump covers CPU state + DRAM + SRAM + VRAM, is deterministic, and writes under
    `debug/traces/`; event log writes under `debug/logs/`.
  - Golden-oracle byte-stability asserted across repeated runs.
- Dependencies: S1 (record reuse), S2 (SRAM/VRAM regions).

### M10-S4 — openMSX opcode-trace parity harness (resolves R5)

- Scope: produce a REAL captured per-instruction trace-diff between this emulator and openMSX
  19.1 for a controlled parity program run to a defined checkpoint. Extend
  `tools/openmsx-ab-smoke.ps1` or add a dedicated `tools/openmsx-trace-parity.ps1`
  (+ a Python diff helper if clearer) that: (1) runs the committed parity program on this
  emulator via the S1 trace-export to emit trace A; (2) drives openMSX to run the same program
  and emit a comparable per-instruction trace B; (3) normalizes and diffs A vs B on the agreed
  fields; (4) writes the ACTUAL diff outcome to a durable artifact.
- Reference program: a small, fixed, committed Z80 program (see Section 3 note below) — NOT the
  real BIOS. Optionally a zexdoc/zexall-style self-checking ROM as a broader-coverage secondary
  (signature-level) check.
- openMSX drive strategy (in priority order, each with a fallback):
  1. Drive openMSX post-boot via `openmsx -control` stdio single-stepping after the machine
     reaches a RAM-ready state, loading the same program bytes at the same base address and
     stepping. (Assumption: post-boot `step` advances PC even though the `-script` startup-
     context probe showed `step_advanced=0`. Verification action below.)
  2. If per-instruction stepping is not drivable, fall back to a self-checking-ROM signature
     comparison (final CRC / pass-fail signature comparable across both emulators) and record
     that this is a signature-level (not per-instruction) parity result.
  3. If neither faithful comparison is producible, the slice reports BLOCKED with the real
     captured probe output and the coordinator STOPS AND ASKS. NO parity is asserted.
- Target files:
  - `tools/openmsx-trace-parity.ps1` (or extended `tools/openmsx-ab-smoke.ps1`), optional
    `tools/trace-diff.py`; parity program committed under `tests/parity/` (e.g.
    `tests/parity/z80_parity_checkpoint.bin` + a documented source/listing).
  - Artifact: `docs/m10-parity-trace-diff.md` (dedicated) and/or the R5 section of
    `docs/openmsx-ab-smoke.md`, containing the ACTUAL diff outcome.
- Tests:
  - Integration `tests/integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp`
    (suite `Machine_Hbf1xvParityCheckpoint_Integration`): our-side trace for the parity program
    reaches the defined checkpoint deterministically and matches a committed golden (this makes
    the emulator's half of the diff regression-locked, independent of the WSL/openMSX
    environment).
- Acceptance criteria: see Section 4 (R5 acceptance test). A real diff artifact must exist;
  parity is defined as architectural-state diff empty over the program to the checkpoint.
- Dependencies: S1 (our trace-export), S2 (comparable program can load/run to checkpoint).
- SCOPE FLAG: if faithful parity is judged to require real BIOS boot / full slot+VDP wiring,
  STOP AND ASK (Section 4, DP-1/DP-2/DP-4).

### M10-S5 — `tools/` converters (memory -> png / memory -> audio)

- Scope: deterministic converters that transform dumped memory regions (from S3) into
  inspectable artifacts: memory/VRAM buffer -> PNG, and memory buffer -> audio (e.g. WAV) as
  raw-buffer visualizers/serializers. These operate on dumped byte buffers only; they do NOT
  synthesize VDP frames or PSG audio (those are Section-4 device milestones).
- Target files: `tools/mem-to-png.py`, `tools/mem-to-audio.py` (+ `tools/README.md` update).
- Tests / evidence: deterministic conversion of a fixed input buffer yields a byte-identical
  output artifact across runs (self-check within the script or a small fixture); recorded in
  the slice implementation report. (Converters are `tools/` scripts; C++ ctest coverage is not
  required, but determinism must be demonstrated.)
- Acceptance criteria:
  - Given a fixed dumped buffer, each converter emits a byte-stable artifact across runs.
  - Converters are documented in `tools/README.md` with usage and determinism notes.
- Dependencies: S2 (regions) + S3 (dump output to convert).

Parity program note (applies to S4): keep the committed parity program self-contained,
RAM-only, no I/O, terminating in a HALT at a known PC after a bounded fixed instruction count,
and exercising a representative opcode mix (unprefixed ALU/load/branch, CB, an ED block op,
and a DD/FD indexed op). This maximizes cross-emulator comparability while avoiding the
open-bus I/O stubs and any device dependence.

---

## 4. R5 Resolution Acceptance Test (exact definition)

R5 = "openMSX opcode-trace / per-instruction parity." This is resolved ONLY by a genuine,
captured trace-diff artifact. The exact definition:

- Test program / state: the committed, fixed parity program (`tests/parity/`), RAM-only, no
  I/O, loaded at a fixed base address, both emulators started with identical initial CPU state
  (PC at base, defined SP, registers per a documented initial vector), run instruction-by-
  instruction to the defined checkpoint (HALT at a known PC after a bounded fixed instruction
  count). Optional secondary: a zexdoc/zexall-style self-checking ROM compared at signature
  level.
- Reference (B): openMSX 19.1 on WSL `/usr/bin/openmsx`, driven to run the SAME program bytes
  at the SAME base address and single-stepped (Section 3, S4 drive strategy).
- Subject (A): this emulator via the S1 trace-export for the same program.
- Fields compared per instruction: PC (pre-execution), opcode byte(s) fetched, A, F with each
  flag bit (S, Z, Y, H, X, P/V, N, C), B, C, D, E, H, L, IX, IY, SP, I, R, IFF1, IFF2, IM, and
  the T-state cost (instruction + cumulative).
- Pass condition (primary): a REAL captured diff of the two per-instruction traces over the
  whole program to the checkpoint is EMPTY for architectural state — PC, opcode, and all
  registers/flags (A,F+bits, B,C,D,E,H,L, IX, IY, SP, I, R, IFF1, IFF2, IM) match on every
  instruction.
- T-state / cycle field (secondary, explicitly bounded): compared and reported separately. If
  the two sides diverge only by MSX M1 wait-state insertion (a known, structural difference —
  see risk below), that is recorded with a DECLARED handling: either (i) exclude wait-state
  timing from the pass gate and report canonical Z80 T-state parity, or (ii) model MSX M1 wait
  states — which is a Section-4 decision point, NOT assumed in M10. The architectural-state
  diff being empty is the R5 pass gate; the cycle field is reported with its declared handling.
- Artifact requirement (hard): the actual diff outcome is captured to `docs/m10-parity-trace-
  diff.md` (and/or the R5 section of `docs/openmsx-ab-smoke.md`). NO parity result — pass or
  fail — may be claimed without this real captured diff. If the comparison cannot be produced,
  the harness records the real blocker output and the coordinator STOPS AND ASKS; parity stays
  UNRESOLVED (never fabricated, never assumed).

---

## 5. Dependency & Risk Map — prerequisites that may need a SEPARATE milestone

M10 as scoped (Section 1.3) is deliverable with MINIMUM, inert memory-region wiring. However,
faithful per-instruction parity against a REAL MSX boot, and a truly "full-machine" dump, could
be read to require subsystems large enough to warrant their OWN milestone. Each is classified
below as (i) in-scope-minimum for M10 or (ii) a candidate SEPARATE milestone requiring a
coordinator/human decision. Per the human STOP-AND-ASK policy (`current-phase.md`, REQ-M10-001),
the developer must halt and ask before implementing any item marked (ii).

- DP-1 — Full slot/subslot/mapper + real BIOS-boot fidelity.
  - Needed for: parity that boots the actual HB-F1XV BIOS and matches openMSX's full-machine
    execution.
  - Classification: (ii) SEPARATE milestone. M10 uses a controlled RAM-only parity program, NOT
    a BIOS boot. If R5 is interpreted to require real BIOS boot, STOP AND ASK.
  - M10 in-scope-minimum instead: inert flat/region memory sufficient to run the committed
    parity program to a checkpoint (S2).

- DP-2 — V9958 VDP device (VRAM command engine, rendering, timing/contention).
  - Needed for: VRAM "content" that reflects real VDP behavior; any display parity.
  - Classification: (ii) SEPARATE milestone (a full device workstream). M10 exposes an INERT,
    dumpable VRAM byte region only (S2). If VRAM dump is expected to reflect real VDP
    rendering, STOP AND ASK.

- DP-3 — FMPAC / SRAM device (mapper, battery-backed persistence semantics).
  - Needed for: real SRAM behavior and persistence.
  - Classification: (ii) SEPARATE milestone. M10 exposes an INERT, dumpable SRAM byte region
    only (S2). If SRAM dump is expected to reflect real FMPAC behavior, STOP AND ASK.

- DP-4 — MSX M1 wait-state / VDP-contention timing model.
  - Needed for: EXACT cross-emulator T-state parity (openMSX inserts MSX-specific M1 wait
    states; the M9 core uses canonical Z80 T-states).
  - Classification: (ii) SEPARATE milestone OR an explicit tolerance decision. M10 gates R5 on
    architectural-state parity and reports the cycle field with declared handling (Section 4).
    If EXACT cycle parity is required in M10, STOP AND ASK.

- DP-5 — Full I/O-port device bus (real IN/OUT backing).
  - Needed for: programs using ports; block-I/O data fidelity.
  - Classification: (ii) SEPARATE milestone (already noted in M9 as a "later milestone"). M10's
    parity program is deliberately I/O-free so the open-bus stubs are not exercised.

- DP-6 (minor, structural) — a dedicated `src/debug/` source folder.
  - `src/CLAUDE.md` lists strict folders (core/devices/peripherals/machine/frontend). Housing
    debug code in a new `src/debug/` is a small structure decision. Default: avoid it — put the
    CPU-side hook in `src/devices/cpu/` and the sink in `src/machine/`. Flagged for awareness,
    not blocking.

In-scope-minimum for M10 (no decision needed): S1 trace-export; S2 inert DRAM/SRAM/VRAM
regions; S3 full-state dump + event log; S4 controlled RAM-only parity program + real trace
diff; S5 memory->png/audio converters.

---

## 6. Evidence-Gate Mapping (per slice)

All slices run the standard gates and capture real output (never fabricated). Behavior of the
gates:

- `tools/validate-assets.ps1` — required BIOS present + >=1 ROM.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums.
- `cmake --build build --config Debug` — build succeeds.
- `ctest --test-dir build -C Debug --output-on-failure` — deterministic suite passes (must
  include all existing M9 suites still green + the new suites for the slice).
- `tools/openmsx-ab-smoke.ps1` / `tools/openmsx-trace-parity.ps1` -> `docs/openmsx-ab-smoke.md`
  (and `docs/m10-parity-trace-diff.md`) — real A/B parity evidence; required for S4.

| Slice | validate-assets | checksum-assets | build (Debug) | ctest | openMSX parity artifact |
|-------|-----------------|-----------------|---------------|-------|-------------------------|
| M10-S1 | yes | yes | yes | yes (+ trace-export suites) | not required |
| M10-S2 | yes | yes | yes | yes (+ memory-region suite) | not required |
| M10-S3 | yes | yes | yes | yes (+ dump/event-log suites) | not required |
| M10-S4 | yes | yes | yes | yes (+ parity-checkpoint suite) | REQUIRED — real captured trace-diff |
| M10-S5 | yes | yes | yes | yes (existing suites stay green) | not required (converter determinism demonstrated) |

Milestone-close gate: after S4, R5 is resolved ONLY if `docs/m10-parity-trace-diff.md`
(and/or the R5 section of `docs/openmsx-ab-smoke.md`) contains a REAL captured diff whose
architectural-state comparison is empty over the parity program to the checkpoint. QA sign-off
is required before M10 closes; existing suites (all M9 CPU suites) must remain green.

---

## 7. Risks and Assumptions (each with a verification action)

- Assumption: openMSX 19.1 CAN be driven to single-step a loaded program per-instruction
  post-boot via `-control` stdio, even though the `-script` startup-context probe showed
  `step_advanced=0` (`docs/openmsx-ab-smoke.md`). Verification action (S4): attempt post-boot
  `-control` single-stepping; if it still fails, fall back to signature-level parity, and if
  that fails too, report BLOCKED and STOP AND ASK.
- Assumption: a RAM-only, I/O-free parity program can be loaded at the same base address in
  both emulators AFTER openMSX reaches a RAM-ready state (the probe's `0xC000 -> 0xFF` was at
  reset before slot config). Verification action (S4): confirm readback of the loaded program
  bytes on the openMSX side before stepping; if RAM is not addressable without full slot
  config, that promotes DP-1 and triggers STOP AND ASK.
- Risk (High, structural): EXACT T-state parity may be impossible without modeling MSX M1 wait
  states (DP-4). Mitigation: R5 pass gate is architectural-state parity; cycle field reported
  with declared handling. Verification action: measure the per-M1 delta on the parity program
  and record it in the diff artifact.
- Risk (Medium): "full-state dump" and "minimum wiring" could be read to require real
  VDP/FMPAC/slot behavior (DP-1/2/3). Mitigation: M10 exposes INERT regions and documents them
  as such; any expansion is a STOP-AND-ASK decision. Verification action: confirm the
  interpretation with the coordinator before S2 if ambiguity is raised.
- Risk (Medium): trace-observer hook could perturb CPU timing/semantics. Mitigation: observer
  is nullable and off by default; S1 asserts trace-OFF produces identical end-state/cycles vs
  the M9 baseline. Verification action: the S1 no-op regression test.
- Risk (Low): line-ending / locale / radix differences make trace text non-deterministic across
  environments. Mitigation: fixed field order, fixed hex width, `\n`-only, ASCII-only.
  Verification action: S1/S3 golden-oracle byte-stability tests run twice.
- Assumption: `tools/` Python converters are acceptable without C++ ctest coverage, with
  determinism demonstrated via a fixed fixture (consistent with existing `tools/` PowerShell
  scripts not being ctest targets). Verification action: include a self-check in each converter
  and record output in the S5 implementation report.
- Risk (Low, structural): introducing `src/debug/` (DP-6). Mitigation: default to existing
  folders. Verification action: none required unless the team elects the new folder.

---

## 8. Handoff Summary (for the coordinator / developer)

- Recommended execution order: S1 -> S2 -> S3 -> S4 -> S5 (S3 and S4 both depend on S1+S2 and
  may proceed once those land; S5 last).
- Planner-first is satisfied by this package. No production code, tests, or QA sign-off were
  produced here.
- CRITICAL before any implementation: the developer must STOP AND ASK if any slice is found to
  require an item classified (ii) in Section 5 (DP-1 real BIOS/slot fidelity, DP-2 V9958 VDP,
  DP-3 FMPAC/SRAM behavior, DP-4 exact-cycle wait-state model, DP-5 full I/O bus). M10 as
  planned uses inert regions + a controlled RAM-only parity program and does NOT require these.
- R5 is resolved ONLY by a genuine captured trace-diff (Section 4). No parity may be claimed
  otherwise; if unproducible, STOP AND ASK.
