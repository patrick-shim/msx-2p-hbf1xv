# M24 Implementation Report — ZEXDOC/ZEXALL Full Parity Sweep (closes backlog C3)

- Milestone ID: M24
- Developer: MSX Developer Agent
- Planner package: `docs/m24-planner-package.md`
- Status recorded here: developer implementation COMPLETE, evidence captured. NOT yet closed —
  QA sign-off + coordinator release decision are the remaining gates (per the standing M24-M25
  continuation directive: proceed through the release-decision/tag step without an extra pause on
  a clean QA PASS; STOP and consult the human otherwise). The `deferred-backlog.md` status-column
  ledger flip and the `v1.0.24` tag are left to the coordinator at closure time.

---

## 1. Milestone Target

This is a **CPU-validation milestone**, not a new-device milestone: run the existing,
already-QA-verified Z80A CPU core (built in M9/M10/M12) against the industry-standard
ZEXALL/ZEXDOC Z80 instruction-set exerciser suite (YAZE-AG project, GPL, `references/zexall/`),
via a small, genuinely generic CP/M/BDOS trap harness. Slice plan followed exactly as scoped:

- **S0** — commit the reference asset (`references/zexall/`, staged; actual commit left to the
  coordinator's normal milestone-closure flow per this project's established pattern of one commit
  per milestone).
- **S1** — `CpmBdosHarness`: the generic mechanism, proven against a hand-written synthetic
  fixture ONLY (never the real binaries) before the real GPL binaries were ever loaded.
- **S2** — `--cpm-run` CLI mode + the device-isolation invariant, proven via the same synthetic
  fixture driven through the full `Hbf1xvMachine` API.
- **S3** — `tools/zexall-report.py` (+ its own self-check) and the dedicated system test that loads
  the REAL `references/zexall/zexall.com`/`zexdoc.com` for the first time this cycle, plus the
  mandatory adversarial self-check.
- **S4** — openMSX A/B evidence (bounded-prefix technique) + backlog/documentation closure.

## 2. Code Changes

### New (production, `src/`)

- **`src/machine/cpm_bdos_harness.h` / `.cpp`** — `CpmBdosHarness`, a small, generic, reusable
  class implementing ONLY the standard, third-party Digital Research CP/M ".com" loading
  convention (`org 0x0100` load base; the "top of memory" word at address `0x0006-0x0007`; `CALL 5`
  BDOS dispatch trap for functions C=9/print-`$`-string and C=2/console-char-out; `JP 0x0000`
  warm-boot trap). It carries **zero knowledge of zexall/zexdoc specifically** — no message
  strings, no CRC constants, no group names/counts, nothing transcribed from
  `zexall.z80`/`zexdoc.z80`'s own source (see §6, License-Isolation Confirmation, below).
  - `LoadResult load_com(Hbf1xvMachine&, std::vector<uint8_t> image, uint16_t top_of_memory =
    0xFF00)`: calls `machine.map_flat_ram()` unconditionally (harmless/idempotent even on a
    rejected load, since it writes no image bytes itself); validates
    `0x0100 + image.size() < top_of_memory` (a defensive, GENERIC safety guard, not a
    zexall-specific magic number) — refuses to load (`ImageTooLargeForMemory`, without writing the
    image, the top-of-memory word, or PC) rather than risk silently corrupting memory; otherwise
    `load_memory(0x0100, ...)`, writes `top_of_memory` little-endian at `0x0006-0x0007`, sets
    `PC = 0x0100`.
  - `RunResult run(Hbf1xvMachine&, uint64_t max_instructions)`: loops, checking `PC == 0x0000`
    (warm-boot -> `finished = true`, stop) and `PC == 0x0005` (BDOS trap: dispatch on register C,
    capture C=9/C=2 output, record any other C value as a non-fatal "unexpected BDOS call", then
    synthesize the `RET` the real BDOS entry point would eventually execute by popping the stack)
    BEFORE the budget check every iteration — neither trap consumes a `step_cpu_instruction()` call
    or counts against `max_instructions`; only genuine CPU steps do. Returns
    `{finished, instructions_executed, captured_output, unexpected_bdos_calls}`.
  - Registered in `CMakeLists.txt`'s `sony_msx_core` source list (additive line only).

### Edited (additive only)

- **`src/main.cpp`** — new `--cpm-run <program.com> <max_instructions> <out_log_path>` CLI mode,
  mirroring the existing `--parity-trace`/`--bios-boot-trace` shape exactly: reads the `.com` file,
  `cold_boot()`s a fresh `Hbf1xvMachine`, calls `CpmBdosHarness::load_com` + `run`, writes the
  captured output bytes verbatim to `out_log_path`, prints a one-line diagnostic summary to stderr
  (`instructions_executed=... finished=... unexpected_bdos_calls=... captured_bytes=...`), and
  exits `0` only on a genuine `finished` completion — a distinct non-zero exit (`3`) on budget
  exhaustion (an honest "ran out of budget" signal, never silently reported as success), or `2` on
  a file/load error. **`git diff v1.0.23` confirms this is the ONLY change to `src/main.cpp`: 67
  insertions, 0 deletions, 0 modifications to any existing function.**
- **`CMakeLists.txt`** — one additive line registering `src/machine/cpm_bdos_harness.cpp` in the
  `sony_msx_core` static-library source list.

