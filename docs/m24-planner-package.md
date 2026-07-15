# M24 Planner Package — ZEXDOC/ZEXALL Full Parity Sweep (closes backlog C3)

- Milestone ID: M24
- Title: ZEXDOC/ZEXALL Full Parity Sweep — genuine CP/M/BDOS shim + full-suite run against the
  existing, QA-verified Z80A core
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M24-001 (kickoff, planner-first, no production code;
  `agent-protocol/channels/requests.md:1156-1164`)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package restates all
  34 rows), the human's 2026-07-08 "zero license-sensitive future work" standing directive
  (`agent-protocol/state/current-phase.md:3`), the human's 2026-07-08 M24-M25 continuation directive
  (same cadence as the pre-authorized M21-M23 run: planner → developer → QA → release
  decision/tag, proceeding without an extra pause on a clean QA PASS, STOP-and-consult otherwise).
- Backlog effect this cycle: **C3 (ZEXDOC/ZEXALL full parity sweep) → target: closes IN FULL**, subject
  to the runtime-feasibility and A/B-scope caveats named in §1/§3.4/§7 below (if genuine, honestly-reported
  test failures are found, C3 still "closes" in the sense of "the sweep ran to completion and reported
  real results" — see Acceptance Criterion 4 for the precise, non-negotiable disposition rule). No other
  backlog row is touched. All 34 rows re-affirmed in §5.
- Gate: normal human-release-decision gate; per the human's 2026-07-08 directive the coordinator is
  pre-authorized to proceed through the release-decision/tag step for this M24-M25 continuation without
  an additional pause, UNLESS QA does not reach a clean PASS (real blocker → stop and consult the human).

> Grounding rule: every claim about `zexall.z80`/`zexdoc.z80`'s actual behavior below was independently
> verified this cycle by directly reading `references/zexall/zexall.z80` (1547 lines, read in full) and
> `references/zexall/zexdoc.z80` (spot-verified structurally identical via targeted greps + a direct read
> of its `tests:` table and flag-mask bytes), plus `references/zexall/zexlax.pl`, `README.md`, `LICENSE`,
> and `.gitattributes`. The coordinator's preliminary summary is treated as a hypothesis to verify, not a
> fact to trust — §1.3 documents one place it was imprecise (the C=2 call-site claim) and corrects it.
> `references/zexall/` (GPL-licensed, YAZE-AG project) is a **behavior/validation reference, like
> `references/openmsx-21.0/`** — read for understanding; its assembly source and data tables are **never
> copied into `src/`** (§1.5 makes this explicit and auditable).

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) Commit `references/zexall/`** (present, uncommitted since M19/DEC-0015's discovery note). The
  existing `.gitattributes` (read this cycle) already declares `*.com binary`, `*.z80`/`*.mac` text with
  `linguist-language=Assembly`, and `README.md`/`LICENSE` as text — a normal `git add references/zexall/`
  needs no new attribute plumbing.
- **(b) A new, genuinely reusable, generic CP/M-style BDOS-trap harness** (`src/machine/cpm_bdos_harness.*`,
  §3.1) implementing exactly the three mechanics `zexall.z80`/`zexdoc.z80` actually use (verified by direct
  read, §1.3): load a flat `.com`-style image at `0x0100`, seed the CP/M "top of memory" word at address
  `0x0006-0x0007`, and trap `CALL 5` (BDOS dispatch for functions **C=2** console-char-out and **C=9**
  `$`-terminated string-out) plus `JP 0` (CP/M warm-boot = program-finished signal). This is **original,
  independently-designed mechanism code grounded in the standard, third-party, non-GPL CP/M loading
  convention** (Digital Research's own OS design, which `zexall.z80` merely relies on as a client — not
  something the YAZE-AG project invented) — **not** a transliteration of `zexall.z80`'s own assembly (§1.5).
- **(c) A dedicated system-integration test** that runs **both** `zexall.com` and `zexdoc.com` to genuine
  completion (the CP/M warm-boot trap fires) inside `ctest`, using the harness's C++ API directly (no
  subprocess spawn — faster, less flaky), and asserts real, captured, unfabricated per-group PASS/FAIL
  results (§3.2/§3.3) for all 67 named test groups in each suite (§1.4).
- **(d) `tools/zexall-report.py`** (new): parses the harness's captured BDOS-output text into a structured,
  human-readable per-group PASS/FAIL table, using ONLY the black-box binary's own **observed runtime
  output markers** (§1.5) — never the source's internal group names/CRC constants.
- **(e) A small, additive `--cpm-run` CLI mode** in `src/main.cpp` (mirrors the established `--parity-trace`
  / `--bios-boot-trace` shape, §3.1) for external/manual reproduction and for the `tools/`-driven openMSX
  A/B script.
- **(f) Real openMSX A/B evidence**, explicitly bounded per the analysis in §3.4 (a genuine, feasible
  bounded-prefix cross-engine check exists; a full live-Tcl sweep and a "boot a real MSX-DOS disk" technique
  do **not** — both are named and honestly marked BLOCKED/deferred rather than fabricated).
- **(g) Deterministic unit tests** for the harness mechanism itself, against a tiny, **hand-written-by-this-
  project** synthetic CP/M-style fixture (not `zexall.com`/`zexdoc.com`) — proving the shim's mechanics work
  in isolation before ever touching the real GPL binaries (§3.3).
- **(h) Full deferred-backlog review** — all 34 rows re-affirmed (§5).
- **(i) Zero regression** across the FULL M1-M23 suite (121 tests).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M24 | Owning milestone (candidate) |
|---|---|---|
| **A genuine full-suite live-Tcl single-step A/B trace-diff of the entire ZEXALL/ZEXDOC run against openMSX** (tens of millions of individual Z80 steps, §3.5) | The M23 A/B evidence (`docs/m23-parity-trace-diff.md`) already independently discovered and documented that a live openMSX Tcl session's own scheduler behaves differently under real-time-vs-step-count pressure once enough wall-clock time elapses between round-trips — a per-step Tcl round-trip at this volume is not a feasible, honest technique. | Not owned by any future milestone; a structural environment limit, like the M21 "no computed-pixel-color debuggable" finding. |
| **Booting a genuine MSX-DOS disk image to a command prompt and running `zexall.com`/`zexdoc.com` as real MSX-DOS transient programs**, on either engine | Requires (i) an MSX-DOS system-disk asset this project has not confirmed possessing (verification action, §7 R-M24-6), and (ii) a live, scripted keystroke-driven "boot to a prompt" technique this project has never used, which depends on resolving the still-**IN-PROGRESS** backlog item **C5** (full boot past first device read) — C5 explicitly remains open per M16's carried-forward note (`agent-protocol/state/deferred-backlog.md` row C5). Conflating C3's closure with C5's remainder would misrepresent one backlog item as discharging another. | C5's own future closure (unrelated to C3) |
| **Fixing any genuine CPU-core defect the sweep might discover** | Per the task's explicit framing, this is a CPU-**validation** milestone against an already-QA-verified core (M9/M10/M12). If the sweep finds a real defect, that is a SIGNIFICANT FINDING requiring careful triage (root-cause, regression-risk assessment against the M9/M12 zero-tolerance timing oracles, a possible new milestone) — not a same-cycle silent patch. See Acceptance Criterion 4 and R-M24-1. | A dedicated future CPU-hardening milestone, only if triggered |
| **Assembling `zexall.mac`/`zexdoc.mac`/`zexall.z80`/`zexdoc.z80` from source** | The repository already contains the pre-built, ready-to-run `.com` binaries (`references/zexall/zexall.com`, `zexdoc.com`); the `.z80`/`.mac` sources are read-only grounding material for this package, exactly like `references/openmsx-21.0/` is read for behavior, never compiled/vendored. No assembler dependency is introduced. | N/A |
| **Any change to `src/devices/cpu/*` or `src/devices/video/*`** | This milestone is a validation harness addition, not a CPU/VDP change. If C3 surfaces a genuine defect, the FIX (if any) is explicitly out of THIS milestone's scope per the item above. | N/A this cycle |