### New (tests)

- **`tests/unit/machine/cpm_bdos_harness_unit_test.cpp`** — 13 deterministic cases against a
  hand-written, hand-assembled synthetic Z80 fixture (documented byte-by-byte in the test file
  itself; deliberately NOT derived from `zexall.z80`/`zexdoc.z80` in any way — different structure,
  different messages): the size-guard rejects an image exactly at the `top_of_memory` boundary and
  accepts one byte under it; `load_com` seeds the top-of-memory word (little-endian) and PC
  correctly; a 4-BDOS-call fixture (`LD DE,msg1/LD C,9/CALL 5` ... `LD E,'!'/LD C,2/CALL 5` ...
  repeated with a second message and character) proves the captured-output buffer accumulates C=9
  and C=2 output in the exact interleaved order, that the RET-synthesis correctly resumes execution
  after EVERY one of the 4 sequential BDOS calls (not just the first), and that `JP 0` sets
  `finished=true` cleanly; an "unexpected BDOS function" fixture (`LD C,99/CALL 5/JP 0`) proves the
  non-fatal diagnostic path; an infinite-loop fixture (`JP 0x0100`, self-jump) with a small budget
  proves an out-of-budget run honestly reports `finished=false` (not silently treated as success).
- **`tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`** — drives the SAME
  synthetic fixture through the full `Hbf1xvMachine` + `CpmBdosHarness` API (not bypassing the
  machine), additionally asserting the **device-isolation invariant**: PSG (16 registers)/VDP (32
  control regs + 15 command-engine regs + 10 status regs + 16 palette entries + the full 128 KB
  VRAM + the VRAM pointer)/PPI (control/port-C latch/selected row/cassette-motor/CAPS-LED/key-click)
  /RTC (52 registers + mode register + address latch)/FDC (status/track/sector/command registers)
  state snapshots taken immediately after `cold_boot()`+`map_flat_ram()` are BYTE-FOR-BYTE identical
  to a second snapshot taken after the harness run completes — confirming this CPU-only exerciser
  never strays into real I/O device state (it never issues `IN`/`OUT` and never calls
  `run_frame()`, so no VDP vsync/line IRQ source is ever exercised either).
- **`tests/system/hbf1xv_m24_zexall_system_test.cpp`** — the first point any real GPL binary is
  loaded: loads `references/zexall/zexall.com` and separately `references/zexall/zexdoc.com` (via
  the new `SONY_MSX_ZEXALL_DIR` compile definition, wired in `tests/CMakeLists.txt` mirroring the
  established `SONY_MSX_BIOS_DIR` pattern EXACTLY, per DEC-0016's standing watch-item), runs each to
  completion via `CpmBdosHarness` directly (in-process, no subprocess spawn), asserts
  `finished == true` for both, asserts exactly 67 PASS-or-FAIL markers appear in each captured
  output, asserts zero unexpected BDOS calls, and reports the real per-suite marker counts to
  stderr. Registered in `tests/CMakeLists.txt` with a 7200-second CTest `TIMEOUT` and a
  `m24_slow_full_sweep` CTest LABEL (so the standard evidence-gate cadence can exclude it via
  `ctest -LE m24_slow_full_sweep` — see §4's runtime disclosure below for why this is necessary,
  disclosed rather than silently resolved).

### New (tools, non-shipped)

- **`tools/zexall-report.py`** (+ its own `--self-check`, mirroring the established
  `mem-to-png.py`/`mem-to-audio.py` self-check convention) — parses a captured-output log (as
  written by `--cpm-run`) into a structured per-group PASS/FAIL Markdown table. Recognizes ONLY the
  program's own OBSERVED, black-box runtime-output markers (`"  OK"` / `"  ERROR **** crc
  expected:"` / `" found:"`) — an explicit code comment in the file cites the planner package's
  §1.5 license-isolation reasoning for why this is fundamentally different from copying source.
  Anchors parsing on the exerciser's own fixed 30-byte, `.`-padded group-name field structure (per
  the `tmsg` macro, independently confirmed by reading `zexall.z80:183-192`/`1205-1210` this
  cycle): after the startup banner, each group's name is read as exactly the next 30 bytes, and the
  PASS/FAIL marker is required to start at that EXACT computed offset — never searched for
  anywhere else in the buffer. This defeats the R-M24-4 "decoy substring" risk (a group name that
  happens to itself contain "OK"/"ERROR") **by construction**; `run_self_check()` proves this with
  a synthetic fixture containing exactly that decoy pattern, plus a missing-banner and a
  corrupted-terminator malformed-input case (both must raise, not silently pass).
- **`tools/openmsx-m24-zexall-parity.ps1`** — the openMSX A/B bounded-prefix harness (§5).

### Unmodified (verified, not merely asserted)

`git diff v1.0.23 --name-only -- src/devices/ src/peripherals/ src/core/` returns **empty** — zero
changes to any CPU-core, device, peripheral, or core-scheduling file. The Z80A CPU core
(`src/devices/cpu/z80a_cpu.cpp` and everything else under `src/devices/`) is byte-for-byte
unchanged this cycle, confirmed directly, not merely claimed.

## 3. Unit Test Results

```
ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure
...
122/123 Test #122: machine_cpm_bdos_harness_unit_test .................................   Passed    0.03 sec
123/123 Test #123: machine_hbf1xv_m24_cpm_run_integration_test ........................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 123
Total Test time (real) =   3.43 sec
```

`machine_cpm_bdos_harness_unit_test` (13 cases, all against the hand-written synthetic fixture,
NEVER the real zexall/zexdoc binaries — proving the generic mechanism in complete isolation):

- `LoadCom_ExactlyAtTopOfMemoryBoundary_Rejected`
- `LoadCom_OneByteUnderTopOfMemoryBoundary_Accepted`
- `LoadCom_TopOfMemoryWordAndPc_SeededCorrectly`
- `MultiBdosFixture_Load_Ok`
- `MultiBdosFixture_WarmBootReached_FinishedTrue`
- `MultiBdosFixture_CapturedOutput_InterleavedOrderCorrect`
- `MultiBdosFixture_NoUnexpectedBdosCalls`
- `MultiBdosFixture_RealInstructionsWereExecuted`
- `UnexpectedBdosFixture_WarmBootReached_FinishedTrue`
- `UnexpectedBdosFixture_FunctionCodeRecorded_NinetyNine`
- `UnexpectedBdosFixture_NoOutputCaptured`
- `InfiniteLoopFixture_OutOfBudget_FinishedFalse`
- `InfiniteLoopFixture_OutOfBudget_InstructionsExecutedEqualsBudget`

All 13 PASS. Full regression: **123/123** (121 prior M1-M23 + 2 new fast M24 tests), 0 failed,
independently re-run from a clean `cmake --build build --config Debug`.

`tools/zexall-report.py --self-check`:

```
Self-check: parsing a synthetic fixture with decoy OK/ERROR substrings inside group names
  [OK] exactly 2 groups parsed
  [OK] group 1 verdict is PASS (not falsely short-circuited by the decoy 'OK')
  [OK] group 1 name captured verbatim (including the decoy substring)
  [OK] group 2 verdict is FAIL
  [OK] group 2 name captured verbatim (including the decoy substring)
  [OK] group 2 expected CRC parsed correctly
  [OK] group 2 found CRC parsed correctly
  [OK] missing-banner buffer raises ParseError
  [OK] corrupted OK terminator raises ParseError
SELF-CHECK PASSED
```

## 4. Integration / System Test Results — THE REAL ZEXALL/ZEXDOC SWEEP

### 4.1 Runtime measurement protocol (per planner §2.2 — never assume, always measure)

1. **Small-subset timing first.** Ran `--cpm-run references/zexall/zexall.com <budget> <out>` at
   increasing budgets (200,000 / 2,000,000 / 10,000,000 / 50,000,000 / 100,000,000 / 200,000,000 /
   300,000,000 instructions) and inspected the captured-output length at each point. Group 1
   (`adc16`, `<adc,sbc> hl,<bc,de,hl,sp>`) — independently re-summed this cycle from the source's
   own per-group header annotations (see §4.2) at **38,912** combinatorial cases, ~2.25% of the
   suite's total — did not complete ("  OK" not yet printed) at 250,000,000 instructions but had
   completed by 300,000,000. Measured throughput at this scale: ~8.3-8.5 million
   `step_cpu_instruction()` calls/second on this development machine.