### 1.3 Assumptions (each grounded by direct source read, each with a verification action)

- **A-M24-1 (CP/M `.com` load convention: `org 100h`, confirmed identical in BOTH files).** Read
  `references/zexall/zexall.z80:20-23` and `references/zexall/zexdoc.z80:20-23`: both begin
  `aseg` / `org 100h` / `jp start`. This is the standard CP/M TPA base — the assembler places the first
  emitted byte (the 3-byte `jp start` instruction) at address `0x0100`; a `.com` file loader simply copies
  the raw file bytes starting at `0x0100` and transfers control there. No other convention is used or
  needed. *Verify:* the developer confirms at implementation time that `load_memory(0x0100, bytes, size)`
  followed by `cpu().state().regs().pc = 0x0100` reproduces this (no special first-byte handling required).
- **A-M24-2 (the "top of memory" word at address 6-7, confirmed identical mechanism in both files).** Read
  `zexall.z80:80-81` / `zexdoc.z80:80-81`: `start: ld hl,(6) / ld sp,hl` — the program's FIRST action is to
  load a 16-bit value from address `0x0006` and use it, unmodified, as the initial stack pointer. This is
  the standard CP/M "top of TPA" convention (real CP/M stores the BDOS base there; a program's SP is set at
  or just below it). Since this harness **traps** `CALL 5` rather than hosting real BDOS machine code in
  memory, the exact numeric value stored at 6-7 need not correspond to any real BDOS address — it only needs
  to be a safe stack-pointer seed clear of the loaded program image. *Verify:* the developer confirms the
  actual `.com` file sizes (not measured this cycle — a verification action, R-M24-2) and picks/validates a
  safe constant (§3.1 proposes `0xFF00`, with a defensive size-guard in the harness itself so this remains a
  genuinely reusable primitive, not a zexall-specific magic number).