2. **Extrapolation.** At ~2.25% of the suite completing in ~270-300M instructions, the full suite
   was projected at roughly 12-13.3 billion instructions, ~25-30 minutes wall-clock — clearly a
   multi-minute, not sub-second, cost, and a genuine candidate for R-M24-3 ("prohibitive for the
   standard ctest cadence").
3. **Actual full-sweep measurement (real, not projected).** Ran BOTH suites to completion via the
   production `sony_msx_headless --cpm-run` CLI with a 30-billion-instruction budget (comfortably
   above the extrapolated need):

   ```
   === zexall.com full sweep started 2026-07-07T10:46:22Z ===
   cpm-run: instructions_executed=5764169474 finished=1 unexpected_bdos_calls=0 captured_bytes=2453
   real    12m28.142s
   zexall exit=0
   === zexall.com full sweep ended 2026-07-07T10:58:50Z ===
   === zexdoc.com full sweep started 2026-07-07T10:58:50Z ===
   cpm-run: instructions_executed=5764169474 finished=1 unexpected_bdos_calls=0 captured_bytes=2453
   real    11m24.860s
   zexdoc exit=0
   === zexdoc.com full sweep ended 2026-07-07T11:10:15Z ===
   ```

   **Real, measured wall-clock time: 12m28.142s (zexall.com) + 11m24.860s (zexdoc.com) = ~23m53s
   total for both suites**, each executing **5,764,169,474** real `step_cpu_instruction()` calls
   (both suites needed the IDENTICAL total instruction count — expected, since A-M24-5/A-M24-7
   establish the two suites share identical combinatorics, differing only in the flag-mask byte and
   expected-CRC constant, neither of which affects the instruction SEQUENCE executed). Both
   completed with `finished=1` (genuine CP/M warm-boot, not budget exhaustion) and
   `unexpected_bdos_calls=0`.

4. **The real ctest-registered system test** (`hbf1xv_m24_zexall_system_test`, which performs this
   IDENTICAL computation for BOTH suites sequentially, in-process, via the same
   `CpmBdosHarness::load_com`/`run` calls) was run directly via
   `ctest --test-dir build -C Debug -R hbf1xv_m24_zexall_system_test`:

   ```
   Test project C:/Users/pashim/source/sony-msx-hbf1xv/build
       Start 124: hbf1xv_m24_zexall_system_test
   1/1 Test #124: hbf1xv_m24_zexall_system_test ....   Passed  1453.25 sec

   100% tests passed, 0 tests failed out of 1

   Label Time Summary:
   m24_slow_full_sweep    = 1453.25 sec*proc (1 test)

   Total Test time (real) = 1453.27 sec
   ```

   **This is the genuine, ctest-driven confirmation**: `1453.25` seconds (~24m13s) for both suites
   run sequentially inside the SAME test process, closely matching (within measurement/scheduling
   variance) the ~23m53s measured via the standalone `--cpm-run` CLI invocations above — as
   expected from a fully deterministic computation performed via the identical
   `CpmBdosHarness::load_com`/`run` code path either way. `Passed` confirms the test's own internal
   assertions (`finished == true` for both suites, exactly 67 PASS-or-FAIL markers each, zero
   unexpected BDOS calls) all held.

### 4.2 Independently re-derived combinatorial total (recompute, don't trust)

Summed all 67 groups' own header-comment "(N cycles)" annotations this cycle directly from
`references/zexall/zexall.z80` (a Python pass over every `^; .*\([\d,]+ cycles\)$`-shaped
full-line comment, plus the one group — `<daa,cpl,scf,ccf>` — whose header comment omits the
aggregate figure, for which the increment-vector sub-annotation `65,536 cycles` (independently
verified: 2^(popcount of its 10-bit-set increment vector) = 1024, and the accompanying source
snippet directly states `65,536`) is the group total since its shift vector is `1 cycle`):
**1,727,144** total combinatorial "instruction under test" executions per suite. This is very
close to, but not textually identical to, the planner package's own stated "≈1,727,400" figure — a
minor (0.015%) discrepancy worth a QA re-check, not a material one.

### 4.3 REAL per-suite, per-group PASS/FAIL disposition (134 total verdicts)

Parsed via `python tools/zexall-report.py build/m24_scratch/zexall_full_output.log` and
`build/m24_scratch/zexdoc_full_output.log` (the exact captured-output bytes from the real
`--cpm-run` invocations above) — **NOT fabricated, NOT assumed**: both parses independently report
**67/67 groups parsed, 67 PASS, 0 FAIL** for each suite.

| # | Group name (observed output) | ZEXALL | ZEXDOC |
|---|---|---|---|
| 1 | `<adc,sbc> hl,<bc,de,hl,sp>....` | PASS | PASS |
| 2 | `add hl,<bc,de,hl,sp>..........` | PASS | PASS |
| 3 | `add ix,<bc,de,ix,sp>..........` | PASS | PASS |
| 4 | `add iy,<bc,de,iy,sp>..........` | PASS | PASS |
| 5 | `aluop a,nn....................` | PASS | PASS |
| 6 | `aluop a,<b,c,d,e,h,l,(hl),a>..` | PASS | PASS |
| 7 | `aluop a,<ixh,ixl,iyh,iyl>.....` | PASS | PASS |
| 8 | `aluop a,(<ix,iy>+1)...........` | PASS | PASS |
| 9 | `bit n,(<ix,iy>+1).............` | PASS | PASS |
| 10 | `bit n,<b,c,d,e,h,l,(hl),a>....` | PASS | PASS |
| 11 | `cpd<r>........................` | PASS | PASS |
| 12 | `cpi<r>........................` | PASS | PASS |
| 13 | `<daa,cpl,scf,ccf>.............` | PASS | PASS |
| 14 | `<inc,dec> a...................` | PASS | PASS |
| 15 | `<inc,dec> b...................` | PASS | PASS |
| 16 | `<inc,dec> bc..................` | PASS | PASS |
| 17 | `<inc,dec> c...................` | PASS | PASS |
| 18 | `<inc,dec> d...................` | PASS | PASS |
| 19 | `<inc,dec> de..................` | PASS | PASS |
| 20 | `<inc,dec> e...................` | PASS | PASS |
| 21 | `<inc,dec> h...................` | PASS | PASS |
| 22 | `<inc,dec> hl..................` | PASS | PASS |
| 23 | `<inc,dec> ix..................` | PASS | PASS |
| 24 | `<inc,dec> iy..................` | PASS | PASS |
| 25 | `<inc,dec> l...................` | PASS | PASS |
| 26 | `<inc,dec> (hl)................` | PASS | PASS |
| 27 | `<inc,dec> sp..................` | PASS | PASS |
| 28 | `<inc,dec> (<ix,iy>+1).........` | PASS | PASS |
| 29 | `<inc,dec> ixh.................` | PASS | PASS |
| 30 | `<inc,dec> ixl.................` | PASS | PASS |
| 31 | `<inc,dec> iyh.................` | PASS | PASS |
| 32 | `<inc,dec> iyl.................` | PASS | PASS |
| 33 | `ld <bc,de>,(nnnn).............` | PASS | PASS |
| 34 | `ld hl,(nnnn)..................` | PASS | PASS |
| 35 | `ld sp,(nnnn)..................` | PASS | PASS |
| 36 | `ld <ix,iy>,(nnnn).............` | PASS | PASS |
| 37 | `ld (nnnn),<bc,de>.............` | PASS | PASS |
| 38 | `ld (nnnn),hl..................` | PASS | PASS |
| 39 | `ld (nnnn),sp..................` | PASS | PASS |
| 40 | `ld (nnnn),<ix,iy>.............` | PASS | PASS |
| 41 | `ld <bc,de,hl,sp>,nnnn.........` | PASS | PASS |
| 42 | `ld <ix,iy>,nnnn...............` | PASS | PASS |
| 43 | `ld a,<(bc),(de)>..............` | PASS | PASS |
| 44 | `ld <b,c,d,e,h,l,(hl),a>,nn....` | PASS | PASS |
| 45 | `ld (<ix,iy>+1),nn.............` | PASS | PASS |
| 46 | `ld <b,c,d,e>,(<ix,iy>+1)......` | PASS | PASS |
| 47 | `ld <h,l>,(<ix,iy>+1)..........` | PASS | PASS |
| 48 | `ld a,(<ix,iy>+1)..............` | PASS | PASS |
| 49 | `ld <ixh,ixl,iyh,iyl>,nn.......` | PASS | PASS |
| 50 | `ld <bcdehla>,<bcdehla>........` | PASS | PASS |
| 51 | `ld <bcdexya>,<bcdexya>........` | PASS | PASS |
| 52 | `ld a,(nnnn) / ld (nnnn),a.....` | PASS | PASS |
| 53 | `ldd<r> (1)....................` | PASS | PASS |
| 54 | `ldd<r> (2)....................` | PASS | PASS |
| 55 | `ldi<r> (1)....................` | PASS | PASS |
| 56 | `ldi<r> (2)....................` | PASS | PASS |
| 57 | `neg...........................` | PASS | PASS |
| 58 | `<rrd,rld>.....................` | PASS | PASS |
| 59 | `<rlca,rrca,rla,rra>...........` | PASS | PASS |
| 60 | `shf/rot (<ix,iy>+1)...........` | PASS | PASS |
| 61 | `shf/rot <b,c,d,e,h,l,(hl),a>..` | PASS | PASS |
| 62 | `<set,res> n,<bcdehl(hl)a>.....` | PASS | PASS |
| 63 | `<set,res> n,(<ix,iy>+1).......` | PASS | PASS |
| 64 | `ld (<ix,iy>+1),<b,c,d,e>......` | PASS | PASS |
| 65 | `ld (<ix,iy>+1),<h,l>..........` | PASS | PASS |
| 66 | `ld (<ix,iy>+1),a..............` | PASS | PASS |
| 67 | `ld (<bc,de>),a................` | PASS | PASS |

**Disposition per Acceptance Criterion 4's non-negotiable rule: all 134 of 134 group verdicts (67
ZEXALL + 67 ZEXDOC) are genuinely PASS ("  OK"). This is the expected outcome (an already-QA-
verified CPU core, from M9/M10/M12, running against an independent industry-standard oracle) — it
was NOT pre-assumed; it was actually observed, via a real, full, unfabricated run of both suites to
genuine CP/M warm-boot completion.** No group reported FAIL; there is no triage disposition to
report on this axis.

### 4.4 Mandatory adversarial self-check (proving the harness can detect a genuine failure)

Per the milestone's explicit mandate, before trusting the 100% PASS result: deliberately corrupted
**one byte of an IN-MEMORY COPY** of the real, committed `references/zexall/zexall.com` (the real
file on disk was NEVER written to — verified before/after via `git status` showing only the
pre-existing untracked marker and via `sha256sum` showing an unchanged digest
`6e2da55147a04f28d303d5da6a1e6b771557ac244653590a0f24a2d39c8537e8`, 8704 bytes, both times).

The corrupted byte: file offset 255 (the first of the `adc16` group's own 4-byte "expected CRC"
constant, independently located this cycle by searching the file for the literal group-name text
`<adc,sbc> hl,<bc,de,hl,sp>` and reading the 4 bytes immediately preceding it — `D4 8A D5 19`),
XORed with `0xFF` to `2B 8A D5 19`. Ran this corrupted in-memory image through the REAL, UNMODIFIED
`CpmBdosHarness` (budget 400,000,000 instructions, enough to complete just group 1):

```
Corrupting in-memory byte[255]: 0xd4 -> 0x2b (real committed file on disk untouched)
instructions_executed=400000000 finished(should be false...)=0 captured_bytes=138
```

Parsed the resulting captured output with the SAME, unmodified `tools/zexall-report.py`:

```
Groups parsed: 1 (expected 67)
PASS: 0  FAIL: 1

| # | Verdict | Name (observed output) | Expected CRC | Found CRC |
|---|---------|-------------------------|--------------|-----------|
| 1 | FAIL | `<adc,sbc> hl,<bc,de,hl,sp>....` | 2b8ad519 | d48ad519 |
```

**Result: the harness and the report tool BOTH correctly detected and reported a genuine failure**
— `FAIL`, with the corrupted expected value (`2B8AD519`) and the REAL, correctly-computed actual
CRC (`D48AD519`, matching the ORIGINAL uncorrupted constant — confirming the CPU's own computation
is genuinely correct; only the artificially-corrupted "expected" comparison value differs) — rather
than silently reporting success. This proves the 100% PASS result reported in §4.3 is a genuine,
discriminating finding, not a vacuous no-op. The deliberate corruption existed only in a throwaway,
in-memory `std::vector<uint8_t>` inside a disposable development-only harness; it was never written
to disk, and the real `references/zexall/zexall.com` is confirmed byte-identical before and after.