- **A-M24-3 (exactly two real BDOS functions are invoked, C=9 and C=2 — CONFIRMED, with ONE correction to
  the coordinator's preliminary summary, found by direct read, not assumed).** All BDOS access in both files
  routes through a single local subroutine (`zexall.z80:1194-1203` / identical in `zexdoc.z80`):
  ```
  bdos:   push af / push bc / push de / push hl
          call 5
          pop hl / pop de / pop bc / pop af
          ret
  ```
  Every call site in the program calls this `bdos:` label (never `call 5` directly except inside `bdos:`
  itself) — so the CPU's PC reaches the literal address `0x0005` exactly once per BDOS invocation, at a
  single, generic choke point, regardless of which caller invoked it. Grepping `ld\s+c,\s*(2|9)` /
  `call\s+bdos`/`call\s+5` across the full file (done this cycle) finds:
  - **C=9** (print `$`-terminated string, `DE` points to it): the startup banner (`msg1`), the per-test-group
    descriptive name (printed once per group, BEFORE its combinatorial loop runs), the "  OK\r\n"/error
    messages, and the final "Tests complete" banner (`msg2`) before `jp 0`.
  - **C=2** (console char-out, byte in `E`): **the coordinator's summary said this is "used somewhere in the
    per-test-case reporting path" — independently verified this cycle to be imprecise in one respect: TWO
    of the THREE `ld c,2`/`call bdos` call sites inside the `test:` harness procedure (`zexall.z80:1063-1076`
    and `:1112-1127`) are wrapped in assembler `if 0 ... endif` conditional blocks — i.e. `if 0` is always
    false, so this diagnostic/debug code is NEVER actually assembled into the shipped `.com` binary.** The
    ONE genuinely live, always-assembled `ld c,2 / call bdos` site is inside `phex1` (`zexall.z80:1176-1192`,
    "display low nibble in a"), called by `phex2`→`phex8`, which is invoked ONLY from the CRC-mismatch
    (FAIL) reporting path (`zexall.z80:788,793`) to print each hex digit of the expected/actual 32-bit CRC
    one character at a time. **Net correction: C=2 is real, live, reachable code — but ONLY on a test-group
    FAILURE (mismatch), never on success or during normal group-name printing.** This is exactly the class
    of "verify the actual source, don't trust the summary" finding this project's culture demands.
    *Verify:* the developer re-runs the identical greps/`if 0` scan at implementation time and confirms this
    disposition before finalizing the BDOS dispatch table.
- **A-M24-4 (message-format grammar for PASS/FAIL, read directly, not inferred).** `zexall.z80:1205-1210` /
  identical labels in `zexdoc.z80`:
  ```
  msg1:   db 'Z80 instruction exerciser',10,13,'$'      ; startup banner (LF then CR, NOT the conventional
  msg2:   db 'Tests complete$'                           ;  CR-then-LF order — read exactly as written)
  okmsg:  db '  OK',10,13,'$'
  ermsg1: db '  ERROR **** crc expected:$'
  ermsg2: db ' found:$'
  crlf:   db 10,13,'$'
  ```
  Each named test group (`tmsg` macro, `zexall.z80:183-192`) prints a fixed 30-character, `'.'`-padded
  descriptive string (content is this project's FIRST-HAND OBSERVATION of the binary's own runtime output,
  not transcribed source — see §1.5) via C=9, exactly once, BEFORE that group's combinatorial test loop
  runs. After the loop exhausts every combinatorial iteration for that group and compares the accumulated
  32-bit CRC (`updcrc`/`cmpcrc`, `zexall.z80:766,782` and `:1212-1271`) against the descriptor's expected
  value: on match, prints `"  OK"` + LF,CR (C=9, one call); on mismatch, prints `"  ERROR **** crc
  expected:"` (C=9) + 8 hex digits of the EXPECTED crc (C=2 × 8, via `phex8`/`phex2`/`phex1`) + `" found:"`
  (C=9) + 8 hex digits of the ACTUAL crc (C=2 × 8) + LF,CR (C=9). This means: **the harness can reliably
  detect PASS vs FAIL per named group by scanning the captured output stream for the literal, fixed
  substrings `"  OK"` (success) vs `"  ERROR **** crc expected:"` (failure)**, in the order they appear,
  without needing to know the 67 group names in advance. *Verify:* the developer's parser
  (`tools/zexall-report.py`) is unit-tested against a small, synthetic captured-output fixture proving this
  exact grammar before being trusted on the real binaries.
- **A-M24-5 (both suites test the IDENTICAL 67 named instruction-family groups, in the identical order — an
  independently re-derived finding, not assumed from the suite names).** Both `tests:` dispatch tables
  (`zexall.z80:100-168`, `zexdoc.z80:100-168`) list the exact same 67 labels in the exact same order
  (`adc16` … `stabd`, terminated by `dw 0`), independently counted this cycle. Spot-reading the `adc16`
  entry in both files shows byte-for-byte identical base-case/increment/shift-vector combinatorics, differing
  ONLY in the flag-mask byte (`zexall.z80:195`: `0FFh`; `zexdoc.z80:195`: `0C7h`) and, consequently, the
  expected CRC constant (§1.3 A-M24-7 explains the shared origin). *Verify:* the developer independently
  re-diffs both `tests:` tables at implementation time (a two-line `diff` is sufficient) before relying on
  "same 67 groups, same order" for any shared tooling.
- **A-M24-6 (total combinatorial instruction-execution volume — independently summed this cycle from the
  source's own per-group annotated cycle counts, not an externally-recalled number).** Each group's header
  comment states its own total combinatorial iteration count (e.g. `; add hl,<bc,de,hl,sp> (19,456 cycles)`,
  `zexall.z80:202`), independently cross-checked against the PRODUCT of that group's two sub-vector
  annotations (e.g. `(512 cycles)` × `(38 cycles)` = `19,456`, confirmed matching in every spot-checked
  case). Summing all 67 groups' totals this cycle (arithmetic shown, not merely asserted): **≈1,727,400**
  total individual instruction-under-test executions per full run of ONE suite (zexall.z80 OR zexdoc.z80 —
  identical combinatorics per A-M24-5). This is the count of instructions the ORIGINAL AUTHOR'S own
  comments describe as "test cases"; it is **NOT** the same as "67 test groups" (the PASS/FAIL reporting
  granularity, §1.4) NOR the total number of Z80 instructions this project's `step_cpu_instruction()` will
  actually execute (§1.4/§3.5 — the driver/bookkeeping code the test program ITSELF executes around every
  one of those ~1.7M iterations is additional, unmeasured overhead this milestone must measure empirically,
  not estimate from the source's comments alone). *Verify:* QA independently re-sums at least a sample of
  the 67 per-group totals by hand before accepting this figure (mirrors this project's established
  "recompute, don't trust" discipline, e.g. M23's A-M23-4).
- **A-M24-7 (zexall.z80/zexdoc.z80 are machine-generated from a shared template — independently confirmed,
  not assumed from the README).** `references/zexall/zexlax.pl:1-10` is a Perl generator that reads a
  shared `zexlax` template (after `__DATA__`, not read this cycle — irrelevant to the shipped `.com`/`.z80`
  files already present) and emits either the "full-flag" (`zexall`) or "documented-flag" (`zexdoc`) variant
  by substituting the flag-mask byte and selecting one of two pre-computed expected-CRC values per an
  `@c doc,all` template directive. This independently explains (not merely restates) why the two `.z80`
  files are structurally identical except for flag-mask bytes and expected CRCs (A-M24-5). *No verification
  action needed* — this is purely explanatory grounding, not a load-bearing behavioral claim.
- **A-M24-8 (the program never touches real I/O ports or asserts any interrupt-generating device access —
  a device-isolation claim grounded in the source's own design description, with a cheap defensive
  verification).** The source's own design comments (`zexall.z80:34-45`) state each group's base
  instruction + increment/shift bit-vectors are hand-chosen to stay within ONE instruction family; spot
  reads confirm every group's base opcode is a data-processing instruction (ADC/ADD/ALU/BIT/CPD/CPI/DAA/
  INC/DEC/LD-family/NEG/RLD/RRD/rotate/SET/RES/ST(register-to-memory)/LD(BC,DE),A) — none is an `IN`/`OUT`
  family opcode, and the documented bit-vectors only ever touch operand/register-selector bits, never
  opcode-family-selector bits that could accidentally produce an `IN`/`OUT` opcode. Interrupts are
  explicitly `DI`-guarded around the sensitive test-execution window (`zexall.z80:1077,1097`) and this
  harness never calls `run_frame()` (so no VDP vsync/line IRQ source is ever exercised — mirrors the
  established, already-safe M21/M22 system-test pattern of driving the CPU purely via
  `step_cpu_instruction()`). *Verify:* the system test snapshots PSG/VDP/PPI/RTC/FDC device state
  immediately after `cold_boot()`+`map_flat_ram()` and again after the full sweep completes, asserting
  byte-for-byte equality — a cheap, concrete, "nothing strayed into real I/O" invariant (Acceptance
  Criterion 7).

### 1.4 Clarifying "test case" (the task's own ambiguous term) — TWO distinct, both-reportable numbers

- **67 named, individually PASS/FAIL-REPORTED test GROUPS** per suite (§1.3 A-M24-5) — this is the
  granularity the program's OWN output actually reports at (one "  OK"/"  ERROR" line per group); this is
  what Acceptance Criterion 2/3 requires the harness to parse and report per-group.
- **≈1,727,400 individual instruction-under-test EXECUTIONS** per suite (§1.3 A-M24-6), the finer-grained
  combinatorial volume each group's CRC actually integrates over — not separately pass/fail-reported by the
  program itself (a group's CRC either matches or it doesn't, as one atomic verdict over its whole
  combinatorial space), but directly relevant to runtime feasibility (§3.5).

### 1.5 License-isolation confirmation (per the human's "zero license-sensitive future work" directive)

- **Confirmed, explicitly, before any implementation begins:** running the pre-built `zexall.com`/
  `zexdoc.com` binaries as black-box DATA test fixtures (loaded into RAM exactly as `bios/`, `roms/`, and
  `tests/parity/z80_parity_checkpoint.bin` assets already are throughout this project) is categorically
  different from, and far lower-risk than, transcribing a GPL project's own source code or data tables into
  `src/`. This mirrors the treatment `references/openmsx-21.0/` already receives project-wide.
- **Nothing from `zexall.z80`/`zexdoc.z80`'s own Z80 assembly source — including the 256-entry×4-byte CRC
  table, the 67 test descriptors' base-case/increment/shift-vector byte constants, or the expected-CRC
  constants — is copied into `src/` anywhere.** The new `src/machine/cpm_bdos_harness.*` class implements
  ONLY the generic, third-party (Digital Research CP/M) loading convention independently described in
  §1.3 A-M24-1/A-M24-2/A-M24-3 (org `0x0100`, word-at-`0x0006` SP seed, trap `CALL 5`/`JP 0`) — a convention
  that predates and is external to the YAZE-AG/zexall project, exactly as the Z80/CP/M ecosystem's own
  public documentation describes it, not a transliteration of `zexall.z80`'s own code.
- **The one design point closest to "derived from the binary's own content"** is `tools/zexall-report.py`'s
  recognition of the literal runtime-output marker strings `"  OK"` / `"  ERROR **** crc expected:"` /
  `" found:"` (§1.3 A-M24-4). This is **observed black-box runtime OUTPUT**, not copied source — the exact
  same category of thing as this project's own established practice of capturing and parsing raw openMSX
  Tcl session output (`docs/m19-parity-trace-diff.md`, `docs/m23-parity-trace-diff.md`, etc., all GPL
  openMSX's own runtime output, already captured/quoted project-wide) or as any CI harness recognizing a
  third-party test runner's own `"PASS"`/`"FAIL"`/TAP `"ok"`/`"not ok"` output convention. Per the developer
  handoff (§8), this reasoning must be recorded as an explicit code comment in `tools/zexall-report.py`
  itself, and the parser is placed in `tools/` (Python, non-shipped, explicitly a "develop/test/debug" tool
  per the project baseline) rather than `src/`, for maximum insulation.
- **The 67 group NAMES**, if captured in a run log or report (`debug/logs/`, `docs/m24-implementation-
  report.md`), are likewise OBSERVED OUTPUT of running the binary, not transcribed source — the same
  category as this project's existing, accepted practice of capturing raw register/memory dump text
  throughout M9-M23's A/B evidence artifacts.

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`) and the "does this even belong in src/?" question

`src/CLAUDE.md`'s five folders (`core`, `devices`, `peripherals`, `machine`, `frontend`) are ALL scoped to
modeling the real Sony HB-F1XV — a CP/M/BDOS shim models **no** part of the Target Machine Specification (the
HB-F1XV never ran CP/M; MSX-DOS is a different, though related, OS). This is a genuine boundary question the
task explicitly asks the planner to resolve.

**Decision: a small, generic mechanism class DOES belong under `src/machine/`, mirroring the established
`cpu_trace_sink.h`/`debug_event_log.h`/`rom_asset_loader.h` precedent** — none of which model HB-F1XV
hardware either; all three are machine-composition-adjacent **validation/debug infrastructure**, explicitly
in the project baseline's In-Scope list ("Debug capabilities... full state dump... and logs of the execution
events"). `CpmBdosHarness` is the same category of thing: **CPU-core validation infrastructure**, reusing
`Hbf1xvMachine`'s ALREADY-PUBLIC primitives (`map_flat_ram()`, `load_memory()`, `read_memory()`,
`step_cpu_instruction()`, `cpu().state().regs()`) rather than modeling any new hardware. This mirrors the
established M10 three-part pattern for this exact class of need (a small reusable `src/machine/` class + a
`main.cpp` CLI mode + a `tools/` PowerShell driver) — see `src/machine/cpu_trace_sink.h` +
`run_parity_trace()` in `main.cpp` + `tools/openmsx-trace-parity.ps1`.

**Everything zexall/zexdoc-SPECIFIC (message-grammar parsing, per-group PASS/FAIL reporting) stays OUT of
`src/` entirely**, living in `tools/zexall-report.py` (Python, non-shipped) — this is a deliberate, tighter
boundary than even the M10 precedent, precisely because of the license-sensitivity discipline (§1.5).

| File | Responsibility | Grounding |
|---|---|---|
| `src/machine/cpm_bdos_harness.h` / `.cpp` (**new**) | `CpmBdosHarness` — a small, generic, reusable class. `LoadResult load_com(Hbf1xvMachine&, std::vector<uint8_t> image, uint16_t top_of_memory = 0xFF00)`: calls `machine.map_flat_ram()`, validates `0x0100 + image.size() < top_of_memory` (defensive, generic safety guard — NOT a zexall-specific magic number), `load_memory(0x0100, image.data(), image.size())`, writes `top_of_memory` little-endian at address `0x0006` via `load_memory`, sets `cpu().state().regs().pc = 0x0100`. `RunResult run(Hbf1xvMachine&, std::uint64_t max_instructions)`: loops `step_cpu_instruction()`; BEFORE each step, inspects `regs().pc` — if `0x0000`, sets `finished = true` and stops (CP/M warm-boot trap, A-M24-1's convention); if `0x0005`, dispatches the BDOS trap (reads `regs().c()`; `c==9` reads bytes from `regs().d()<<8 \| regs().e()` via `read_memory()` until a `'$'` (0x24) byte, appending each non-`'$'` byte to the captured-output buffer; `c==2` appends `regs().e()` directly; any other `c` value is recorded as an "unexpected BDOS function" diagnostic, non-fatal) then **synthesizes the RET effect** (`uint16_t sp = regs().sp; uint16_t ret = read_memory(sp) \| (read_memory(sp+1)<<8); regs().sp = sp+2; regs().pc = ret;`) WITHOUT calling `step_cpu_instruction()` for that iteration (the CPU never actually decodes whatever raw byte sits at `0x0005`/`0x0000`). Returns `{finished, instructions_executed, captured_output, unexpected_bdos_calls}`. | Independently derived from A-M24-1/A-M24-2/A-M24-3 (the generic CP/M convention); zero code from `zexall.z80` transcribed (§1.5). |
| `src/main.cpp` (**edit**, additive-only) | New `--cpm-run <program.com> <max_instructions> <out_log_path>` mode (mirrors the existing `--parity-trace`/`--bios-boot-trace` shape, `src/main.cpp:739-755`): reads the `.com` file, `cold_boot()`s a fresh `Hbf1xvMachine`, calls `CpmBdosHarness::load_com`+`run`, writes the captured output text verbatim to `out_log_path`, prints a one-line summary to stderr (steps, finished, unexpected BDOS calls — mirrors the existing `parity-trace: steps=... halted=...` diagnostic line style, `main.cpp:159-161`), and exits 0 if `finished` else a distinct non-zero code (an honest, non-fabricated "ran out of budget" signal, never silently reported as success). | New design, additive; mirrors `run_parity_trace()`'s existing shape exactly. |
| `tools/zexall-report.py` (**new**) | Reads a captured-output log (as written by `--cpm-run`/the harness), scans for the OBSERVED output markers `"  OK"` / `"  ERROR **** crc expected:"` / `" found:"` (§1.3 A-M24-4, §1.5) in order of appearance, and emits a structured per-group PASS/FAIL table (group index 1..67, PASS/FAIL, and — only as OBSERVED OUTPUT TEXT, never transcribed source — the group's own printed descriptive name) as Markdown/JSON. Used by both the `ctest` system test's own report-generation step and any manual/tools-driven run. Explicit code comment citing §1.5's black-box-output-vs-source-code distinction. | New design; parses OBSERVED runtime output only (§1.5). |
| `tools/openmsx-m24-zexall-parity.ps1` (**new**) | Mirrors `tools/openmsx-trace-parity.ps1`'s established Tcl pattern (`debug write memory`/`reg <name> <value>`/`debug break`+`after break`+`step`) to drive the SAME `.com` bytes through openMSX's own CPU core with the SAME BDOS-trap technique, bounded to a small prefix (§3.4). | Reuses the exact, already-working M10 Tcl mechanics (`tools/openmsx-trace-parity.ps1:66-104`, read this cycle). |

Boundary compliance: `cpm_bdos_harness.*` carries no zexall-specific knowledge (no message strings, no CRC
constants, no group names/counts) — it is a genuinely reusable "run any small CP/M-style `.com` image and
capture its C=2/C=9 BDOS output" primitive, usable by any future milestone needing the same mechanism (e.g.
running a different Z80 CP/M-style validation tool later). It is placed in `src/machine/` (not `devices/`,
`peripherals/`, or `frontend/`) because it composes `Hbf1xvMachine`'s public surface, exactly like the other
non-hardware-modeling `src/machine/` validation utilities already established (`cpu_trace_sink.h`,
`debug_event_log.h`, `rom_asset_loader.h`).

### 2.2 Runtime feasibility (task item 3) — an explicit, honest, unresolved-until-measured question

The source's own comment (`zexall.z80:68-71`, quoted verbatim: *"Each individual test case can take a few
milliseconds to execute, due to the overhead of test setup and crc calculation, so test design is a
compromise between coverage and execution time"*) is a REAL HARDWARE (1980s Z80-class silicon) design
constraint quote — it is **not** extrapolated here into any specific total-wall-clock figure for either real
hardware or this project's own harness, since neither has been measured this cycle (this project's discipline
requires never asserting an unmeasured number as fact).

What IS independently established this cycle (§1.3 A-M24-6/§1.4): **≈1,727,400 combinatorial
instruction-under-test executions per suite**, PLUS an unmeasured multiplier of surrounding driver/bookkeeping
Z80 instructions this project's OWN `step_cpu_instruction()` loop must ALSO execute for every one of those
iterations (unlike a compiled-native validator, this project's harness interprets `zexall.com`'s ENTIRE
program — `count`/`shift`/`setup`/`subyte`/`updcrc`'s 256-entry-table CRC loop/the register save-restore
cascade in `test:` — not just the ~1.7M "instructions under test" themselves). **This milestone does not
assume a specific total is "fine"** — it requires the developer to:

1. Run a SMALL subset first (e.g., the smallest few named groups by combinatorial count, per §1.3 A-M24-6's
   per-group figures — `ld162`/`ld163` at 16 each are the smallest) and measure actual wall-clock time per
   `step_cpu_instruction()` call empirically.
2. Extrapolate to the full ~1.7M-plus-overhead total and report the projected AND (once run) the actual
   measured wall-clock time for the full sweep, honestly, in the implementation report.
3. If the full sweep genuinely proves prohibitive for the standard `ctest` cadence (a real possibility this
   package does not pre-judge), the fallback is to **honestly report the actual measured runtime**, not to
   silently truncate/skip groups or fabricate a "ran to completion" claim — and to escalate to the
   coordinator/human for an explicit scope decision (e.g., a longer-running dedicated `ctest` label, or a
   documented, disclosed reduced/bounded run) rather than resolving this unilaterally. See R-M24-3.

### 2.3 openMSX A/B evidence plan (task item 4)

Grounded in the ALREADY-ESTABLISHED, working `tools/openmsx-trace-parity.ps1` Tcl mechanics (read this
cycle, `tools/openmsx-trace-parity.ps1:66-104`): `debug write memory <addr> <byte>` (poke bytes),
`reg <name> <value>` / `reg <name>` (write/read CPU registers, incl. `pc`/`sp`/`af`/`bc`/`de`/`hl`/`r`),
`debug break` + `after break <proc>` + `step` (single-step with a callback each step) — this project has
used this EXACT toolkit successfully since M10, most recently for M23's C2 evidence
(`docs/m23-parity-trace-diff.md`).

- **PRIMARY, FEASIBLE technique: mirror this project's OWN BDOS-trap harness on openMSX's Tcl side, bounded
  to a small prefix.** Using the SAME probe-machine convention as M10 (`C-BIOS_MSX2+`, a plain RAM-heavy
  MSX2+ profile — **not** the real `Sony_HB-F1XV` profile, since that machine's page 0 is real BIOS ROM,
  not writable RAM, and a fair CP/M-style flat-RAM comparison needs RAM at `0x0000-0xFFFF` on BOTH sides,
  exactly mirroring why M10 chose a RAM-only probe machine for CPU-only correctness checks): poke the
  identical `zexall.com` (or `zexdoc.com`) bytes at `0x0100`, seed `reg sp`/memory-at-6 identically, `reg pc
  0x0100`, then single-step via the SAME `after break emit / step` pattern, with the Tcl `emit` procedure
  implementing the IDENTICAL PC==5/PC==0 trap logic (read `reg c`, dispatch, synthesize RET by reading the
  stack via `debug read memory`, matching this project's own harness) for a BOUNDED prefix (a small,
  explicit instruction/step ceiling — e.g. enough to complete the first 1-2 named test groups, verified
  live at implementation time, not assumed). Diff the captured BDOS-output TEXT between this project's
  engine and openMSX's engine for that bounded prefix — a genuine, feasible, decision-relevant comparison
  (does openMSX's OWN Z80 core, driven through the identical mechanism, produce the identical "  OK"/CRC
  text for the same combinatorial slice).
- **EXPLICITLY BLOCKED / not attempted: a genuine FULL-suite live-Tcl single-step A/B** (§1.2) — the M23
  A/B evidence already discovered that live-Tcl-session real-time-vs-step-count scheduling behavior diverges
  once enough wall-clock time passes between round-trips; at ~1.7M+ steps this is not a feasible, honest
  technique, and this milestone does not pretend otherwise.
- **EXPLICITLY BLOCKED / not attempted: booting a real MSX-DOS disk to a prompt and running the binaries as
  genuine MSX-DOS transient programs** (§1.2) — depends on an unconfirmed MSX-DOS system-disk asset and the
  still-open C5 backlog item; named honestly as infeasible for THIS milestone rather than silently
  attempted or fabricated.
- Mechanics: `tools/openmsx-m24-zexall-parity.ps1` (new) → `docs/m24-parity-trace-diff.md` (new), following
  the established pattern (write raw B-side capture even on partial/blocked outcomes, never fabricate).

---

## 3. Milestone Slice Plan (M24-S0 … S5)

Every slice runs the full evidence-gate set (§6) and leaves `ctest` green across the **entire M1-M23 suite
(121 tests)**.

### M24-S0 — Commit the reference asset

- `git add references/zexall/` (README, LICENSE, `.gitattributes`, `zexall.z80`/`.com`/`.mac`,
  `zexdoc.z80`/`.com`/`.mac`, `zexlax.pl`) — no code, no test changes. Confirms the existing
  `.gitattributes` binary/text declarations apply cleanly.

### M24-S1 — `CpmBdosHarness` (the generic mechanism) + isolated unit tests

- New `src/machine/cpm_bdos_harness.h`/`.cpp` per §2.1.
- New unit tests (`tests/unit/machine/cpm_bdos_harness_unit_test.cpp`) against a **tiny, hand-written-by-
  this-project synthetic fixture** (a handful of literal Z80 bytes assembled by hand/by a comment-documented
  byte table in the TEST FILE ITSELF — e.g., `LD DE,<msg>` / `LD C,9` / `CALL 5` / `LD E,'!'` / `LD C,2` /
  `CALL 5` / `JP 0`, with a short `$`-terminated message — deliberately NOT derived from `zexall.z80`/
  `zexdoc.z80`, proving the shim's mechanics independently): confirms `load_com` rejects an oversized image
  (the defensive `top_of_memory` guard), confirms the captured-output buffer accumulates both a C=9 string
  and a C=2 character in the correct order, confirms the CP/M warm-boot (`JP 0`) trap sets `finished=true`
  and stops stepping, confirms the RET-synthesis correctly resumes execution after a BDOS call (a
  multi-BDOS-call fixture), and confirms an out-of-budget run (a fixture that never reaches `JP 0`) reports
  `finished=false` honestly rather than silently treating budget exhaustion as success.
- **Gates:** build + ctest green (full suite, 121 prior + new).

### M24-S2 — CLI mode + device-isolation invariant plumbing

- `--cpm-run` mode in `src/main.cpp` per §2.1.
- A small integration test (`tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`) exercising
  the SAME synthetic fixture through the `Hbf1xvMachine`+`CpmBdosHarness` API directly (proving the machine
  wiring, not just the class in isolation) and asserting the device-isolation invariant (§1.3 A-M24-8): PSG/
  VDP/PPI/RTC/FDC state snapshots taken immediately after `cold_boot()`+`map_flat_ram()` and again after the
  harness run are byte-for-byte identical.
- **Gates:** build + ctest green (full suite).

### M24-S3 — `tools/zexall-report.py` + the dedicated ZEXALL/ZEXDOC system integration test

- `tools/zexall-report.py` per §2.1/§1.5, with its own small Python unit test against a synthetic captured-
  output fixture proving the OK/ERROR marker-scanning grammar (§1.3 A-M24-4) before it is trusted on the real
  binaries.
- **New dedicated system test** (`tests/system/hbf1xv_m24_zexall_system_test.cpp`): loads `references/zexall/
  zexall.com` and separately `references/zexall/zexdoc.com` (via `SONY_MSX_ZEXALL_DIR`-style compile
  definition, mirroring the established `SONY_MSX_BIOS_DIR` asset-root wiring pattern — DEC-0016's standing
  watch-item explicitly applies here: this test MUST wire its own asset path correctly or it will silently
  degrade under `ctest`'s different working directory), runs each to completion via `CpmBdosHarness`
  directly (in-process, no subprocess), asserts `finished == true` (CP/M warm-boot reached, not a budget
  timeout) for both, asserts exactly 67 PASS-or-FAIL markers appear in each captured output (via the same
  marker grammar), and reports the REAL, per-group PASS/FAIL disposition — see Acceptance Criterion 4 for
  the exact, non-negotiable rule about what happens if any group reports FAIL.
- **A mandatory adversarial self-check** (mirrors this project's established "empty-side → BLOCKED,
  corrupted-field → DIVERGENCE" comparator-validation discipline used in every prior A/B harness, M11-M23):
  the developer demonstrates the harness's own discriminating power by deliberately, temporarily forcing ONE
  group to fail (e.g., temporarily truncating the max-instruction budget so the run genuinely cannot reach
  `JP 0`, or temporarily corrupting one byte of the loaded `.com` image in a controlled unit-test-only
  fixture — NOT the real committed `references/zexall/zexall.com` file) and confirming the harness correctly
  reports FAIL/INCOMPLETE rather than silently passing — then reverts the deliberate corruption. This proves
  a "100% PASS" result (if that is what is actually observed) is a genuine, discriminating finding, not a
  vacuous no-op.
- **Gates:** build + ctest green (full suite + new); actual measured wall-clock time for the new system test
  reported honestly in the implementation report (§2.2).

### M24-S4 — openMSX A/B evidence + backlog/documentation closure

- `tools/openmsx-m24-zexall-parity.ps1` → `docs/m24-parity-trace-diff.md` per §2.3.
- `docs/m24-implementation-report.md`: full per-suite PASS/FAIL table (both suites, all 67 groups each),
  measured runtime, the adversarial self-check result, and the A/B evidence disposition (bounded-prefix
  PARITY/DIVERGENCE, or honest BLOCKED where named).
- Full 34-row backlog review written into `agent-protocol/state/deferred-backlog.md` (C3 disposition per
  Acceptance Criterion 4's exact rule).
- **Gates:** all of the above green; QA sign-off required before closure (normal gate, pre-authorized
  release-decision step per the M24-M25 continuation).

---

## 4. Full Deferred-Backlog Review (all 34 rows re-affirmed)

Per DEC-0005, every planner package must consult `agent-protocol/state/deferred-backlog.md` in full and
restate every row. All 34 rows, one-line justification each (current disposition per
`agent-protocol/state/deferred-backlog.md`, read in full this cycle):

**Section A (human-directive-tracked, 2026-07-06):**
- **B1** PSG/YM2149 internals — DONE (M15), re-affirmed unchanged; M24 touches no audio device.
- **B2** RTC/RP5C01 internals — DONE (M15), re-affirmed unchanged; M24 touches no RTC device.
- **B3** FM-PAC/YM2413 device — DONE (M17), re-affirmed unchanged; M24 touches no audio device.
- **B4** MSX-JE 16 KB SRAM — DONE (M20), re-affirmed unchanged; M24 touches no memory-mapper device.
- **B5** Kanji font ROM I/O — DONE (M18), re-affirmed unchanged; M24 touches no Kanji device.
- **B6** Halnote/MSX-JE firmware mapping — DONE (M20), re-affirmed unchanged; M24 touches no cartridge/
  mapper device.
- **B7** Cartridge loading — DONE (M19), re-affirmed unchanged; M24 touches no cartridge device.
- **B8** FDC drive mechanics — DONE (M16), re-affirmed unchanged; M24 touches no FDC device (the
  device-isolation invariant, §1.3 A-M24-8, positively CONFIRMS this rather than merely assuming it).
- **B9** VRAM/V9958 VDP contract — DONE (M14), re-affirmed unchanged; M24 touches no VDP device/register
  path (`map_flat_ram()` only affects the memory decode fabric, never `vdp_`).

**Section B (other known deferrals):**
- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged; M24 adds
  no CPU-timing-affecting code (`cpm_bdos_harness.*` calls only the already-existing `step_cpu_instruction()`
  in a loop, exactly like every prior M21/M22/M23 system test).
- **C2** Z80 HALT-R increment — DONE (M23), re-affirmed unchanged; M24 touches no CPU-core file.
- **C3** ZEXDOC/ZEXALL full parity sweep — **THIS MILESTONE.** Target: closes per Acceptance Criterion 4's
  precise disposition rule (a genuine, honestly-reported completed sweep of both suites — 100% PASS if that
  is what is actually observed, or an honestly-reported and triaged finding otherwise, either outcome
  constituting genuine closure of the "run the sweep and report real results" backlog commitment).
- **C4** S1985 backup-RAM `.sram` persistence — DONE (M15), re-affirmed unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged; explicitly
  NOT touched or advanced by M24 (§1.2 — the "boot real MSX-DOS" A/B alternative is out of scope specifically
  because it would require C5's own unresolved remainder, and this package deliberately does not conflate
  the two).
- **C6** Peripherals (keyboard/joystick) — DONE (M15), re-affirmed unchanged.
- **C7** Printer + cassette — DONE (M18), re-affirmed unchanged.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — re-affirmed OPEN; unrelated to this milestone
  (indicative next milestone, M25, per the human's 2026-07-08 directive).
- **C9** SDL3 frontend — re-affirmed OPEN; unrelated to this milestone (still indicative M26).
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed OPEN; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):**
- **D1** Pixel-accurate raster rendering pipeline — DONE (M21), re-affirmed unchanged.
- **D2** Sprite rendering + collision — DONE (M22), re-affirmed unchanged.
- **D3** VDP command engine — DONE (M22), re-affirmed unchanged.
- **D4** Cycle-accurate VDP access-slot/command timing — re-affirmed **IN-PROGRESS (M23 partial)**,
  unchanged; M24 touches no VDP-timing file.
- **D5** YJK/YJK+YAE color decode — DONE (M21), re-affirmed unchanged.
- **D6** Scroll/interlace/blink/superimpose — DONE (M21), re-affirmed unchanged.
- **D7** G6/G7 planar interleave — DONE (M22, closed in full), re-affirmed unchanged.

**Section D (M17 YM2413 depth deferrals):**
- **E1** YM2413 FM-synthesis DSP/audio depth — re-affirmed OPEN; unrelated to this milestone.
- **E2** YM2413 register-write timing constraint — re-affirmed OPEN; unrelated to this milestone (a
  different device, a different port range).

**Section E (M18 printer/cassette depth deferrals):**
- **F1** Cassette tape image-format/signal fidelity — re-affirmed OPEN; unrelated to this milestone.
- **F2** Printer image/ESC-sequence rendering depth — re-affirmed OPEN; unrelated to this milestone.

**Section F (M19 cartridge-mapper depth deferrals):**
- **G1** KonamiSCC + embedded SCC chip — re-affirmed OPEN; unrelated to this milestone.
- **G2** ROM-database/heuristic mapper auto-detection — re-affirmed OPEN; unrelated to this milestone.
- **G3** Full runtime cartridge hot-plug — re-affirmed OPEN; unrelated to this milestone.
- **G4** Long tail of ~80 other mapper types — re-affirmed OPEN; unrelated to this milestone.

---

## 5. Acceptance Criteria

1. **`references/zexall/` is committed** to version control (README, LICENSE, `.gitattributes`, both `.z80`/
   `.mac`/`.com` pairs, `zexlax.pl`), discharging the 5-milestone-old uncommitted flag (M19 discovery →
   M19-M23 carried watch-item #1, `agent-protocol/state/current-phase.md:17`).
2. **`CpmBdosHarness` is a genuinely reusable, generic mechanism** — its unit tests (M24-S1) use ONLY a
   hand-written-by-this-project synthetic fixture, never `zexall.com`/`zexdoc.com` — proving the shim works
   independent of any GPL binary before the real binaries are ever loaded.
3. **Both `zexall.com` and `zexdoc.com` run to genuine completion** (`finished == true`, the CP/M warm-boot
   trap fires, not a budget-exhaustion timeout) inside a dedicated `ctest` system test, with real, captured,
   unfabricated output for all 67 named groups in each suite (134 total group verdicts).
4. **The precise, non-negotiable disposition rule for results:** if every one of the 134 group verdicts is
   PASS ("  OK"), the implementation/QA reports "100% PASS, N/N groups, real run, real CRC comparisons" —
   this is the expected, but NOT pre-assumed, outcome (an already-QA-verified core, M9/M10/M12, running
   against an independent industry-standard oracle). If ANY group reports FAIL ("  ERROR"), this is reported
   as a genuine, significant finding: the specific group name(s) (as OBSERVED OUTPUT, §1.5), the
   expected/actual CRC values (as printed), and an explicit triage note (is this plausibly a genuine CPU-core
   defect, or a harness/fixture artifact — e.g. a wrong `top_of_memory` collision) — WITHOUT silently
   patching the CPU core within this milestone (§1.2) and WITHOUT forcing a "100% PASS" narrative that does
   not match the actual captured output. QA must independently re-run and independently re-derive the
   captured pass/fail count before accepting either disposition.
5. **The adversarial self-check (M24-S3) is performed and reported**, proving the harness can detect a
   genuine failure before its "100% PASS" (or honestly-reported partial) result is trusted.
6. **The device-isolation invariant holds** (§1.3 A-M24-8): PSG/VDP/PPI/RTC/FDC state is byte-for-byte
   identical immediately after `cold_boot()`+`map_flat_ram()` and again after the full sweep completes.
7. **Zero regression**: the FULL M1-M23 suite (121 tests) plus new M24 tests pass, 100%, independently
   reproduced by both the developer and QA from a clean rebuild; `git diff` against `v1.0.23` shows no
   change to any file under `src/devices/`, `src/peripherals/`, or `src/core/` (only new `src/machine/
   cpm_bdos_harness.*` and an additive edit to `src/main.cpp`).
8. **Actual measured wall-clock runtime for the new system test is reported honestly** in the implementation
   report (§2.2) — no unmeasured claim about feasibility is presented as fact.
9. **Real openMSX A/B evidence** captured in `docs/m24-parity-trace-diff.md` for the bounded-prefix technique
   (§2.3) — genuine PARITY/DIVERGENCE, not fabricated; the full-sweep-live-Tcl and real-MSX-DOS-boot
   sub-claims are explicitly, honestly marked BLOCKED/not attempted with the reasoning from §1.2/§2.3, not
   silently skipped.
10. **Nothing from `zexall.z80`/`zexdoc.z80`'s own assembly source is copied into `src/`** (§1.5) — verified
    by the developer/QA via a direct review of the new `src/machine/cpm_bdos_harness.*` diff against the
    cited grounding, confirming it implements only the generic, independently-described CP/M convention.
11. **Full 34-row deferred-backlog review** completed and written into `deferred-backlog.md` (§4) — C3
    dispositioned per Acceptance Criterion 4's exact rule; all other 33 rows re-affirmed with a one-line
    justification, no silent drift.
12. **Evidence gates executed and captured**: `tools/validate-assets.ps1`;
    `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `cmake --build build --config Debug`;
    `ctest --test-dir build -C Debug --output-on-failure` (full suite); conditional
    `tools/openmsx-m24-zexall-parity.ps1` → `docs/m24-parity-trace-diff.md`.

---

## 6. Risks (each with a verification action)

- **R-M24-1 (a genuine CPU-core defect is found — the single highest-value, highest-sensitivity possible
  outcome of this milestone).** *Verification:* Acceptance Criterion 4's disposition rule is followed
  exactly — no silent patch, no forced "100% PASS" narrative; the coordinator/human is informed and a
  dedicated future milestone is proposed for any real fix, mirroring how M12's own gap-analysis findings were
  handled (documented, then closed via a deliberate, separately-planned hardening pass, not an ad-hoc patch).
- **R-M24-2 (the chosen `top_of_memory` constant collides with the actual `.com` file's real size/BSS
  footprint — not measured this cycle).** *Verification:* the developer measures the actual byte size of
  `references/zexall/zexall.com` and `zexdoc.com` at implementation time and confirms `0xFF00` (or an
  adjusted, still-safe value) leaves genuine headroom; the harness's own defensive size-guard (§2.1) makes a
  silent collision structurally impossible (it refuses to load rather than corrupting memory silently).
- **R-M24-3 (the full sweep's wall-clock runtime proves genuinely prohibitive for the standard `ctest`
  cadence).** *Verification:* §2.2's three-step measurement protocol (small-subset timing → extrapolation →
  actual full measurement, reported honestly) is followed; if genuinely prohibitive, escalate to the
  coordinator/human for an explicit scope decision rather than silently truncating/skipping groups or
  fabricating a "ran to completion" claim.
- **R-M24-4 (the message-format grammar parser in `tools/zexall-report.py` mis-parses a boundary case — e.g.
  a group name that happens to contain the substring `"OK"` or `"ERROR"` before the real marker, given the
  30-char `'.'`-padded name format).** *Verification:* the parser's own unit test (M24-S3) exercises this
  exact edge case (a synthetic fixture with a decoy substring inside a name field) before being trusted on
  the real captured output; the parser anchors on the marker appearing immediately after a group's own name
  block (a structural position check), not a bare substring search.
- **R-M24-5 (license-isolation risk if a future or over-eager implementation reproduces the 256-entry CRC
  table or the 67 test descriptors' byte constants verbatim under `src/` "to make the harness self-
  contained").** *Verification:* this milestone's diff contains no large (>20-entry) numeric array under
  `src/` reproducing any part of `zexall.z80`'s own data; the harness never needs to know the CRC table or
  descriptors at all (it is a generic loader/trap, not a re-implementation of the exerciser's own logic) —
  the real `.com` binaries are loaded as opaque DATA, exactly like `bios/`/`roms/` assets.
- **R-M24-6 (the "real MSX-DOS boot" A/B alternative is dismissed as infeasible without actually checking
  whether an MSX-DOS system-disk asset already exists in this repository).** *Verification:* the developer
  performs a concrete `bios/`/`roms/` directory check for any MSX-DOS kernel/COMMAND.COM-class asset before
  finalizing the BLOCKED disposition in `docs/m24-parity-trace-diff.md` — if one genuinely exists, this
  should be reported and the alternative reconsidered as a possible FUTURE (not this-cycle) technique, not
  silently assumed absent.
- **R-M24-7 (the bounded-prefix openMSX A/B technique's chosen "small prefix" is so small it proves nothing
  decision-relevant, or so large it silently becomes the infeasible full-sweep technique in disguise).**
  *Verification:* the implementation report explicitly states the exact bound chosen (instruction count or
  named-group count) and the reasoning for it, live-verified for actual Tcl round-trip feasibility at
  implementation time, mirroring R-M23-7's "narrowing must be documented, not silently under-delivered"
  discipline.
- **R-M24-8 (a future planner/developer conflates C3's closure with C5's remainder, since both involve
  "running a program to completion").** *Verification:* §1.2/§4 explicitly, textually distinguish the two —
  C3 is "run a CP/M-style validation binary via a purpose-built trap harness," C5 is "reach a real,
  unattended BIOS/MSX-DOS boot prompt" — and this package's Out-of-Scope table names the distinction
  directly so it cannot be silently merged in a future cycle's summary.

---

## 7. Developer Handoff

**Build order:** M24-S0 (commit the asset) first — trivial, unlocks everything else. M24-S1 (`CpmBdosHarness`
+ isolated unit tests against a synthetic fixture) second — proves the generic mechanism BEFORE touching the
real GPL binaries. M24-S2 (CLI mode + device-isolation invariant) third. M24-S3 (the real `zexall.com`/
`zexdoc.com` system test + report tool + adversarial self-check) fourth — the first point any real GPL binary
is loaded. M24-S4 (A/B evidence + backlog/documentation closure) last.

**Hard constraints (do not deviate without a `decisions.md` entry):**
- Do NOT copy any byte constant, table, or message string FROM `zexall.z80`/`zexdoc.z80`'s own source into
  `src/` (§1.5/R-M24-5) — the harness must remain generic; only `tools/zexall-report.py` (Python, non-shipped)
  may reason about the OBSERVED output-marker grammar.
- Do NOT touch `src/devices/cpu/*`, `src/devices/video/*`, or any other existing device file this cycle —
  if a genuine CPU-core defect is found, follow R-M24-1/Acceptance Criterion 4 (report, triage, do not
  silently patch within M24).
- Do NOT fabricate a "100% PASS" or "ran to completion" claim if the actual captured output says otherwise —
  Acceptance Criterion 4 is non-negotiable.
- Do NOT skip the small-subset timing measurement before attempting the full sweep (§2.2) — this protects
  against an open-ended, unmeasured `ctest` hang.
- Run the FULL M1-M23 suite (121 tests), not a subset, before and after each slice.

**Files to create:** `src/machine/cpm_bdos_harness.h`, `src/machine/cpm_bdos_harness.cpp`,
`tests/unit/machine/cpm_bdos_harness_unit_test.cpp`,
`tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`,
`tests/system/hbf1xv_m24_zexall_system_test.cpp`, `tools/zexall-report.py` (+ its own small unit test),
`tools/openmsx-m24-zexall-parity.ps1`, `docs/m24-implementation-report.md`, `docs/m24-parity-trace-diff.md`.

**Files to edit (additive only):** `src/main.cpp` (the new `--cpm-run` branch), `tests/CMakeLists.txt`
(register new test executables + the `SONY_MSX_ZEXALL_DIR`-style compile definition for the system test,
mirroring the established `SONY_MSX_BIOS_DIR` pattern per DEC-0016's standing watch-item),
`agent-protocol/state/deferred-backlog.md` (§4 dispositions), `agent-protocol/state/milestones.md`,
`agent-protocol/state/definition-of-done.yaml`, `agent-protocol/state/current-phase.md`.

**Evidence to capture before requesting QA:** full `ctest` output (121 + new, 0 failed, with the new
system test's measured wall-clock time called out explicitly); the real per-group PASS/FAIL table for both
suites (134 verdicts); the adversarial self-check's before/after result; the openMSX A/B trace-diff (bounded
prefix) or its honest BLOCKED disposition; the four asset/build/test evidence-gate script outputs.