## 5. openMSX A/B Evidence (`docs/m24-parity-trace-diff.md`)

**PRIMARY, FEASIBLE technique — bounded-prefix live-Tcl BDOS trap, mirroring
`tools/openmsx-trace-parity.ps1`'s established mechanics.** `tools/openmsx-m24-zexall-parity.ps1`
pokes the real `zexall.com`/`zexdoc.com` bytes into a RAM-only probe machine (`C-BIOS_MSX2+`) and
single-steps through the IDENTICAL PC==5/PC==0 BDOS-trap logic as `CpmBdosHarness` itself, bounded
to a small, live-verified prefix.

**A genuine, real environment finding surfaced and fixed during implementation**: an initial
attempt to poke bytes directly at address `0x0100` silently failed (`ram_base_readback` did not
match the poked byte) — `C-BIOS_MSX2+`'s page 0 defaults to its own BIOS ROM, not writable RAM
(confirmed this cycle by reading `/usr/share/openmsx/machines/C-BIOS_MSX2+.xml`: primary slot 0 =
C-BIOS ROM; the 512 KB "Main RAM" `MemoryMapper` lives at primary slot 3, secondary slot 2). Fixed
by issuing the SAME kind of slot-routing I/O writes this project's OWN `map_flat_ram()` uses against
ITS OWN slot map, adapted to this probe machine's layout: `debug write ioports 0xA8 0xFF` (route
all 4 pages to primary 3) + `debug write memory 0xFFFF 0xAA` (select secondary 2, the MemoryMapper,
for all 4 pages) + explicit `0xFC-0xFF` segment-register writes for a clean, deterministic flat
64 KB identity mapping. After this fix, the readback matched exactly.

**Bound chosen (R-M24-7 disclosure): 11 real Z80 instructions** — empirically, live-verified at
implementation time (not assumed): both zexall.com and zexdoc.com execute EXACTLY 11 real
instructions from the CP/M entry point (`JP start`; `LD HL,(6)`; `LD SP,HL`; `LD DE,msg1`;
`LD C,9`; `CALL bdos`; 4× `PUSH`; `CALL 5`) before PC first reaches the BDOS trap address, where
the startup banner (the FIRST BDOS call, C=9, `"Z80 instruction exerciser"`) is captured. A larger
bound ("complete the first 1-2 named test groups", as the planner package originally suggested)
was found NOT live-Tcl-feasible: even the SMALLEST named group needs on the order of 10^5-10^8 real
instructions (§4.1) — firmly in the SAME infeasible-for-live-Tcl territory the full sweep already
is. This smaller bound is a genuine, honest, decision-relevant substitute: it exercises the actual
NEW mechanism this milestone adds (the loading convention + BDOS dispatch trap), not a
re-verification of CPU-instruction-level correctness the M9/M10/M12 oracles and the in-process real
system test (§4) already cover.

**Result: genuine PARITY for BOTH suites.**

```
Emulator A (zexall) captured: 'Z80 instruction exerciser'
openMSX B (zexall) captured: 'Z80 instruction exerciser'
-> PARITY

Emulator A (zexdoc) captured: 'Z80 instruction exerciser'
openMSX B (zexdoc) captured: 'Z80 instruction exerciser'
-> PARITY

M24 C3 bounded-prefix A/B RESULT: PARITY (both suites). See docs/m24-parity-trace-diff.md
```

Both engines, driven through the identical loading convention + BDOS-trap mechanism, produce
byte-identical captured BDOS output for this bounded 11-instruction prefix, for both zexall.com and
zexdoc.com.

**Explicitly BLOCKED / not attempted (honest disposition, not silently skipped):**

1. **A genuine full-suite live-Tcl single-step A/B of the entire ZEXALL/ZEXDOC run.** Infeasible
   per the real-time-scheduling artifact `docs/m23-parity-trace-diff.md` already discovered (a live
   openMSX Tcl session's scheduler behaves differently once enough real/wall-clock time elapses
   between per-step round-trips), compounded by the sheer instruction volume this milestone's own
   runtime measurement found necessary (§4.1: ~5.76 billion real steps per suite).
2. **Booting a real MSX-DOS disk to a command prompt and running the binaries as genuine MSX-DOS
   transient programs.** Depends on an MSX-DOS system-disk asset. Checked this cycle, not assumed
   absent: `ls bios/` and `ls roms/` were both listed in full — `bios/` contains only the Sony
   HB-F1XV's own BIOS/SUB/Kanji-driver/disk-ROM/FM-MUSIC/Kanji-font/Halnote-firmware images;
   `roms/` contains only the two disclosed cartridge-mapper smoke fixtures (`aleste.rom`,
   `metalgear.rom`). No MSX-DOS kernel/COMMAND.COM-class asset exists in either directory. Also
   depends on the still-open backlog **C5** remainder (full unattended boot) — not conflated with
   C3's own closure (§1.2 of the planner package draws this distinction explicitly).

   **NOTE (2026-07-08, added post-implementation, DEC-0021):** the human has since added real
   MSX-DOS system-disk assets at `disks/msxdos22.dsk`, `disks/msxdos23.dsk`, and an unpacked
   `disks/msxdos24/` tree — verified present at the time this note was added. This does NOT
   retroactively obligate this milestone to redo the disk-boot-A/B sub-claim (per DEC-0021,
   ratified by the coordinator): the "no MSX-DOS asset found" reasoning above is now stale for any
   FUTURE attempt, but this milestone's own already-gathered evidence stands as recorded above,
   genuinely reflecting the state at the time it was gathered. Whether to redo this specific
   sub-claim for M24 itself, or leave it BLOCKED-as-recorded and instead reserve `disks/` for a
   dedicated future C5 (full auto-disk-boot-trigger) investigation, is left as an explicit
   judgment call for QA/closure — not force-resolved by this developer.

## 6. License-Isolation Confirmation

- `src/machine/cpm_bdos_harness.{h,cpp}` contains **zero** lines transcribed from
  `zexall.z80`/`zexdoc.z80`'s own source: no message strings, no CRC table, no test descriptors, no
  group names or counts. It implements only the generic, third-party CP/M loading convention
  (`org 0x0100`; the "top of memory" word; `CALL 5`/`JP 0`), independently described from first
  principles and cross-checked against the exerciser's OWN use of that convention (a client
  relationship, not an origination — the convention predates and is external to the YAZE-AG
  project).
- `git diff v1.0.23 --name-only -- src/` shows exactly two touched paths: `src/main.cpp` (67
  additive lines, confirmed via `git diff --stat`) and the two new, untracked
  `src/machine/cpm_bdos_harness.{h,cpp}` files. Nothing under `src/devices/`, `src/peripherals/`,
  or `src/core/` was touched — confirmed via `git diff v1.0.23 --name-only -- src/devices/
  src/peripherals/ src/core/`, which returns empty.
- The one design point closest to "derived from the binary's own content" is
  `tools/zexall-report.py`'s recognition of the literal runtime-output marker strings — this is
  OBSERVED BLACK-BOX RUNTIME OUTPUT, not copied source (an explicit code comment in that file cites
  this reasoning), the same category as this project's own established practice of capturing and
  parsing raw openMSX Tcl session output throughout M10-M23's own A/B evidence artifacts. It lives
  in `tools/` (Python, non-shipped), not `src/`, for maximum insulation.
- `references/zexall/` itself (README, LICENSE, `.gitattributes`, both `.z80`/`.mac`/`.com` pairs,
  `zexlax.pl`) is staged for commit as a read-only, GPL-licensed validation reference — the exact
  same treatment `references/openmsx-21.0/` already receives project-wide. Running the pre-built
  `.com` binaries as black-box DATA test fixtures is categorically different from, and far
  lower-risk than, transcribing the project's own source code or data tables into `src/`.

## 7. Full Deferred-Backlog Review (all 34 rows, per DEC-0005)

**Section A (human-directive-tracked, 2026-07-06):** B1-B9 all re-affirmed **DONE** (M15/M15/M17/
M20/M18/M20/M19/M16/M14 respectively), unchanged; M24 touches no PSG/RTC/YM2413/SRAM/Kanji/
Halnote/cartridge/FDC/VDP device file (confirmed via `git diff v1.0.23 -- src/devices/`, empty).

**Section B (other known deferrals):**

- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged;
  M24 adds no CPU-timing-affecting code (`cpm_bdos_harness.*` calls only the already-existing
  `step_cpu_instruction()` in a loop, exactly like every prior M21/M22/M23 system test).
- **C2** Z80 HALT-R increment — re-affirmed **DONE (M23)**, unchanged; M24 touches no CPU-core
  file (confirmed via `git diff v1.0.23 -- src/devices/cpu/`, empty).
- **C3** ZEXDOC/ZEXALL full parity sweep — **THIS MILESTONE.** Implementation complete this cycle:
  134/134 group verdicts genuinely PASS (§4.3), adversarial self-check performed and passed (§4.4),
  bounded-prefix openMSX A/B PARITY for both suites (§5). Ledger status transition to DONE (M24)
  reserved for the coordinator at closure, pending QA sign-off (see `deferred-backlog.md`'s own
  C3 row for the full disposition text).
- **C4** S1985 backup-RAM `.sram` persistence — re-affirmed **DONE (M15)**, unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged;
  explicitly NOT touched or advanced by M24 (the "boot real MSX-DOS" A/B alternative was
  considered and explicitly marked BLOCKED, §5, precisely because it would require C5's own
  unresolved remainder — not conflated with C3's own closure).
- **C6** Peripherals (keyboard/joystick) — re-affirmed **DONE (M15)**, unchanged.
- **C7** Printer + cassette — re-affirmed **DONE (M18)**, unchanged.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — re-affirmed **OPEN**; unrelated to this
  milestone (indicative next milestone, M25).
- **C9** SDL3 frontend — re-affirmed **OPEN**; unrelated to this milestone (indicative M26).
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed **OPEN**; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):** D1-D3, D5-D7 re-affirmed **DONE** (M21/M22/M21/M21/M22
respectively), unchanged; **D4** re-affirmed **IN-PROGRESS (M23 partial)**, unchanged — M24 touches
no VDP-timing file.

**Section D (M17 YM2413 depth deferrals):** E1 (DSP/audio depth), E2 (register-write timing
constraint) — both re-affirmed **OPEN**; unrelated to this milestone (different device).

**Section E (M18 printer/cassette depth deferrals):** F1 (cassette tape-format fidelity), F2
(printer rendering depth) — both re-affirmed **OPEN**; unrelated to this milestone.

**Section F (M19 cartridge-mapper depth deferrals):** G1 (KonamiSCC+SCC chip), G2 (ROM-database
auto-detection), G3 (runtime hot-plug), G4 (long tail of ~80 mapper types) — all re-affirmed
**OPEN**; unrelated to this milestone.

All 34 rows accounted for; only C3 changes disposition this cycle (implementation complete,
pending QA — not yet flipped to DONE in the ledger, per the coordinator-reserved closure
transition).

## 8. Known Issues / Residual Risks

- **The `hbf1xv_m24_zexall_system_test` ctest run genuinely takes ~24 minutes** (both suites,
  sequentially, in one process) — a disclosed, deliberate consequence of Acceptance Criterion 3 (a
  genuine, non-truncated full-suite run), not an oversight. It is registered with a CTest LABEL
  (`m24_slow_full_sweep`) so the routine evidence-gate cadence can exclude it explicitly via
  `ctest -LE m24_slow_full_sweep` (as this report's own §3 fast-suite run does) while still being
  genuinely present and runnable via the default, unfiltered `ctest` invocation, satisfying the
  literal acceptance criterion. This is flagged explicitly for the coordinator/QA to weigh in on
  per R-M24-3's own anticipated escalation path (e.g., whether a permanent CI/evidence-gate
  convention should always pass `-LE m24_slow_full_sweep` going forward) — not resolved unilaterally
  by this developer.
- **Minor, non-blocking arithmetic discrepancy** (§4.2): this cycle's independently-recomputed
  total combinatorial case count (1,727,144) differs from the planner package's own stated
  "≈1,727,400" figure by 256 (0.015%). Both figures are approximate/independently-derived, neither
  is load-bearing for any pass/fail gate (the actual gate is the suite's OWN 67-group "OK"/"ERROR"
  verdict, not this combinatorial-count arithmetic) — flagged for QA's own independent re-check per
  the planner package's own A-M24-6 verification action.
- **No genuine CPU-core defect was found.** Per Acceptance Criterion 4/R-M24-1, this is reported
  as the expected-but-not-pre-assumed outcome, with real evidence (§4.3/§4.4), not asserted from
  memory or precedent alone.
- `references/zexall/` is staged (`git add`) but this developer did not create the actual `git
  commit` — commit creation is reserved for the milestone-closure flow, matching this project's
  established one-commit-per-milestone pattern (every prior milestone's tag/commit was created at
  closure time, not mid-slice by the developer).

## 9. QA Handoff

- Full regression: **123/123** (fast suite, `-LE m24_slow_full_sweep`) + the dedicated
  `hbf1xv_m24_zexall_system_test` (124th test, genuinely executed via `ctest -R
  hbf1xv_m24_zexall_system_test`, `Passed 1453.25 sec` — see §4.1 item 4 for the full captured
  ctest output). 124/124 total, 0 failed.
- `git diff v1.0.23` confirms zero changes to `src/devices/`, `src/peripherals/`, `src/core/`; only
  `src/main.cpp` (additive) and two new `src/machine/cpm_bdos_harness.*` files.
- 134/134 group verdicts PASS (67 ZEXALL + 67 ZEXDOC), genuinely observed, not assumed.
- Adversarial self-check performed and passed (§4.4) — the harness/tooling genuinely detects a
  real failure when one is deliberately introduced (in-memory only; the real committed file is
  confirmed byte-identical before and after via SHA-256).
- openMSX A/B: genuine PARITY for the bounded 11-instruction prefix on both suites
  (`docs/m24-parity-trace-diff.md`); full-suite live-Tcl A/B and MSX-DOS-disk-boot A/B both
  explicitly, honestly BLOCKED with concrete reasoning (not silently skipped).
- QA should independently: (1) re-run the fast suite fresh; (2) independently re-run
  `ctest -R hbf1xv_m24_zexall_system_test` (or the equivalent `--cpm-run` invocations) from a clean
  rebuild and confirm the SAME 134/134 PASS disposition and instruction counts; (3) independently
  re-derive at least a sample of the 67 per-group combinatorial totals (§4.2) and reconcile the
  1,727,144-vs-≈1,727,400 discrepancy; (4) independently review the adversarial self-check's
  reasoning and confirm the real `references/zexall/zexall.com` file is genuinely untouched; (5)
  weigh in on the `m24_slow_full_sweep` CTest-label disclosure (§7/§8) as a permanent
  evidence-gate-cadence question; (6) per DEC-0021 (recorded mid-cycle, `disks/` MSX-DOS assets
  now present): make the explicit, disclosed judgment call on whether M24's own disk-boot-A/B
  sub-claim should be redone against the new `disks/` assets before closure, or left BLOCKED-as-
  recorded (§5's NOTE) with `disks/` reserved for a future dedicated C5 investigation instead —
  this developer did not force-resolve that question, per DEC-0021's own explicit terms.
